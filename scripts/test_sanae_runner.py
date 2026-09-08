#!/usr/bin/env python3
"""Test SANAE integration against an existing Unix Makefiles headless build.

Does not need any game data. Also syntax-checks the full patched OpenAL backend,
which the no-audio executable does not compile. This is not a Switch/device test.
"""
import argparse
import json
from pathlib import Path
import shlex
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1]


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--sanitize-audio', action='store_true')
    parser.add_argument('--build', type=Path, default=ROOT/'.cache/sanae-build-headless')
    args = parser.parse_args()
    build = args.build.resolve()
    commands = json.loads((build/'compile_commands.json').read_text())
    entry = next(item for item in commands if item['file'].endswith('/src/vm_builtins.c'))
    source = Path(entry['file']).parents[1]
    flags = shlex.split(entry['command'])
    i = flags.index('-o'); del flags[i:i+2]
    flags.remove('-c'); flags.remove(entry['file'])
    flags = [f for f in flags if f not in ('-DNDEBUG', '-O3', '-w')]
    flags += ['-O0', '-I'+str(source/'src')]
    with tempfile.TemporaryDirectory(prefix='sanae-integration-') as work:
        output = Path(work)/'test.o'
        subprocess.run([*flags, '-c', str(ROOT/'tests/native/sanae_runner.c'), '-o', str(output)], cwd=build, check=True)
        link = shlex.split((build/'CMakeFiles/butterscotch.dir/link.txt').read_text())
        link = [f for f in link if not f.endswith('/src/cli/main.c.o')]
        i = link.index('-o'); link[i+1] = str(Path(work)/'test')
        link.append(str(output))
        subprocess.run(link, cwd=build, check=True)
        subprocess.run([str(Path(work)/'test')], cwd=work, check=True)
        audio_sanitizers = ['-fsanitize=address,undefined', '-fno-omit-frame-pointer'] if args.sanitize_audio else []
        subprocess.run([*flags, *audio_sanitizers, '-ffunction-sections', '-fdata-sections',
                        '-I'+str(source/'vendor/mojoal'), '-I'+str(source/'vendor/stb/vorbis'),
                        str(ROOT/'tests/native/sanae_audio.c'), str(build/'CMakeFiles/butterscotch.dir/src/stb_ds.c.o'), '-Wl,--gc-sections', '-lm',
                        '-o', str(Path(work)/'audio')], cwd=build, check=True)
        subprocess.run([str(Path(work)/'audio')], cwd=work, check=True)
    subprocess.run([*flags, '-fsyntax-only', '-I'+str(source/'vendor/mojoal'),
                    '-I'+str(source/'vendor/stb/vorbis'),
                    str(source/'src/audio/openal/al_audio_system.c')], cwd=build, check=True)
    print('Full OpenAL backend syntax check passed (host headers; no audio device test).')


if __name__ == '__main__':
    main()
