/* hrp.h - public core of the open TH01-style engine (game logic, no platform).
 *
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2026 contributors to the 1warriorscats1-sys/- repository.
 *
 * Everything here is original code: a homage to the *gameplay* of the first
 * PC-98 Touhou title (block-breaking card stages alternating with boss
 * encounters). No original artwork, music, data or trademarks are used.
 */
#ifndef HRP_H
#define HRP_H

#include <stdint.h>
#include <stddef.h>

#include "hrp_config.h"
#include "hrp_math.h"

/* ------------------------------------------------------------------ input */

#define HRP_BTN_LEFT    0x0001u
#define HRP_BTN_RIGHT   0x0002u
#define HRP_BTN_SHOOT   0x0004u /* launch the orb / fire / swing the rod */
#define HRP_BTN_BOMB    0x0008u /* clear bullets, damage the boss */
#define HRP_BTN_PAUSE   0x0010u
#define HRP_BTN_CONFIRM 0x0020u

typedef struct {
    uint16_t held;    /* currently held buttons */
    uint16_t pressed; /* buttons that went down this frame */
} hrp_input;

/* ------------------------------------------------------------- game types */

typedef enum {
    HRP_STAGE_CARD = 0,
    HRP_STAGE_BOSS = 1
} hrp_stage_type;

typedef enum {
    HRP_CARD_EMPTY  = 0,
    HRP_CARD_NORMAL = 1, /* one orb hit */
    HRP_CARD_TOUGH  = 2, /* two orb hits */
    HRP_CARD_SOLID  = 3, /* indestructible, still bounces the orb */
    HRP_CARD_POWER  = 4  /* destroyed, then drops a power item */
} hrp_card_kind;

typedef enum {
    HRP_BOSS_SWEEP  = 0, /* side to side, aimed 3-way spread */
    HRP_BOSS_ORBIT  = 1, /* figure eight, radial rings */
    HRP_BOSS_STREAM = 2  /* hops, aimed stream plus slow ring */
} hrp_boss_pattern;

typedef struct {
    uint8_t  type;        /* hrp_stage_type */
    uint8_t  rows, cols;
    uint8_t  orb_speed;   /* orb speed in 1/4 px per frame */
    uint8_t  boss_pattern;/* hrp_boss_pattern */
    uint16_t boss_hp;
    uint16_t time_limit;  /* frames; 0 = none */
    char     name[HRP_STAGE_NAME];
    uint8_t  cells[HRP_MAX_CARDS]; /* rows*cols, hrp_card_kind */
} hrp_stage_def;

typedef enum {
    HRP_STATE_TITLE = 0,
    HRP_STATE_PLAY,
    HRP_STATE_STAGE_CLEAR,
    HRP_STATE_RESPAWN,   /* orb lost / player hit: short reset */
    HRP_STATE_GAME_OVER,
    HRP_STATE_ALL_CLEAR,
    HRP_STATE_PAUSED
} hrp_state;

typedef enum {
    HRP_SFX_NONE = 0,
    HRP_SFX_BOUNCE,
    HRP_SFX_CARD,
    HRP_SFX_BREAK,
    HRP_SFX_SHOOT,
    HRP_SFX_BOMB,
    HRP_SFX_HIT,
    HRP_SFX_CLEAR,
    HRP_SFX_MENU
} hrp_sfx;

typedef struct {
    hrp_fx x, y, vx, vy;
    int    radius;
    uint8_t active;   /* flying */
    uint8_t stuck;    /* held on the player, waiting for launch */
    uint8_t spin;     /* animation phase */
} hrp_orb;

typedef struct {
    int x, y, w, h;
    int hp;
    uint8_t kind;
    uint8_t alive;
    uint8_t flash;
} hrp_card;

typedef struct {
    hrp_fx x, y, vx, vy;
    int  r;
    uint8_t alive;
    uint8_t kind;
} hrp_bullet;

typedef struct {
    int x, y, vy;
    uint8_t alive;
} hrp_shot;

typedef struct {
    hrp_fx x, y, vx, vy;
    int life;
    uint32_t color;
    uint8_t alive;
} hrp_particle;

typedef struct {
    hrp_fx x, y, vy;
    uint8_t alive;
    uint8_t kind; /* 0 = power, 1 = extend */
} hrp_drop;

typedef struct {
    hrp_fx x, y;
    int w;
    int hp, max_hp;
    uint8_t phase;
    uint8_t pattern;
    uint8_t alive;
    uint16_t timer;
    uint16_t shot_timer;
    uint16_t hit_flash;
} hrp_boss;

