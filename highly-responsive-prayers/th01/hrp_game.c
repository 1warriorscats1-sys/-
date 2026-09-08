/* hrp_game.c - deterministic frame-stepped game logic.
 *
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2026 contributors to the 1warriorscats1-sys/- repository.
 *
 * Original implementation of a card-breaking / boss-duel loop, homaging the
 * gameplay structure of the first PC-98 Touhou title. No data from any
 * commercial release is compiled in; hrp_demo_data.c generates the fallback
 * content set procedurally.
 */
#include <string.h>

#include "hrp.h"
#include "hrp_math.h"

/* ------------------------------------------------------------------- rng */

uint32_t hrp_rng_next(uint32_t *state)
{
    uint32_t x = *state;
    if (x == 0) x = 0x6d2b79f5u;
    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
    *state = x;
    return x;
}

int hrp_rng_range(uint32_t *state, int lo, int hi)
{
    if (hi <= lo) return lo;
    return lo + (int)(hrp_rng_next(state) % (uint32_t)(hi - lo + 1));
}

/* --------------------------------------------------------------- sfx queue */

static void hrp_push_sfx(hrp_game *g, uint8_t id)
{
    if (g->sfx_count < (int)sizeof(g->sfx))
        g->sfx[g->sfx_count++] = id;
}

/* ---------------------------------------------------------------- particles */

static void hrp_spawn_particles(hrp_game *g, int x, int y, int count, uint32_t color, int spread)
{
    for (int i = 0; i < count; i++) {
        int ang = hrp_rng_range(&g->rng_state, 0, 359);
        int spd = hrp_rng_range(&g->rng_state, spread / 3 + 1, spread);
        hrp_fx vx = hrp_fx_mul(hrp_cos_fx(ang), HRP_FX(spd) / 60);
        hrp_fx vy = hrp_fx_mul(hrp_sin_fx(ang), HRP_FX(spd) / 60);
        for (int slot = 0; slot < HRP_MAX_PARTICLES; slot++) {
            hrp_particle *p = &g->particles[slot];
            if (p->alive) continue;
            p->x = HRP_FX(x);
            p->y = HRP_FX(y);
            p->vx = vx;
            p->vy = vy;
            p->life = hrp_rng_range(&g->rng_state, 18, 40);
            p->color = color;
            p->alive = 1;
            break;
        }
    }
}

static void hrp_update_particles(hrp_game *g)
{
    for (int i = 0; i < HRP_MAX_PARTICLES; i++) {
        hrp_particle *p = &g->particles[i];
        if (!p->alive) continue;
        p->x += p->vx;
        p->y += p->vy;
        p->vy += HRP_FX(1) / 10;
        if (--p->life <= 0) p->alive = 0;
    }
}

/* -------------------------------------------------------------- card field */

static void hrp_layout_cards(hrp_game *g, const hrp_stage_def *def)
{
    g->card_count = 0;
    g->cards_left = 0;
    int rows = def->rows, cols = def->cols;
    if (rows <= 0 || cols <= 0) return;
    if (rows * cols > HRP_MAX_CARDS) rows = HRP_MAX_CARDS / cols;
    if (rows <= 0) return;

    int grid_w = cols * (HRP_CARD_W + HRP_CARD_GAP_X) - HRP_CARD_GAP_X;
    int start_x = HRP_FIELD_LEFT + ((HRP_FIELD_RIGHT - HRP_FIELD_LEFT) - grid_w) / 2;
    if (start_x < HRP_FIELD_LEFT) start_x = HRP_FIELD_LEFT;

    for (int r = 0; r < rows; r++) {
        for (int c = 0; c < cols; c++) {
            uint8_t kind = def->cells[r * def->cols + c];
            hrp_card *card = &g->cards[g->card_count++];
            card->x = start_x + c * (HRP_CARD_W + HRP_CARD_GAP_X);
            card->y = HRP_FIELD_TOP + 12 + r * (HRP_CARD_H + HRP_CARD_GAP_Y);
            card->w = HRP_CARD_W;
            card->h = HRP_CARD_H;
            card->kind = kind;
            card->hp = (kind == HRP_CARD_TOUGH) ? 2 : (kind == HRP_CARD_SOLID ? 0 : 1);
            card->alive = (kind != HRP_CARD_EMPTY);
            card->flash = 0;
            if (card->alive && kind != HRP_CARD_SOLID) g->cards_left++;
        }
    }
}

