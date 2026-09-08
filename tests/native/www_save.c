/* Functional tests for the thWWW save replacement and frame policy.
 * Original integration test code: MIT. No game data is used. */
#include "www_save.h"
#include "www_frame_policy.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

static char DIR[] = "/tmp/thwww-save-testXXXXXX";
static char PATH[128], BAK[160], TMP[160];

static void paths(void) {
    snprintf(PATH, sizeof PATH, "%s/save.ini", DIR);
    snprintf(BAK, sizeof BAK, "%s.bak", PATH);
    snprintf(TMP, sizeof TMP, "%s.tmp", PATH);
}

static char *slurp(const char *path) {
    static char buffer[512];
    FILE *f = fopen(path, "rb");
    size_t n;
    if (!f) return NULL;
    n = fread(buffer, 1, sizeof buffer - 1, f);
    fclose(f);
    buffer[n] = '\0';
    return buffer;
}

static void test_writes_and_reads_back(void) {
    assert(www_save_text(PATH, "score=100"));
    assert(strcmp(slurp(PATH), "score=100") == 0);
    /* First write has no previous generation to retain. */
    assert(!www_save_exists(BAK));
    /* No temporary file may be left behind. */
    assert(!www_save_exists(TMP));
}

static void test_rewrite_retains_previous_generation(void) {
    assert(www_save_text(PATH, "score=200"));
    assert(strcmp(slurp(PATH), "score=200") == 0);
    assert(www_save_exists(BAK));
    assert(strcmp(slurp(BAK), "score=100") == 0);
    assert(!www_save_exists(TMP));

    /* A third write rotates again rather than accumulating generations. */
    assert(www_save_text(PATH, "score=300"));
    assert(strcmp(slurp(PATH), "score=300") == 0);
    assert(strcmp(slurp(BAK), "score=200") == 0);
}

static void test_recovery_after_interrupted_replace(void) {
    /* Simulate a crash between rename(path, backup) and rename(temp, path). */
    assert(rename(PATH, BAK) == 0);
    assert(!www_save_exists(PATH));
    www_save_recover(PATH);
    assert(www_save_exists(PATH));
    assert(strcmp(slurp(PATH), "score=300") == 0);
}

static void test_recover_never_clobbers_a_live_save(void) {
    assert(www_save_text(PATH, "current"));
    /* A stale backup must not overwrite the file that is actually present. */
    FILE *f = fopen(BAK, "wb");
    assert(f && fputs("stale", f) >= 0 && fclose(f) == 0);
    www_save_recover(PATH);
    assert(strcmp(slurp(PATH), "current") == 0);
}

static void test_empty_and_large_payloads(void) {
    char big[4096];
    memset(big, 'x', sizeof big - 1);
    big[sizeof big - 1] = '\0';
    assert(www_save_text(PATH, ""));
    assert(strcmp(slurp(PATH), "") == 0);
    assert(www_save_text(PATH, big));
    assert(strlen(slurp(PATH)) == 511); /* slurp truncates; the write succeeded */
}

static void test_unwritable_path_reports_failure(void) {
    char bad[192];
    snprintf(bad, sizeof bad, "%s/missing-dir/save.ini", DIR);
    assert(!www_save_text(bad, "data"));
}

static void test_frame_policy(void) {
    /* Present only when not exiting and no room change is pending. */
    assert(www_should_present(0, -1));
    assert(!www_should_present(1, -1));
    assert(!www_should_present(0, 4));
    assert(!www_should_present(1, 4));
}

int main(void) {
    assert(mkdtemp(DIR) != NULL);
    paths();
    test_writes_and_reads_back();
    test_rewrite_retains_previous_generation();
    test_recovery_after_interrupted_replace();
    test_recover_never_clobbers_a_live_save();
    test_empty_and_large_payloads();
    test_unwritable_path_reports_failure();
    test_frame_policy();
    remove(PATH); remove(BAK); remove(TMP); rmdir(DIR);
    printf("thWWW save/frame-policy tests passed\n");
    return 0;
}
