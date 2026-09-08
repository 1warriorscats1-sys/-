/* hrp_demo_data.c - procedurally generated fallback content set.
 *
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2026 contributors to the 1warriorscats1-sys/- repository.
 *
 * The engine never ships game data from a commercial release. When no open data
 * package is present on the SD card it plays this set, which is generated at
 * runtime from a fixed seed: it is original content, byte-identical on every
 * platform, and small enough to live in .rodata.
 */
#include <string.h>

#include "hrp.h"

#define HRP_DEMO_STAGES 8
#define HRP_DEMO_COLS   12

static hrp_stage_def g_demo[HRP_DEMO_STAGES];
static int g_demo_built = 0;

static uint32_t demo_lcg(uint32_t *s)
{
    *s = (*s * 1664525u + 1013904223u);
    return *s >> 8;
}

static void demo_fill(hrp_stage_def *st, int rows, int cols, int seed, int style)
{
    uint32_t s = (uint32_t)(seed * 2654435761u) | 1u;
    memset(st->cells, 0, sizeof(st->cells));
    st->rows = (uint8_t)rows;
    st->cols = (uint8_t)cols;
    st->type = HRP_STAGE_CARD;

    for (int r = 0; r < rows; r++) {
        for (int c = 0; c < cols; c++) {
            uint8_t kind = HRP_CARD_NORMAL;
            int mirror = c < cols / 2 ? c : cols - 1 - c;
            int mid = (cols - 1) / 2;

            switch (style) {
            case 0: /* banded rows with a centre gap */
                if (r == 1 && (c == mid || c == mid + 1)) kind = HRP_CARD_EMPTY;
                if (r == 3) kind = HRP_CARD_TOUGH;
                if (r == rows - 1 && (demo_lcg(&s) % 5) == 0) kind = HRP_CARD_POWER;
                break;
            case 1: /* pyramid */
                kind = (c >= r && c < cols - r) ? HRP_CARD_NORMAL : HRP_CARD_EMPTY;
                if (r == 0) kind = HRP_CARD_TOUGH;
                break;
            case 2: /* twin towers with a solid lintel */
                if (r == 0) kind = (c > 1 && c < cols - 2) ? HRP_CARD_SOLID : HRP_CARD_NORMAL;
                else kind = (c <= 2 || c >= cols - 3) ? HRP_CARD_NORMAL : HRP_CARD_EMPTY;
                if (r == 2 && (c == 1 || c == cols - 2)) kind = HRP_CARD_POWER;
                break;
            case 3: /* diamond */
                kind = (hrp_abs_i(mirror - mid / 2) + hrp_abs_i(r - rows / 2) <= rows / 2 + 1)
                           ? HRP_CARD_NORMAL
                           : HRP_CARD_EMPTY;
                if (r == rows / 2) kind = HRP_CARD_TOUGH;
                break;
            default: /* arch with power cards on the keystones */
                {
                    int arch = (r * 2 <= rows) ? 1 : 0;
                    kind = arch ? HRP_CARD_NORMAL : HRP_CARD_EMPTY;
                    if (r == 0) kind = HRP_CARD_SOLID;
                    if (r == rows / 2 && (c % 4 == 1)) kind = HRP_CARD_POWER;
                }
                break;
            }
            st->cells[r * HRP_DEMO_COLS + c] = kind;
        }
    }
    st->orb_speed = (uint8_t)(20 + seed);
}

static void demo_boss(hrp_stage_def *st, int index, uint16_t hp, uint8_t pattern, const char *name)
{
    memset(st, 0, sizeof(*st));
    st->type = HRP_STAGE_BOSS;
    st->boss_hp = hp;
    st->boss_pattern = pattern;
    st->time_limit = 3600; /* 60s survival cap, then the stage ends */
    st->rows = 0;
    st->cols = 0;
    st->orb_speed = 20;
    (void)index;
    strncpy(st->name, name, HRP_STAGE_NAME - 1);
}

static void demo_card(hrp_stage_def *st, int seed, int style, const char *name)
{
    demo_fill(st, 5, HRP_DEMO_COLS, seed, style);
    strncpy(st->name, name, HRP_STAGE_NAME - 1);
}

const hrp_stage_def *hrp_demo_stages(int *out_count)
{
    if (!g_demo_built) {
        demo_card(&g_demo[0], 0, 0, "SHRINE GATE");
        demo_card(&g_demo[1], 2, 1, "HALL OF CARDS");
        demo_boss(&g_demo[2], 0, 900, HRP_BOSS_SWEEP, "KEEPER OF THE GATE");
        demo_card(&g_demo[3], 4, 2, "TWIN TOWERS");
        demo_card(&g_demo[4], 6, 3, "CRYSTAL FIELD");
        demo_boss(&g_demo[5], 1, 1200, HRP_BOSS_ORBIT, "WARDEN OF THE LAKE");
        demo_card(&g_demo[6], 8, 4, "ARCHWAY");
        demo_boss(&g_demo[7], 2, 1800, HRP_BOSS_STREAM, "FINAL GUARDIAN");
        g_demo_built = 1;
    }
    if (out_count) *out_count = HRP_DEMO_STAGES;
    return g_demo;
}