int hrp_cards_alive(const hrp_game *g)
{
    int n = 0;
    for (int i = 0; i < g->card_count; i++)
        if (g->cards[i].alive && g->cards[i].kind != HRP_CARD_SOLID) n++;
    return n;
}

/* --------------------------------------------------------------- orb math */

static hrp_fx hrp_orb_speed(const hrp_game *g)
{
    int base = g->stages[g->stage_index].orb_speed; /* quarter px per frame */
    if (base == 0) base = 20;
    return (hrp_fx)(((int64_t)base * HRP_FX_ONE) / 4);
}

void hrp_launch_orb(hrp_game *g)
{
    if (!g->orb.stuck) return;
    hrp_fx speed = hrp_orb_speed(g);
    int dir = g->player.facing ? 1 : -1;
    g->orb.vx = hrp_fx_mul(speed, hrp_sin_fx(20)) * dir;
    g->orb.vy = -hrp_fx_mul(speed, hrp_cos_fx(20));
    g->orb.stuck = 0;
    g->orb.active = 1;
    hrp_push_sfx(g, HRP_SFX_BOUNCE);
}

static void hrp_orb_reflect_from_paddle(hrp_game *g, int boost_percent)
{
    int cx = g->player.x + g->player.w / 2;
    int off = hrp_fx_floor(g->orb.x) - cx;
    int span = g->player.w / 2 + 8;
    if (off < -span) off = -span;
    if (off > span) off = span;

    int ang = (off * 56) / span; /* -56..+56 degrees from vertical */
    hrp_fx speed = hrp_orb_speed(g);
    hrp_fx vx = hrp_fx_mul(speed, hrp_sin_fx(ang));
    hrp_fx vy = -hrp_fx_mul(speed, hrp_cos_fx(ang));

    if (boost_percent != 100) {
        vx = (hrp_fx)(((int64_t)vx * boost_percent) / 100);
        vy = (hrp_fx)(((int64_t)vy * boost_percent) / 100);
    }
    g->orb.vx = vx;
    g->orb.vy = vy;
    hrp_push_sfx(g, HRP_SFX_BOUNCE);
}

static void hrp_rod_swing(hrp_game *g)
{
    if (g->player.rod_timer > 0) return;
    g->player.rod_timer = 14;
    int px = g->player.x + g->player.w / 2;
    int py = HRP_PLAYER_Y + 10;
    int dx = hrp_fx_floor(g->orb.x) - px;
    int dy = hrp_fx_floor(g->orb.y) - py;
    if (g->orb.active && !g->orb.stuck && dx * dx + dy * dy < 72 * 72)
        hrp_orb_reflect_from_paddle(g, 130);
}

static void hrp_damage_card(hrp_game *g, hrp_card *card)
{
    if (card->kind == HRP_CARD_SOLID) {
        hrp_push_sfx(g, HRP_SFX_BOUNCE);
        return;
    }
    g->orb_stall = 0;
    card->flash = 6;
    card->hp--;
    g->combo++;
    g->score += 10 + g->combo * 2;
    if (card->hp > 0) {
        hrp_push_sfx(g, HRP_SFX_CARD);
        return;
    }
    card->alive = 0;
    g->cards_left--;
    g->score += 100;
    hrp_push_sfx(g, HRP_SFX_BREAK);
    hrp_spawn_particles(g, card->x + card->w / 2, card->y + card->h / 2, 8, 0xffe0e0ffu, 90);
    if (card->kind == HRP_CARD_POWER) {
        for (int i = 0; i < HRP_MAX_DROPS; i++) {
            hrp_drop *d = &g->drops[i];
            if (d->alive) continue;
            d->x = HRP_FX(card->x + card->w / 2);
            d->y = HRP_FX(card->y + card->h / 2);
            d->vy = HRP_FX(1);
            d->kind = 0;
            d->alive = 1;
            break;
        }
    }
}

