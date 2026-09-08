#!/usr/bin/env python3
"""Private CI input inspection. Never upload the archive, game assets or scripts."""
import hashlib
import json
import os
from pathlib import Path, PurePosixPath
import subprocess

REPORT = Path('.cache/user-data-report.json')
EXPECTED = '94893a08f434e0698c2cc46bf0414b97ef033d13330efc127475c6a525a8cd07'

def main():
    import py7zr
    download = Path('.cache/user-download')
    download.mkdir(parents=True, exist_ok=True)
    archive = download / 'input.7z'
    report = {'status': 'downloading', 'completed': False}
    REPORT.write_text(json.dumps(report))
    try:
        subprocess.run(['gdown', '--id', os.environ['SANAE_USER_ARCHIVE_ID'],
                        '-O', str(archive)], check=True, timeout=300, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
        with py7zr.SevenZipFile(archive, 'r') as reader:
            selected = []
            total = 0
            for member in reader.list():
                path = PurePosixPath(member.filename.replace('\\', '/'))
                if path.is_absolute() or '..' in path.parts or ':' in str(path):
                    raise ValueError('Unsafe archive path')
                if member.is_directory:
                    continue
                if path.suffix.lower() in ('.win', '.csv', '.dat'):
                    total += member.uncompressed
                    selected.append(member.filename)
            if len(selected) > 5000 or total > 1500 * 1024 * 1024:
                raise ValueError('Archive exceeds diagnostic limits')
            reader.extract(path='.cache/user-game', targets=selected)
        candidates = list(Path('.cache/user-game').rglob('data.win'))
        if len(candidates) != 1:
            raise ValueError('Expected exactly one data.win')
        data = candidates[0]
        digest = hashlib.sha256(data.read_bytes()).hexdigest()
        assets = list(Path('.cache/user-game').rglob('*'))
        report.update(status='extracted', data_sha256=digest,
                      matches_previous_data=digest == EXPECTED,
                      data_bytes=data.stat().st_size,
                      csv_files=sorted(p.name for p in assets if p.suffix.lower()=='.csv'),
                      audio_groups=sorted(p.name for p in assets if p.name.lower().startswith('audiogroup') and p.suffix.lower()=='.dat'))
    except Exception as error:
        report.update(status='download/extraction failed', error=type(error).__name__)
        raise
    finally:
        REPORT.write_text(json.dumps(report, indent=2))

if __name__ == '__main__':
    main()
