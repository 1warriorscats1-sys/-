/* main_headless.c - no-render simulation runner used by the native tests.
 *
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2026 contributors to the 1warriorscats1-sys/- repository.
 *
 * Usage: hrp_headless [--scenario NAME] [--frames N] [--seed S] [--data FILE]
 *                     [--json]
 * Exit status: 0 = pass, 1 = assertion failed, 2 = usage error.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "hrp.h"

static uint32_t hrp_state_checksum(const hrp_game *g)
{
    uint32_t h = 0x811c9dc5u;
    const unsigned char *p = (const unsigned char *)g;
    /* hash the fields that describe the simulation; skip the content pointer
     * (offset of hrp_game::stages) and padding */
    size_t skip_start = offsetof(hrp_game, stages);
    size_t skip_end = skip_start + sizeof(g->stages);
    for (size_t i = 0; i < sizeof(*g); i++) {
        if (i >= skip_start && i < skip_end) continue;
        h ^= p[i];
        h *= 16777619u;
    }
    return h;
}

static void hrp_step(hrp_game *g, uint16_t buttons, uint16_t *prev)
{
    hrp_input in;
    in.held = buttons;
    in.pressed = (uint16_t)(buttons & ~(*prev));
    *prev = buttons;
    hrp_game_update(g, &in);
}

static void hrp_run(hrp_game *g, int frames, uint16_t *prev)
{
    for (int i = 0; i < frames; i++)
        hrp_step(g, hrp_autoplay_buttons(g), prev);
}

static int find_stage_of_type(const hrp_stage_def *stages, int count, int type)
{
    for (int i = 0; i < count; i++)
        if (stages[i].type == type) return i;
    return -1;
}

static void usage(void)
{
    fprintf(stderr,
            "usage: hrp_headless [--scenario boot|card|boss|death|determinism|all]\n"
            "                    [--frames N] [--seed S] [--data FILE] [--json]\n");
}