typedef struct {
    int x, w;
    int speed;
    int lives;
    int bombs;
    int invuln;
    int shot_level;
    int shot_cooldown;
    uint8_t rod_timer;
    uint8_t facing;
} hrp_player;

typedef struct {
    /* content */
    const hrp_stage_def *stages;
    int                  stage_count;
    uint32_t             content_id; /* 0 = built-in demo set */

    /* run state */
    hrp_state state;
    int       stage_index;
    int       state_timer;
    int       frame;
    uint32_t  rng_state;
    int       score;
    int       combo;
    int       deaths;
    int       paused_prev_state;
    int       orb_stall;   /* frames since the last destructible card was hit */

    /* field */
    hrp_player   player;
    hrp_orb      orb;
    hrp_boss     boss;
    hrp_card     cards[HRP_MAX_CARDS];
    int          card_count;
    int          cards_left;
    hrp_bullet   bullets[HRP_MAX_BULLETS];
    hrp_shot     shots[HRP_MAX_SHOTS];
    hrp_particle particles[HRP_MAX_PARTICLES];
    hrp_drop     drops[HRP_MAX_DROPS];

    /* presentation events consumed by the platform layer */
    uint8_t sfx[8];
    int     sfx_count;
    uint8_t shake;
    uint8_t flash;

    /* last event, useful for tests and for the host HUD */
    int last_event; /* 0 none, 1 life lost, 2 stage cleared, 3 game over, 4 all clear */
} hrp_game;

/* ------------------------------------------------------------------- rng */

uint32_t hrp_rng_next(uint32_t *state);
int      hrp_rng_range(uint32_t *state, int lo, int hi);

/* ------------------------------------------------------------------ audio */

#define HRP_AUDIO_VOICES 8

typedef enum {
    HRP_MUSIC_NONE = 0,
    HRP_MUSIC_TITLE,
    HRP_MUSIC_CARD,
    HRP_MUSIC_BOSS
} hrp_music;

typedef struct {
    uint32_t phase[HRP_AUDIO_VOICES];
    uint32_t inc[HRP_AUDIO_VOICES];
    int      life[HRP_AUDIO_VOICES];
    int      total[HRP_AUDIO_VOICES];
    int      vol[HRP_AUDIO_VOICES];
    uint8_t  wave[HRP_AUDIO_VOICES];
    uint32_t noise;
    uint32_t step;      /* sequencer position in samples */
    int      music;     /* hrp_music */
} hrp_audio_state;

void hrp_audio_init(hrp_audio_state *st);
void hrp_audio_mix(int16_t *stereo_out, int frames, int sample_rate,
                   hrp_audio_state *st, const uint8_t *sfx, int sfx_count);

/* ------------------------------------------------------------------ logic */

void hrp_game_init(hrp_game *g, const hrp_stage_def *stages, int stage_count, uint32_t seed);
void hrp_game_start_stage(hrp_game *g, int index);
void hrp_game_update(hrp_game *g, const hrp_input *in);
void hrp_game_reset(hrp_game *g);

/* scripted input used by the host demo and the headless tests */
uint16_t hrp_autoplay_buttons(const hrp_game *g);

/* helpers shared with tests */
void hrp_launch_orb(hrp_game *g);
int  hrp_cards_alive(const hrp_game *g);

/* --------------------------------------------------- built-in demo content */

const hrp_stage_def *hrp_demo_stages(int *out_count);

/* -------------------------------------------------------- content package */

/* Result codes for the open data package reader. */
#define HRP_PKG_OK            0
#define HRP_PKG_ERR_TOO_SMALL -1
#define HRP_PKG_ERR_MAGIC     -2
#define HRP_PKG_ERR_VERSION   -3
#define HRP_PKG_ERR_SECTION   -4
#define HRP_PKG_ERR_STAGES    -5

typedef struct {
    uint32_t colors[32];
    int      count;
} hrp_palette;

int hrp_content_load_file(const char *path, hrp_stage_def *stages, int max_stages,
                         int *out_stage_count, char *title, size_t title_size);

int hrp_pkg_parse(const uint8_t *buf, size_t len,
                  hrp_stage_def *stages, int max_stages, int *out_stage_count,
                  hrp_palette *palette, char *title, size_t title_size);

/* ------------------------------------------------------------------ render */

void hrp_render_frame(const hrp_game *g, uint32_t *rgba, int width, int height);

#endif /* HRP_H */
