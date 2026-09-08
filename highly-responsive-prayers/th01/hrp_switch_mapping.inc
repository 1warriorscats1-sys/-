/* hrp_switch_mapping.inc - controller mapping for the Switch build.
 *
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2026 contributors to the 1warriorscats1-sys/- repository.
 *
 * Touhou-style convention, matching the mapping used by the other ports in this
 * repository: d-pad and the left stick move, A acts, B bombs, Plus pauses. The
 * right stick is deliberately not read.
 *
 * This file is included (not compiled) so the mapping stays auditable as data.
 */

/* { key, hrp button } */
static const struct {
    uint64_t key;
    uint16_t button;
} hrp_switch_key_map[] = {
    { KEY_DLEFT,  HRP_BTN_LEFT },
    { KEY_DRIGHT, HRP_BTN_RIGHT },
    { KEY_A,      HRP_BTN_SHOOT },
    { KEY_ZR,     HRP_BTN_SHOOT },
    { KEY_B,      HRP_BTN_BOMB },
    { KEY_ZL,     HRP_BTN_BOMB },
    { KEY_PLUS,   HRP_BTN_PAUSE },
    { KEY_X,      HRP_BTN_CONFIRM },
    { KEY_Y,      HRP_BTN_CONFIRM },
};

/* Analogue stick dead zone, in libnx units (0..32767). */
#define HRP_SWITCH_DEADZONE 12000

/* Right stick: intentionally unmapped. */
#define HRP_SWITCH_RIGHT_STICK_ENABLED 0
