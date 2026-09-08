#!/usr/bin/env python3
"""Audit tracked working-tree files for accidental proprietary/binary distribution.

Not a legal audit and not a history scrubber. Run before each source publication.
"""
from pathlib import Path
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[1]
FORBIDDEN_EXTENSIONS = {
    '.exe', '.dll', '.win', '.dat', '.nsp', '.nca', '.nro', '.nrr', '.npdm', '.nso',
    '.zip', '.7z', '.xz', '.rar', '.keys', '.png', '.jpg', '.jpeg', '.ogg', '.wav', '.mp3',
}
FORBIDDEN_NAMES = {'main', 'sdk', 'rtld', 'prod.keys', 'keys.dat', 'title.keys'}
FORBIDDEN_DIRS = {'local-inputs', 'build-inputs', 'sdcard-pack', 'exefs', 'romfs'}
MAGICS = (b'MZ', b'\x7fELF', b'NSO0', b'NRO0', b'PFS0', b'FORM', b'PK\x03\x04', b'7z\xbc\xaf\x27\x1c')


def violations(name, data):
    path = Path(name)
    parts = [p.casefold() for p in path.parts]
    reasons = []
    if path.suffix.casefold() in FORBIDDEN_EXTENSIONS:
        reasons.append('non-source/third-party input extension')
    if path.name.casefold() in FORBIDDEN_NAMES or path.name.casefold().startswith('subsdk'):
        reasons.append('runtime/key filename')
    if any(p in FORBIDDEN_DIRS or p.startswith('sdcard-pack') for p in parts):
        reasons.append('private input/output directory')
    if data.startswith(MAGICS):
        reasons.append('binary/game/archive signature')
    if len(data) > 1024*1024:
        reasons.append('unexpectedly large source file (over 1 MiB)')
    try:
        data.decode('utf-8')
    except UnicodeDecodeError:
        reasons.append('not UTF-8 source text')
    if b'\0' in data:
        reasons.append('binary NUL bytes')
    return reasons


def main():
    names = subprocess.check_output(['git', 'ls-files', '-z'], cwd=ROOT).decode().split('\0')
    failed = []
    for name in filter(None, names):
        path = ROOT/name
        if path.is_symlink():
            failed.append((name, ['tracked symlink; review target before publication']))
        elif not path.is_file():
            failed.append((name, ['tracked file is missing']))
        else:
            reasons = violations(name, path.read_bytes())
            if reasons:
                failed.append((name, reasons))
    for name, reasons in failed:
        print(f'REJECT {name}: {", ".join(reasons)}', file=sys.stderr)
    if failed:
        return 1
    print('PASS: tracked current files are source/text only. Git history and licensing are NOT audited.')
    return 0


if __name__ == '__main__':
    sys.exit(main())
