/* hrp_render.c - software renderer (original procedural artwork).
 *
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2026 contributors to the 1warriorscats1-sys/- repository.
 *
 * Every pixel produced here is drawn by this code: sprites are described as
 * primitives and palettes, so the build carries no third-party artwork.
 */
#include <string.h>

#include "hrp.h"
#include "hrp_font.h"

#define HRP_RGBA(r, g, b)                                                      \
    ((uint32_t)0xff000000u | ((uint32_t)(b) << 16) | ((uint32_t)(g) << 8) |    \
     (uint32_t)(r))

typedef struct {
    uint32_t *fb;
    int w, h;
    int ox, oy; /* letterbox offset */
} hrp_target;

static inline void hrp_px(hrp_target *t, int x, int y, uint32_t c)
{
    x += t->ox;
    y += t->oy;
    if ((unsigned)x >= (unsigned)t->w || (unsigned)y >= (unsigned)t->h) return;
    t->fb[(size_t)y * t->w + x] = c;
}

static void hrp_rect(hrp_target *t, int x, int y, int w, int h, uint32_t c)
{
    for (int j = 0; j < h; j++)
        for (int i = 0; i < w; i++)
            hrp_px(t, x + i, y + j, c);
}

static void hrp_frame_rect(hrp_target *t, int x, int y, int w, int h, uint32_t c)
{
    for (int i = 0; i < w; i++) { hrp_px(t, x + i, y, c); hrp_px(t, x + i, y + h - 1, c); }
    for (int j = 0; j < h; j++) { hrp_px(t, x, y + j, c); hrp_px(t, x + w - 1, y + j, c); }
}

static void hrp_disc(hrp_target *t, int cx, int cy, int r, uint32_t c)
{
    if (r <= 0) return;
    int r2 = r * r;
    for (int y = -r; y <= r; y++)
        for (int x = -r; x <= r; x++)
            if (x * x + y * y <= r2) hrp_px(t, cx + x, cy + y, c);
}

static void hrp_ring(hrp_target *t, int cx, int cy, int r, int thickness, uint32_t c)
{
    int outer = r * r;
    int inner = (r - thickness) * (r - thickness);
    for (int y = -r; y <= r; y++)
        for (int x = -r; x <= r; x++) {
            int d = x * x + y * y;
            if (d <= outer && d >= inner) hrp_px(t, cx + x, cy + y, c);
        }
}

static void hrp_line(hrp_target *t, int x0, int y0, int x1, int y1, uint32_t c)
{
    int dx = hrp_abs_i(x1 - x0), dy = hrp_abs_i(y1 - y0);
    int sx = x0 < x1 ? 1 : -1, sy = y0 < y1 ? 1 : -1;
    int err = dx - dy;
    for (;;) {
        hrp_px(t, x0, y0, c);
        if (x0 == x1 && y0 == y1) break;
        int e2 = err * 2;
        if (e2 > -dy) { err -= dy; x0 += sx; }
        if (e2 < dx) { err += dx; y0 += sy; }
    }
}

static int hrp_text_width(const char *s, int scale)
{
    int n = 0;
    for (const char *p = s; *p; p++) n++;
    return n > 0 ? n * (HRP_FONT_W + 1) * scale - scale : 0;
}

static void hrp_text(hrp_target *t, int x, int y, const char *s, int scale, uint32_t c)
{
    int cursor = x;
    for (const unsigned char *p = (const unsigned char *)s; *p; p++) {
        if (*p < HRP_FONT_FIRST || *p > HRP_FONT_LAST) { cursor += (HRP_FONT_W + 1) * scale; continue; }
        const uint8_t *glyph = hrp_font5x7[*p - HRP_FONT_FIRST];
        for (int col = 0; col < HRP_FONT_W; col++) {
            uint8_t bits = glyph[col];
            for (int row = 0; row < HRP_FONT_GLYPH_H; row++) {
                if (!(bits & (1u << row))) continue;
                hrp_rect(t, cursor + col * scale, y + row * scale, scale, scale, c);
            }
        }
        cursor += (HRP_FONT_W + 1) * scale;
    }
}

