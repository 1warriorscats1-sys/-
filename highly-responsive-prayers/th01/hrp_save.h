/* hrp_save.h - crash-safe save replacement.
 *
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2026 contributors to the 1warriorscats1-sys/- repository.
 *
 * Writes to a temporary file, flushes, renames and keeps one previous
 * generation, so an interrupted write leaves either the old or the new file and
 * never a truncated one. Same approach as the repository's other Switch
 * integrations.
 */
#ifndef HRP_SAVE_H
#define HRP_SAVE_H

#include <stdio.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

#if defined(__unix__) || defined(__APPLE__) || defined(__SWITCH__)
#define HRP_SAVE_HAVE_FSYNC 1
#include <unistd.h>
#ifdef __SWITCH__
/* libnx implements fsync() through its fsdev layer; declare it explicitly so
 * the build does not depend on the newlib feature-test selection. */
extern int fsync(int fd);
#endif
#endif

#define HRP_SAVE_MAGIC   0x314f5248u /* "HRO1" little endian */
#define HRP_SAVE_VERSION 1

typedef struct {
    uint32_t magic;
    uint32_t version;
    uint32_t best_score;
    uint32_t clears;
    uint32_t plays;
    uint32_t checksum;
} hrp_save_data;

static uint32_t hrp_save_checksum(const hrp_save_data *d)
{
    const uint8_t *p = (const uint8_t *)d;
    uint32_t sum = 0x811c9dc5u;
    for (size_t i = 0; i < offsetof(hrp_save_data, checksum); i++) {
        sum ^= p[i];
        sum *= 16777619u;
    }
    return sum;
}

static void hrp_save_path(char *out, size_t size, const char *dir, const char *file)
{
    if (!out || size == 0) return;
    if (dir && *dir) snprintf(out, size, "%s/%s", dir, file);
    else snprintf(out, size, "%s", file);
}

static void hrp_sync_flush(FILE *f)
{
    if (!f) return;
    fflush(f);
#ifdef HRP_SAVE_HAVE_FSYNC
    fsync(fileno(f));
#endif
}

/* returns 1 on success */
static int hrp_save_write(const char *dir, const hrp_save_data *data)
{
    char path[512], tmp[512], old[512];
    hrp_save_path(old, sizeof(old), dir, "th01.sav.bak");
    hrp_save_path(path, sizeof(path), dir, "th01.sav");
    hrp_save_path(tmp, sizeof(tmp), dir, "th01.sav.tmp");

    hrp_save_data copy = *data;
    copy.magic = HRP_SAVE_MAGIC;
    copy.version = HRP_SAVE_VERSION;
    copy.checksum = hrp_save_checksum(&copy);

    FILE *f = fopen(tmp, "wb");
    if (!f) return 0;
    size_t wrote = fwrite(&copy, 1, sizeof(copy), f);
    hrp_sync_flush(f);
    fclose(f);
    if (wrote != sizeof(copy)) return 0;

    remove(old);
    rename(path, old); /* keep exactly one previous generation */
    if (rename(tmp, path) != 0) return 0;

#ifdef HRP_SAVE_HAVE_FSYNC
    FILE *d = fopen(dir ? dir : ".", "rb");
    if (d) { fsync(fileno(d)); fclose(d); }
#endif
    return 1;
}

/* returns 1 when a valid save was read; *data is untouched otherwise */
static int hrp_save_read(const char *dir, hrp_save_data *data)
{
    char path[512], old[512];
    hrp_save_path(path, sizeof(path), dir, "th01.sav");
    hrp_save_path(old, sizeof(old), dir, "th01.sav.bak");

    for (int attempt = 0; attempt < 2; attempt++) {
        const char *candidate = attempt == 0 ? path : old;
        FILE *f = fopen(candidate, "rb");
        if (!f) continue;
        hrp_save_data loaded;
        size_t got = fread(&loaded, 1, sizeof(loaded), f);
        fclose(f);
        if (got != sizeof(loaded)) continue;
        if (loaded.magic != HRP_SAVE_MAGIC || loaded.version != HRP_SAVE_VERSION) continue;
        if (loaded.checksum != hrp_save_checksum(&loaded)) continue;
        *data = loaded;
        return 1;
    }
    return 0;
}

#endif /* HRP_SAVE_H */
