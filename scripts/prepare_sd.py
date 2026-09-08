#!/usr/bin/env python3
"""Prepare user-owned SANAE PC data locally. No downloads or runtime included."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import shutil
import struct
import subprocess
import sys
import tempfile

ROOT = Path(__file__).resolve().parents[1]
TITLE_ID = '010000000005A1E1'
# Referenced by the known game's dialogue loader, outside data.win.
REQUIRED_INCLUDED_FILES = ('scenario_sanae.csv', 'scenario_sanae_en.csv')
# The misspelling belongs to an additional object; don't invent a replacement.
OPTIONAL_INCLUDED_FILES = ('scenaorio_sanae.csv', 'item.txt')
# A previously verified converted file; never shipped or downloaded by this tool.
KNOWN_CONVERTED = 'f8b816d0bea0cf35b6f6eb793fc49cf0aa948806a646abd245310ee2f727097f'


class InputError(ValueError):
    pass


class GameData:
    """Bounded FORM reader used before the legacy conversion code runs."""
    def __init__(self, data):
        data = self.data = bytes(data)
        self.chunks = {}
        if len(data) < 8 or data[:4] != b'FORM':
            raise InputError('Expected a GameMaker FORM data.win, not an EXE or archive.')
        if self.u32(4) + 8 != len(data):
            raise InputError('FORM size mismatch: file is truncated or has trailing data.')
        at = 8
        while at < len(data):
            self.bounds(at, 8)
            name = data[at:at+4]
            size = self.u32(at+4)
            self.bounds(at+8, size)
            if name in self.chunks:
                raise InputError('Duplicate FORM chunk.')
            self.chunks[name] = (at+8, size)
            at += 8+size

    def bounds(self, pos, length):
        if pos < 0 or length < 0 or pos+length > len(self.data):
            raise InputError('Invalid data pointer or truncated chunk.')

    def u32(self, at):
        self.bounds(at, 4)
        return struct.unpack_from('<I', self.data, at)[0]

    def string(self, at):
        if at < 4:
            raise InputError('Invalid game name pointer.')
        length = self.u32(at-4)
        if length > 4096:
            raise InputError('Unreasonable game name length.')
        self.bounds(at, length+1)
        if self.data[at+length] != 0:
            raise InputError('Unterminated string.')
        return self.data[at:at+length].decode('utf-8', 'strict')

    def pointers(self, name):
        if name not in self.chunks:
            raise InputError(f'Missing chunk {name.decode()}.')
        start, size = self.chunks[name]
        if size < 4:
            raise InputError('Truncated pointer table.')
        count = self.u32(start)
        if count > (size-4)//4:
            raise InputError('Invalid pointer count.')
        return [self.u32(start+4+i*4) for i in range(count)]

    def validate_sanae(self):
        required = [b'GEN8', b'CODE', b'FUNC', b'VARI', b'ROOM', b'AGRP',
                    b'SOND', b'SPRT', b'TGIN', b'STRG']
        if any(c not in self.chunks for c in required):
            raise InputError('Unsupported data: expected SANAE GameMaker VM build, not YYC.')
        gen, size = self.chunks[b'GEN8']
        if size < 44 or self.data[gen+1] != 17:
            raise InputError('Unsupported bytecode version; expected 17.')
        if self.string(self.u32(gen+40)) != 'SANAE_Sylphid_Breeze':
            raise InputError("This is not SANAE's Sylphid Breeze data.")
        rooms, groups = self.pointers(b'ROOM'), self.pointers(b'AGRP')
        if len(rooms) != 104 or len(groups) != 19 or len(self.pointers(b'SPRT')) != 1568:
            raise InputError('Unsupported game revision: room/audio/sprite layout differs.')
        for ptr in rooms:
            if not ptr:
                raise InputError('Null room pointer.')
            self.bounds(ptr, 0x60)
        for ptr in groups:
            if not ptr:
                raise InputError('Null audio group pointer.')
            self.string(self.u32(ptr))
        counts = {}
        for ptr in self.pointers(b'SOND'):
            if ptr:
                self.bounds(ptr, 40)
                group = self.u32(ptr+28)
                if group >= len(groups):
                    raise InputError('Sound references an invalid audio group.')
                counts[group] = counts.get(group, 0)+1
        return {i for i in range(1, len(groups)) if counts.get(i, 0)}


def sha256(path):
    h = hashlib.sha256()
    with Path(path).open('rb') as f:
        for block in iter(lambda: f.read(1024*1024), b''):
            h.update(block)
    return h.hexdigest()


def find_file(folder, name, required=True):
    found = [p for p in folder.iterdir() if p.name.casefold() == name.casefold() and p.is_file()]
    if len(found) > 1:
        raise InputError(f'Ambiguous filenames for {name}.')
    if not found:
        if required:
            raise InputError(f'Missing {name}. Select the Steam folder containing data.win.')
        return None
    if found[0].is_symlink():
        raise InputError(f'Symlink input not supported: {name}. Use actual local files.')
    return found[0]


def run_step(script, *args):
    result = subprocess.run([sys.executable, str(ROOT/'scripts'/script), *map(str, args)],
                            capture_output=True, text=True, encoding='utf-8', errors='replace')
    log = result.stdout + result.stderr
    if result.returncode:
        raise InputError(f'{script} rejected these files; no output was installed.\n{log[-4000:]}')
    return log


def prepare(game, output):
    game, output = Path(game).resolve(), Path(output).resolve()
    if not game.is_dir():
        raise InputError('Game folder does not exist.')
    if output == game or game in output.parents or output in game.parents:
        raise InputError('Output and Steam folder must not overlap. Originals are read-only.')
    if output.exists():
        raise InputError('Output already exists. Choose a NEW folder; existing files are never overwritten.')
    source = find_file(game, 'data.win')
    if source.stat().st_size > 256*1024*1024:
        raise InputError('Unsupported data.win size (over 256 MiB).')
    data = GameData(source.read_bytes())
    required_audio = data.validate_sanae()
    inputs = {'data.win': source}
    for i in range(1, 19):
        name = f'audiogroup{i}.dat'
        audio = find_file(game, name, required=i in required_audio)
        if audio:
            # Audio archives can be large; validate their outer FORM without loading them.
            with audio.open('rb') as f:
                head = f.read(8)
            if len(head) != 8 or head[:4] != b'FORM' or struct.unpack_from('<I', head, 4)[0]+8 != audio.stat().st_size:
                raise InputError(f'{name} is not an intact GameMaker audio archive.')
            inputs[name] = audio
    for name in REQUIRED_INCLUDED_FILES + OPTIONAL_INCLUDED_FILES:
        included = find_file(game, name, required=name in REQUIRED_INCLUDED_FILES)
        if included:
            if included.stat().st_size == 0 and name in REQUIRED_INCLUDED_FILES:
                raise InputError(f'{name} is empty. Verify the files in your own Steam installation.')
            inputs[name] = included
    before = {name: sha256(path) for name, path in inputs.items()}
    output.parent.mkdir(parents=True, exist_ok=True)
    with tempfile.TemporaryDirectory(prefix='.sanae-', dir=output.parent) as temp:
        temp = Path(temp)
        pack = temp/'pack'
        romfs = pack/'atmosphere'/'contents'/TITLE_ID/'romfs'
        romfs.mkdir(parents=True)
        log = ''
        # Snapshot data.win first so a concurrent Steam update cannot mix versions.
        original = temp/'original.win'
        original.write_bytes(data.data)
        if sha256(original) != before['data.win']:
            raise InputError('data.win changed while being read. Close Steam and retry.')
        if before['data.win'] == KNOWN_CONVERTED:
            shutil.copyfile(original, romfs/'game.win')
            log += 'Recognized already converted and Steam-adapted revision; no double patch.\n'
        else:
            log += run_step('inspect_data.py', original)
            log += run_step('convert_data.py', original, temp/'converted.win')
            # Never fall back to unpatched data: unavailable Steam APIs would crash.
            log += run_step('patch_steam.py', temp/'converted.win', romfs/'game.win')
        log += run_step('verify_data.py', romfs/'game.win')
        for name, path in inputs.items():
            if name != 'data.win':
                shutil.copyfile(path, romfs/name)
                if sha256(romfs/name) != before[name]:
                    raise InputError(f'{name} changed during preparation. Close Steam and retry.')
        if any(sha256(path) != before[name] for name, path in inputs.items()):
            raise InputError('Input files changed during preparation; retry with Steam closed.')
        # No absolute PC paths in the distributable instructions/manifest.
        files = {p.relative_to(pack).as_posix(): sha256(p) for p in sorted(romfs.iterdir())}
        (pack/'manifest.json').write_text(json.dumps({
            'format': 1, 'title_id': TITLE_ID, 'inputs_sha256': before,
            'outputs_sha256': files, 'runtime_included': False,
            'private_output_do_not_redistribute': True,
        }, indent=2)+'\n', encoding='utf-8')
        # Replace temporary paths in the diagnostic log.
        (pack/'prepare.log').write_text(log.replace(str(temp), '<temporary>'), encoding='utf-8')
        (pack/'INSTALL.txt').write_text(
            'PRIVATE OUTPUT — contains your game data. Do not upload or redistribute.\n'
            'Requires an independently and lawfully obtained compatible SANAE Switch application.\n'
            'No runner, NRO, NSP, keys or SDK is included. This pack alone is not bootable.\n'
            'Close the game (HOME -> X). Back up existing romfs files on your SD.\n'
            'Merge the atmosphere folder into the SD root. Do not delete saves or exefs.\n'
            'The Steam originals have not been changed. See docs/INSTALL.md in the tools repository.\n', encoding='utf-8')
        if output.exists():
            raise InputError('Output appeared during conversion; refusing to replace it.')
        os.rename(pack, output)
    return output


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('game', nargs='?', help='Steam game folder containing data.win (omit for folder picker)')
    parser.add_argument('--output', type=Path, default=ROOT/'sdcard-pack', help='NEW staging folder, not the SD card itself')
    args = parser.parse_args(argv)
    if args.game is None:
        try:
            import tkinter as tk
            from tkinter import filedialog
            root = tk.Tk(); root.withdraw()
            args.game = filedialog.askdirectory(title='Select your SANAE Steam folder (data.win)')
            root.destroy()
        except Exception as e:
            parser.error(f'Folder picker unavailable ({e}). Pass the game folder on the command line.')
        if not args.game:
            return 1
    try:
        path = prepare(args.game, args.output)
    except (InputError, OSError, UnicodeError, struct.error) as e:
        print(f'ERROR: {e}', file=sys.stderr)
        return 1
    print(f'Prepared: {path}\nMerge its atmosphere folder into the SD root.\n'
          'Game data only; a lawful compatible Switch runtime is still required. Do not share this output.')
    return 0


if __name__ == '__main__':
    sys.exit(main())