static void hrp_text_centered(hrp_target *t, int y, const char *s, int scale, uint32_t c)
{
    int w = hrp_text_width(s, scale);
    hrp_text(t, (HRP_SCREEN_W - w) / 2, y, s, scale, c);
}

static void hrp_int_to_str(int value, char *out, size_t size, int width)
{
    char tmp[16];
    int n = 0;
    int v = value < 0 ? -value : value;
    if (v == 0) tmp[n++] = '0';
    while (v > 0 && n < (int)sizeof(tmp)) { tmp[n++] = (char)('0' + v % 10); v /= 10; }
    size_t pos = 0;
    if (value < 0 && pos + 1 < size) out[pos++] = '-';
    while (n > 0 && pos + 1 < size) out[pos++] = tmp[--n];
    while ((int)pos < width && pos + 1 < size) out[pos++] = '0';
    out[pos] = 0;
}

/* ------------------------------------------------------------- background */

static void hrp_draw_background(hrp_target *t, int frame)
{
    for (int y = 0; y < HRP_SCREEN_H; y++) {
        int shade = 6 + (y * 18) / HRP_SCREEN_H;
        uint32_t c = HRP_RGBA(shade, shade / 2, shade * 2 + 20);
        for (int x = 0; x < HRP_SCREEN_W; x++) hrp_px(t, x, y, c);
    }
    /* deterministic starfield */
    for (int i = 0; i < 60; i++) {
        int sx = (i * 97) % HRP_SCREEN_W;
        int sy = ((i * 53) % HRP_SCREEN_H + frame / (2 + i % 5)) % HRP_SCREEN_H;
        uint32_t c = HRP_RGBA(120 + (i % 3) * 40, 110 + (i % 4) * 30, 200);
        hrp_px(t, sx, sy, c);
    }
    /* field outline */
    uint32_t border = HRP_RGBA(90, 60, 140);
    for (int y = HRP_FIELD_TOP - 8; y < HRP_SCREEN_H - 8; y++) {
        hrp_px(t, HRP_FIELD_LEFT - 4, y, border);
        hrp_px(t, HRP_FIELD_RIGHT + 3, y, border);
    }
    for (int x = HRP_FIELD_LEFT - 4; x <= HRP_FIELD_RIGHT + 3; x++)
        hrp_px(t, x, HRP_FIELD_TOP - 8, border);
}

/* -------------------------------------------------------------------- HUD */

static void hrp_draw_hud(hrp_target *t, const hrp_game *g)
{
    char buf[32];
    hrp_text(t, 10, 6, "SCORE", 1, HRP_RGBA(200, 200, 220));
    hrp_int_to_str(g->score, buf, sizeof(buf), 8);
    hrp_text(t, 46, 6, buf, 1, HRP_RGBA(255, 255, 255));

    hrp_text(t, 150, 6, "STAGE", 1, HRP_RGBA(200, 200, 220));
    hrp_int_to_str(g->stage_index + 1, buf, sizeof(buf), 2);
    hrp_text(t, 186, 6, buf, 1, HRP_RGBA(255, 255, 255));
    hrp_text(t, 206, 6, g->stages[g->stage_index].name, 1, HRP_RGBA(180, 230, 255));

    hrp_text(t, 430, 6, "LIVES", 1, HRP_RGBA(200, 200, 220));
    for (int i = 0; i < g->player.lives && i < 6; i++)
        hrp_disc(t, 470 + i * 12, 10, 4, HRP_RGBA(255, 90, 110));

    hrp_text(t, 550, 6, "BOMB", 1, HRP_RGBA(200, 200, 220));
    for (int i = 0; i < g->player.bombs && i < 4; i++)
        hrp_disc(t, 586 + i * 12, 10, 4, HRP_RGBA(120, 220, 255));

    /* power bar */
    hrp_text(t, 10, 18, "POWER", 1, HRP_RGBA(200, 200, 220));
    for (int i = 0; i < 4; i++) {
        uint32_t c = (i < g->player.shot_level) ? HRP_RGBA(255, 220, 90) : HRP_RGBA(60, 60, 80);
        hrp_rect(t, 50 + i * 14, 18, 11, 7, c);
    }

    if (g->stages[g->stage_index].type == HRP_STAGE_CARD) {
        hrp_text(t, 300, 18, "CARDS", 1, HRP_RGBA(200, 200, 220));
        hrp_int_to_str(g->cards_left, buf, sizeof(buf), 3);
        hrp_text(t, 336, 18, buf, 1, HRP_RGBA(255, 255, 255));
    }
}

