#!/usr/bin/env python3
"""Compile and run the thWWW native integration tests.

Needs no game data and no Switch toolchain: the save/frame-policy headers are
plain C and are exercised directly on the host, with sanitizers when asked.
"""
import argparse
from pathlib import Path
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[1]
OVERLAY = ROOT / 'wonderful-waking-world/www'


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--sanitize', action='store_true')
    parser.add_argument('--cc', default='gcc')
    args = parser.parse_args()

    base = [args.cc, '-std=c2x', '-Wall', '-Wextra', '-Werror',
            '-I' + str(OVERLAY), '-D_DEFAULT_SOURCE']
    variants = [('plain', [])]
    if args.sanitize:
        variants.append(('sanitized', ['-fsanitize=address,undefined', '-fno-omit-frame-pointer']))

    for label, extra in variants:
        binary = ROOT / '.cache' / f'thwww-native-{label}'
        binary.parent.mkdir(parents=True, exist_ok=True)
        subprocess.run([*base, *extra, str(ROOT / 'tests/native/www_save.c'),
                        '-o', str(binary)], check=True)
        subprocess.run([str(binary)], check=True)
        print(f'thWWW native tests passed ({label})', flush=True)

    # The libnx entry point cannot be linked on the host, but its structure can
    # still be checked for the invariants the port depends on.
    main_c = (OVERLAY / 'main.c').read_text()
    for needle in ('sdmc:/switch/thwww', 'data.win', 'saveFolder', 'thwww.log'):
        if needle not in main_c:
            raise RuntimeError(f'Entry point lost a required element: {needle}')
    print('thWWW entry-point invariants verified.')


if __name__ == '__main__':
    try:
        main()
    except (subprocess.CalledProcessError, OSError, RuntimeError) as error:
        print(f'ERROR: {error}', file=sys.stderr)
        sys.exit(1)
