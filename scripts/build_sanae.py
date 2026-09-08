#!/usr/bin/env python3
"""Build the experimental SANAE runner (original Steam files; no conversion).

Fetches only pinned open-source engine code. Never downloads game data or a
proprietary runtime. A successful compile is NOT a hardware compatibility test.
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
from apply_sanae_overlay import apply


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--target', choices=['headless', 'switch'], required=True)
    parser.add_argument('--source', type=Path, default=ROOT/'.cache/butterscotch-open')
    parser.add_argument('--cmake', default='cmake')
    parser.add_argument('--prepare-only', action='store_true', help='Prepare source without requiring a compiler/toolchain')
    args = parser.parse_args()
    if not args.prepare_only:
        if not shutil.which(args.cmake) and not Path(args.cmake).is_file():
            parser.error('CMake 3.21+ is required.')
        if args.target == 'switch' and not (Path(os.environ.get('DEVKITPRO', '/nonexistent'))/'cmake/Switch.cmake').is_file():
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
    stage = ROOT/'.cache'/f'sanae-source-{args.target}'
    marker = stage/'.sanae-generated'
    if src == stage or src.is_relative_to(stage) or stage.is_relative_to(src):
        parser.error('The upstream checkout must not overlap the disposable SANAE source directory.')
    if stage.exists():
        if not marker.is_file() or marker.read_text() != PIN['revision']:
            parser.error(f'Refusing to replace a directory not owned by this builder: {stage}')
        shutil.rmtree(stage)
    shutil.copytree(src, stage, ignore=shutil.ignore_patterns('.git', '__pycache__', 'build', '*.win', '*.dat'))
    marker.write_text(PIN['revision'])
    apply(stage)
    print(f'Prepared SANAE-specific source: {stage}', flush=True)
    if args.prepare_only:
        return
    build = ROOT/'.cache'/f'sanae-build-{args.target}'
    options = ['-DCMAKE_BUILD_TYPE=Release', '-DENABLE_ASAN=OFF', '-DCMAKE_EXPORT_COMPILE_COMMANDS=ON',
               '-DBUTTERSCOTCH_COMMIT_HASH='+PIN['revision'][:12]]
    if args.target == 'headless':
        options += ['-DBACKEND=noop', '-DAUDIO_BACKEND=none']
    else:
        options += ['-DPLATFORM=switch', '-DCMAKE_C_FLAGS_RELEASE=-O3 -DNDEBUG -g1']
    run([args.cmake, '-S', stage, '-B', build, '-G', 'Unix Makefiles', *options])
    run([args.cmake, '--build', build, '--parallel', '4'])
    binary = build/('butterscotch.nro' if args.target == 'switch' else 'butterscotch')
    if not binary.is_file():
        raise RuntimeError(f'Missing build output: {binary}')
    if args.target == 'headless':
        run([sys.executable, ROOT/'scripts/test_sanae_runner.py', '--build', build])
    output = ROOT/'dist/sanae'/args.target
    output.mkdir(parents=True, exist_ok=True)
    named = output/('sanae.nro' if args.target == 'switch' else 'sanae-headless')
    shutil.copy2(binary, named)
    manifest = {'game': "SANAE's Sylphid Breeze", 'status': 'experimental; not hardware-verified',
                'engine': PIN, 'target': args.target, 'cmake_options': options,
                'display_version': '01.01', 'game_author': 'sorehodoh',
                'sha256': hashlib.sha256(named.read_bytes()).hexdigest(),
                'save_directory': 'sdmc:/switch/sanae/saves', 'game_data_included': False}
    (output/'BUILD.json').write_text(json.dumps(manifest, indent=2)+'\n')
    # Ship the EXACT patched engine source and the integration/build instructions.
    with tarfile.open(output/'sanae-source.tar.gz', 'w:gz') as archive:
        archive.add(stage, arcname='engine')
        for relative in ['LICENSE.md', 'open-runner', 'scripts/build_sanae.py',
                         'scripts/build_open_runner.py', 'scripts/apply_sanae_overlay.py',
                         'scripts/test_sanae_runner.py', 'tests/native', 'tests/test_sanae_native.py']:
            archive.add(ROOT/relative, arcname='integration/'+relative)
        archive.add(output/'BUILD.json', arcname='BUILD.json')
    if args.target == 'switch':
        # Retain the exact linked ELF for future Atmosphere PC/build-ID matching.
        # It is diagnostic source-engine output, not an extra file for the SD card.
        elf = None
        for candidate in [build/'butterscotch.elf', build/'butterscotch']:
            if candidate.is_file():
                with candidate.open('rb') as stream:
                    if stream.read(4) == b'\x7fELF':
                        elf = candidate
                        break
        if elf is None:
            raise RuntimeError('Switch ELF missing; refusing to publish without crash symbols')
        with zipfile.ZipFile(output/'sanae-symbols.zip', 'w', zipfile.ZIP_DEFLATED) as symbols:
            symbols.write(elf, 'sanae.elf')
            symbols.write(output/'BUILD.json', 'BUILD.json')
        instructions = (ROOT/'open-runner/SANAE_INSTALL.txt').read_text()
        with zipfile.ZipFile(output/'SANAE-experimental-switch.zip', 'w', zipfile.ZIP_DEFLATED) as archive:
            archive.write(named, 'switch/sanae/sanae.nro')
            archive.writestr('INSTALL.txt', instructions)
            archive.write(output/'BUILD.json', 'BUILD.json')
            for name in ['LICENSE', 'LICENSE.md', 'COPYING']:
                if (stage/name).is_file():
                    archive.write(stage/name, 'LICENSE-engine-'+name)
            archive.write(ROOT/'LICENSE.md', 'LICENSE-integration.md')
        print('Publish sanae-source.tar.gz alongside the Switch zip; retain dependency notices.')
    print(f'Built experimental SANAE: {named}\nCorresponding source: {output / "sanae-source.tar.gz"}')


if __name__ == '__main__':
    try:
        main()
    except (subprocess.CalledProcessError, OSError, RuntimeError) as error:
        print(f'ERROR: {error}', file=sys.stderr)
        sys.exit(1)
