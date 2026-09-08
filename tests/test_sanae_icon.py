"""Cover conversion and actual NRO asset-table validation (synthetic art only)."""
import importlib.util
from pathlib import Path
import struct
import sys
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT/'scripts'))
from create_sanae_icon import create_icon
from verify_sanae_nro import verify


@unittest.skipUnless(importlib.util.find_spec('PIL'), 'Pillow is required for Switch asset tests')
class SanaeIconTests(unittest.TestCase):
    def fixture(self, directory):
        icon = Path(directory)/'icon.jpg'
        from PIL import Image
        source = Path(directory)/"synthetic-cover.png"
        Image.new("RGB", (320, 180), (60, 180, 90)).save(source)
        create_icon(icon, source)
        jpeg = icon.read_bytes()
        nacp = bytearray(0x4000)
        title = b'SANAE - Sylphid Breeze'; author = b'sorehodoh'
        nacp[:len(title)] = title
        nacp[0x200:0x200+len(author)] = author
        nacp[0x3060:0x3065] = b'01.01'
        header = bytearray(0x80)
        header[0x10:0x14] = b'NRO0'; struct.pack_into('<I', header, 0x18, len(header))
        assets = struct.pack('<4sIQQQQQQ', b'ASET', 0, 56, len(jpeg), 56+len(jpeg), len(nacp), 0, 0)
        nro = Path(directory)/'synthetic.nro'
        nro.write_bytes(header+assets+jpeg+nacp)
        return nro

    def test_cover_icon_and_metadata(self):
        with tempfile.TemporaryDirectory() as directory:
            report = verify(self.fixture(directory))
            self.assertEqual(report['version'], '01.01')
            self.assertEqual(report['author'], 'sorehodoh')
            self.assertGreater(report['icon_bytes'], 100)

    def test_reject_missing_assets(self):
        with tempfile.TemporaryDirectory() as directory:
            nro = self.fixture(directory)
            nro.write_bytes(nro.read_bytes()[:0x80])
            with self.assertRaises(RuntimeError): verify(nro)

    def test_reject_truncated_assets(self):
        with tempfile.TemporaryDirectory() as directory:
            nro = self.fixture(directory)
            nro.write_bytes(nro.read_bytes()[:-1])
            with self.assertRaises(RuntimeError): verify(nro)

    def test_reject_wrong_version(self):
        with tempfile.TemporaryDirectory() as directory:
            nro = self.fixture(directory)
            data = bytearray(nro.read_bytes())
            data[-0x4000+0x3060:-0x4000+0x3065] = b'0.1.0'
            nro.write_bytes(data)
            with self.assertRaises(RuntimeError): verify(nro)

    def test_preserve_non_square_cover_and_baseline_format(self):
        from PIL import Image
        with tempfile.TemporaryDirectory() as directory:
            source = Path(directory)/'wide.png'; output = Path(directory)/'icon.jpg'
            Image.new('RGB', (400, 200), (255, 0, 0)).save(source)
            create_icon(output, source)
            with Image.open(output) as icon:
                self.assertEqual(icon.size, (256, 256))
                self.assertEqual(icon.mode, 'RGB')
                self.assertFalse(icon.info.get('progressive', False))
                self.assertGreater(icon.getpixel((128, 128))[0], 240)
                self.assertLess(icon.getpixel((128, 10))[0], 40)

    def test_missing_cover_never_falls_back_to_emblem(self):
        with tempfile.TemporaryDirectory() as directory:
            output = Path(directory)/'icon.jpg'
            with self.assertRaises(FileNotFoundError):
                create_icon(output, Path(directory)/'missing.png')
            self.assertFalse(output.exists())
