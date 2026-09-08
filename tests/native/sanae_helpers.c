#define _POSIX_C_SOURCE 200809L
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include <math.h>
#include <errno.h>
static int allocation, fail_allocation, renames, fail_rename, fail_sync, fail_close;
static void *test_malloc(size_t n) {
    if (++allocation == fail_allocation) return NULL;
    return malloc(n);
}
static int test_rename(const char *from, const char *to) {
    if (++renames == fail_rename) { errno = EIO; return -1; }
    return rename(from, to);
}
static int test_sync(int fd) { if (fail_sync) { errno = EIO; return -1; } return fsync(fd); }
static int test_close(FILE *f) { int r = fclose(f); return fail_close ? -1 : r; }
#define malloc test_malloc
#define rename test_rename
#define fsync test_sync
#define fclose test_close
#include "sanae_save.h"
#include "sanae_csv.h"
#include "sanae_audio_gain.h"
#undef malloc
#undef rename
#undef fsync
#undef fclose

static void contents(const char *path, const char *expected) {
    char text[512]; FILE *f = fopen(path, "rb"); size_t n;
    assert(f); n = fread(text, 1, sizeof(text)-1, f); assert(!ferror(f)); fclose(f);
    text[n] = 0; assert(strcmp(text, expected) == 0);
}
static void saves(void) {
    int i;
    assert(sanae_save_text("progress.ini", "first"));
    assert(sanae_save_text("progress.ini", "second"));
    contents("progress.ini", "second"); contents("progress.ini.bak", "first");
    assert(rename("progress.ini", "progress.ini.lost") == 0);
    sanae_save_recover("progress.ini"); contents("progress.ini", "first");
    assert(sanae_save_text("progress.ini", "stable"));
    for (i = 1; i <= 2; ++i) {
        allocation = 0; fail_allocation = i;
        assert(!sanae_save_text("progress.ini", "bad allocation"));
        fail_allocation = 0; contents("progress.ini", "stable");
    }
    fail_sync = 1; assert(!sanae_save_text("progress.ini", "bad flush")); fail_sync = 0;
    contents("progress.ini", "stable");
    fail_close = 1; assert(!sanae_save_text("progress.ini", "bad close")); fail_close = 0;
    contents("progress.ini", "stable");
    for (i = 1; i <= 2; ++i) {
        renames = 0; fail_rename = i;
        assert(!sanae_save_text("progress.ini", "bad rename"));
        fail_rename = 0; contents("progress.ini", "stable");
        assert(!sanae_save_exists("progress.ini.tmp"));
    }
    assert(!sanae_save_text("missing/parent/progress.ini", "bad path"));
    assert(sanae_save_text("progress.ini", "")); contents("progress.ini", "");
    puts("save: replace, backup recovery, allocation/flush/close/rename failures passed");
}
static void csvs(void) {
    SanaeCsv csv;
    char *large;
    size_t i;
    assert(sanae_csv_parse("\xef\xbb\xbf" "001,\"a,b\",\"a\"\"b\"\r\n\"two\nlines\",\r\nend", &csv));
    assert(csv.rows == 3 && csv.columns == 3 && csv.count == 6);
    assert(strcmp(csv.cells[0].text, "001") == 0);
    assert(strcmp(csv.cells[1].text, "a,b") == 0);
    assert(strcmp(csv.cells[2].text, "a\"b") == 0);
    assert(strcmp(csv.cells[3].text, "two\nlines") == 0);
    assert(strcmp(csv.cells[4].text, "") == 0);
    assert(csv.cells[5].row == 2 && csv.cells[5].column == 0);
    sanae_csv_free(&csv);
    assert(sanae_csv_parse("a,", &csv) && csv.count == 2); sanae_csv_free(&csv);
    assert(!sanae_csv_parse(NULL, &csv)); assert(!sanae_csv_parse("", &csv));
    assert(!sanae_csv_parse("\"unclosed", &csv)); assert(!sanae_csv_parse("\"a\"x", &csv));
    assert(!sanae_csv_parse("a\"b", &csv)); assert(!sanae_csv_parse("\xef\xbb\xbf", &csv));
    large = malloc(10000); assert(large);
    memset(large, ',', 4096); large[4096] = 0;
    assert(!sanae_csv_parse(large, &csv));
    memset(large, ',', 4095); memset(large+4095, '\n', 5000); large[9095] = 0;
    assert(!sanae_csv_parse(large, &csv)); free(large);
    for (i = 1; i <= 3; ++i) {
        allocation = 0; fail_allocation = (int)i;
        assert(!sanae_csv_parse("a,b,c", &csv)); fail_allocation = 0;
        sanae_csv_free(&csv);
    }
    puts("csv: BOM, quoting, multiline, ragged/trailing cells, bounds and allocation failures passed");
}
static void gains(void) {
    SanaeGain gain;
    sanae_gain_init(&gain); assert(gain.current == 1);
    sanae_gain_set(&gain, 0, 1000); sanae_gain_update(&gain, .25f);
    assert(fabsf(gain.current - .75f) < 1e-6f);
    sanae_gain_set(&gain, 1, 500); sanae_gain_update(&gain, .25f);
    assert(fabsf(gain.current - .875f) < 1e-6f);
    sanae_gain_update(&gain, 10); assert(gain.current == 1 && gain.remaining == 0);
    sanae_gain_set(&gain, -1, 0); assert(gain.current == 0);
    sanae_gain_update(&gain, -1); assert(gain.current == 0);
    sanae_gain_set(&gain, .5f, 0); assert(gain.current == .5f);
    puts("audio envelope: immediate gain, interrupted fade, completion and clamping passed");
}
int main(void) { saves(); csvs(); gains(); return 0; }
