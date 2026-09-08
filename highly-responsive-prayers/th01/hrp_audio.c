/* hrp_audio.c - tiny integer synthesiser (no samples, no third-party audio).
 *
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2026 contributors to the 1warriorscats1-sys/- repository.
 *
 * Square/triangle/noise voices with a linear decay envelope plus a small step
 * sequencer. It exists so the port makes sound without shipping any music.
 */
#include <stdint.h>
#include <string.h>

#include "hrp.h"

#define HRP_WAVE_SQUARE   0
#define HRP_WAVE_TRIANGLE 1
#define HRP_WAVE_NOISE    2

/* MIDI note frequencies in Hz, C4 = 60, one octave-plus range. */
static const uint16_t hrp_note_hz[61] = {
    65, 69, 73, 78, 82, 87, 92, 98, 104, 110, 116, 123,   /* C2..B2 */
    131, 139, 147, 156, 165, 175, 185, 196, 208, 220, 233, 247,
    262, 277, 294, 311, 330, 349, 370, 392, 415, 440, 466, 494,
    523, 554, 587, 622, 659, 698, 740, 784, 831, 880, 932, 988,
    1047, 1109, 1175, 1245, 1319, 1397, 1480, 1568, 1661, 1760, 1865
};

/* 16 step patterns: -1 = rest, otherwise an index into hrp_note_hz */
static const int8_t hrp_music_patterns[3][16] = {
    /* title: slow arpeggio */
    { 24, -1, 28, -1, 31, -1, 28, -1, 24, -1, 19, -1, 24, -1, 28, -1 },
    /* card stage: bouncing eighth notes */
    { 12, 19, 24, 19, 12, 19, 26, 19, 12, 19, 24, 19, 17, 19, 22, 19 },
    /* boss: urgent minor line */
    { 12, 12, 15, 19, 12, 12, 22, 19, 13, 13, 16, 20, 13, 13, 24, 20 },
};

void hrp_audio_init(hrp_audio_state *st)
{
    memset(st, 0, sizeof(*st));
    st->noise = 0x12345678u;
    st->music = HRP_MUSIC_NONE;
}

static void hrp_audio_note(hrp_audio_state *st, int note, int wave, int vol, int ms, int rate)
{
    if (note < 0) return;
    if (note >= (int)(sizeof(hrp_note_hz) / sizeof(hrp_note_hz[0])))
        note = (int)(sizeof(hrp_note_hz) / sizeof(hrp_note_hz[0])) - 1;
    for (int v = 0; v < HRP_AUDIO_VOICES; v++) {
        if (st->life[v] > 0) continue;
        uint32_t freq = hrp_note_hz[note];
        st->phase[v] = 0;
        st->inc[v] = (uint32_t)(((uint64_t)freq << 32) / (uint32_t)rate);
        st->total[v] = (int)((int64_t)ms * rate / 1000);
        st->life[v] = st->total[v];
        st->vol[v] = vol;
        st->wave[v] = (uint8_t)wave;
        return;
    }
}

static void hrp_audio_sfx(hrp_audio_state *st, uint8_t id, int rate)
{
    switch (id) {
    case HRP_SFX_BOUNCE: hrp_audio_note(st, 40, HRP_WAVE_SQUARE, 500, 60, rate); break;
    case HRP_SFX_CARD:   hrp_audio_note(st, 44, HRP_WAVE_SQUARE, 400, 50, rate); break;
    case HRP_SFX_BREAK:  hrp_audio_note(st, 47, HRP_WAVE_TRIANGLE, 700, 120, rate); break;
    case HRP_SFX_SHOOT:  hrp_audio_note(st, 52, HRP_WAVE_SQUARE, 180, 40, rate); break;
    case HRP_SFX_BOMB:   hrp_audio_note(st, 12, HRP_WAVE_NOISE, 1200, 400, rate); break;
    case HRP_SFX_HIT:    hrp_audio_note(st, 10, HRP_WAVE_NOISE, 1000, 260, rate); break;
    case HRP_SFX_CLEAR:  hrp_audio_note(st, 48, HRP_WAVE_TRIANGLE, 900, 420, rate); break;
    case HRP_SFX_MENU:   hrp_audio_note(st, 43, HRP_WAVE_TRIANGLE, 350, 80, rate); break;
    default: break;
    }
}

void hrp_audio_mix(int16_t *stereo_out, int frames, int sample_rate,
                   hrp_audio_state *st, const uint8_t *sfx, int sfx_count)
{
    if (!stereo_out || sample_rate <= 0) return;
    int rate = sample_rate;

    for (int i = 0; i < sfx_count; i++) hrp_audio_sfx(st, sfx[i], rate);

    int steps_per_second = 8;
    int samples_per_step = rate / steps_per_second;
    if (samples_per_step <= 0) samples_per_step = 1;

    for (int f = 0; f < frames; f++) {
        /* sequencer */
        if (st->music > HRP_MUSIC_NONE) {
            int idx = st->music - HRP_MUSIC_TITLE;
            if (idx < 0) idx = 0;
            if (idx > 2) idx = 2;
            if (st->step % (uint32_t)samples_per_step == 0) {
                int pos = (int)((st->step / (uint32_t)samples_per_step) % 16);
                int note = hrp_music_patterns[idx][pos];
                if (note >= 0)
                    hrp_audio_note(st, note, idx == 2 ? HRP_WAVE_SQUARE : HRP_WAVE_TRIANGLE,
                                   idx == 2 ? 260 : 200, 140, rate);
            }
            if (idx == 1 && st->step % (uint32_t)(samples_per_step * 4) == 0)
                hrp_audio_note(st, 0, HRP_WAVE_TRIANGLE, 420, 220, rate);
        }
        st->step++;

        int32_t mix = 0;
        for (int v = 0; v < HRP_AUDIO_VOICES; v++) {
            if (st->life[v] <= 0) continue;
            st->phase[v] += st->inc[v];
            int32_t sample;
            switch (st->wave[v]) {
            case HRP_WAVE_TRIANGLE: {
                uint32_t p = st->phase[v];
                int32_t raw = (p & 0x80000000u) ? (int32_t)(0xffffffffu - p) >> 16
                                                : (int32_t)(p >> 16);
                sample = (raw - 16384) / 8;
                break;
            }
            case HRP_WAVE_NOISE:
                st->noise = st->noise * 1664525u + 1013904223u;
                sample = (int32_t)((st->noise >> 16) & 0xffff) - 32768;
                sample /= 4;
                break;
            case HRP_WAVE_SQUARE:
            default:
                sample = (st->phase[v] & 0x80000000u) ? 1800 : -1800;
                break;
            }
            int envelope = (int)((int64_t)st->life[v] * 256 / (st->total[v] ? st->total[v] : 1));
            mix += (int32_t)((int64_t)sample * st->vol[v] * envelope / (256 * 512));
            if (--st->life[v] <= 0) {
                st->life[v] = 0;
                st->inc[v] = 0;
            }
        }

        if (mix > 22000) mix = 22000;
        if (mix < -22000) mix = -22000;
        int16_t out = (int16_t)mix;
        stereo_out[f * 2 + 0] = out;
        stereo_out[f * 2 + 1] = out;
    }
}
