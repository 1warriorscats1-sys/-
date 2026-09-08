/* SANAE-specific libnx entry point. Original integration code (MIT).
 * Links with Butterscotch under AGPL-3.0; ship corresponding source. */
#include <loop.h>
#include <switch.h>
#include <sys/stat.h>
#include <stdio.h>
#include <errno.h>
#include <unistd.h>
#include <SDL2/SDL_main.h>

#define GAME_DIR "sdmc:/switch/sanae"
#define SAVE_DIR GAME_DIR "/saves"

static int message(const char *text) {
    PadState pad;
    consoleInit(NULL);
    padConfigureInput(1, HidNpadStyleSet_NpadStandard);
    padInitializeDefault(&pad);
    printf("SANAE's Sylphid Breeze - open NRO\n\n%s\n\nPress + to return.\n", text);
    while (appletMainLoop()) {
        padUpdate(&pad);
        if (padGetButtonsDown(&pad) & HidNpadButton_Plus) break;
        consoleUpdate(NULL);
    }
    consoleExit(NULL); return 1;
}

int main(int argc, char **argv) {
    int i, result;
    char path[256], error[512];
    const char *required[] = {"data.win", "scenario_sanae.csv", "scenario_sanae_en.csv"};
    CommandLineArgs args = {0};
    (void)argc;
    setbuf(stdout, NULL); setbuf(stderr, NULL);
    fsdevMountSdmc();
    for (i = 0; i < 3; ++i) {
        snprintf(path, sizeof(path), "%s/%s", GAME_DIR, required[i]);
        if (access(path, R_OK) != 0) {
            snprintf(error, sizeof(error), "Missing file:\n%s\n\nCopy ORIGINAL files from your own Steam game.\nNo data.win conversion is needed.", path);
            return message(error);
        }
    }
    for (i = 2; i <= 17; ++i) {
        snprintf(path, sizeof(path), GAME_DIR "/audiogroup%d.dat", i);
        if (access(path, R_OK) != 0) {
            snprintf(error, sizeof(error), "Missing audio archive:\n%s\n\nCopy your original Steam audiogroup files.", path);
            return message(error);
        }
    }
    if (mkdir(SAVE_DIR, 0777) != 0 && errno != EEXIST)
        return message("Cannot create " SAVE_DIR ". Check SD write access.");
    /* Keep diagnostics alongside this port, never in another game's directory. */
    (void)freopen(GAME_DIR "/sanae.log", "w", stderr);
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
    if (result != 0) return message("SANAE stopped with an error.\nA save may not have completed.\nSee sanae.log; keep the .bak files.");
    /* Returning through libnx exits to hbmenu; no C-runtime machine patch. */
    return result;
}
