/* th01_pkg.c - native tests for the open data package reader.
 *
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2026 contributors to the 1warriorscats1-sys/- repository.
 */
#include <stdio.h>
#include <stdlib.h>
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

static size_t put_u32(uint8_t *b, size_t o, uint32_t v)
{
    b[o++] = (uint8_t)(v & 0xff);
    b[o++] = (uint8_t)((v >> 8) & 0xff);
    b[o++] = (uint8_t)((v >> 16) & 0xff);
    b[o++] = (uint8_t)((v >> 24) & 0xff);
    return o;
}

static size_t put_u16(uint8_t *b, size_t o, uint16_t v)
{
    b[o++] = (uint8_t)(v & 0xff);
    b[o++] = (uint8_t)((v >> 8) & 0xff);
    return o;
}

static size_t put_bytes(uint8_t *b, size_t o, const void *data, size_t n)
{
    memcpy(b + o, data, n);
    return o + n;
}

/* builds: header + stages section (one card stage, one boss stage) + text */
static size_t build_package(uint8_t *b, size_t capacity)
{
    uint8_t payload[1024];
    memset(payload, 0, sizeof(payload));

    size_t p = 0;
    p = put_u16(payload, p, 2); /* stage count */

    /* card stage: 2 rows x 3 columns */
    p = put_bytes(payload, p, (uint8_t[]){ 0, 2, 3, 20, 0 }, 5);
    p = put_u16(payload, p, 0); /* boss_hp */
    p = put_u16(payload, p, 0); /* time_limit */
    p = put_bytes(payload, p, (uint8_t[]){ 0 }, 1); /* reserved */
    p = put_bytes(payload, p, "CARD STAGE", 10);
    p += HRP_STAGE_NAME - 10; /* zero padded name field */
    p = put_bytes(payload, p, (uint8_t[]){ 1, 1, 2, 3, 4, 5 }, 6);

    /* boss stage */
    p = put_bytes(payload, p, (uint8_t[]){ 1, 0, 0, 20, 1 }, 5);
    p = put_u16(payload, p, 900);
    p = put_u16(payload, p, 3600);
    p = put_bytes(payload, p, (uint8_t[]){ 0 }, 1);
    p = put_bytes(payload, p, "BOSS STAGE", 10);
    p += HRP_STAGE_NAME - 10;

    size_t q = 0;
    q = put_bytes(b, q, HRP_PKG_MAGIC, 8);
    q = put_u16(b, q, HRP_PKG_VERSION);
    q = put_u16(b, q, 2); /* section count */
    q = put_u32(b, q, 0);

    q = put_u32(b, q, 1); /* STAGES */
    q = put_u32(b, q, (uint32_t)p);
    q = put_bytes(b, q, payload, p);
    while (q % 4) b[q++] = 0;

    uint8_t text[64];
    memset(text, 0, sizeof(text));
    size_t t = 0;
    const char *line = "TITLE=unit test set";
    text[t++] = (uint8_t)strlen(line);
    memcpy(text + t, line, strlen(line));
    t += strlen(line);

    q = put_u32(b, q, 3); /* TEXT */
    q = put_u32(b, q, (uint32_t)t);
    q = put_bytes(b, q, text, t);
    while (q % 4) b[q++] = 0;

    (void)capacity;
    return q;
}

int main(void)
{
    static uint8_t pkg[2048];
    size_t size = build_package(pkg, sizeof(pkg));

    hrp_stage_def stages[HRP_MAX_STAGES];
    int count = 0;
    char title[64] = "";
    int rc = hrp_pkg_parse(pkg, size, stages, HRP_MAX_STAGES, &count, NULL, title, sizeof(title));
    CHECK(rc == HRP_PKG_OK, "a well formed package must parse");
    CHECK(count == 2, "two stages must be read");
    CHECK(stages[0].type == HRP_STAGE_CARD, "first stage is a card stage");
    CHECK(stages[0].rows == 2 && stages[0].cols == 3, "grid size must survive the round trip");
    CHECK(stages[0].orb_speed == 20, "orb speed must survive the round trip");
    CHECK(strcmp(stages[0].name, "CARD STAGE") == 0, "stage name must survive the round trip");
    CHECK(stages[0].cells[0] == 1 && stages[0].cells[5] == 5, "cells must survive the round trip");
    CHECK(stages[1].type == HRP_STAGE_BOSS, "second stage is a boss stage");
    CHECK(stages[1].boss_hp == 900, "boss hp must survive the round trip");
    CHECK(stages[1].boss_pattern == 1, "boss pattern must survive the round trip");
    CHECK(strcmp(title, "unit test set") == 0, "TITLE= line must be picked up");

    /* reject truncated data */
    int dummy = 0;
    CHECK(hrp_pkg_parse(pkg, 8, stages, HRP_MAX_STAGES, &dummy, NULL, NULL, 0) == HRP_PKG_ERR_TOO_SMALL,
          "a short buffer must be rejected");
    CHECK(hrp_pkg_parse(pkg, size - 40, stages, HRP_MAX_STAGES, &dummy, NULL, NULL, 0) != HRP_PKG_OK,
          "a truncated section must be rejected");

    /* reject a bad magic */
    uint8_t broken[2048];
    memcpy(broken, pkg, size);
    broken[0] = 'X';
    CHECK(hrp_pkg_parse(broken, size, stages, HRP_MAX_STAGES, &dummy, NULL, NULL, 0) == HRP_PKG_ERR_MAGIC,
          "a bad magic must be rejected");

    /* reject an unsupported version */
    memcpy(broken, pkg, size);
    broken[8] = 9;
    CHECK(hrp_pkg_parse(broken, size, stages, HRP_MAX_STAGES, &dummy, NULL, NULL, 0) == HRP_PKG_ERR_VERSION,
          "an unknown version must be rejected");

    /* reject an impossible grid */
    memcpy(broken, pkg, size);
    broken[16 + 8 + 3] = 100; /* rows = 100 with cols = 3 exceeds HRP_MAX_CARDS */
    CHECK(hrp_pkg_parse(broken, size, stages, HRP_MAX_STAGES, &dummy, NULL, NULL, 0) != HRP_PKG_OK,
          "an oversized card grid must be rejected");

    /* a file on disk goes through the same path */
    const char *path = getenv("TMPDIR") ? getenv("TMPDIR") : "/tmp";
    char file_path[512];
    snprintf(file_path, sizeof(file_path), "%s/th01_pkg_test.dat", path);
    FILE *f = fopen(file_path, "wb");
    CHECK(f != NULL, "the temporary package file must be writable");
    if (f) {
        fwrite(pkg, 1, size, f);
        fclose(f);
        int file_count = 0;
        char file_title[64] = "";
        int file_rc = hrp_content_load_file(file_path, stages, HRP_MAX_STAGES, &file_count,
                                            file_title, sizeof(file_title));
        CHECK(file_rc == HRP_PKG_OK, "loading a package from disk must work");
        CHECK(file_count == 2, "the on-disk package holds two stages");
        remove(file_path);
    }

    if (failures) {
        printf("th01_pkg: %d failure(s)\n", failures);
        return 1;
    }
    printf("th01_pkg: all checks passed\n");
    return 0;
}
