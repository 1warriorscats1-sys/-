/* th01_game.c - native unit tests for the simulation core.
 *
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2026 contributors to the 1warriorscats1-sys/- repository.
 *
 * No platform code: the tests drive hrp_game_* directly so the same assertions
 * hold on the host and on the Switch.
 */
#include <stdio.h>
#include <string.h>

#include "hrp.h"

static int failures = 0;

#define CHECK(cond, message)                                                   \
    do {                                                                       \
        if (!(cond)) {                                                         \
            printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, (message));        \
            failures++;                                                        \
        }                                                                      \
    } while (0)

static hrp_stage_def g_stages[4];

static void fixture(void)
{
    memset(g_stages, 0, sizeof(g_stages));
    g_stages[0].type = HRP_STAGE_CARD;
    g_stages[0].rows = 3;
    g_stages[0].cols = 4;
    g_stages[0].orb_speed = 20;
    strcpy(g_stages[0].name, "TEST CARDS");
    for (int i = 0; i < 12; i++) g_stages[0].cells[i] = HRP_CARD_NORMAL;

    g_stages[1].type = HRP_STAGE_BOSS;
    g_stages[1].boss_hp = 400;
    g_stages[1].boss_pattern = HRP_BOSS_SWEEP;
    g_stages[1].time_limit = 3600;
    strcpy(g_stages[1].name, "TEST BOSS");
}

static void step(hrp_game *g, uint16_t buttons, uint16_t *prev, int frames)
{
    for (int i = 0; i < frames; i++) {
        hrp_input in;
        in.held = buttons;
        in.pressed = (uint16_t)(buttons & ~(*prev));
        *prev = buttons;
        hrp_game_update(g, &in);
    }
}

static void test_trig(void)
{
    /* integer trig must stay usable: sin(90 deg) ~= 1, cos(0) ~= 1 */
    int s90 = hrp_fx_round(hrp_sin_fx(90));
    int c0 = hrp_fx_round(hrp_cos_fx(0));
    int s0 = hrp_fx_round(hrp_sin_fx(0));
    CHECK(s90 == 1, "sin(90) should be 1");
    CHECK(c0 == 1, "cos(0) should be 1");
    CHECK(s0 == 0, "sin(0) should be 0");
    CHECK(hrp_sin_fx(180) == 0 || hrp_abs_i(hrp_sin_fx(180)) < 1000, "sin(180) should be ~0");
    CHECK(hrp_sin_fx(-90) == -hrp_sin_fx(90), "sin must be odd");
}

static void test_orb_bounce(void)
{
    hrp_game g;
    hrp_game_init(&g, g_stages, 2, 0x1234u);
    hrp_game_start_stage(&g, 0);
    uint16_t prev = 0;

    /* launch the orb straight up and let it hit the top wall */
    g.orb.vx = 0;
    step(&g, HRP_BTN_SHOOT, &prev, 1); /* pressed edge launches */
    CHECK(!g.orb.stuck, "orb should be launched by the shoot edge");
    CHECK(g.orb.vy < 0, "orb should travel upwards after launch");

    hrp_fx vy_before = g.orb.vy;
    step(&g, 0, &prev, 200);
    CHECK(g.orb.vy > 0 || g.state != HRP_STATE_PLAY || g.orb.stuck,
          "orb must come back down or be re-placed");
    (void)vy_before;
}

static void test_card_destruction(void)
{
    hrp_game g;
    hrp_game_init(&g, g_stages, 2, 0x1234u);
    hrp_game_start_stage(&g, 0);
    int before = hrp_cards_alive(&g);
    CHECK(before == 12, "fixture should have 12 cards");

    /* drive the orb straight into the card field */
    g.orb.stuck = 0;
    g.orb.active = 1;
    g.orb.x = HRP_FX(g.cards[8].x + g.cards[8].w / 2);
    g.orb.y = HRP_FX(g.cards[8].y + g.cards[8].h + 12);
    g.orb.vx = 0;
    g.orb.vy = -HRP_FX(6);

    uint16_t prev = 0;
    step(&g, 0, &prev, 30);
    CHECK(hrp_cards_alive(&g) < before, "the orb must destroy at least one card");
    CHECK(g.score > 0, "destroying a card must score");
}

static void test_life_loss(void)
{
    hrp_game g;
    hrp_game_init(&g, g_stages, 2, 0x1234u);
    hrp_game_start_stage(&g, 0);
    int lives = g.player.lives;

    /* drop the orb below the field with no paddle underneath */
    g.player.x = HRP_FIELD_LEFT;
    g.orb.stuck = 0;
    g.orb.active = 1;
    g.orb.x = HRP_FX(HRP_FIELD_RIGHT - 8);
    g.orb.y = HRP_FX(HRP_SCREEN_H - 20);
    g.orb.vx = 0;
    g.orb.vy = HRP_FX(8);

    uint16_t prev = 0;
    step(&g, 0, &prev, 10);
    CHECK(g.state == HRP_STATE_RESPAWN || g.player.lives < lives,
          "losing the orb must cost a life or start a respawn");
    step(&g, 0, &prev, 120);
    CHECK(g.player.lives == lives - 1, "exactly one life is consumed");
}

