#!/usr/bin/env python3
"""Fetch pinned SANAE inputs via gh; verify Git blob hashes before writing.

Requires GitHub CLI. Downloads are ignored by Git. No workflows are triggered.
"""
import base64
import concurrent.futures
import hashlib
import json
from pathlib import Path
import subprocess

REPO = 'saekaze/th8test'
REV = 'bf6516b2f89196b2843625b9fc5a35cda4f89ef8'
ROOT = Path(__file__).resolve().parent.parent


def api(endpoint):
    return json.loads(subprocess.check_output(['gh', 'api', f'repos/{REPO}/{endpoint}']))


def fetch(entry):
    path = ROOT / entry['path']
    def valid(data):
        return hashlib.sha1(b'blob ' + str(len(data)).encode() + b'\0' + data).hexdigest() == entry['sha']
    if path.exists() and valid(path.read_bytes()):
        return f'OK {entry["path"]}'
    blob = api(f'git/blobs/{entry["sha"]}')
    if blob.get('encoding') != 'base64':
        raise ValueError(f'Unexpected encoding for {path}')
    data = base64.b64decode(blob['content'])
    if not valid(data):
        raise ValueError(f'Hash mismatch: {path}')
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_bytes(data)
    return f'Downloaded {entry["path"]}'


def main():
    tree = api(f'git/trees/{REV}?recursive=1')
    if tree.get('truncated'):
        raise ValueError('Truncated input tree')
    entries = [e for e in tree['tree'] if e['type'] == 'blob' and (
        e['path'].startswith('build-inputs/') or e['path'] in (
            'game.win', 'SANAEs_Sylphid_Breeze.nsp'))]
    with concurrent.futures.ThreadPoolExecutor(max_workers=3) as pool:
        for result in pool.map(fetch, entries):
            print(result, flush=True)


if __name__ == '__main__':
    main()