/* ------------------------------------------------------------------ cards */

static void hrp_draw_cards(hrp_target *t, const hrp_game *g, int sx, int sy)
{
    for (int i = 0; i < g->card_count; i++) {
        const hrp_card *c = &g->cards[i];
        if (!c->alive) continue;
        uint32_t base;
        switch (c->kind) {
        case HRP_CARD_SOLID: base = HRP_RGBA(120, 120, 140); break;
        case HRP_CARD_TOUGH: base = HRP_RGBA(220, 150, 60); break;
        case HRP_CARD_POWER: base = HRP_RGBA(90, 220, 160); break;
        default:             base = HRP_RGBA(220, 80, 110); break;
        }
        if (c->flash) base = HRP_RGBA(255, 255, 255);
        hrp_rect(t, c->x + sx, c->y + sy, c->w, c->h, base);
        hrp_frame_rect(t, c->x + sx, c->y + sy, c->w, c->h, HRP_RGBA(20, 10, 30));
        /* inner mark: hp pips */
        if (c->kind == HRP_CARD_TOUGH && c->hp > 1)
            hrp_rect(t, c->x + sx + c->w / 2 - 3, c->y + sy + 5, 6, 6, HRP_RGBA(255, 240, 200));
        if (c->kind == HRP_CARD_POWER)
            hrp_disc(t, c->x + sx + c->w / 2, c->y + sy + c->h / 2, 4, HRP_RGBA(255, 255, 255));
    }
}

/* ----------------------------------------------------------------- player */

static void hrp_draw_player(hrp_target *t, const hrp_game *g, int sx, int sy)
{
    const hrp_player *p = &g->player;
    int x = p->x + sx, y = HRP_PLAYER_Y + sy;

    if (p->invuln > 0 && (g->frame / 4) % 2 == 0) return; /* blink */

    /* rod / gohei swing */
    if (p->rod_timer > 0) {
        int reach = 34 - p->rod_timer;
        int dir = p->facing ? 1 : -1;
        hrp_line(t, x + p->w / 2, y + 8, x + p->w / 2 + dir * reach, y - 18, HRP_RGBA(210, 200, 160));
        hrp_rect(t, x + p->w / 2 + dir * reach - 3, y - 24, 7, 10, HRP_RGBA(255, 255, 255));
    }

    /* hakama (red) and haori (white) */
    hrp_rect(t, x + 6, y + 14, p->w - 12, 12, HRP_RGBA(210, 40, 70));
    hrp_rect(t, x + 4, y + 6, p->w - 8, 12, HRP_RGBA(240, 240, 250));
    /* sleeves */
    hrp_rect(t, x, y + 8, 8, 14, HRP_RGBA(220, 60, 90));
    hrp_rect(t, x + p->w - 8, y + 8, 8, 14, HRP_RGBA(220, 60, 90));
    /* head and hair */
    hrp_disc(t, x + p->w / 2, y + 2, 7, HRP_RGBA(255, 224, 196));
    hrp_rect(t, x + p->w / 2 - 9, y - 6, 18, 7, HRP_RGBA(40, 30, 45));
    hrp_rect(t, x + p->w / 2 - 10, y - 2, 4, 14, HRP_RGBA(40, 30, 45));
    hrp_rect(t, x + p->w / 2 + 6, y - 2, 4, 14, HRP_RGBA(40, 30, 45));
    hrp_rect(t, x + p->w / 2 - 6, y - 9, 12, 4, HRP_RGBA(235, 90, 120)); /* ribbon */
}

