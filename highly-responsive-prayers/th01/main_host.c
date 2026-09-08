/* main_host.c - Linux/SDL-free host build: renders frames and audio to disk.
 *
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2026 contributors to the 1warriorscats1-sys/- repository.
 *
 * Usage: hrp_host [--frames N] [--every K] [--dump DIR] [--wav FILE]
 *                 [--data FILE] [--seed S] [--stage N] [--idle] [--autoplay]
 *
 * The host target has no windowing dependency: frames are written as P6 PPM
 * files (convert them with scripts/render_th01_preview.py) and audio as WAV.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "hrp.h"

static int write_ppm(const char *path, const uint32_t *rgba, int w, int h)
{
    FILE *f = fopen(path, "wb");
    if (!f) return -1;
    fprintf(f, "P6\n%d %d\n255\n", w, h);
    for (int i = 0; i < w * h; i++) {
        uint32_t p = rgba[i];
        unsigned char rgb[3] = { (unsigned char)(p & 0xff), (unsigned char)((p >> 8) & 0xff),
                                 (unsigned char)((p >> 16) & 0xff) };
        fwrite(rgb, 1, 3, f);
    }
    fclose(f);
    return 0;
}

static int write_wav(const char *path, const int16_t *pcm, int frames, int rate)
{
    FILE *f = fopen(path, "wb");
    if (!f) return -1;
    uint32_t data_bytes = (uint32_t)frames * 4; /* 16-bit stereo */
    uint32_t riff = 36 + data_bytes;

    fwrite("RIFF", 1, 4, f);
    fwrite(&riff, 4, 1, f);
    fwrite("WAVEfmt ", 1, 8, f);
    uint32_t fmt_len = 16;
    uint16_t audio_fmt = 1, channels = 2, bits = 16;
    uint32_t byte_rate = (uint32_t)rate * 4;
    uint16_t block_align = 4;
    fwrite(&fmt_len, 4, 1, f);
    fwrite(&audio_fmt, 2, 1, f);
    fwrite(&channels, 2, 1, f);
    uint32_t sample_rate = (uint32_t)rate;
    fwrite(&sample_rate, 4, 1, f);
    fwrite(&byte_rate, 4, 1, f);
    fwrite(&block_align, 2, 1, f);
    fwrite(&bits, 2, 1, f);
    fwrite("data", 1, 4, f);
    fwrite(&data_bytes, 4, 1, f);
    fwrite(pcm, 1, data_bytes, f);
    fclose(f);
    return 0;
}

static void usage(void)
{
    fprintf(stderr,
            "usage: hrp_host [--frames N] [--every K] [--dump DIR] [--wav FILE]\n"
            "                [--data FILE] [--seed S] [--stage N] [--idle]\n");
}

