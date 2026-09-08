#!/usr/bin/env python3
"""Build the pinned OPEN Butterscotch engine, not a proprietary GameMaker binary.

No game inputs are fetched or copied. Result is an upstream compatibility probe,
NOT a fully verified SANAE port. Switch builds require devkitPro/libnx installed.
"""
import argparse
import json
import os
from pathlib import Path
import shutil
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[1]
PIN = json.loads((ROOT/'open-runner/upstream.json').read_text())


def run(args, **kw):
    print('+', ' '.join(map(str, args)), flush=True)
    subprocess.run(list(map(str, args)), check=True, **kw)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--target', choices=['headless', 'switch'], required=True)
    parser.add_argument('--source', type=Path, default=ROOT/'.cache/butterscotch-open')
    parser.add_argument('--cmake', default='cmake')
    args = parser.parse_args()
    if not shutil.which(args.cmake) and not Path(args.cmake).is_file():
        parser.error('CMake 3.21+ is required.')
    if args.target == 'switch' and not (Path(os.environ.get('DEVKITPRO', '/nonexistent'))/'cmake/Switch.cmake').is_file():
        parser.error('Install the open devkitPro/devkitA64/libnx toolchain and set DEVKITPRO first.')
    src = args.source.resolve()
    if src == ROOT or ROOT.is_relative_to(src):
        parser.error('The engine checkout must not be the toolkit root or its parent.')
    if not src.exists():
        src.parent.mkdir(parents=True, exist_ok=True)
        run(['git', 'init', str(src)])
        run(['git', '-C', src, 'remote', 'add', 'origin', PIN['repository']])
        run(['git', '-C', src, 'fetch', '--depth', '1', 'origin', PIN['revision']])
        # Only the separate upstream source checkout is detached, not this repository.
        run(['git', '-C', src, 'checkout', '--detach', 'FETCH_HEAD'])
    rev = subprocess.check_output(['git', '-C', src, 'rev-parse', 'HEAD'], text=True).strip()
    dirty = subprocess.check_output(['git', '-C', src, 'status', '--porcelain'], text=True).strip()
    if rev != PIN['revision'] or dirty:
        parser.error('Upstream checkout is modified or has a different revision. Use a new --source directory.')
    build = ROOT/'.cache'/f'open-runner-build-{args.target}'
    options = ['-DCMAKE_BUILD_TYPE=Release', '-DENABLE_ASAN=OFF']
    if args.target == 'headless':
        options += ['-DBACKEND=noop', '-DAUDIO_BACKEND=none']
    else:
        options += ['-DPLATFORM=switch']
    run([args.cmake, '-S', src, '-B', build, *options])
    run([args.cmake, '--build', build, '--parallel', '4'])
    artifact = build/('butterscotch.nro' if args.target == 'switch' else 'butterscotch')
    if not artifact.is_file():
        raise RuntimeError(f'Expected build output is missing: {artifact}')
    print(f'Built upstream probe: {artifact}\nRead open-runner/README.md before testing.\n'
          'Do not label this build a finished SANAE port. Keep corresponding AGPL source with any binary release.')


if __name__ == '__main__':
    try:
        main()
    except (subprocess.CalledProcessError, OSError, RuntimeError) as e:
        print(f'ERROR: {e}', file=sys.stderr)
        sys.exit(1)