static void test_boss_damage(void)
{
    hrp_game g;
    hrp_game_init(&g, g_stages, 2, 0x1234u);
    hrp_game_start_stage(&g, 1);
    CHECK(g.boss.alive, "boss stage must spawn a boss");
    int hp0 = g.boss.hp;

    /* stand under the boss and hold shoot */
    g.player.x = hrp_fx_floor(g.boss.x) - g.player.w / 2;
    uint16_t prev = 0;
    step(&g, HRP_BTN_SHOOT, &prev, 240);
    CHECK(g.boss.hp < hp0 || !g.boss.alive, "shots must damage the boss");
}

static void test_solid_row_does_not_trap_the_orb(void)
{
    /* A bottom row of indestructible cards (with small side gaps) must not be
     * able to trap the orb in a vertical loop forever: hrp_orb_break_stall
     * rotates the velocity until the orb escapes sideways. */
    hrp_stage_def trapped[1];
    memset(trapped, 0, sizeof(trapped));
    trapped[0].type = HRP_STAGE_CARD;
    trapped[0].rows = 3;
    trapped[0].cols = 6;
    trapped[0].orb_speed = 20;
    strcpy(trapped[0].name, "TRAP");
    for (int r = 0; r < 3; r++)
        for (int c = 0; c < 6; c++)
            trapped[0].cells[r * 6 + c] = (r < 2) ? HRP_CARD_NORMAL
                                                  : ((c == 0 || c == 5) ? HRP_CARD_EMPTY
                                                                        : HRP_CARD_SOLID);

    hrp_game g;
    hrp_game_init(&g, trapped, 1, 0x51u);
    hrp_game_start_stage(&g, 0);
    int before = hrp_cards_alive(&g);
    CHECK(before == 12, "the trap fixture has 12 destructible cards");

    /* launch straight up into the solid row and let the scripted input play */
    g.orb.stuck = 0;
    g.orb.active = 1;
    g.orb.vx = 0;
    g.orb.vy = -HRP_FX(5);

    uint16_t prev = 0;
    for (int i = 0; i < 2400; i++) {
        uint16_t buttons = hrp_autoplay_buttons(&g);
        hrp_input in = { buttons, (uint16_t)(buttons & ~prev) };
        prev = buttons;
        hrp_game_update(&g, &in);
    }
    CHECK(hrp_cards_alive(&g) < before,
          "the orb must escape a vertical trap and destroy cards");
}

static void test_determinism(void)
{
    hrp_game a, b;
    hrp_game_init(&a, g_stages, 2, 0xbeefu);
    hrp_game_init(&b, g_stages, 2, 0xbeefu);
    uint16_t pa = 0, pb = 0;

    for (int i = 0; i < 900; i++) {
        uint16_t buttons_a = hrp_autoplay_buttons(&a);
        uint16_t buttons_b = hrp_autoplay_buttons(&b);
        hrp_input ina = { buttons_a, (uint16_t)(buttons_a & ~pa) };
        hrp_input inb = { buttons_b, (uint16_t)(buttons_b & ~pb) };
        pa = buttons_a;
        pb = buttons_b;
        hrp_game_update(&a, &ina);
        hrp_game_update(&b, &inb);
    }
    CHECK(a.score == b.score, "identical inputs must give identical scores");
    CHECK(a.frame == b.frame, "frame counters must match");
    CHECK(memcmp(a.bullets, b.bullets, sizeof(a.bullets)) == 0, "bullet state must match");
    CHECK(memcmp(a.cards, b.cards, sizeof(a.cards)) == 0, "card state must match");
}

static void test_pause_and_title(void)
{
    hrp_game g;
    hrp_game_init(&g, g_stages, 2, 0x1234u);
    CHECK(g.state == HRP_STATE_TITLE, "a fresh game starts on the title screen");

    uint16_t prev = 0;
    step(&g, HRP_BTN_CONFIRM, &prev, 1);
    CHECK(g.state == HRP_STATE_PLAY, "confirm must start the first stage");

    step(&g, HRP_BTN_PAUSE, &prev, 1);
    CHECK(g.state == HRP_STATE_PAUSED, "pause must enter the paused state");
    int score = g.score;
    step(&g, 0, &prev, 60);
    CHECK(g.score == score, "nothing may score while paused");
    step(&g, HRP_BTN_PAUSE, &prev, 1);
    CHECK(g.state == HRP_STATE_PLAY, "pause must be reversible");
}

int main(void)
{
    fixture();
    test_trig();
    test_orb_bounce();
    test_card_destruction();
    test_solid_row_does_not_trap_the_orb();
    test_life_loss();
    test_boss_damage();
    test_determinism();
    test_pause_and_title();

    if (failures) {
        printf("th01_game: %d failure(s)\n", failures);
        return 1;
    }
    printf("th01_game: all checks passed\n");
    return 0;
}