/* -------------------------------------------------------------------- orb */

static void hrp_draw_orb(hrp_target *t, const hrp_game *g, int sx, int sy)
{
    if (!g->orb.active && !g->orb.stuck) return;
    int cx = hrp_fx_floor(g->orb.x) + sx;
    int cy = hrp_fx_floor(g->orb.y) + sy;
    int r = g->orb.radius;
    int spin = (g->orb.spin / 8) % 4; /* quarter turns */
    uint32_t light = HRP_RGBA(250, 250, 255);
    uint32_t dark = HRP_RGBA(40, 30, 60);

    for (int y = -r; y <= r; y++) {
        for (int x = -r; x <= r; x++) {
            if (x * x + y * y > r * r) continue;
            int px = x;
            switch (spin) {
            case 1: px = -y; break;
            case 2: px = -x; break;
            case 3: px = y; break;
            default: break;
            }
            hrp_px(t, cx + x, cy + y, px > 0 ? dark : light);
        }
    }
    /* the two eyes of the yin-yang */
    int eye = r / 2;
    if (eye < 1) eye = 1;
    hrp_disc(t, cx, cy - eye, eye, dark);
    hrp_disc(t, cx, cy + eye, eye, light);
    hrp_ring(t, cx, cy, r, 1, HRP_RGBA(255, 120, 160));
}

/* ----------------------------------------------------------------- bullets */

static void hrp_draw_bullets(hrp_target *t, const hrp_game *g, int sx, int sy)
{
    for (int i = 0; i < HRP_MAX_BULLETS; i++) {
        const hrp_bullet *b = &g->bullets[i];
        if (!b->alive) continue;
        uint32_t outer = b->kind ? HRP_RGBA(150, 110, 255) : HRP_RGBA(255, 90, 90);
        hrp_disc(t, hrp_fx_floor(b->x) + sx, hrp_fx_floor(b->y) + sy, b->r, outer);
        hrp_disc(t, hrp_fx_floor(b->x) + sx, hrp_fx_floor(b->y) + sy, b->r / 2, HRP_RGBA(255, 245, 245));
    }
}

static void hrp_draw_shots(hrp_target *t, const hrp_game *g, int sx, int sy)
{
    for (int i = 0; i < HRP_MAX_SHOTS; i++) {
        const hrp_shot *s = &g->shots[i];
        if (!s->alive) continue;
        hrp_rect(t, s->x + sx - 2, s->y + sy - 8, 4, 12, HRP_RGBA(255, 240, 160));
        hrp_rect(t, s->x + sx - 1, s->y + sy - 12, 2, 6, HRP_RGBA(255, 255, 255));
    }
}

static void hrp_draw_drops(hrp_target *t, const hrp_game *g, int sx, int sy)
{
    for (int i = 0; i < HRP_MAX_DROPS; i++) {
        const hrp_drop *d = &g->drops[i];
        if (!d->alive) continue;
        int x = hrp_fx_floor(d->x) + sx, y = hrp_fx_floor(d->y) + sy;
        uint32_t c = d->kind ? HRP_RGBA(255, 120, 200) : HRP_RGBA(120, 255, 180);
        hrp_rect(t, x - 5, y - 5, 10, 10, c);
        hrp_rect(t, x - 2, y - 2, 4, 4, HRP_RGBA(255, 255, 255));
    }
}

