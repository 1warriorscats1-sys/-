#!/usr/bin/env python3
"""Build a LayeredFS exit fix, without NSP encryption keys or CI."""
import hashlib
from pathlib import Path
import subprocess
import sys
import tempfile
import zipfile

import patch_nso

ROOT = Path(__file__).resolve().parents[1]
OUTPUT = ROOT / 'SANAE_exit_fix.zip'


def main():
    src = ROOT / 'build-inputs/runtime-2024.14.3.260/bin/main'
    with tempfile.TemporaryDirectory() as temp:
        patched = Path(temp) / 'main'
        subprocess.run([sys.executable, str(ROOT/'scripts/patch_nso.py'), str(src), str(patched)], check=True)
        header, flags, segs, raw = patch_nso.load_nso(patched)
        for i, segment in enumerate(raw):
            expected = header[0xa0+i*32:0xc0+i*32]
            if hashlib.sha256(segment).digest() != expected:
                raise ValueError(f'NSO segment {i} checksum mismatch')
        for i in range(2):
            if segs[i][1]+len(raw[i]) > segs[i+1][1]:
                raise ValueError('NSO segments overlap')
        main_data = patched.read_bytes()
        path = 'atmosphere/contents/010000000005A1E1/exefs/main'
        digest = hashlib.sha256(main_data).hexdigest()
        with zipfile.ZipFile(OUTPUT, 'w', zipfile.ZIP_DEFLATED) as z:
            z.writestr(path, main_data)
            z.writestr('README_RU.txt', (ROOT/'SANAE_EXIT_FIX.md').read_bytes())
            z.writestr('SHA256SUMS.txt', f'{digest}  {path}\n')
        with zipfile.ZipFile(OUTPUT) as z:
            if z.testzip() is not None:
                raise ValueError('ZIP integrity check failed')
        print(f'Built {OUTPUT.name} ({OUTPUT.stat().st_size} bytes)')
        print(f'main SHA256: {digest}')


if __name__ == '__main__':
    main()
