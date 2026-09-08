#!/usr/bin/env python3
"""Pack (and inspect) an open TH01 data package - the "bring your own data" path.

The engine reads a simple, documented container instead of any game's files, so
nothing copyrighted is stored in this repository. Convert your own content into
this format with this script, drop the result on the SD card as

    sdmc:/switch/th01/th01open.dat

and the port uses it. The format is specified in docs/TH01_DATA_FORMAT.md.

Standard library only.

Usage:
    python3 scripts/pack_th01_data.py --emit-example my-content   # starting point
    python3 scripts/pack_th01_data.py --source my-content --output th01open.dat
    python3 scripts/pack_th01_data.py --verify th01open.dat       # reference parser
"""
from __future__ import annotations

import argparse
import json
import struct
import sys
from pathlib import Path

MAGIC = b"TH01OPEN"
VERSION = 1

SEC_STAGES = 1
SEC_PALETTE = 2
SEC_TEXT = 3
SEC_MUSIC = 4

STAGE_NAME = 24
MAX_CARDS = 160

CELL_TO_KIND = {".": 0, "#": 1, "T": 2, "S": 3, "P": 4}
KIND_TO_CELL = {value: key for key, value in CELL_TO_KIND.items()}

BOSS_PATTERNS = {"sweep": 0, "orbit": 1, "stream": 2}


class PackError(RuntimeError):
    pass


# ------------------------------------------------------------------- writer


def _section(kind: int, payload: bytes) -> bytes:
    out = struct.pack("<II", kind, len(payload)) + payload
    pad = (-len(payload)) % 4
    return out + b"\0" * pad


def _stage_payload(stages: list[dict]) -> bytes:
    body = struct.pack("<H", len(stages))
    for stage in stages:
        kind = stage.get("type", "card")
        name = str(stage.get("name", ""))[: STAGE_NAME - 1]
        if kind == "boss":
            hp = int(stage.get("boss_hp", 900))
            if hp <= 0 or hp > 0xFFFF:
                raise PackError(f"boss_hp must be 1..65535 in stage {name!r}")
            pattern = BOSS_PATTERNS.get(str(stage.get("boss_pattern", "sweep")).lower(), 0)
            time_limit = int(stage.get("time_limit", 3600))
            if not 0 <= time_limit <= 0xFFFF:
                raise PackError(f"time_limit out of range in stage {name!r}")
            body += struct.pack("<BBBBBHHB", 1, 0, 0, int(stage.get("orb_speed", 20)),
                                pattern, hp, time_limit, 0)
            body += name.encode("utf-8").ljust(STAGE_NAME, b"\0")
            continue

        rows = stage.get("rows")
        cols = stage.get("cols")
        if rows is None or cols is None:
            rows_list = stage.get("cells") or []
            rows = len(rows_list)
            cols = max((len(row) for row in rows_list), default=0)
        rows, cols = int(rows), int(cols)
        if not 1 <= rows * cols <= MAX_CARDS:
            raise PackError(f"stage {name!r}: rows*cols must be 1..{MAX_CARDS}")

        cells = bytearray(rows * cols)
        grid = stage.get("cells")
        if grid is not None:
            if len(grid) != rows:
                raise PackError(f"stage {name!r}: cells has {len(grid)} rows, expected {rows}")
            for r, row in enumerate(grid):
                row = str(row)
                if len(row) != cols:
                    raise PackError(f"stage {name!r}: row {r} has width {len(row)}, expected {cols}")
                for c, char in enumerate(row):
                    if char not in CELL_TO_KIND:
                        raise PackError(f"stage {name!r}: unknown cell {char!r} at {r},{c}")
                    cells[r * cols + c] = CELL_TO_KIND[char]
        else:
            cells = bytearray([CELL_TO_KIND["#"]]) * (rows * cols)

        body += struct.pack("<BBBBBHHB", 0, rows, cols, int(stage.get("orb_speed", 20)),
                            0, 0, int(stage.get("time_limit", 0)), 0)
        body += name.encode("utf-8").ljust(STAGE_NAME, b"\0")
        body += bytes(cells)
    return body


def _palette_payload(palette: list[str]) -> bytes:
    body = struct.pack("<H", len(palette))
    for entry in palette:
        text = str(entry).lstrip("#")
        if len(text) != 6:
            raise PackError(f"palette entries must be #rrggbb, got {entry!r}")
        r, g, b = (int(text[i:i + 2], 16) for i in (0, 2, 4))
        body += struct.pack("<I", (0xFF << 24) | (b << 16) | (g << 8) | r)
    return body


def _text_payload(lines: list[str]) -> bytes:
    body = b""
    for line in lines:
        raw = line.encode("utf-8")
        if len(raw) > 255:
            raise PackError(f"text line too long ({len(raw)} bytes): {line!r}")
        body += bytes([len(raw)]) + raw
    return body


def build_package(content: dict) -> bytes:
    stages = content.get("stages") or []
    if not stages:
        raise PackError("a package needs at least one stage")
    if len(stages) > 20:
        raise PackError("a package can hold at most 20 stages")

    sections = [_section(SEC_STAGES, _stage_payload(stages))]
    palette = content.get("palette")
    if palette:
        sections.append(_section(SEC_PALETTE, _palette_payload(palette)))
    lines = [f"TITLE={content.get('title', 'th01 open content')}"]
    for extra in content.get("text") or []:
        lines.append(str(extra))
    sections.append(_section(SEC_TEXT, _text_payload(lines)))

    header = MAGIC + struct.pack("<HHI", VERSION, len(sections), 0)
    return header + b"".join(sections)


# ------------------------------------------------------------------- reader


