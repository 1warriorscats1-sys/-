/* Wonderful Waking World (thWWW) libnx entry point.
 * Original integration code (MIT). Links with Butterscotch under AGPL-3.0;
 * ship the corresponding source with any binary.
 *
 * No game data, artwork, audio or proprietary runtime is contained here.
 * The player supplies the original files from their own copy of the game. */
#include <loop.h>
#include <switch.h>
#include <sys/stat.h>
#include <stdio.h>
#include <errno.h>
#include <unistd.h>
#include <SDL2/SDL_main.h>

#define GAME_DIR "sdmc:/switch/thwww"
#define SAVE_DIR GAME_DIR "/saves"

static int message(const char *text) {
    PadState pad;
    consoleInit(NULL);
    padConfigureInput(1, HidNpadStyleSet_NpadStandard);
    padInitializeDefault(&pad);
    printf("Wonderful Waking World - experimental open NRO\n\n%s\n\nPress + to return.\n", text);
    while (appletMainLoop()) {
        padUpdate(&pad);
        if (padGetButtonsDown(&pad) & HidNpadButton_Plus) break;
        consoleUpdate(NULL);
    }
    consoleExit(NULL);
    return 1;
}

/* Optional archives are reported, not required: the amount of audiogroup files
 * differs between the itch.io and Steam builds, so nothing is hardcoded. */
static void report_optional_groups(void) {
    char path[256];
    int i, found = 0;
    for (i = 0; i < 32; ++i) {
        snprintf(path, sizeof(path), GAME_DIR "/audiogroup%d.dat", i);
        if (access(path, R_OK) == 0) ++found;
    }
    fprintf(stderr, "thWWW: %d audiogroup archive(s) present in %s\n", found, GAME_DIR);
}

int main(int argc, char **argv) {
    int result;
    char error[512];
    CommandLineArgs args = {0};
    (void)argc;
    setbuf(stdout, NULL);
    setbuf(stderr, NULL);
    fsdevMountSdmc();
    if (access(GAME_DIR "/data.win", R_OK) != 0) {
        snprintf(error, sizeof(error),
                 "Missing file:\n%s/data.win\n\nCopy the ORIGINAL files from your own copy\n"
                 "of Touhou Nemuri Sekai ~ Wonderful Waking World.\nNo PC conversion is needed.",
                 GAME_DIR);
        return message(error);
    }
    if (mkdir(SAVE_DIR, 0777) != 0 && errno != EEXIST)
        return message("Cannot create " SAVE_DIR ". Check SD write access.");
    /* Keep diagnostics inside this port's own directory, never another game's. */
    (void)freopen(GAME_DIR "/thwww.log", "w", stderr);
    report_optional_groups();
    args.exitAtFrame = -1;
    args.speedMultiplier = 1.0;
    args.osType = OS_WINDOWS;
    args.loadType = DATAWINLOADTYPE_LOAD_IN_MEMORY_AHEAD_OF_TIME;
    args.renderer = MODERN_GL;
    args.dataWinPath = GAME_DIR "/data.win";
    args.saveFolder = SAVE_DIR;
    result = loop(args, argv[0]);
    freeCommandLineArgs(&args);
    fflush(NULL);
    if (result != 0)
        return message("Wonderful Waking World stopped with an error.\n"
                       "A save may not have completed.\nSee thwww.log; keep the .bak files.");
    return result;
}
