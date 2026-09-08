"""Public tests for the experimental th01 Switch port.

These run on any machine with Python 3.10+ and the standard library: no game
data, no SDK and no console. Pillow-dependent checks skip themselves when
Pillow is not installed.
"""
from __future__ import annotations

import importlib.util
import io
import json
import struct
import subprocess
import sys
import tempfile
import unittest
import zipfile
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
PORT = ROOT / "highly-responsive-prayers"
SRC = PORT / "th01"
SCRIPTS = ROOT / "scripts"


def load_module(name: str, path: Path):
    spec = importlib.util.spec_from_file_location(name, path)
    assert spec and spec.loader
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


packer = load_module("pack_th01_data", SCRIPTS / "pack_th01_data.py")
preview = load_module("render_th01_preview", SCRIPTS / "render_th01_preview.py")

try:  # Pillow is only needed for the icon/NRO checks
    import PIL  # noqa: F401

    HAVE_PIL = True
except ImportError:  # pragma: no cover - depends on the environment
    HAVE_PIL = False


class PackageFormatTests(unittest.TestCase):
    def setUp(self) -> None:
        self.example = json.loads(json.dumps(packer.EXAMPLE))

    def test_roundtrip(self):
        data = packer.build_package(self.example)
        self.assertEqual(data[:8], b"TH01OPEN")
        version, sections = struct.unpack_from("<HH", data, 8)
        self.assertEqual(version, 1)
        self.assertEqual(sections, 3)  # stages, palette, text

        info = packer.parse_package(data)
        self.assertEqual(info["title"], "th01 open example set")
        self.assertEqual(len(info["stages"]), 3)
        card = info["stages"][0]
        self.assertEqual(card["type"], "card")
        self.assertEqual(card["rows"], 5)
        self.assertEqual(card["cols"], 12)
        self.assertEqual(card["orb_speed"], 20)
        self.assertEqual(len(card["cells"]), 60)
        expected_row = [packer.CELL_TO_KIND[char]
                        for char in self.example["stages"][0]["cells"][0]]
        self.assertEqual(card["cells"][:12], expected_row)
        self.assertIn(packer.CELL_TO_KIND["#"], card["cells"][:12])
        self.assertIn(packer.CELL_TO_KIND["T"], card["cells"])
        self.assertEqual(info["stages"][2]["type"], "boss")
        self.assertEqual(info["stages"][2]["boss_hp"], 900)
        self.assertEqual(info["stages"][2]["boss_pattern"], 0)
        self.assertEqual(info["palette"][0], 0xFF6E50DC)

    def test_sections_are_four_byte_aligned(self):
        data = packer.build_package(self.example)
        offset = 16
        for _ in range(3):
            kind, length = struct.unpack_from("<II", data, offset)
            offset += 8 + length
            offset += (-length) % 4
        self.assertEqual(offset, len(data))

    def test_stage_kinds_are_encoded(self):
        content = {"stages": [{"type": "card", "name": "X", "rows": 1, "cols": 4,
                               "cells": ["#TSP"]}]}
        info = packer.parse_package(packer.build_package(content))
        self.assertEqual(info["stages"][0]["cells"], [1, 2, 3, 4])

    def test_rejects_bad_input(self):
        with self.assertRaises(packer.PackError):
            packer.build_package({"stages": []})
        with self.assertRaises(packer.PackError):
            packer.build_package({"stages": [{"type": "card", "rows": 40, "cols": 40}]})
        with self.assertRaises(packer.PackError):
            packer.build_package({"stages": [{"type": "card", "rows": 1, "cols": 3,
                                              "cells": ["#?."]}]})
        with self.assertRaises(packer.PackError):
            packer.build_package({"stages": [{"type": "boss", "boss_hp": 0}]})
        with self.assertRaises(packer.PackError):
            packer.build_package({"stages": [{"type": "card", "rows": 1, "cols": 2,
                                              "cells": ["##"]}], "palette": ["nope"]})

    def test_reference_parser_rejects_truncated_packages(self):
        data = packer.build_package(self.example)
        with self.assertRaises(packer.PackError):
            packer.parse_package(data[:12])
        with self.assertRaises(packer.PackError):
            packer.parse_package(data[: len(data) - 20])
        with self.assertRaises(packer.PackError):
            packer.parse_package(b"NOPE1234" + data[8:])

    def test_emit_example_then_pack(self):
        with tempfile.TemporaryDirectory() as tmp:
            directory = Path(tmp)
            path = packer.emit_example(directory)
            self.assertTrue(path.exists())
            content = json.loads(path.read_text(encoding="utf-8"))
            out = directory / "th01open.dat"
            out.write_bytes(packer.build_package(content))
            info = packer.parse_package(out.read_bytes())
            self.assertEqual(len(info["stages"]), 3)


