#!/usr/bin/env python3
"""Inspect your own game files locally - read-only, no conversion.

This tool never extracts, converts, decrypts or copies content: it prints an
identifying report (size, SHA-256, header bytes, entropy, text-likeness, known
generic signatures) for files you point it at. It makes no network requests.

Why it exists: the port reads its own documented package format instead of any
game's data (see docs/TH01_ORIGINAL_DATA.md). If you own a legal copy and want
to discuss a converter, this report is the starting point - together with the
rights holder's permission, which we do not have.

Usage:
    python3 scripts/probe_th01_original.py ~/my-pc98-copy
    python3 scripts/probe_th01_original.py ~/my-pc98-copy --json
"""
from __future__ import annotations

import argparse
import hashlib
import json
import math
import sys
from pathlib import Path

MAX_FILES = 400
MAX_READ = 1 << 20  # header/entropy sample

# Only generic container signatures we can state without guessing at any
# proprietary layout. Anything else is reported as "unknown".
SIGNATURES = [
    (b"MZ", "DOS/Windows executable header"),
    (b"PK\x03\x04", "ZIP archive"),
    (b"\x1f\x8b", "gzip stream"),
    (b"RIFF", "RIFF container"),
    (b"BM", "BMP image"),
    (b"\x89PNG", "PNG image"),
    (b"GIF8", "GIF image"),
    (b"ID3", "MP3 with ID3 tag"),
    (b"OggS", "Ogg container"),
    (b"7z\xbc\xaf", "7z archive"),
    (b"\xfd7zXZ", "xz stream"),
    (b"ustar", "tar archive"),
]


def entropy(sample: bytes) -> float:
    if not sample:
        return 0.0
    counts = [0] * 256
    for byte in sample:
        counts[byte] += 1
    total = len(sample)
    return -sum((count / total) * math.log2(count / total) for count in counts if count)


def printable_ratio(sample: bytes) -> float:
    if not sample:
        return 0.0
    good = sum(1 for byte in sample if 9 <= byte <= 13 or 32 <= byte < 127)
    return good / len(sample)


def ascii_preview(sample: bytes, limit: int = 40) -> str:
    out = []
    for byte in sample[:limit]:
        out.append(chr(byte) if 32 <= byte < 127 else ".")
    return "".join(out)


def probe_file(path: Path) -> dict:
    stat = path.stat()
    with path.open("rb") as handle:
        head = handle.read(64)
        handle.seek(0)
        data = handle.read(MAX_READ)
    digest = hashlib.sha256()
    for offset in range(0, len(data), 65536):
        digest.update(data[offset:offset + 65536])
    if stat.st_size <= MAX_READ:
        with path.open("rb") as handle:
            digest = hashlib.sha256(handle.read())

    signatures = [name for magic, name in SIGNATURES if head.startswith(magic)]
    ent = entropy(data[:65536])
    text_ratio = printable_ratio(data[:4096])

    if signatures:
        guess = "; ".join(signatures)
    elif text_ratio > 0.85:
        guess = "looks like plain text or a script"
    elif ent > 7.5:
        guess = "high entropy: compressed, packed or encrypted"
    else:
        guess = "unknown layout (no generic signature matched)"

    return {
        "name": path.name,
        "size": stat.st_size,
        "sha256": digest.hexdigest(),
        "head_hex": head[:32].hex(),
        "preview": ascii_preview(head),
        "entropy_bits": round(ent, 2),
        "printable_ratio": round(text_ratio, 3),
        "guess": guess,
    }


def collect(target: Path, follow: bool) -> list[Path]:
    if target.is_file():
        return [target]
    pattern = "**/*" if follow else "*"
    files = [path for path in target.glob(pattern) if path.is_file()]
    return sorted(files)[:MAX_FILES]


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__,
                                     formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("target", type=Path, help="file or directory to inspect (local only)")
    parser.add_argument("--recursive", action="store_true", help="walk subdirectories")
    parser.add_argument("--json", action="store_true")
    args = parser.parse_args()

    if not args.target.exists():
        print(f"error: {args.target} does not exist", file=sys.stderr)
        return 2

    files = collect(args.target, args.recursive)
    if not files:
        print(f"no files found under {args.target}")
        return 1

    report = [probe_file(path) for path in files]

    if args.json:
        print(json.dumps({"target": str(args.target), "files": report}, indent=2))
    else:
        print(f"read-only inspection of {args.target} ({len(report)} file(s))")
        print("nothing is converted, extracted or uploaded\n")
        for entry in report:
            print(f"{entry['name']}  {entry['size']} bytes")
            print(f"  sha256   {entry['sha256']}")
            print(f"  head     {entry['head_hex']}")
            print(f"  preview  {entry['preview']!r}")
            print(f"  entropy  {entry['entropy_bits']} bits/byte, printable {entry['printable_ratio']}")
            print(f"  guess    {entry['guess']}\n")
        print("Next step: describe the layout in docs/TH01_DATA_FORMAT.md terms, pack your own")
        print("content with scripts/pack_th01_data.py, and keep the rights holder's permission.")
    return 0


if __name__ == "__main__":
    sys.exit(main())