def parse_package(data: bytes) -> dict:
    """Reference parser used by the tests and by --verify."""
    if len(data) < 16:
        raise PackError("package is shorter than the 16 byte header")
    if data[:8] != MAGIC:
        raise PackError("bad magic (expected TH01OPEN)")
    version, count = struct.unpack_from("<HH", data, 8)
    if version != VERSION:
        raise PackError(f"unsupported version {version}")

    offset = 16
    result: dict = {"version": version, "sections": [], "stages": [], "title": None}
    for _ in range(count):
        if offset + 8 > len(data):
            raise PackError("truncated section header")
        kind, length = struct.unpack_from("<II", data, offset)
        offset += 8
        if offset + length > len(data):
            raise PackError("truncated section payload")
        payload = data[offset:offset + length]
        offset += length + ((-length) % 4)

        if kind == SEC_STAGES:
            (stage_count,) = struct.unpack_from("<H", payload, 0)
            pos = 2
            for _ in range(stage_count):
                if pos + 10 > len(payload):
                    raise PackError("truncated stage record")
                (stype, rows, cols, orb_speed, pattern, hp,
                 time_limit, _reserved) = struct.unpack_from("<BBBBBHHB", payload, pos)
                pos += 10
                raw_name = payload[pos:pos + STAGE_NAME]
                pos += STAGE_NAME
                name = raw_name.split(b"\0", 1)[0].decode("utf-8", "replace")
                cells = []
                if stype == 0:
                    cells = list(payload[pos:pos + rows * cols])
                    pos += rows * cols
                result["stages"].append({
                    "type": "boss" if stype else "card",
                    "name": name,
                    "rows": rows,
                    "cols": cols,
                    "orb_speed": orb_speed,
                    "boss_pattern": pattern,
                    "boss_hp": hp,
                    "time_limit": time_limit,
                    "cells": cells,
                })
        elif kind == SEC_PALETTE:
            (colours,) = struct.unpack_from("<H", payload, 0)
            entries = []
            for i in range(colours):
                if 2 + i * 4 + 4 > len(payload):
                    raise PackError("truncated palette")
                entries.append(struct.unpack_from("<I", payload, 2 + i * 4)[0])
            result["palette"] = entries
        elif kind == SEC_TEXT:
            pos = 0
            while pos < len(payload):
                length = payload[pos]
                pos += 1
                line = payload[pos:pos + length].decode("utf-8", "replace")
                pos += length
                if line.startswith("TITLE=") and result["title"] is None:
                    result["title"] = line[len("TITLE="):]
        result["sections"].append(kind)
    return result


# ------------------------------------------------------------------ example


EXAMPLE = {
    "title": "th01 open example set",
    "stages": [
        {
            "type": "card",
            "name": "SHRINE GATE",
            "rows": 5,
            "cols": 12,
            "orb_speed": 20,
            "cells": [
                "SS..####..SS",
                "TT########TT",
                "#####PP#####",
                "############",
                "..########..",
            ],
        },
        {
            "type": "card",
            "name": "HALL OF CARDS",
            "rows": 5,
            "cols": 12,
            "orb_speed": 22,
            "cells": [
                "############",
                ".##########.",
                "..T######T..",
                "...######...",
                "....#PP#....",
            ],
        },
        {
            "type": "boss",
            "name": "KEEPER OF THE GATE",
            "boss_hp": 900,
            "boss_pattern": "sweep",
            "time_limit": 3600,
        },
    ],
    "palette": ["#dc506e", "#b4551e", "#5adc96", "#ffffff"],
    "text": ["NOTE=edit stages here and repack with pack_th01_data.py"],
}


def emit_example(directory: Path) -> Path:
    directory = Path(directory)
    directory.mkdir(parents=True, exist_ok=True)
    path = directory / "content.json"
    path.write_text(json.dumps(EXAMPLE, indent=2) + "\n", encoding="utf-8")
    return path


# --------------------------------------------------------------------- cli


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__,
                                     formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--source", type=Path, help="directory containing content.json")
    parser.add_argument("--content", type=Path, help="a content.json file instead of a directory")
    parser.add_argument("--output", type=Path, default=Path("th01open.dat"))
    parser.add_argument("--verify", type=Path, help="parse an existing package and print a summary")
    parser.add_argument("--emit-example", type=Path, help="write an example content.json")
    parser.add_argument("--json", action="store_true")
    args = parser.parse_args()

    if args.emit_example:
        path = emit_example(args.emit_example)
        print(f"example content written to {path}")
        return 0

    if args.verify:
        info = parse_package(args.verify.read_bytes())
        if args.json:
            print(json.dumps(info, indent=2))
        else:
            print(f"{args.verify}: version {info['version']}, sections {info['sections']}, "
                  f"title {info['title']!r}, {len(info['stages'])} stages")
            for stage in info["stages"]:
                print(f"  - {stage['name']} ({stage['type']}, "
                      f"{stage['rows']}x{stage['cols']}, hp={stage['boss_hp']})")
        return 0

    content_file = args.content or (args.source / "content.json" if args.source else None)
    if not content_file or not Path(content_file).exists():
        parser.error("provide --source DIR, --content FILE, --verify FILE or --emit-example DIR")

    content = json.loads(Path(content_file).read_text(encoding="utf-8"))
    data = build_package(content)
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_bytes(data)

    # round-trip through the reference parser before declaring success
    info = parse_package(data)
    print(f"wrote {args.output} ({len(data)} bytes, {len(info['stages'])} stages, "
          f"title {info['title']!r})")
    return 0


if __name__ == "__main__":
    try:
        sys.exit(main())
    except PackError as error:
        print(f"error: {error}", file=sys.stderr)
        sys.exit(1)