/* returns 1 when the orb left the field */
static int hrp_update_orb(hrp_game *g)
{
    if (g->orb.stuck) {
        g->orb.x = HRP_FX(g->player.x + g->player.w / 2);
        g->orb.y = HRP_FX(HRP_PLAYER_Y - 10);
        return 0;
    }
    if (!g->orb.active) return 0;

    g->orb.spin = (uint8_t)((g->orb.spin + 4) & 0xff);
    g->orb.x += g->orb.vx;
    g->orb.y += g->orb.vy;

    int r = g->orb.radius;
    int x = hrp_fx_floor(g->orb.x), y = hrp_fx_floor(g->orb.y);

    if (x - r < HRP_FIELD_LEFT)  { g->orb.x = HRP_FX(HRP_FIELD_LEFT + r);  g->orb.vx = -g->orb.vx; hrp_push_sfx(g, HRP_SFX_BOUNCE); }
    if (x + r > HRP_FIELD_RIGHT) { g->orb.x = HRP_FX(HRP_FIELD_RIGHT - r); g->orb.vx = -g->orb.vx; hrp_push_sfx(g, HRP_SFX_BOUNCE); }
    if (y - r < HRP_FIELD_TOP)   { g->orb.y = HRP_FX(HRP_FIELD_TOP + r);   g->orb.vy = -g->orb.vy; hrp_push_sfx(g, HRP_SFX_BOUNCE); }

    x = hrp_fx_floor(g->orb.x);
    y = hrp_fx_floor(g->orb.y);

    for (int i = 0; i < g->card_count; i++) {
        hrp_card *card = &g->cards[i];
        if (!card->alive) continue;
        int cx = card->x + card->w / 2, cy = card->y + card->h / 2;
        int ox = (card->w / 2 + r) - hrp_abs_i(x - cx);
        int oy = (card->h / 2 + r) - hrp_abs_i(y - cy);
        if (ox <= 0 || oy <= 0) continue;
        if (ox < oy) {
            g->orb.vx = -g->orb.vx;
            g->orb.x = HRP_FX(x + (x > cx ? ox : -ox));
        } else {
            g->orb.vy = -g->orb.vy;
            g->orb.y = HRP_FX(y + (y > cy ? oy : -oy));
        }
        hrp_damage_card(g, card);
        break; /* one card per frame: keeps the bounce unambiguous */
    }

    if (g->orb.vy > 0) {
        int py = HRP_PLAYER_Y;
        int px0 = g->player.x - 6, px1 = g->player.x + g->player.w + 6;
        x = hrp_fx_floor(g->orb.x);
        y = hrp_fx_floor(g->orb.y);
        if (y + r >= py && y - r <= py + 22 && x >= px0 && x <= px1) {
            g->orb.y = HRP_FX(py - r);
            hrp_orb_reflect_from_paddle(g, g->player.rod_timer > 0 ? 130 : 100);
        }
    }

    if (hrp_fx_floor(g->orb.y) - r > HRP_SCREEN_H) {
        g->orb.active = 0;
        g->orb.stuck = 1;
        g->combo = 0;
        return 1;
    }
    return 0;
}

/* A perfectly vertical loop between indestructible cards can trap the orb
 * forever. Rotate the velocity slightly once the orb has gone too long without
 * damaging anything, which always breaks the loop while keeping its speed. */
static void hrp_orb_break_stall(hrp_game *g)
{
    if (g->orb_stall < 300 || (g->orb_stall % 45) != 0) return;
    int ang = ((g->orb_stall / 45) % 2) ? 4 : -4;
    hrp_fx c = hrp_cos_fx(ang), s = hrp_sin_fx(ang);
    hrp_fx vx = hrp_fx_mul(g->orb.vx, c) - hrp_fx_mul(g->orb.vy, s);
    hrp_fx vy = hrp_fx_mul(g->orb.vx, s) + hrp_fx_mul(g->orb.vy, c);

    /* never let the orb settle into a horizontal bounce */
    hrp_fx magnitude = hrp_isqrt((int64_t)vx * vx + (int64_t)vy * vy);
    hrp_fx min_vy = magnitude / 8;
    if (magnitude > 0 && hrp_abs_i(vy) < min_vy)
        vy = vy >= 0 ? min_vy : -min_vy;

    g->orb.vx = vx;
    g->orb.vy = vy;
}

