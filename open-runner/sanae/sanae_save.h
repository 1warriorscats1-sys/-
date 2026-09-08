/* Original SANAE compatibility code. MIT; see the toolkit LICENSE.md.
 * A checked, flushed replacement with a retained previous generation.
 * Not a guarantee of atomic metadata operations under SD power loss. */
#ifndef SANAE_SAVE_H
#define SANAE_SAVE_H
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>

static int sanae_save_exists(const char *path) {
    struct stat st;
    return stat(path, &st) == 0 && S_ISREG(st.st_mode);
}

/* If replacement was interrupted after the old file moved, restore it. */
static void sanae_save_recover(const char *path) {
    size_t n;
    char *backup;
    if (sanae_save_exists(path)) return;
    n = strlen(path); backup = (char *)malloc(n + 5);
    if (!backup) return;
    memcpy(backup, path, n); memcpy(backup+n, ".bak", 5);
    if (sanae_save_exists(backup)) (void)rename(backup, path);
    free(backup);
}

static int sanae_save_text(const char *path, const char *text) {
    size_t n = strlen(path), length = strlen(text);
    char *temp = (char *)malloc(n + 5), *backup = (char *)malloc(n + 5);
    FILE *f = NULL;
    int ok = 0, moved = 0, written, temp_owned = 0;
    if (!temp || !backup) goto done;
    memcpy(temp, path, n); memcpy(temp+n, ".tmp", 5);
    memcpy(backup, path, n); memcpy(backup+n, ".bak", 5);
    sanae_save_recover(path);
    f = fopen(temp, "wb");
    if (!f) goto done;
    temp_owned = 1;
    written = fwrite(text, 1, length, f) == length;
    if (fflush(f) != 0) written = 0;
    if (fsync(fileno(f)) != 0) written = 0;
    if (fclose(f) != 0) written = 0;
    f = NULL;
    if (!written) goto done;
    if (sanae_save_exists(path)) {
        if (sanae_save_exists(backup) && remove(backup) != 0) goto done;
        if (rename(path, backup) != 0) goto done;
        moved = 1;
    }
    if (rename(temp, path) != 0) {
        if (moved) (void)rename(backup, path);
        goto done;
    }
    ok = 1;
done:
    if (f) fclose(f);
    if (!ok && temp_owned) (void)remove(temp);
    free(temp); free(backup); return ok;
}
#endif