class SourceLayoutTests(unittest.TestCase):
    def test_engine_sources_exist(self):
        for name in ("hrp.h", "hrp_config.h", "hrp_math.h", "hrp_game.c", "hrp_pkg.c",
                     "hrp_render.c", "hrp_audio.c", "hrp_autoplay.c", "hrp_demo_data.c",
                     "hrp_save.h", "hrp_frame_policy.h", "hrp_switch_mapping.inc",
                     "hrp_font.h", "main_host.c", "main_headless.c", "main_switch.c"):
            self.assertTrue((SRC / name).exists(), f"missing {name}")

    def test_switch_entry_point_reads_the_sd_card_paths(self):
        source = (SRC / "main_switch.c").read_text(encoding="utf-8")
        self.assertIn("sdmc:/switch/th01", source)
        self.assertIn("th01open.dat", source)
        self.assertIn("appletMainLoop", source)
        self.assertNotIn("http://", source)
        self.assertNotIn("https://", source)
        # 3DS-era key names do not exist in libnx; the port must use HidNpadButton_*
        self.assertNotIn("KEY_A", source)
        self.assertNotIn("KEY_DLEFT", source)

    def test_no_game_data_is_committed(self):
        banned_suffixes = {".dat", ".bin", ".grp", ".png", ".jpg", ".wav", ".nro",
                           ".elf", ".nacp", ".zip", ".exe", ".hdi", ".d88"}
        offenders = []
        for path in PORT.rglob("*"):
            if path.is_file() and path.suffix.lower() in banned_suffixes:
                offenders.append(str(path.relative_to(ROOT)))
        self.assertEqual(offenders, [], f"binary/game data must not be committed: {offenders}")

    def test_mapping_leaves_the_right_stick_unused(self):
        mapping = (SRC / "hrp_switch_mapping.inc").read_text(encoding="utf-8")
        self.assertIn("HRP_SWITCH_RIGHT_STICK_ENABLED 0", mapping)
        self.assertIn("HidNpadButton_Left", mapping)
        self.assertIn("HidNpadButton_A", mapping)
        self.assertIn("HidNpadButton_Plus", mapping)
        # the 3DS KEY_* names do not exist in libnx and must not creep back in
        self.assertNotIn("KEY_A", mapping.replace("KEY_*", ""))
        self.assertNotIn("KEY_DLEFT", mapping.replace("KEY_*", ""))

    def test_documentation_exists(self):
        for name in ("README.md", "TH01_INSTALL.txt"):
            self.assertTrue((PORT / name).exists(), f"missing {name}")
        for name in ("TH01_DATA_FORMAT.md", "TH01_ORIGINAL_DATA.md"):
            self.assertTrue((ROOT / "docs" / name).exists(), f"missing docs/{name}")

    def test_workflow_builds_and_verifies(self):
        workflow = ROOT / ".github" / "workflows" / "th01-switch.yml"
        self.assertTrue(workflow.exists())
        text = workflow.read_text(encoding="utf-8")
        self.assertIn("devkitpro/devkita64", text)
        self.assertIn("scripts/build_th01.py --target switch", text)
        self.assertIn("scripts/test_th01_native.py", text)


class PreviewCodecTests(unittest.TestCase):
    def test_lzw_roundtrip(self):
        for size in (1, 2, 17, 300, 1500, 5000):
            indices = [(i * 37 + 11) % 256 for i in range(size)]
            encoded = preview.lzw_encode(indices, 8)
            self.assertEqual(preview.lzw_decode(encoded, 8), indices)

    def test_png_roundtrip_header(self):
        with tempfile.TemporaryDirectory() as tmp:
            path = Path(tmp) / "sheet.png"
            pixels = bytes([(x * 7) % 256 for x in range(4 * 4 * 3)])
            preview.write_png(path, 4, 4, pixels, scale=2)
            data = path.read_bytes()
            self.assertEqual(data[:8], b"\x89PNG\r\n\x1a\n")
            width, height = struct.unpack(">II", data[16:24])
            self.assertEqual((width, height), (8, 8))