/* ----------------------------------------------------------------- bullets */

static void hrp_spawn_bullet(hrp_game *g, int x, int y, hrp_fx vx, hrp_fx vy, int r, uint8_t kind)
{
    for (int i = 0; i < HRP_MAX_BULLETS; i++) {
        hrp_bullet *b = &g->bullets[i];
        if (b->alive) continue;
        b->x = HRP_FX(x);
        b->y = HRP_FX(y);
        b->vx = vx;
        b->vy = vy;
        b->r = r;
        b->kind = kind;
        b->alive = 1;
        return;
    }
}

static void hrp_aimed_bullets(hrp_game *g, int x, int y, hrp_fx speed, int spread_deg, int count, int r, uint8_t kind)
{
    int px = g->player.x + g->player.w / 2;
    int py = HRP_PLAYER_Y + 10;
    int dx = px - x, dy = py - y;
    int len = hrp_isqrt((int64_t)dx * dx + (int64_t)dy * dy);
    if (len <= 0) return;
    hrp_fx ux = hrp_fx_div(HRP_FX(dx), HRP_FX(len));
    hrp_fx uy = hrp_fx_div(HRP_FX(dy), HRP_FX(len));

    if (count <= 1) {
        hrp_spawn_bullet(g, x, y, hrp_fx_mul(ux, speed), hrp_fx_mul(uy, speed), r, kind);
        return;
    }
    for (int k = 0; k < count; k++) {
        int a = (k - (count - 1) / 2) * spread_deg;
        /* rotate (ux,uy) by a degrees with integer trig */
        hrp_fx ca = hrp_cos_fx(a), sa = hrp_sin_fx(a);
        hrp_fx rx = hrp_fx_mul(ux, ca) - hrp_fx_mul(uy, sa);
        hrp_fx ry = hrp_fx_mul(ux, sa) + hrp_fx_mul(uy, ca);
        hrp_spawn_bullet(g, x, y, hrp_fx_mul(rx, speed), hrp_fx_mul(ry, speed), r, kind);
    }
}

static void hrp_ring_bullets(hrp_game *g, int x, int y, hrp_fx speed, int count, int r, uint8_t kind)
{
    for (int i = 0; i < count; i++) {
        int a = (360 * i) / count + (g->frame / 4) % 30;
        hrp_spawn_bullet(g, x, y,
                         hrp_fx_mul(speed, hrp_cos_fx(a)),
                         hrp_fx_mul(speed, hrp_sin_fx(a)), r, kind);
    }
}

static int hrp_update_bullets(hrp_game *g)
{
    for (int i = 0; i < HRP_MAX_BULLETS; i++) {
        hrp_bullet *b = &g->bullets[i];
        if (!b->alive) continue;
        b->x += b->vx;
        b->y += b->vy;
        int x = hrp_fx_floor(b->x), y = hrp_fx_floor(b->y);
        if (x < -32 || x > HRP_SCREEN_W + 32 || y < -32 || y > HRP_SCREEN_H + 32) {
            b->alive = 0;
            continue;
        }
        if (g->player.invuln > 0) continue;
        int px = g->player.x + g->player.w / 2;
        int py = HRP_PLAYER_Y + 12;
        int dx = x - px, dy = y - py;
        int rr = b->r + 6;
        if (dx * dx + dy * dy <= rr * rr)
            return 1; /* player hit */
    }
    return 0;
}

static void hrp_clear_bullets(hrp_game *g)
{
    for (int i = 0; i < HRP_MAX_BULLETS; i++) g->bullets[i].alive = 0;
}

/* ------------------------------------------------------------ player shots */