int main(int argc, char **argv)
{
    const char *scenario = "all";
    const char *data_path = NULL;
    int frames = 1800;
    int json = 0;
    unsigned seed = 0x1234u;

    for (int i = 1; i < argc; i++) {
        if (!strcmp(argv[i], "--scenario") && i + 1 < argc) scenario = argv[++i];
        else if (!strcmp(argv[i], "--frames") && i + 1 < argc) frames = atoi(argv[++i]);
        else if (!strcmp(argv[i], "--seed") && i + 1 < argc) seed = (unsigned)strtoul(argv[++i], NULL, 0);
        else if (!strcmp(argv[i], "--data") && i + 1 < argc) data_path = argv[++i];
        else if (!strcmp(argv[i], "--json")) json = 1;
        else { usage(); return 2; }
    }

    hrp_stage_def stages[HRP_MAX_STAGES];
    int stage_count = 0;
    char title[64] = "";

    if (data_path) {
        int rc = hrp_content_load_file(data_path, stages, HRP_MAX_STAGES, &stage_count, title, sizeof(title));
        if (rc != HRP_PKG_OK) {
            fprintf(stderr, "headless: cannot load data package %s (error %d)\n", data_path, rc);
            return 1;
        }
    } else {
        const hrp_stage_def *demo = hrp_demo_stages(&stage_count);
        memcpy(stages, demo, sizeof(hrp_stage_def) * (size_t)stage_count);
    }

    if (!strcmp(scenario, "determinism")) {
        hrp_game a, b;
        hrp_game_init(&a, stages, stage_count, seed);
        hrp_game_init(&b, stages, stage_count, seed);
        uint16_t pa = 0, pb = 0;
        hrp_run(&a, frames, &pa);
        hrp_run(&b, frames, &pb);
        uint32_t ca = hrp_state_checksum(&a), cb = hrp_state_checksum(&b);
        int ok = (ca == cb) && (a.score == b.score) && (a.frame == b.frame);
        if (json) printf("{\"scenario\":\"determinism\",\"ok\":%d,\"checksum\":%u,\"score\":%d}\n", ok, ca, a.score);
        else printf("determinism: %s (checksum %u, score %d)\n", ok ? "pass" : "FAIL", ca, a.score);
        return ok ? 0 : 1;
    }

    hrp_game g;
    hrp_game_init(&g, stages, stage_count, seed);
    uint16_t prev = 0;
    int rc = 0;

    if (!strcmp(scenario, "boot")) {
        hrp_run(&g, frames, &prev);
        int ok = g.frame == frames && g.state != HRP_STATE_TITLE;
        if (json) printf("{\"scenario\":\"boot\",\"ok\":%d,\"frames\":%d,\"state\":%d,\"score\":%d}\n",
                         ok, g.frame, (int)g.state, g.score);
        else printf("boot: %s (%d frames, state %d, score %d)\n", ok ? "pass" : "FAIL", g.frame, (int)g.state, g.score);
        rc = ok ? 0 : 1;
    } else if (!strcmp(scenario, "card")) {
        int card = find_stage_of_type(stages, stage_count, HRP_STAGE_CARD);
        if (card < 0) { fprintf(stderr, "card: no card stage in the content set\n"); return 1; }
        hrp_game_start_stage(&g, card);
        int before = hrp_cards_alive(&g);
        hrp_run(&g, frames, &prev);
        int after = hrp_cards_alive(&g);
        int ok = before > 0 && after < before && g.score > 0;
        if (json) printf("{\"scenario\":\"card\",\"ok\":%d,\"before\":%d,\"after\":%d,\"score\":%d}\n",
                         ok, before, after, g.score);
        else printf("card: %s (cards %d -> %d, score %d)\n", ok ? "pass" : "FAIL", before, after, g.score);
        rc = ok ? 0 : 1;
    } else if (!strcmp(scenario, "boss")) {
        int boss_stage = find_stage_of_type(stages, stage_count, HRP_STAGE_BOSS);
        if (boss_stage < 0) { fprintf(stderr, "boss: no boss stage in the content set\n"); return 1; }
        hrp_game_start_stage(&g, boss_stage);
        int hp0 = g.boss.hp;
        hrp_run(&g, frames, &prev);
        int ok = (g.boss.hp < hp0) || g.state == HRP_STATE_STAGE_CLEAR || g.stage_index > boss_stage;
        if (json) printf("{\"scenario\":\"boss\",\"ok\":%d,\"hp0\":%d,\"hp\":%d,\"alive\":%d,\"stage\":%d}\n",
                         ok, hp0, g.boss.hp, g.boss.alive, g.stage_index);
        else printf("boss: %s (hp %d -> %d, alive %d, stage %d)\n", ok ? "pass" : "FAIL",
                    hp0, g.boss.hp, g.boss.alive, g.stage_index);
        rc = ok ? 0 : 1;
    } else if (!strcmp(scenario, "death")) {
        /* no input at all: the orb is launched once, then lost repeatedly */
        int card = find_stage_of_type(stages, stage_count, HRP_STAGE_CARD);
        if (card < 0) { fprintf(stderr, "death: no card stage in the content set\n"); return 1; }
        hrp_game_start_stage(&g, card);
        hrp_input in;
        in.held = HRP_BTN_SHOOT; /* press once: pressed edge only on frame 1 */
        in.pressed = HRP_BTN_SHOOT;
        hrp_game_update(&g, &in);
        in.pressed = 0;
        for (int i = 0; i < frames; i++) hrp_game_update(&g, &in);
        int ok = g.deaths > 0 && (g.player.lives < 3);
        if (json) printf("{\"scenario\":\"death\",\"ok\":%d,\"deaths\":%d,\"lives\":%d,\"state\":%d}\n",
                         ok, g.deaths, g.player.lives, (int)g.state);
        else printf("death: %s (deaths %d, lives %d, state %d)\n", ok ? "pass" : "FAIL",
                    g.deaths, g.player.lives, (int)g.state);
        rc = ok ? 0 : 1;
    } else if (!strcmp(scenario, "all")) {
        hrp_run(&g, frames, &prev);
        int ok = g.frame == frames;
        if (json)
            printf("{\"scenario\":\"all\",\"ok\":%d,\"frames\":%d,\"score\":%d,\"stage\":%d,\"state\":%d,\"deaths\":%d}\n",
                   ok, g.frame, g.score, g.stage_index, (int)g.state, g.deaths);
        else
            printf("headless summary: frames %d, score %d, stage %d, state %d, deaths %d\n",
                   g.frame, g.score, g.stage_index, (int)g.state, g.deaths);
        rc = ok ? 0 : 1;
    } else {
        usage();
        return 2;
    }

    return rc;
}