static void hrp_draw_particles(hrp_target *t, const hrp_game *g, int sx, int sy)
{
    for (int i = 0; i < HRP_MAX_PARTICLES; i++) {
        const hrp_particle *p = &g->particles[i];
        if (!p->alive) continue;
        int size = p->life > 20 ? 3 : 2;
        hrp_rect(t, hrp_fx_floor(p->x) + sx, hrp_fx_floor(p->y) + sy, size, size, p->color);
    }
}

/* -------------------------------------------------------------------- boss */

static void hrp_draw_boss(hrp_target *t, const hrp_game *g, int sx, int sy)
{
    if (!g->boss.alive) return;
    int cx = hrp_fx_floor(g->boss.x) + sx;
    int cy = hrp_fx_floor(g->boss.y) + sy;
    uint32_t base = g->boss.hit_flash ? HRP_RGBA(255, 255, 255) : HRP_RGBA(180, 90, 220);

    /* rotating hexagonal emblem made of primitives */
    for (int layer = 0; layer < 3; layer++) {
        int r = 30 - layer * 8;
        int phase = (g->frame / (2 + layer)) % 360;
        for (int v = 0; v < 6; v++) {
            int a0 = phase + v * 60;
            int a1 = phase + ((v + 1) % 6) * 60;
            int px = hrp_fx_floor(hrp_fx_mul(HRP_FX(r), hrp_cos_fx(a0)));
            int py = hrp_fx_floor(hrp_fx_mul(HRP_FX(r), hrp_sin_fx(a0)));
            int qx = hrp_fx_floor(hrp_fx_mul(HRP_FX(r), hrp_cos_fx(a1)));
            int qy = hrp_fx_floor(hrp_fx_mul(HRP_FX(r), hrp_sin_fx(a1)));
            hrp_line(t, cx + px, cy + py, cx + qx, cy + qy, base);
        }
    }
    hrp_disc(t, cx, cy, 10, g->boss.phase ? HRP_RGBA(255, 140, 90) : HRP_RGBA(255, 255, 255));
    hrp_ring(t, cx, cy, 14, 2, base);

    /* boss hp bar */
    int w = 240, x = (HRP_SCREEN_W - w) / 2, y = HRP_FIELD_TOP - 22;
    hrp_frame_rect(t, x - 1, y - 1, w + 2, 8, HRP_RGBA(200, 200, 220));
    int filled = g->boss.max_hp ? (int)((int64_t)g->boss.hp * w / g->boss.max_hp) : 0;
    if (filled < 0) filled = 0;
    if (filled > w) filled = w;
    hrp_rect(t, x, y, filled, 6, g->boss.phase ? HRP_RGBA(255, 120, 80) : HRP_RGBA(120, 230, 140));
}

/* ---------------------------------------------------------------- overlays */

static void hrp_overlay_panel(hrp_target *t, int y, int h)
{
    for (int j = 0; j < h; j++)
        for (int x = 0; x < HRP_SCREEN_W; x++) {
            uint32_t c = HRP_RGBA(10, 6, 24);
            hrp_px(t, x, y + j, c);
        }
}

