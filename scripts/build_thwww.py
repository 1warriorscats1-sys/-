#!/usr/bin/env python3
"""Build the experimental Wonderful Waking World (thWWW) Switch runner.

Fetches only pinned open-source engine code. Never downloads game data, game
artwork or a proprietary runtime. A successful compile is NOT a hardware
compatibility test.
"""
import argparse
import hashlib
import json
import os
from pathlib import Path
import shutil
import subprocess
import sys
import tarfile
import zipfile

from build_open_runner import ROOT, PIN, run
from apply_www_overlay import apply, TITLE, AUTHOR, VERSION

GAME_DIR = 'sdmc:/switch/thwww'


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--target', choices=['headless', 'switch'], required=True)
    parser.add_argument('--source', type=Path, default=ROOT / '.cache/butterscotch-open')
    parser.add_argument('--cmake', default='cmake')
    parser.add_argument('--prepare-only', action='store_true',
                        help='Prepare source without requiring a compiler/toolchain')
    args = parser.parse_args()

    if not args.prepare_only:
        if not shutil.which(args.cmake) and not Path(args.cmake).is_file():
            parser.error('CMake 3.21+ is required.')
        if args.target == 'switch' and not (Path(os.environ.get('DEVKITPRO', '/nonexistent')) / 'cmake/Switch.cmake').is_file():
            parser.error('Install devkitPro/devkitA64/libnx and Switch portlibs; set DEVKITPRO first.')

    src = args.source.resolve()
    if src == ROOT or ROOT.is_relative_to(src):
        parser.error('Use a separate pinned upstream checkout, not the toolkit root/parent.')
    if not src.exists():
        src.parent.mkdir(parents=True, exist_ok=True)
        run(['git', 'init', src])
        run(['git', '-C', src, 'remote', 'add', 'origin', PIN['repository']])
        run(['git', '-C', src, 'fetch', '--depth', '1', 'origin', PIN['revision']])
        run(['git', '-C', src, 'checkout', '--detach', 'FETCH_HEAD'])
    revision = subprocess.check_output(['git', '-C', src, 'rev-parse', 'HEAD'], text=True).strip()
    dirty = subprocess.check_output(['git', '-C', src, 'status', '--porcelain'], text=True).strip()
    if revision != PIN['revision'] or dirty:
        parser.error('Upstream checkout must be clean and at the pinned revision. Use a new --source directory.')

    stage = ROOT / '.cache' / f'thwww-source-{args.target}'
    marker = stage / '.thwww-generated'
    if src == stage or src.is_relative_to(stage) or stage.is_relative_to(src):
        parser.error('The upstream checkout must not overlap the disposable thWWW source directory.')
    if stage.exists():
        if not marker.is_file() or marker.read_text() != PIN['revision']:
            parser.error(f'Refusing to replace a directory not owned by this builder: {stage}')
        shutil.rmtree(stage)
    shutil.copytree(src, stage, ignore=shutil.ignore_patterns('.git', '__pycache__', 'build', '*.win', '*.dat'))
    marker.write_text(PIN['revision'])
    apply(stage)

    if args.target == 'switch':
        from create_www_icon import create_icon
        try:
            create_icon(stage / 'thwww-icon.jpg')
        except ImportError as error:
            raise RuntimeError('Switch icon generation requires Pillow (python3-pil).') from error
    print(f'Prepared thWWW-specific source: {stage}', flush=True)
    if args.prepare_only:
        return

    build = ROOT / '.cache' / f'thwww-build-{args.target}'
    options = ['-DCMAKE_BUILD_TYPE=Release', '-DENABLE_ASAN=OFF', '-DCMAKE_EXPORT_COMPILE_COMMANDS=ON',
               '-DBUTTERSCOTCH_COMMIT_HASH=' + PIN['revision'][:12]]
    if args.target == 'headless':
        options += ['-DBACKEND=noop', '-DAUDIO_BACKEND=none']
    else:
        options += ['-DPLATFORM=switch', '-DCMAKE_C_FLAGS_RELEASE=-O3 -DNDEBUG -g1']
    run([args.cmake, '-S', stage, '-B', build, '-G', 'Unix Makefiles', *options])
    run([args.cmake, '--build', build, '--parallel', '4'])

    binary = build / ('butterscotch.nro' if args.target == 'switch' else 'butterscotch')
    if not binary.is_file():
        raise RuntimeError(f'Missing build output: {binary}')
    asset_report = None
    if args.target == 'switch':
        from verify_www_nro import verify
        asset_report = verify(binary)
        print('Verified embedded NRO assets:', json.dumps(asset_report), flush=True)

    output = ROOT / 'dist/thwww' / args.target
    output.mkdir(parents=True, exist_ok=True)
    named = output / ('thwww.nro' if args.target == 'switch' else 'thwww-headless')
    shutil.copy2(binary, named)
    manifest = {'game': TITLE, 'status': 'experimental; not hardware-verified',
                'engine': PIN, 'target': args.target, 'cmake_options': options,
                'display_version': VERSION, 'game_author': AUTHOR,
                'verified_nro_assets': asset_report,
                'sha256': hashlib.sha256(named.read_bytes()).hexdigest(),
                'save_directory': GAME_DIR + '/saves', 'game_data_included': False}
    (output / 'BUILD.json').write_text(json.dumps(manifest, indent=2) + '\n')

    # Ship the EXACT patched engine source and the integration/build recipe.
    with tarfile.open(output / 'thwww-source.tar.gz', 'w:gz') as archive:
        archive.add(stage, arcname='engine')
        for relative in ['LICENSE.md', 'wonderful-waking-world',
                         'scripts/build_thwww.py', 'scripts/build_open_runner.py',
                         'scripts/apply_www_overlay.py', 'scripts/create_www_icon.py',
                         'scripts/verify_www_nro.py', 'tests/test_thwww.py']:
            archive.add(ROOT / relative, arcname='integration/' + relative)
        archive.add(output / 'BUILD.json', arcname='BUILD.json')

    if args.target == 'switch':
        # Retain the exact linked ELF for later crash-report symbolication.
        elf = None
        for candidate in [build / 'butterscotch.elf', build / 'butterscotch']:
            if candidate.is_file():
                with candidate.open('rb') as stream:
                    if stream.read(4) == b'\x7fELF':
                        elf = candidate
                        break
        if elf is None:
            raise RuntimeError('Switch ELF missing; refusing to publish without crash symbols')
        with zipfile.ZipFile(output / 'thwww-symbols.zip', 'w', zipfile.ZIP_DEFLATED) as symbols:
            symbols.write(elf, 'thwww.elf')
            symbols.write(output / 'BUILD.json', 'BUILD.json')
        instructions = (ROOT / 'wonderful-waking-world/THWWW_INSTALL.txt').read_text()
        with zipfile.ZipFile(output / 'thWWW-experimental-switch.zip', 'w', zipfile.ZIP_DEFLATED) as archive:
            archive.write(named, 'switch/thwww/thwww.nro')
            archive.writestr('INSTALL.txt', instructions)
            archive.write(output / 'BUILD.json', 'BUILD.json')
            for name in ['LICENSE', 'LICENSE.md', 'COPYING']:
                if (stage / name).is_file():
                    archive.write(stage / name, 'LICENSE-engine-' + name)
            archive.write(ROOT / 'LICENSE.md', 'LICENSE-integration.md')
        print('Publish thwww-source.tar.gz alongside the Switch zip; retain dependency notices.')
    print(f'Built experimental thWWW: {named}\nCorresponding source: {output / "thwww-source.tar.gz"}')


if __name__ == '__main__':
    try:
        main()
    except (subprocess.CalledProcessError, OSError, RuntimeError) as error:
        print(f'ERROR: {error}', file=sys.stderr)
        sys.exit(1)