static void hrp_fire_shots(hrp_game *g)
{
    if (g->player.shot_cooldown > 0) return;
    int level = g->player.shot_level;
    if (level < 1) level = 1;
    if (level > 4) level = 4;
    int cx = g->player.x + g->player.w / 2;
    int spawned = 0;
    for (int i = 0; i < level; i++) {
        int off = (i - (level - 1)) * 12;
        for (int s = 0; s < HRP_MAX_SHOTS; s++) {
            hrp_shot *sh = &g->shots[s];
            if (sh->alive) continue;
            sh->x = cx + off;
            sh->y = HRP_PLAYER_Y - 4;
            sh->vy = -12;
            sh->alive = 1;
            spawned++;
            break;
        }
    }
    if (spawned) {
        g->player.shot_cooldown = 5;
        hrp_push_sfx(g, HRP_SFX_SHOOT);
    }
}

static void hrp_update_shots(hrp_game *g)
{
    for (int i = 0; i < HRP_MAX_SHOTS; i++) {
        hrp_shot *sh = &g->shots[i];
        if (!sh->alive) continue;
        sh->y += sh->vy;
        if (sh->y < HRP_FIELD_TOP - 16) { sh->alive = 0; continue; }
        if (!g->boss.alive) continue;
        int bx = hrp_fx_floor(g->boss.x), by = hrp_fx_floor(g->boss.y);
        int dx = sh->x - bx, dy = sh->y - by;
        if (dx * dx + dy * dy > 34 * 34) continue;

        sh->alive = 0;
        g->boss.hp -= 6;
        g->boss.hit_flash = 6;
        g->score += 8;
        hrp_push_sfx(g, HRP_SFX_HIT);
        hrp_spawn_particles(g, sh->x, sh->y, 2, 0xfffff0c0u, 60);
        if (g->boss.hp <= g->boss.max_hp / 2) g->boss.phase = 1;
        if (g->boss.hp <= 0) {
            g->boss.hp = 0;
            g->boss.alive = 0;
            g->score += 5000;
            hrp_push_sfx(g, HRP_SFX_CLEAR);
            hrp_spawn_particles(g, bx, by, 40, 0xffffffffu, 160);
            hrp_clear_bullets(g);
            g->state = HRP_STATE_STAGE_CLEAR;
            g->state_timer = 0;
            g->last_event = 2;
        }
    }
}

/* -------------------------------------------------------------------- boss */

static void hrp_update_boss(hrp_game *g)
{
    if (!g->boss.alive) return;
    hrp_boss *b = &g->boss;
    b->timer++;
    if (b->hit_flash > 0) b->hit_flash--;

    int bx = hrp_fx_floor(b->x), by = hrp_fx_floor(b->y);

    switch (b->pattern) {
    case HRP_BOSS_SWEEP: {
        int span = (HRP_FIELD_RIGHT - HRP_FIELD_LEFT) / 2 - 70;
        int cx = (HRP_FIELD_LEFT + HRP_FIELD_RIGHT) / 2;
        int t = (int)(b->timer * 360 / 314); /* ~1.15 deg per frame */
        b->x = HRP_FX(cx) + hrp_fx_mul(HRP_FX(span), hrp_sin_fx(t));
        b->y = HRP_FX(96) + hrp_fx_mul(HRP_FX(18), hrp_sin_fx(t * 2));
        if (b->timer % (b->phase ? 26 : 40) == 0)
            hrp_aimed_bullets(g, bx, by, HRP_FX(3) + HRP_FX(b->phase), 16, 3, 7, 0);
        break;
    }
    case HRP_BOSS_ORBIT: {
        int cx = (HRP_FIELD_LEFT + HRP_FIELD_RIGHT) / 2;
        int t = (int)(b->timer * 360 / 251);
        b->x = HRP_FX(cx) + hrp_fx_mul(HRP_FX(180), hrp_sin_fx(t));
        b->y = HRP_FX(92) + hrp_fx_mul(HRP_FX(34), hrp_sin_fx(t * 2));
        if (b->timer % (b->phase ? 42 : 62) == 0) {
            hrp_ring_bullets(g, bx, by, HRP_FX(2) + HRP_FX(b->phase) / 2, 12, 6, 1);
            if (b->phase) hrp_aimed_bullets(g, bx, by, HRP_FX(4), 10, 2, 6, 0);
        }
        break;
    }
    case HRP_BOSS_STREAM:
    default: {
        if (b->timer % 90 == 1) {
            int nx = hrp_rng_range(&g->rng_state, HRP_FIELD_LEFT + 60, HRP_FIELD_RIGHT - 60);
            b->x = HRP_FX(nx);
        }
        int bob = (b->timer % 40);
        if (bob > 20) bob = 40 - bob;
        b->y = HRP_FX(84 + bob);
        if (b->timer % (b->phase ? 6 : 9) == 0)
            hrp_aimed_bullets(g, bx, by, HRP_FX(4), 6, 1, 5, 0);
        break;
    }
    }
}