static void hrp_draw_overlays(hrp_target *t, const hrp_game *g)
{
    switch (g->state) {
    case HRP_STATE_TITLE:
        hrp_text_centered(t, 90, "HIGHLY RESPONSIVE ORBS", 3, HRP_RGBA(255, 230, 240));
        hrp_text_centered(t, 132, "an open card-breaking homage", 1, HRP_RGBA(200, 200, 230));
        hrp_text_centered(t, 190, "MOVE   DPAD / LEFT STICK", 1, HRP_RGBA(180, 220, 255));
        hrp_text_centered(t, 208, "A      LAUNCH ORB / SHOOT / ROD", 1, HRP_RGBA(180, 220, 255));
        hrp_text_centered(t, 226, "B      BOMB (BOSS STAGES)", 1, HRP_RGBA(180, 220, 255));
        hrp_text_centered(t, 244, "PLUS   PAUSE", 1, HRP_RGBA(180, 220, 255));
        if ((g->frame / 30) % 2 == 0)
            hrp_text_centered(t, 290, "PRESS A TO START", 2, HRP_RGBA(255, 255, 180));
        hrp_text_centered(t, 330, "no original game data is bundled with this build", 1, HRP_RGBA(150, 150, 180));
        break;
    case HRP_STATE_RESPAWN:
        if (g->state_timer > 8)
            hrp_text_centered(t, 170, "READY", 2, HRP_RGBA(255, 255, 180));
        break;
    case HRP_STATE_STAGE_CLEAR:
        hrp_overlay_panel(t, 150, 60);
        hrp_text_centered(t, 162, "STAGE CLEAR", 2, HRP_RGBA(255, 255, 200));
        break;
    case HRP_STATE_PAUSED:
        hrp_overlay_panel(t, 150, 60);
        hrp_text_centered(t, 168, "PAUSED", 2, HRP_RGBA(200, 230, 255));
        break;
    case HRP_STATE_GAME_OVER:
        hrp_overlay_panel(t, 140, 80);
        hrp_text_centered(t, 152, "GAME OVER", 3, HRP_RGBA(255, 140, 140));
        if (g->state_timer > 150)
            hrp_text_centered(t, 196, "PRESS A", 1, HRP_RGBA(255, 255, 200));
        break;
    case HRP_STATE_ALL_CLEAR:
        hrp_overlay_panel(t, 130, 100);
        hrp_text_centered(t, 144, "ALL STAGES CLEAR", 2, HRP_RGBA(255, 255, 200));
        if (g->state_timer > 300)
            hrp_text_centered(t, 200, "PRESS A", 1, HRP_RGBA(255, 255, 200));
        break;
    default:
        break;
    }
}

/* ------------------------------------------------------------------- entry */

void hrp_render_frame(const hrp_game *g, uint32_t *rgba, int width, int height)
{
    if (!rgba || !g) return;
    hrp_target t;
    t.fb = rgba;
    t.w = width;
    t.h = height;
    t.ox = (width - HRP_SCREEN_W) / 2;
    t.oy = (height - HRP_SCREEN_H) / 2;
    if (t.ox < 0) t.ox = 0;
    if (t.oy < 0) t.oy = 0;

    /* clear the letterboxed area */
    for (int y = 0; y < height; y++)
        for (int x = 0; x < width; x++)
            rgba[(size_t)y * width + x] = HRP_RGBA(0, 0, 0);

    int shake = g->shake;
    int sx = 0, sy = 0;
    if (shake > 0) {
        sx = ((g->frame * 7) % (shake + 1)) - shake / 2;
        sy = ((g->frame * 5) % (shake + 1)) - shake / 2;
    }

    hrp_draw_background(&t, g->frame);
    hrp_draw_cards(&t, g, sx, sy);
    hrp_draw_boss(&t, g, sx, sy);
    hrp_draw_drops(&t, g, sx, sy);
    hrp_draw_bullets(&t, g, sx, sy);
    hrp_draw_shots(&t, g, sx, sy);
    if (g->state != HRP_STATE_TITLE)
        hrp_draw_player(&t, g, sx, sy);
    hrp_draw_orb(&t, g, sx, sy);
    hrp_draw_particles(&t, g, sx, sy);
    hrp_draw_hud(&t, g);
    hrp_draw_overlays(&t, g);

    if (g->flash > 0) {
        uint32_t white = HRP_RGBA(255, 255, 255);
        int alpha_step = g->flash;
        for (int y = 0; y < HRP_SCREEN_H; y += 2)
            for (int x = 0; x < HRP_SCREEN_W; x += 2)
                if ((x + y + alpha_step) % 3 == 0) hrp_px(&t, x, y, white);
    }
}
