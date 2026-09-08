#!/usr/bin/env python3
"""Validate the real th01 NRO: magic, embedded assets, icon and NACP strings.

Checks the artefact itself, not the build arguments that produced it.
"""
from __future__ import annotations

import argparse
import io
import json
import struct
from pathlib import Path

EXPECTED_TITLE = "TH01 open port"
EXPECTED_AUTHOR = "1warriorscats1-sys/-"
EXPECTED_VERSION = "01.01"


def verify(path: Path) -> dict:
    from PIL import Image

    data = Path(path).read_bytes()
    if data[0x10:0x14] != b"NRO0":
        raise RuntimeError("not an NRO (missing NRO0 magic at 0x10)")
    (size,) = struct.unpack_from("<I", data, 0x18)
    if size + 4 > len(data):
        raise RuntimeError("NRO size field points outside the file")
    if data[size:size + 4] != b"ASET":
        raise RuntimeError("NRO is missing the embedded asset table")

    (_, version, icon_offset, icon_size, nacp_offset, nacp_size, romfs_offset,
     romfs_size) = struct.unpack_from("<IIQQQQQQ", data, size)
    if version != 0:
        raise RuntimeError(f"unexpected asset table version {version}")
    if icon_size == 0 or icon_offset < 56:
        raise RuntimeError("NRO has no icon")
    if nacp_size < 0x4000 or nacp_offset < 56:
        raise RuntimeError("NRO has no control NACP")
    if size + icon_offset + icon_size > len(data) or size + nacp_offset + nacp_size > len(data):
        raise RuntimeError("truncated NRO assets")

    icon = data[size + icon_offset:size + icon_offset + icon_size]
    with Image.open(io.BytesIO(icon)) as image:
        image.load()
        if image.format != "JPEG" or image.size != (256, 256) or image.mode != "RGB":
            raise RuntimeError("NRO icon is not a 256x256 RGB JPEG")

    nacp = data[size + nacp_offset:size + nacp_offset + nacp_size]
    title = nacp[:0x200].split(b"\0", 1)[0].decode("utf-8", "replace")
    author = nacp[0x200:0x300].split(b"\0", 1)[0].decode("utf-8", "replace")
    display_version = nacp[0x3060:0x3070].split(b"\0", 1)[0].decode("utf-8", "replace")

    problems = []
    if title != EXPECTED_TITLE:
        problems.append(f"title {title!r} != {EXPECTED_TITLE!r}")
    if author != EXPECTED_AUTHOR:
        problems.append(f"author {author!r} != {EXPECTED_AUTHOR!r}")
    if display_version != EXPECTED_VERSION:
        problems.append(f"version {display_version!r} != {EXPECTED_VERSION!r}")
    if problems:
        raise RuntimeError("unexpected NACP: " + "; ".join(problems))

    return {
        "nro": str(path),
        "bytes": len(data),
        "icon_bytes": icon_size,
        "title": title,
        "author": author,
        "version": display_version,
        "romfs_bytes": romfs_size,
        "module_id": data[0x40:0x60].hex().upper(),
    }


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("nro", type=Path)
    parser.add_argument("--json", action="store_true")
    args = parser.parse_args()

    info = verify(args.nro)
    print(json.dumps(info, indent=2) if args.json else
          "NRO OK: {bytes} bytes, icon {icon_bytes} bytes, {title} / {author} {version}".format(**info))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
