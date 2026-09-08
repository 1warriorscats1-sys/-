#!/usr/bin/env python3
"""Apply the thWWW (Wonderful Waking World) integration to a disposable copy
of the pinned upstream Butterscotch sources.

Fails loudly on source drift. Never run this against the toolkit itself or
against your clean upstream checkout.

Original integration script: MIT; the resulting linked runner: AGPL-3.0.
No game data, artwork or proprietary runtime is touched by this script.
"""
from pathlib import Path
import shutil

ROOT = Path(__file__).resolve().parents[1]
OVERLAY = ROOT / 'wonderful-waking-world/www'

TITLE = 'Touhou Nemuri Sekai - Wonderful Waking World'
AUTHOR = 'Oligarchomp'
VERSION = '01.00'


def apply(source: Path):
    """Patch `source` in place. Raises RuntimeError on any upstream drift."""

    def replace(name, before, after):
        p = source / name
        text = p.read_text()
        if text.count(before) != 1:
            raise RuntimeError(f'Upstream drift / already patched: {name}: {before[:90]!r}')
        p.write_text(text.replace(before, after, 1))

    for p in OVERLAY.iterdir():
        if p.suffix in ('.h', '.inc'):
            shutil.copy2(p, source / 'src' / p.name)
    shutil.copy2(OVERLAY / 'main.c', source / 'src/switch/main.c')

    # --- Controller mapping -------------------------------------------------
    # Replace the whole upstream mapping function with the danmaku layout.
    replace('src/switch/switch_input.c', '''static void mapLibnxToGml(GamepadSlot* slot, PadState* pad, u64 cur) {
    if (cur & HidNpadButton_A) slot->buttonDown[0] = true;
    if (cur & HidNpadButton_B) slot->buttonDown[1] = true;
    if (cur & HidNpadButton_Y) slot->buttonDown[2] = true;
    if (cur & HidNpadButton_X) slot->buttonDown[3] = true;
    if (cur & HidNpadButton_L) slot->buttonDown[4] = true;
    if (cur & HidNpadButton_R) slot->buttonDown[5] = true;
    slot->buttonValue[6] = (cur & HidNpadButton_ZL) ? 1.0f : 0.0f;
    slot->buttonValue[7] = (cur & HidNpadButton_ZR) ? 1.0f : 0.0f;
    if (cur & HidNpadButton_Minus) slot->buttonDown[8] = true;
    if (cur & HidNpadButton_Plus) slot->buttonDown[9] = true;
    if (cur & HidNpadButton_StickL) slot->buttonDown[10] = true;
    if (cur & HidNpadButton_StickR) slot->buttonDown[11] = true;
    if (cur & HidNpadButton_AnyUp) slot->buttonDown[12] = true;
    if (cur & HidNpadButton_AnyDown) slot->buttonDown[13] = true;
    if (cur & HidNpadButton_AnyLeft) slot->buttonDown[14] = true;
    if (cur & HidNpadButton_AnyRight) slot->buttonDown[15] = true;

    HidAnalogStickState l = padGetStickPos(pad, 0);
    HidAnalogStickState r = padGetStickPos(pad, 1);
    slot->axisValue[0] = l.x / 32767.0f;
    slot->axisValue[1] = -l.y / 32767.0f;
    slot->axisValue[2] = r.x / 32767.0f;
    slot->axisValue[3] = -r.y / 32767.0f;
}
''', '#include "www_switch_mapping.inc"\n')

    # --- Application identity ----------------------------------------------
    replace('CMakeLists.txt', 'NAME "Butterscotch" AUTHOR "Butterscotch" VERSION "1.0.0"',
            f'NAME "{TITLE}" AUTHOR "{AUTHOR}" VERSION "{VERSION}"')
    replace('CMakeLists.txt', 'nx_create_nro(butterscotch NACP Butterscotch.nacp)',
            'nx_create_nro(butterscotch NACP Butterscotch.nacp '
            'ICON "${CMAKE_CURRENT_SOURCE_DIR}/thwww-icon.jpg")')

    # --- Checked saves ------------------------------------------------------
    # thWWW keeps scores/settings in its save folder; an interrupted write must
    # not lose the previous generation.
    replace('src/overlay_file_system.c', '#include "overlay_file_system.h"',
            '#include "overlay_file_system.h"\n#include "www_save.h"')
    replace('src/overlay_file_system.c', '''    char* saveFull = joinPath(ofs->savePath, normalized);
    if (pathExists(saveFull))''', '''    char* saveFull = joinPath(ofs->savePath, normalized);
    www_save_recover(saveFull);
    if (pathExists(saveFull))''')
    replace('src/overlay_file_system.c', '''    FILE* f = fopen(fullPath, "wb");
    free(fullPath);
    if (f == nullptr) return false;

    size_t len = strlen(contents);
    size_t written = fwrite(contents, 1, len, f);
    fclose(f);
    return written == len;''', '''    bool ok = www_save_text(fullPath, contents) != 0;
    if (!ok) logError("thWWW: save failed: %s\\n", fullPath);
    free(fullPath);
    return ok;''')
    replace('src/overlay_file_system.c', '''    int result = remove(fullPath);
    free(fullPath);
    return result == 0;''', '''    // A deliberate delete must not resurrect the previous generation on read.
    size_t n = strlen(fullPath);
    char *backup = (char *)safeMalloc(n + 5);
    memcpy(backup, fullPath, n); memcpy(backup + n, ".bak", 5);
    if (www_save_exists(backup) && remove(backup) != 0) {
        free(backup); free(fullPath); return false;
    }
    free(backup);
    int result = remove(fullPath);
    free(fullPath);
    return result == 0;''')

    # --- Clean exit ---------------------------------------------------------
    # game_end during Step must not present a half-drawn frame before quitting.
    replace('src/loop.c', '#include "loop.h"', '#include "loop.h"\n#include "www_frame_policy.h"')
    replace('src/loop.c', '                if (runner->pendingRoom == -1)\n                    platformSwapBuffers();',
            '                if (www_should_present(runner->shouldExit, runner->pendingRoom))\n'
            '                    platformSwapBuffers();')

    # --- Diagnostics --------------------------------------------------------
    # The entry point redirects stderr to thwww.log; upstream logged to stdout.
    replace('src/switch/log.c', '    printf("%s%s%s", colourPrefix, buffer, ANSI_COLOUR_CODE_RESET);',
            '    (void)colourPrefix;\n    fputs(buffer, stderr);\n    fflush(stderr);')


if __name__ == '__main__':
    import argparse
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('source', type=Path, help='Disposable copy of the pinned engine source')
    apply(parser.parse_args().source)
    print('Applied thWWW integration.')
