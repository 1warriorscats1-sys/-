"""Generated, non-game icon and actual NRO asset-table validation."""
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
        create_icon(icon)
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

    def test_generated_icon_and_metadata(self):
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