/* ------------------------------------------------------------------- drops */

static void hrp_update_drops(hrp_game *g)
{
    for (int i = 0; i < HRP_MAX_DROPS; i++) {
        hrp_drop *d = &g->drops[i];
        if (!d->alive) continue;
        if (d->vy < HRP_FX(4)) d->vy += HRP_FX(1) / 8;
        d->y += d->vy;
        int x = hrp_fx_floor(d->x), y = hrp_fx_floor(d->y);
        if (y > HRP_SCREEN_H + 16) { d->alive = 0; continue; }
        if (y >= HRP_PLAYER_Y - 6 && x >= g->player.x - 8 && x <= g->player.x + g->player.w + 8) {
            d->alive = 0;
            if (d->kind == 1) {
                g->player.lives++;
                g->score += 3000;
            } else if (g->player.shot_level < 4) {
                g->player.shot_level++;
                g->score += 500;
            } else {
                g->score += 1000;
            }
            hrp_push_sfx(g, HRP_SFX_MENU);
        }
    }
}

/* ------------------------------------------------------------------ player */

static void hrp_update_player(hrp_game *g, const hrp_input *in)
{
    if (in->held & HRP_BTN_LEFT)  { g->player.x -= g->player.speed; g->player.facing = 0; }
    if (in->held & HRP_BTN_RIGHT) { g->player.x += g->player.speed; g->player.facing = 1; }
    if (g->player.x < HRP_FIELD_LEFT) g->player.x = HRP_FIELD_LEFT;
    if (g->player.x + g->player.w > HRP_FIELD_RIGHT) g->player.x = HRP_FIELD_RIGHT - g->player.w;
    if (g->player.invuln > 0) g->player.invuln--;
    if (g->player.shot_cooldown > 0) g->player.shot_cooldown--;
    if (g->player.rod_timer > 0) g->player.rod_timer--;
}

static void hrp_lose_life(hrp_game *g)
{
    g->player.lives--;
    g->deaths++;
    g->combo = 0;
    if (g->player.shot_level > 2) g->player.shot_level--;
    hrp_clear_bullets(g);
    hrp_spawn_particles(g, g->player.x + g->player.w / 2, HRP_PLAYER_Y + 10, 28, 0xffff8080u, 140);
    hrp_push_sfx(g, HRP_SFX_HIT);
    g->shake = 12;
    if (g->player.lives < 0) {
        g->state = HRP_STATE_GAME_OVER;
        g->state_timer = 0;
        g->last_event = 3;
    }
}

/* ------------------------------------------------------------------ stages */