int main(int argc, char **argv)
{
    int frames = 600;
    int every = 4;
    const char *dump_dir = NULL;
    const char *wav_path = NULL;
    const char *data_path = NULL;
    int start_stage = -1;
    int idle = 0;
    int idle_frames = 0; /* hold the title screen before the scripted input starts */
    unsigned seed = 0x1234u;

    for (int i = 1; i < argc; i++) {
        if (!strcmp(argv[i], "--frames") && i + 1 < argc) frames = atoi(argv[++i]);
        else if (!strcmp(argv[i], "--every") && i + 1 < argc) every = atoi(argv[++i]);
        else if (!strcmp(argv[i], "--idle-frames") && i + 1 < argc) idle_frames = atoi(argv[++i]);
        else if (!strcmp(argv[i], "--dump") && i + 1 < argc) dump_dir = argv[++i];
        else if (!strcmp(argv[i], "--wav") && i + 1 < argc) wav_path = argv[++i];
        else if (!strcmp(argv[i], "--data") && i + 1 < argc) data_path = argv[++i];
        else if (!strcmp(argv[i], "--stage") && i + 1 < argc) start_stage = atoi(argv[++i]);
        else if (!strcmp(argv[i], "--seed") && i + 1 < argc) seed = (unsigned)strtoul(argv[++i], NULL, 0);
        else if (!strcmp(argv[i], "--idle")) idle = 1;
        else { usage(); return 2; }
    }
    if (every < 1) every = 1;

    hrp_stage_def stages[HRP_MAX_STAGES];
    int stage_count = 0;
    char title[64] = "";

    if (data_path) {
        int rc = hrp_content_load_file(data_path, stages, HRP_MAX_STAGES, &stage_count, title, sizeof(title));
        if (rc != HRP_PKG_OK) {
            fprintf(stderr, "host: cannot load data package %s (error %d)\n", data_path, rc);
            return 1;
        }
        printf("host: loaded %d stages from %s (%s)\n", stage_count, data_path, title[0] ? title : "untitled");
    } else {
        const hrp_stage_def *demo = hrp_demo_stages(&stage_count);
        memcpy(stages, demo, sizeof(hrp_stage_def) * (size_t)stage_count);
        printf("host: using the built-in procedural content set (%d stages)\n", stage_count);
    }

    hrp_game g;
    hrp_game_init(&g, stages, stage_count, seed);
    if (start_stage >= 0) hrp_game_start_stage(&g, start_stage);

    static uint32_t framebuffer[HRP_SCREEN_W * HRP_SCREEN_H];
    const int rate = 44100;
    const int frames_per_tick = rate / HRP_FPS;
    static int16_t pcm[4096 * 2];
    hrp_audio_state audio;
    hrp_audio_init(&audio);

    FILE *wav = NULL;
    long wav_frames = 0;
    if (wav_path) {
        wav = fopen(wav_path, "wb");
        if (wav) {
            /* header is written at the end once the length is known */
            fclose(wav);
            wav = NULL;
        }
    }

    static int16_t wav_buffer[44100 * 30 * 2]; /* up to 30 seconds */
    long wav_capacity = (long)(sizeof(wav_buffer) / (sizeof(wav_buffer[0]) * 2));

    uint16_t prev = 0;
    int dumped = 0;
    int music_prev = -1;

    for (int f = 0; f < frames; f++) {
        uint16_t buttons = (idle || f < idle_frames) ? 0 : hrp_autoplay_buttons(&g);
        hrp_input in;
        in.held = buttons;
        in.pressed = (uint16_t)(buttons & ~prev);
        prev = buttons;

        hrp_game_update(&g, &in);

        int music = HRP_MUSIC_NONE;
        if (g.state == HRP_STATE_TITLE) music = HRP_MUSIC_TITLE;
        else if (g.state == HRP_STATE_PLAY || g.state == HRP_STATE_RESPAWN) {
            music = (g.stages[g.stage_index].type == HRP_STAGE_BOSS) ? HRP_MUSIC_BOSS : HRP_MUSIC_CARD;
        }
        if (music != music_prev) { audio.music = music; music_prev = music; }

        int want = frames_per_tick;
        while (want > 0) {
            int chunk = want > 4096 ? 4096 : want;
            hrp_audio_mix(pcm, chunk, rate, &audio, g.sfx, g.sfx_count);
            if (wav_frames + chunk <= wav_capacity)
                memcpy(wav_buffer + wav_frames * 2, pcm, (size_t)chunk * 4);
            wav_frames += chunk;
            want -= chunk;
            g.sfx_count = 0; /* events are consumed once */
        }

        if (dump_dir && (f % every) == 0) {
            hrp_render_frame(&g, framebuffer, HRP_SCREEN_W, HRP_SCREEN_H);
            char path[1024];
            snprintf(path, sizeof(path), "%s/frame_%05d.ppm", dump_dir, dumped);
            if (write_ppm(path, framebuffer, HRP_SCREEN_W, HRP_SCREEN_H) != 0) {
                fprintf(stderr, "host: cannot write %s\n", path);
                return 1;
            }
            dumped++;
        }
    }

    if (wav_path && wav_frames > 0)
        write_wav(wav_path, wav_buffer, (int)wav_frames, rate);

    printf("host: %d frames simulated, %d frames dumped, state %d, score %d, stage %d, lives %d\n",
           frames, dumped, (int)g.state, g.score, g.stage_index + 1, g.player.lives);
    return 0;
}
