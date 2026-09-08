/* hrp_switch_mapping.inc - controller mapping for the Switch build.
 *
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2026 contributors to the 1warriorscats1-sys/- repository.
 *
 * Touhou-style convention, matching the mapping used by the other ports in this
 * repository: d-pad and the left stick move, A acts, B bombs, Plus pauses. The
 * right stick is deliberately not read.
 *
 * Button names are libnx HidNpadButton_* values (not the 3DS KEY_* names).
 *
 * This file is included (not compiled) so the mapping stays auditable as data.
 */

/* { key, hrp button } */
static const struct {
    uint64_t key;
    uint16_t button;
} hrp_switch_key_map[] = {
    { HidNpadButton_Left,  HRP_BTN_LEFT },
    { HidNpadButton_Right, HRP_BTN_RIGHT },
    { HidNpadButton_A,     HRP_BTN_SHOOT },
    { HidNpadButton_ZR,    HRP_BTN_SHOOT },
    { HidNpadButton_B,     HRP_BTN_BOMB },
    { HidNpadButton_ZL,    HRP_BTN_BOMB },
    { HidNpadButton_Plus,  HRP_BTN_PAUSE },
    { HidNpadButton_X,     HRP_BTN_CONFIRM },
    { HidNpadButton_Y,     HRP_BTN_CONFIRM },
};

/* Analogue stick dead zone, in libnx units (0..32767). */
#define HRP_SWITCH_DEADZONE 12000

/* Right stick: intentionally unmapped. */
#define HRP_SWITCH_RIGHT_STICK_ENABLED 0
