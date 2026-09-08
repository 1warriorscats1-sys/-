"""Host tests for the thWWW (Wonderful Waking World) port scaffolding.

No game data or artwork is used: the icon fixture is synthetic and the overlay
is applied to a minimal stub of the pinned engine's source tree.
"""
import importlib.util
from pathlib import Path
import struct
import sys
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / 'scripts'))

from apply_www_overlay import apply, TITLE, AUTHOR, VERSION  # noqa: E402
from verify_www_nro import verify  # noqa: E402

OVERLAY = ROOT / 'wonderful-waking-world/www'

UPSTREAM_MAPPING = '''static void mapLibnxToGml(GamepadSlot* slot, PadState* pad, u64 cur) {
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
'''

STUB_FILES = {
    'CMakeLists.txt': 'NAME "Butterscotch" AUTHOR "Butterscotch" VERSION "1.0.0"\n'
                      'nx_create_nro(butterscotch NACP Butterscotch.nacp)\n',
    'src/switch/switch_input.c': UPSTREAM_MAPPING,
    'src/switch/main.c': 'int main(void) { return 0; }\n',
    'src/switch/log.c': '    printf("%s%s%s", colourPrefix, buffer, ANSI_COLOUR_CODE_RESET);\n',
    'src/loop.c': '''#include "loop.h"
                Runner_step(runner);
                if (runner->pendingRoom == -1)
                    platformSwapBuffers();
''',
    'src/overlay_file_system.c': '''#include "overlay_file_system.h"
    char* saveFull = joinPath(ofs->savePath, normalized);
    if (pathExists(saveFull))
        return saveFull;

    FILE* f = fopen(fullPath, "wb");
    free(fullPath);
    if (f == nullptr) return false;

    size_t len = strlen(contents);
    size_t written = fwrite(contents, 1, len, f);
    fclose(f);
    return written == len;

    int result = remove(fullPath);
    free(fullPath);
    return result == 0;
''',
}


def make_stub(directory):
    source = Path(directory) / 'engine'
    for name, text in STUB_FILES.items():
        path = source / name
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_text(text)
    return source


class OverlayTests(unittest.TestCase):
    def test_overlay_applies_to_pinned_layout(self):
        with tempfile.TemporaryDirectory() as directory:
            source = make_stub(directory)
            apply(source)

            cmake = (source / 'CMakeLists.txt').read_text()
            self.assertIn(f'NAME "{TITLE}" AUTHOR "{AUTHOR}" VERSION "{VERSION}"', cmake)
            self.assertIn('thwww-icon.jpg', cmake)

            self.assertIn('www_switch_mapping.inc', (source / 'src/switch/switch_input.c').read_text())
            self.assertIn('www_should_present', (source / 'src/loop.c').read_text())

            fs = (source / 'src/overlay_file_system.c').read_text()
            self.assertIn('www_save_text', fs)
            self.assertIn('www_save_recover', fs)
            self.assertNotIn('fwrite(contents', fs)

            for name in ('www_save.h', 'www_frame_policy.h', 'www_switch_mapping.inc'):
                self.assertTrue((source / 'src' / name).is_file(), name)
            self.assertIn('sdmc:/switch/thwww', (source / 'src/switch/main.c').read_text())

    def test_overlay_refuses_drifted_source(self):
        with tempfile.TemporaryDirectory() as directory:
            source = make_stub(directory)
            (source / 'src/switch/switch_input.c').write_text('/* rewritten upstream */\n')
            with self.assertRaises(RuntimeError):
                apply(source)

    def test_overlay_is_not_reapplied(self):
        with tempfile.TemporaryDirectory() as directory:
            source = make_stub(directory)
            apply(source)
            with self.assertRaises(RuntimeError):
                apply(source)


class MappingTests(unittest.TestCase):
    def test_right_stick_is_fully_disabled(self):
        text = (OVERLAY / 'www_switch_mapping.inc').read_text()
        for bit in ('HidNpadButton_StickR', 'HidNpadButton_StickRLeft', 'HidNpadButton_StickRUp',
                    'HidNpadButton_StickRRight', 'HidNpadButton_StickRDown'):
            self.assertIn(bit, text)
        self.assertIn('slot->axisValue[2] = 0.0f;', text)
        self.assertIn('slot->axisValue[3] = 0.0f;', text)
        self.assertIn('slot->buttonDown[11] = false;', text)
        self.assertNotIn('padGetStickPos(pad, 1)', text)

    def test_shoot_and_bomb_use_touhou_layout(self):
        text = (OVERLAY / 'www_switch_mapping.inc').read_text()
        self.assertIn('if (cur & HidNpadButton_A) slot->buttonDown[0] = true;', text)
        self.assertIn('if (cur & HidNpadButton_B) slot->buttonDown[1] = true;', text)


class DistributionTests(unittest.TestCase):
    def test_no_game_data_is_committed(self):
        folder = ROOT / 'wonderful-waking-world'
        bad = [p for p in folder.rglob('*') if p.suffix.lower() in
               ('.win', '.dat', '.exe', '.dll', '.nro', '.nsp', '.ogg', '.wav')]
        self.assertEqual(bad, [])

    def test_install_notes_state_the_requirements(self):
        text = (ROOT / 'wonderful-waking-world/THWWW_INSTALL.txt').read_text()
        self.assertIn('sd:/switch/thwww/', text)
        self.assertIn('data.win', text)
        self.assertIn('AGPL-3.0', text)


@unittest.skipUnless(importlib.util.find_spec('PIL'), 'Pillow is required for Switch asset tests')
class IconTests(unittest.TestCase):
    def fixture(self, directory, *, title=TITLE, author=AUTHOR, version=VERSION):
        from create_www_icon import create_icon
        icon = Path(directory) / 'icon.jpg'
        create_icon(icon)
        jpeg = icon.read_bytes()
        nacp = bytearray(0x4000)
        nacp[:len(title.encode())] = title.encode()
        nacp[0x200:0x200 + len(author.encode())] = author.encode()
        nacp[0x3060:0x3060 + len(version.encode())] = version.encode()
        header = bytearray(0x80)
        header[0x10:0x14] = b'NRO0'
        struct.pack_into('<I', header, 0x18, len(header))
        assets = struct.pack('<4sIQQQQQQ', b'ASET', 0, 56, len(jpeg), 56 + len(jpeg), len(nacp), 0, 0)
        nro = Path(directory) / 'synthetic.nro'
        nro.write_bytes(header + assets + jpeg + nacp)
        return nro

    def test_placeholder_icon_is_valid_and_metadata_matches(self):
        with tempfile.TemporaryDirectory() as directory:
            report = verify(self.fixture(directory))
            self.assertEqual(report['title'], TITLE)
            self.assertEqual(report['author'], AUTHOR)
            self.assertEqual(report['version'], VERSION)

    def test_wrong_metadata_is_rejected(self):
        with tempfile.TemporaryDirectory() as directory:
            with self.assertRaises(RuntimeError):
                verify(self.fixture(directory, title='Butterscotch'))

    def test_custom_cover_is_padded_not_cropped(self):
        from PIL import Image
        from create_www_icon import create_icon
        with tempfile.TemporaryDirectory() as directory:
            cover = Path(directory) / 'cover.png'
            Image.new('RGB', (320, 180), (60, 180, 90)).save(cover)
            icon = create_icon(Path(directory) / 'out.jpg', cover)
            with Image.open(icon) as image:
                self.assertEqual(image.size, (256, 256))
                self.assertEqual(image.format, 'JPEG')


if __name__ == '__main__':
    unittest.main()