void hrp_game_start_stage(hrp_game *g, int index)
{
    if (index < 0 || index >= g->stage_count) index = 0;
    g->stage_index = index;
    const hrp_stage_def *def = &g->stages[index];

    memset(g->cards, 0, sizeof(g->cards));
    memset(g->bullets, 0, sizeof(g->bullets));
    memset(g->shots, 0, sizeof(g->shots));
    memset(g->particles, 0, sizeof(g->particles));
    memset(g->drops, 0, sizeof(g->drops));

    g->player.x = (HRP_FIELD_LEFT + HRP_FIELD_RIGHT) / 2 - g->player.w / 2;
    g->player.invuln = 90;
    g->player.rod_timer = 0;

    g->orb.radius = 7;
    g->orb.stuck = 1;
    g->orb.active = 0;
    g->orb.vx = g->orb.vy = 0;
    g->orb.x = HRP_FX(g->player.x + g->player.w / 2);
    g->orb.y = HRP_FX(HRP_PLAYER_Y - 10);

    g->boss.alive = 0;
    g->boss.hp = 0;
    g->boss.max_hp = 0;
    g->boss.phase = 0;
    g->boss.pattern = def->boss_pattern;
    g->boss.timer = 0;
    g->boss.hit_flash = 0;

    g->cards_left = 0;
    g->card_count = 0;
    hrp_layout_cards(g, def);

    if (def->type == HRP_STAGE_BOSS) {
        g->boss.alive = 1;
        g->boss.hp = def->boss_hp;
        g->boss.max_hp = def->boss_hp;
        g->boss.x = HRP_FX((HRP_FIELD_LEFT + HRP_FIELD_RIGHT) / 2);
        g->boss.y = HRP_FX(92);
        g->orb.stuck = 0;
        g->orb.active = 0;
    }

    g->orb_stall = 0;
    g->state = HRP_STATE_PLAY;
    g->state_timer = 0;
    g->last_event = 0;
}

void hrp_game_reset(hrp_game *g)
{
    memset(g->bullets, 0, sizeof(g->bullets));
    memset(g->shots, 0, sizeof(g->shots));
    memset(g->particles, 0, sizeof(g->particles));
    memset(g->drops, 0, sizeof(g->drops));
    memset(g->cards, 0, sizeof(g->cards));
    g->card_count = 0;
    g->cards_left = 0;
    g->score = 0;
    g->combo = 0;
    g->deaths = 0;
    g->player.lives = 3;
    g->player.bombs = 2;
    g->player.shot_level = 1;
    g->player.invuln = 0;
    g->player.shot_cooldown = 0;
    g->player.rod_timer = 0;
    g->player.speed = 4;
    g->player.w = 48;
    g->player.facing = 1;
    g->boss.alive = 0;
    g->orb.active = 0;
    g->orb.stuck = 1;
    g->state = HRP_STATE_TITLE;
    g->stage_index = 0;
    g->state_timer = 0;
    g->last_event = 0;
}

void hrp_game_init(hrp_game *g, const hrp_stage_def *stages, int stage_count, uint32_t seed)
{
    memset(g, 0, sizeof(*g));
    g->stages = stages;
    g->stage_count = stage_count;
    g->content_id = 0;
    g->rng_state = seed ? seed : 0x9e3779b9u;
    g->player.speed = 4;
    g->player.w = 48;
    g->player.facing = 1;
    g->orb.radius = 7;
    hrp_game_reset(g);
}

/* ------------------------------------------------------------------ update */

