#!/usr/bin/env python3
"""Privately patch a user-supplied, lawfully obtained runner. Never distribute output."""
import argparse
import hashlib
from pathlib import Path
import subprocess
import sys
import tempfile
import zipfile

import patch_nso

ROOT = Path(__file__).resolve().parents[1]



def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--main', required=True, type=Path, help='Your original 2024.14.3.260 ExeFS main')
    parser.add_argument('--output', type=Path, default=ROOT/'local-inputs/SANAE_exit_fix.zip')
    args = parser.parse_args()
    src, output = args.main.resolve(), args.output.resolve()
    if output.exists():
        parser.error('Output exists; choose a new path. No existing files are overwritten.')
    if not src.is_file():
        parser.error('Original main does not exist. This tool does not download runtimes.')
    output.parent.mkdir(parents=True, exist_ok=True)
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
        with zipfile.ZipFile(output, 'x', zipfile.ZIP_DEFLATED) as z:
            z.writestr(path, main_data)
            z.writestr('README_RU.txt', (ROOT/'docs/EXIT_FIX.md').read_bytes())
            z.writestr('SHA256SUMS.txt', f'{digest}  {path}\n')
        with zipfile.ZipFile(output) as z:
            if z.testzip() is not None:
                raise ValueError('ZIP integrity check failed')
        print(f'Built {output.name} ({output.stat().st_size} bytes)')
        print(f'main SHA256: {digest}')
        print('PRIVATE OUTPUT: contains the proprietary runner. Do not upload or redistribute.')


if __name__ == '__main__':
    main()
