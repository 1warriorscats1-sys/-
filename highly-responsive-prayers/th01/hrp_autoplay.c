/* hrp_autoplay.c - deterministic scripted input.
 *
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2026 contributors to the 1warriorscats1-sys/- repository.
 *
 * Used by the host demo (so a build can be watched without a controller) and by
 * the headless tests (so coverage does not depend on a human). It only looks at
 * the simulation state, so it behaves identically on every platform.
 */
#include "hrp.h"

static int hrp_nearest_bullet_dx(const hrp_game *g, int *dist)
{
    int best = 0, best_dist = 1 << 30, found = 0;
    int px = g->player.x + g->player.w / 2;
    int py = HRP_PLAYER_Y + 12;
    for (int i = 0; i < HRP_MAX_BULLETS; i++) {
        const hrp_bullet *b = &g->bullets[i];
        if (!b->alive) continue;
        int bx = hrp_fx_floor(b->x), by = hrp_fx_floor(b->y);
        if (by < py - 140) continue; /* ignore bullets far above */
        int dx = bx - px;
        int d = dx * dx + (by - py) * (by - py);
        if (d < best_dist) { best_dist = d; best = dx; found = 1; }
    }
    if (dist) *dist = found ? best_dist : -1;
    return found ? best : 0;
}

uint16_t hrp_autoplay_buttons(const hrp_game *g)
{
    uint16_t buttons = 0;
    const hrp_stage_def *def = &g->stages[g->stage_index];

    if (g->state == HRP_STATE_TITLE || g->state == HRP_STATE_GAME_OVER ||
        g->state == HRP_STATE_ALL_CLEAR) {
        buttons |= HRP_BTN_CONFIRM;
        return buttons;
    }
    if (g->state != HRP_STATE_PLAY) return buttons;

    int px = g->player.x + g->player.w / 2;

    if (def->type == HRP_STAGE_CARD) {
        int ox = hrp_fx_floor(g->orb.x);
        int oy = hrp_fx_floor(g->orb.y);
        if (g->orb.stuck) {
            /* line the launch up so the orb climbs towards the card mass */
            int target = (HRP_FIELD_LEFT + HRP_FIELD_RIGHT) / 2;
            if (px < target - 4) buttons |= HRP_BTN_RIGHT;
            else if (px > target + 4) buttons |= HRP_BTN_LEFT;
            if (g->frame % 20 == 0) buttons |= HRP_BTN_SHOOT;
            return buttons;
        }
        if (g->orb.vy > 0) {
            /* descending: get under the orb and line the return up */
            if (px < ox - 2) buttons |= HRP_BTN_RIGHT;
            else if (px > ox + 2) buttons |= HRP_BTN_LEFT;
            if (oy > HRP_PLAYER_Y - 46 && (ox - px) * (ox - px) < 70 * 70)
                buttons |= HRP_BTN_SHOOT; /* rod swing for a powered return */
        } else {
            /* climbing: drift towards the side the orb will come back on */
            int centre = (HRP_FIELD_LEFT + HRP_FIELD_RIGHT) / 2;
            int target = centre + (ox - centre) / 2;
            if (px < target - 4) buttons |= HRP_BTN_RIGHT;
            else if (px > target + 4) buttons |= HRP_BTN_LEFT;
        }
        return buttons;
    }

    /* boss stage: keep shooting, dodge the closest bullet */
    buttons |= HRP_BTN_SHOOT;
    int dist = -1;
    int dx = hrp_nearest_bullet_dx(g, &dist);
    int live = 0;
    for (int i = 0; i < HRP_MAX_BULLETS; i++) if (g->bullets[i].alive) live++;

    if (live > 26 && g->player.bombs > 0 && g->frame % 30 == 0)
        buttons |= HRP_BTN_BOMB;

    int boss_x = hrp_fx_floor(g->boss.x);
    if (dist >= 0 && dist < 70 * 70) {
        /* step away from the threat, staying inside the field */
        if (dx > 0 && g->player.x > HRP_FIELD_LEFT + 4) buttons |= HRP_BTN_LEFT;
        else if (dx <= 0 && g->player.x + g->player.w < HRP_FIELD_RIGHT - 4) buttons |= HRP_BTN_RIGHT;
    } else if (px < boss_x - 20) {
        buttons |= HRP_BTN_RIGHT;
    } else if (px > boss_x + 20) {
        buttons |= HRP_BTN_LEFT;
    }
    return buttons;
}
