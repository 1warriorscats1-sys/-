/* hrp_pkg.c - reader for the open TH01 data package ("TH01OPEN").
 *
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2026 contributors to the 1warriorscats1-sys/- repository.
 *
 * The package format is documented in docs/TH01_DATA_FORMAT.md. It exists so a
 * player can point the engine at their own converted content without the engine
 * ever shipping third-party data. The reader is deliberately strict: unknown
 * versions, truncated sections or out-of-range grids are rejected instead of
 * half-applied.
 */
#include <stdio.h>
#include <string.h>

#include "hrp.h"

#define HRP_PKG_HDR 16
#define HRP_SEC_STAGES 1u
#define HRP_SEC_PALETTE 2u
#define HRP_SEC_TEXT 3u
#define HRP_SEC_MUSIC 4u

static uint32_t hrp_read_u32(const uint8_t *p)
{
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

static uint16_t hrp_read_u16(const uint8_t *p)
{
    return (uint16_t)(p[0] | ((uint16_t)p[1] << 8));
}

int hrp_pkg_parse(const uint8_t *buf, size_t len,
                  hrp_stage_def *stages, int max_stages, int *out_stage_count,
                  hrp_palette *palette, char *title, size_t title_size)
{
    if (!buf || !stages || !out_stage_count) return HRP_PKG_ERR_TOO_SMALL;
    *out_stage_count = 0;
    if (len < HRP_PKG_HDR) return HRP_PKG_ERR_TOO_SMALL;
    if (memcmp(buf, HRP_PKG_MAGIC, 8) != 0) return HRP_PKG_ERR_MAGIC;

    uint16_t version = hrp_read_u16(buf + 8);
    uint16_t sections = hrp_read_u16(buf + 10);
    if (version != HRP_PKG_VERSION) return HRP_PKG_ERR_VERSION;

    size_t off = HRP_PKG_HDR;
    int stages_seen = 0;
    int stage_count = 0;

    for (uint16_t i = 0; i < sections; i++) {
        if (off + 8 > len) return HRP_PKG_ERR_SECTION;
        uint32_t type = hrp_read_u32(buf + off);
        uint32_t slen = hrp_read_u32(buf + off + 4);
        off += 8;
        if (slen > len - off) return HRP_PKG_ERR_SECTION;
        const uint8_t *p = buf + off;

        if (type == HRP_SEC_STAGES) {
            if (slen < 2) return HRP_PKG_ERR_STAGES;
            uint16_t count = hrp_read_u16(p);
            size_t q = 2;
            for (uint16_t s = 0; s < count; s++) {
                if (q + 10 > slen) return HRP_PKG_ERR_STAGES;
                if (s >= max_stages) return HRP_PKG_ERR_STAGES;
                hrp_stage_def *st = &stages[stage_count];
                memset(st, 0, sizeof(*st));
                st->type = p[q];
                st->rows = p[q + 1];
                st->cols = p[q + 2];
                st->orb_speed = p[q + 3];
                st->boss_pattern = p[q + 4];
                st->boss_hp = hrp_read_u16(p + q + 5);
                st->time_limit = hrp_read_u16(p + q + 7);
                /* 1 byte reserved at q+9 */
                q += 10;
                if (q + HRP_STAGE_NAME > slen) return HRP_PKG_ERR_STAGES;
                memcpy(st->name, p + q, HRP_STAGE_NAME - 1);
                st->name[HRP_STAGE_NAME - 1] = 0;
                q += HRP_STAGE_NAME;

                int cells = (int)st->rows * (int)st->cols;
                if (cells < 0 || cells > HRP_MAX_CARDS) return HRP_PKG_ERR_STAGES;
                if (q + (size_t)cells > slen) return HRP_PKG_ERR_STAGES;
                if (cells > 0) memcpy(st->cells, p + q, (size_t)cells);
                q += (size_t)cells;

                /* sanity: reject unknown stage kinds and impossible grids */
                if (st->type != HRP_STAGE_CARD && st->type != HRP_STAGE_BOSS)
                    return HRP_PKG_ERR_STAGES;
                if (st->type == HRP_STAGE_CARD && (st->rows == 0 || st->cols == 0))
                    return HRP_PKG_ERR_STAGES;
                if (st->type == HRP_STAGE_BOSS && st->boss_hp == 0)
                    return HRP_PKG_ERR_STAGES;

                stage_count++;
            }
            stages_seen = 1;
        } else if (type == HRP_SEC_PALETTE && palette) {
            if (slen >= 2) {
                uint16_t count = hrp_read_u16(p);
                if (count > 32) count = 32;
                if (slen >= (size_t)(2 + count * 4)) {
                    for (uint16_t c = 0; c < count; c++)
                        palette->colors[c] = hrp_read_u32(p + 2 + c * 4);
                    palette->count = count;
                }
            }
        } else if (type == HRP_SEC_TEXT && title && title_size) {
            /* first line "TITLE=..." wins */
            size_t q = 0;
            while (q < slen) {
                uint8_t l = p[q++];
                if (q + l > slen) break;
                if (l > 5 && memcmp(p + q, "TITLE=", 6) == 0) {
                    size_t n = l - 6;
                    if (n > title_size - 1) n = title_size - 1;
                    memcpy(title, p + q + 6, n);
                    title[n] = 0;
                    break;
                }
                q += l;
            }
        }
        /* HRP_SEC_MUSIC is accepted and ignored by the current synth. */

        off += slen;
        if ((slen & 3u) != 0u) {
            size_t pad = 4 - (slen & 3u);
            if (pad > len - off) break;
            off += pad;
        }
    }

    if (!stages_seen || stage_count == 0) return HRP_PKG_ERR_STAGES;
    *out_stage_count = stage_count;
    return HRP_PKG_OK;
}

/* Convenience helper shared by every platform backend: read a package from
 * disk and parse it. Returns HRP_PKG_OK or a negative error code. */
int hrp_content_load_file(const char *path, hrp_stage_def *stages, int max_stages,
                          int *out_stage_count, char *title, size_t title_size)
{
    if (!path || !stages || !out_stage_count) return HRP_PKG_ERR_TOO_SMALL;
    *out_stage_count = 0;

    FILE *f = fopen(path, "rb");
    if (!f) return HRP_PKG_ERR_TOO_SMALL;
    if (fseek(f, 0, SEEK_END) != 0) { fclose(f); return HRP_PKG_ERR_TOO_SMALL; }
    long size = ftell(f);
    if (size < 0 || size > (1L << 20)) { fclose(f); return HRP_PKG_ERR_TOO_SMALL; }
    rewind(f);

    static uint8_t buffer[1 << 20];
    size_t got = fread(buffer, 1, (size_t)size, f);
    fclose(f);
    if (got != (size_t)size) return HRP_PKG_ERR_TOO_SMALL;

    hrp_palette palette;
    memset(&palette, 0, sizeof(palette));
    return hrp_pkg_parse(buffer, got, stages, max_stages, out_stage_count, &palette, title, title_size);
}
