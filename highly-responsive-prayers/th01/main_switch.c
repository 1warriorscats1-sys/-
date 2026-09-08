/* main_switch.c - libnx entry point for the Switch homebrew .nro.
 *
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2026 contributors to the 1warriorscats1-sys/- repository.
 *
 * Build: devkitA64 + libnx (see scripts/build_th01.py --target switch).
 * No Nintendo SDK, no proprietary runtime and no game data are used.
 *
 * Data layout on the SD card:
 *   sdmc:/switch/th01/th01open.dat   optional open content package (BYOD)
 *   sdmc:/switch/th01/th01.sav       high score / clear counter
 *   sdmc:/switch/th01/th01.log       start-up diagnostics
 */
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <switch.h>

#include "hrp.h"
#include "hrp_save.h"
#include "hrp_frame_policy.h"

#define HRP_SD_DIR      "sdmc:/switch/th01"
#define HRP_PKG_PATH    HRP_SD_DIR "/th01open.dat"
#define HRP_LOG_PATH    HRP_SD_DIR "/th01.log"
#define HRP_SCREEN_W    640
#define HRP_SWITCH_W    1280
#define HRP_SWITCH_H    720
#define HRP_SCALE       2
#define HRP_AUDIO_RATE  48000
#define HRP_AUDIO_FRAMES (HRP_AUDIO_RATE / 60)

#include "hrp_switch_mapping.inc"

static hrp_game g_game;
static uint32_t g_framebuffer[HRP_SCREEN_W * HRP_SCREEN_H];
static hrp_stage_def g_stages[HRP_MAX_STAGES];
static int16_t g_pcm[HRP_AUDIO_FRAMES * 2];

static FILE *g_log;

static void hrp_log_line(const char *fmt, ...)
{
    if (!g_log) return;
    va_list args;
    va_start(args, fmt);
    vfprintf(g_log, fmt, args);
    va_end(args);
    fprintf(g_log, "\n");
    fflush(g_log);
}

static uint16_t hrp_read_pad(PadState *pad)
{
    padUpdate(pad);
    uint64_t held = padGetButtons(pad);
    uint16_t buttons = 0;

    for (size_t i = 0; i < sizeof(hrp_switch_key_map) / sizeof(hrp_switch_key_map[0]); i++)
        if (held & hrp_switch_key_map[i].key) buttons |= hrp_switch_key_map[i].button;

    HidAnalogStickState stick = padGetStickPos(pad, 0);
    if (stick.x < -HRP_SWITCH_DEADZONE) buttons |= HRP_BTN_LEFT;
    else if (stick.x > HRP_SWITCH_DEADZONE) buttons |= HRP_BTN_RIGHT;

#if HRP_SWITCH_RIGHT_STICK_ENABLED
    /* intentionally disabled: see hrp_switch_mapping.inc */
#endif

    return buttons;
}

/* dst_w/dst_h are the *requested* framebuffer size; the blit never writes past
 * them, so a smaller-than-expected framebuffer is clipped instead of overrun. */
static void hrp_blit_scaled(uint32_t *dst, int dst_w, int dst_h, size_t stride_pixels,
                            const uint32_t *src)
{
    for (int y = 0; y < HRP_SWITCH_H && y < dst_h; y++) {
        const uint32_t *s = src + (size_t)(y / HRP_SCALE) * HRP_SCREEN_W;
        uint32_t *d = dst + (size_t)y * stride_pixels;
        for (int x = 0; x < HRP_SWITCH_W && x < dst_w; x++)
            d[x] = s[x / HRP_SCALE];
    }
}