void hrp_game_update(hrp_game *g, const hrp_input *in)
{
    g->frame++;
    g->state_timer++;
    g->sfx_count = 0;
    g->last_event = 0;
    if (g->shake > 0) g->shake--;
    if (g->flash > 0) g->flash--;

    if (in->pressed & HRP_BTN_PAUSE) {
        if (g->state == HRP_STATE_PLAY) {
            g->paused_prev_state = g->state;
            g->state = HRP_STATE_PAUSED;
            hrp_push_sfx(g, HRP_SFX_MENU);
            return;
        }
        if (g->state == HRP_STATE_PAUSED) {
            g->state = (hrp_state)g->paused_prev_state;
            hrp_push_sfx(g, HRP_SFX_MENU);
            return;
        }
    }

    switch (g->state) {
    case HRP_STATE_TITLE:
        hrp_update_particles(g);
        if (in->pressed & (HRP_BTN_SHOOT | HRP_BTN_CONFIRM)) {
            hrp_game_reset(g);
            hrp_game_start_stage(g, 0);
            hrp_push_sfx(g, HRP_SFX_MENU);
        }
        break;

    case HRP_STATE_PAUSED:
        break;

    case HRP_STATE_PLAY: {
        const hrp_stage_def *def = &g->stages[g->stage_index];
        hrp_update_player(g, in);
        int hit = 0;

        if (def->type == HRP_STAGE_CARD) {
            if (in->pressed & HRP_BTN_SHOOT) {
                if (g->orb.stuck) hrp_launch_orb(g);
                else hrp_rod_swing(g);
            }
            hit = hrp_update_orb(g);
            if (hit) {
                g->orb_stall = 0;
            } else {
                if (g->orb.active) g->orb_stall++;
                hrp_orb_break_stall(g);
            }
            if (!hit && g->cards_left <= 0) {
                g->state = HRP_STATE_STAGE_CLEAR;
                g->state_timer = 0;
                g->last_event = 2;
                g->score += 1000;
                hrp_push_sfx(g, HRP_SFX_CLEAR);
            }
        } else {
            if (in->held & HRP_BTN_SHOOT) hrp_fire_shots(g);
            if ((in->pressed & HRP_BTN_BOMB) && g->player.bombs > 0) {
                g->player.bombs--;
                hrp_clear_bullets(g);
                g->flash = 12;
                g->shake = 10;
                hrp_push_sfx(g, HRP_SFX_BOMB);
                if (g->boss.alive) {
                    g->boss.hp -= 60;
                    g->boss.hit_flash = 10;
                    if (g->boss.hp <= g->boss.max_hp / 2) g->boss.phase = 1;
                    if (g->boss.hp <= 0) {
                        g->boss.hp = 0;
                        g->boss.alive = 0;
                        g->score += 5000;
                        g->state = HRP_STATE_STAGE_CLEAR;
                        g->state_timer = 0;
                        g->last_event = 2;
                    }
                }
            }
            hrp_update_shots(g);
            if (g->state == HRP_STATE_PLAY) {
                hrp_update_boss(g);
                hit = hrp_update_bullets(g);
            }
            if (!hit && g->state == HRP_STATE_PLAY && def->time_limit &&
                g->state_timer >= def->time_limit) {
                g->state = HRP_STATE_STAGE_CLEAR;
                g->state_timer = 0;
                g->last_event = 2;
            }
        }

        if (hit) {
            g->last_event = 1;
            g->state = HRP_STATE_RESPAWN;
            g->state_timer = 0;
            hrp_update_drops(g);
            hrp_update_particles(g);
            break;
        }

        hrp_update_drops(g);
        hrp_update_particles(g);
        for (int i = 0; i < g->card_count; i++)
            if (g->cards[i].flash > 0) g->cards[i].flash--;
        break;
    }

    case HRP_STATE_RESPAWN:
        if (g->state_timer == 1) hrp_lose_life(g);
        hrp_update_particles(g);
        hrp_update_drops(g);
        if (g->state != HRP_STATE_GAME_OVER && g->state_timer > 60) {
            g->orb.stuck = 1;
            g->orb.active = 0;
            g->player.invuln = 120;
            g->state = HRP_STATE_PLAY;
            g->state_timer = 0;
        }
        break;

    case HRP_STATE_STAGE_CLEAR:
        hrp_update_particles(g);
        hrp_update_drops(g);
        if (g->state_timer > 150) {
            if (g->stage_index + 1 < g->stage_count) {
                hrp_game_start_stage(g, g->stage_index + 1);
            } else {
                g->state = HRP_STATE_ALL_CLEAR;
                g->state_timer = 0;
                g->last_event = 4;
                g->score += (g->player.lives > 0 ? g->player.lives : 0) * 2000;
            }
        }
        break;

    case HRP_STATE_GAME_OVER:
        hrp_update_particles(g);
        if (g->state_timer > 150 && (in->pressed & (HRP_BTN_SHOOT | HRP_BTN_CONFIRM))) {
            hrp_game_reset(g);
            hrp_push_sfx(g, HRP_SFX_MENU);
        }
        break;

    case HRP_STATE_ALL_CLEAR:
        hrp_update_particles(g);
        if (g->state_timer > 300 && (in->pressed & (HRP_BTN_SHOOT | HRP_BTN_CONFIRM))) {
            hrp_game_reset(g);
            hrp_push_sfx(g, HRP_SFX_MENU);
        }
        break;
    }
}