@unittest.skipUnless(HAVE_PIL, "Pillow is not installed")
class IconTests(unittest.TestCase):
    def test_procedural_icon_is_a_256_rgb_jpeg(self):
        from PIL import Image

        icon = load_module("create_th01_icon", SCRIPTS / "create_th01_icon.py")
        with tempfile.TemporaryDirectory() as tmp:
            path = Path(tmp) / "icon.jpg"
            info = icon.create_icon(path)
            self.assertEqual(info["size"], (256, 256))
            with Image.open(path) as image:
                image.load()
                self.assertEqual(image.format, "JPEG")
                self.assertEqual(image.size, (256, 256))
                self.assertEqual(image.mode, "RGB")

    def test_nro_verifier_accepts_a_synthetic_nro(self):
        from PIL import Image

        icon_module = load_module("create_th01_icon", SCRIPTS / "create_th01_icon.py")
        verifier = load_module("verify_th01_nro", SCRIPTS / "verify_th01_nro.py")

        with tempfile.TemporaryDirectory() as tmp:
            tmp_path = Path(tmp)
            icon_path = tmp_path / "icon.jpg"
            icon_module.create_icon(icon_path)
            icon = icon_path.read_bytes()

            nacp = bytearray(0x4000)
            nacp[0:len("TH01 open port")] = b"TH01 open port"
            nacp[0x200:0x200 + len(verifier.EXPECTED_AUTHOR)] = verifier.EXPECTED_AUTHOR.encode()
            nacp[0x3060:0x3060 + 5] = b"01.01"

            nro = bytearray(b"\0" * 0x10 + b"NRO0")
            nro += struct.pack("<III", 0, 0, 0)  # version, size, flags placeholder
            body_offset = len(nro)
            # size field must point just past the header/segment table
            header_size = 0x70
            nro = bytearray(b"\0" * 0x10 + b"NRO0" + struct.pack("<III", 0, header_size, 0))
            nro += b"\0" * (header_size - len(nro))
            nro += b"ASET"
            nro += struct.pack("<I", 0)
            nro += struct.pack("<QQ", 56, len(icon))
            nro += struct.pack("<QQ", 56 + len(icon), 0x4000)
            nro += struct.pack("<QQ", 0, 0)
            nro += icon
            nro += bytes(nacp)
            (tmp_path / "th01.nro").write_bytes(bytes(nro))

            info = verifier.verify(tmp_path / "th01.nro")
            self.assertEqual(info["title"], "TH01 open port")
            self.assertEqual(info["version"], "01.01")

            # a wrong NACP title must be rejected
            nacp_start = header_size + 56 + len(icon)
            nro[nacp_start:nacp_start + 4] = b"XXXX"
            (tmp_path / "bad.nro").write_bytes(bytes(nro))
            with self.assertRaises(RuntimeError):
                verifier.verify(tmp_path / "bad.nro")

    def test_nro_verifier_rejects_non_nro(self):
        verifier = load_module("verify_th01_nro", SCRIPTS / "verify_th01_nro.py")
        with tempfile.TemporaryDirectory() as tmp:
            path = Path(tmp) / "not.nro"
            path.write_bytes(b"not an nro at all" * 8)
            with self.assertRaises(RuntimeError):
                verifier.verify(path)


class ProbeTests(unittest.TestCase):
    def test_probe_reports_without_converting(self):
        probe = load_module("probe_th01_original", SCRIPTS / "probe_th01_original.py")
        with tempfile.TemporaryDirectory() as tmp:
            directory = Path(tmp)
            (directory / "random.bin").write_bytes(bytes(range(256)) * 8)
            (directory / "text.txt").write_bytes(b"HELLO WORLD THIS IS PLAIN TEXT\n" * 4)

            report = {entry["name"]: entry for entry in
                      [probe.probe_file(path) for path in sorted(directory.iterdir())]}

            self.assertEqual(report["text.txt"]["printable_ratio"], 1.0)
            self.assertIn("plain text", report["text.txt"]["guess"])
            self.assertGreater(report["random.bin"]["entropy_bits"], 7.0)
            self.assertIn("sha256", report["random.bin"])

    def test_probe_refuses_a_missing_path(self):
        probe = load_module("probe_th01_original", SCRIPTS / "probe_th01_original.py")
        with tempfile.TemporaryDirectory() as tmp:
            result = subprocess.run(
                [sys.executable, str(SCRIPTS / "probe_th01_original.py"),
                 str(Path(tmp) / "nope")],
                capture_output=True, text=True, cwd=str(ROOT))
            self.assertEqual(result.returncode, 2)


class BuildScriptTests(unittest.TestCase):
    def test_build_script_targets(self):
        text = (SCRIPTS / "build_th01.py").read_text(encoding="utf-8")
        for target in ("native", "headless", "host", "switch"):
            self.assertIn(target, text)
        self.assertIn("elf2nro", text)
        self.assertIn("nacptool", text)

    def test_pack_and_verify_cli(self):
        with tempfile.TemporaryDirectory() as tmp:
            directory = Path(tmp) / "content"
            packer.emit_example(directory)
            package = Path(tmp) / "th01open.dat"
            result = subprocess.run(
                [sys.executable, str(SCRIPTS / "pack_th01_data.py"),
                 "--source", str(directory), "--output", str(package)],
                capture_output=True, text=True, cwd=str(ROOT))
            self.assertEqual(result.returncode, 0, result.stderr)
            self.assertTrue(package.exists())

            verify = subprocess.run(
                [sys.executable, str(SCRIPTS / "pack_th01_data.py"), "--verify", str(package)],
                capture_output=True, text=True, cwd=str(ROOT))
            self.assertEqual(verify.returncode, 0, verify.stderr)
            self.assertIn("3 stages", verify.stdout)


if __name__ == "__main__":
    unittest.main()