int main(int argc, char **argv)
{
    (void)argc;
    (void)argv;

    fsdevMountSdmc();
    mkdir(HRP_SD_DIR, 0777);
    g_log = fopen(HRP_LOG_PATH, "w");
    hrp_log_line("th01 open port starting");

    /* ---- content: the player's own package, otherwise the built-in set ---- */
    int stage_count = 0;
    char title[64] = "";
    int pkg_rc = hrp_content_load_file(HRP_PKG_PATH, g_stages, HRP_MAX_STAGES, &stage_count,
                                       title, sizeof(title));
    if (pkg_rc != HRP_PKG_OK) {
        const hrp_stage_def *demo = hrp_demo_stages(&stage_count);
        memcpy(g_stages, demo, sizeof(hrp_stage_def) * (size_t)stage_count);
        hrp_log_line("no usable package at %s (error %d); using the procedural set (%d stages)",
                     HRP_PKG_PATH, pkg_rc, stage_count);
    } else {
        hrp_log_line("loaded package %s: %d stages, title '%s'", HRP_PKG_PATH, stage_count, title);
    }

    hrp_save_data save;
    memset(&save, 0, sizeof(save));
    int have_save = hrp_save_read(HRP_SD_DIR, &save);
    hrp_log_line("save: %s (best %u, clears %u, plays %u)", have_save ? "found" : "none",
                 save.best_score, save.clears, save.plays);

    hrp_game_init(&g_game, g_stages, stage_count, 0x1234u);

    /* ---- video ---- */
    Framebuffer fb;
    if (R_FAILED(framebufferCreate(&fb, nwindowGetDefault(), HRP_SWITCH_W, HRP_SWITCH_H,
                                   PIXEL_FORMAT_RGBA_8888, 2))) {
        hrp_log_line("framebufferCreate failed");
        if (g_log) fclose(g_log);
        return 1;
    }
    framebufferMakeLinear(&fb);

    /* ---- input ---- */
    PadState pad;
    padConfigureInput(1, HidNpadStyleSet_NpadStandard);
    padInitializeDefault(&pad);

    /* ---- audio ---- */
    static AudioOutBuffer audio_buffers[2];
    static uint8_t audio_pool[HRP_AUDIO_FRAMES * 4 * 2] __attribute__((aligned(0x1000)));
    memset(audio_pool, 0, sizeof(audio_pool));
    hrp_audio_state audio;
    hrp_audio_init(&audio);
    int audio_ready = 0;

    if (R_SUCCEEDED(audoutInitialize()) && R_SUCCEEDED(audoutStartAudioOut())) {
        for (int i = 0; i < 2; i++) {
            audio_buffers[i].next = NULL;
            audio_buffers[i].buffer = audio_pool + i * HRP_AUDIO_FRAMES * 4;
            audio_buffers[i].buffer_size = HRP_AUDIO_FRAMES * 4;
            audio_buffers[i].data_size = HRP_AUDIO_FRAMES * 4;
            audio_buffers[i].data_offset = 0;
        }
        audoutPlayBuffer(&audio_buffers[0], NULL); /* prime with silence */
        audio_ready = 1;
    } else {
        hrp_log_line("audio unavailable: continuing silently");
    }

    hrp_frame_policy policy;
    hrp_frame_policy_init(&policy);

    uint16_t prev_buttons = 0;
    int music_prev = -1;
    int running = 1;
    int quit_requested = 0;

    while (appletMainLoop() && running) {
        uint16_t buttons = hrp_read_pad(&pad);
        if (padGetButtonsDown(&pad) & HidNpadButton_Minus) quit_requested = 1;

        hrp_input in;
        in.held = buttons;
        in.pressed = (uint16_t)(buttons & ~prev_buttons);
        prev_buttons = buttons;

        hrp_game_update(&g_game, &in);

        int music = HRP_MUSIC_NONE;
        if (g_game.state == HRP_STATE_TITLE) music = HRP_MUSIC_TITLE;
        else if (g_game.state == HRP_STATE_PLAY || g_game.state == HRP_STATE_RESPAWN)
            music = (g_game.stages[g_game.stage_index].type == HRP_STAGE_BOSS) ? HRP_MUSIC_BOSS
                                                                              : HRP_MUSIC_CARD;
        if (music != music_prev) { audio.music = music; music_prev = music; }

        if (audio_ready) {
            hrp_audio_mix(g_pcm, HRP_AUDIO_FRAMES, HRP_AUDIO_RATE, &audio,
                          g_game.sfx, g_game.sfx_count);
            AudioOutBuffer *released = NULL;
            u32 released_count = 0;
            if (R_SUCCEEDED(audoutWaitPlayFinish(&released, &released_count, UINT64_MAX)) &&
                released != NULL) {
                memcpy(released->buffer, g_pcm, HRP_AUDIO_FRAMES * 4);
                released->data_size = HRP_AUDIO_FRAMES * 4;
                released->data_offset = 0;
                audoutPlayBuffer(released, NULL);
            }
        }

        hrp_render_frame(&g_game, g_framebuffer, HRP_SCREEN_W, HRP_SCREEN_H);
        policy.frame_ready = 1;

        if (quit_requested) {
            policy.exiting = 1;
            running = 0;
        }

        if (hrp_frame_policy_should_present(&policy)) {
            u32 stride = 0;
            uint32_t *out = (uint32_t *)framebufferBegin(&fb, &stride);
            if (out) {
                hrp_blit_scaled(out, HRP_SWITCH_W, HRP_SWITCH_H,
                                stride / sizeof(uint32_t), g_framebuffer);
                framebufferEnd(&fb);
                hrp_frame_policy_commit(&policy, 1);
            } else {
                hrp_frame_policy_commit(&policy, 0);
            }
        } else {
            hrp_frame_policy_commit(&policy, 0);
        }
    }

    /* ---- shutdown: never present a partial frame ---- */
    policy.exiting = 1;

    hrp_save_data next = save;
    if (!have_save) { next.best_score = 0; next.clears = 0; next.plays = 0; }
    next.plays++;
    if ((uint32_t)g_game.score > next.best_score) next.best_score = (uint32_t)g_game.score;
    if (g_game.state == HRP_STATE_ALL_CLEAR) next.clears++;
    int saved = hrp_save_write(HRP_SD_DIR, &next);
    hrp_log_line("shutdown: score %d, saved=%d, presented %d frames", g_game.score, saved,
                 policy.presented);

    if (audio_ready) {
        audoutStopAudioOut();
        audoutExit();
    }
    framebufferClose(&fb);
    if (g_log) fclose(g_log);
    return 0;
}
