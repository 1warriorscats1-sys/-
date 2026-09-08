/* th01_save.c - native tests for the crash-safe save helper.
 *
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2026 contributors to the 1warriorscats1-sys/- repository.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "hrp_save.h"

#include <sys/stat.h>
#include <sys/types.h>

static int failures = 0;

#define CHECK(cond, message)                                                   \
    do {                                                                       \
        if (!(cond)) {                                                         \
            printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, (message));        \
            failures++;                                                        \
        }                                                                      \
    } while (0)

static char g_dir[512];

static void path_of(char *out, size_t size, const char *name)
{
    snprintf(out, size, "%s/%s", g_dir, name);
}

static void write_bytes(const char *name, const void *data, size_t size)
{
    char path[600];
    path_of(path, sizeof(path), name);
    FILE *f = fopen(path, "wb");
    if (f) { fwrite(data, 1, size, f); fclose(f); }
}

int main(void)
{
    const char *base = getenv("TMPDIR") ? getenv("TMPDIR") : "/tmp";
    snprintf(g_dir, sizeof(g_dir), "%s/th01_save_test", base);
    mkdir(g_dir, 0777);
    /* start from a clean directory */
    char path[600];
    path_of(path, sizeof(path), "th01.sav");
    remove(path);
    path_of(path, sizeof(path), "th01.sav.bak");
    remove(path);
    path_of(path, sizeof(path), "th01.sav.tmp");
    remove(path);

    hrp_save_data data;
    memset(&data, 0, sizeof(data));
    data.best_score = 12345;
    data.clears = 2;
    data.plays = 7;

    CHECK(hrp_save_write(g_dir, &data) == 1, "writing a save must succeed");

    hrp_save_data loaded;
    memset(&loaded, 0, sizeof(loaded));
    CHECK(hrp_save_read(g_dir, &loaded) == 1, "reading back the save must succeed");
    CHECK(loaded.best_score == 12345, "best score must survive the round trip");
    CHECK(loaded.clears == 2 && loaded.plays == 7, "counters must survive the round trip");

    /* a second write keeps exactly one previous generation */
    data.best_score = 22222;
    CHECK(hrp_save_write(g_dir, &data) == 1, "the second write must succeed");
    memset(&loaded, 0, sizeof(loaded));
    CHECK(hrp_save_read(g_dir, &loaded) == 1, "the newer save must be readable");
    CHECK(loaded.best_score == 22222, "the newest save must win");

    hrp_save_data backup;
    memset(&backup, 0, sizeof(backup));
    path_of(path, sizeof(path), "th01.sav.bak");
    FILE *f = fopen(path, "rb");
    CHECK(f != NULL, "one previous generation must be retained");
    if (f) {
        size_t got = fread(&backup, 1, sizeof(backup), f);
        fclose(f);
        CHECK(got == sizeof(backup), "the retained generation must be complete");
        CHECK(backup.best_score == 12345, "the retained generation holds the previous save");
    }

    /* truncate the primary: the reader must fall back to the backup */
    write_bytes("th01.sav", "junk", 4);
    memset(&loaded, 0, sizeof(loaded));
    CHECK(hrp_save_read(g_dir, &loaded) == 1, "a corrupt primary must fall back to the backup");
    CHECK(loaded.best_score == 12345, "the fallback returns the previous generation");

    /* corrupt both: the reader must report failure, not garbage */
    write_bytes("th01.sav.bak", "junk", 4);
    memset(&loaded, 0, sizeof(loaded));
    CHECK(hrp_save_read(g_dir, &loaded) == 0, "corrupt saves must be reported as missing");
    CHECK(loaded.best_score == 0, "a failed read must not touch the caller's data");

    /* a valid header with a broken checksum is rejected */
    hrp_save_data broken = data;
    broken.checksum = 0xdeadbeefu;
    write_bytes("th01.sav", &broken, sizeof(broken));
    memset(&loaded, 0, sizeof(loaded));
    CHECK(hrp_save_read(g_dir, &loaded) == 0, "a bad checksum must be rejected");

    /* no temporary file is left behind by a successful write */
    memset(&data, 0, sizeof(data));
    data.best_score = 999;
    CHECK(hrp_save_write(g_dir, &data) == 1, "writing after recovery must succeed");
    path_of(path, sizeof(path), "th01.sav.tmp");
    FILE *tmp = fopen(path, "rb");
    CHECK(tmp == NULL, "the temporary file must be renamed away");
    if (tmp) fclose(tmp);

    if (failures) {
        printf("th01_save: %d failure(s)\n", failures);
        return 1;
    }
    printf("th01_save: all checks passed\n");
    return 0;
}
