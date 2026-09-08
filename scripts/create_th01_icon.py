#!/usr/bin/env python3
"""Create the 256x256 JPEG icon embedded in the th01 NRO.

The default emblem is drawn procedurally because no artwork from any game is
shipped in this repository. Point TH01_ICON_SOURCE (or --source) at a local
image you are allowed to use to embed your own cover instead; nothing is
downloaded.

Requires Pillow (the same dependency as the other icon scripts in this
repository):  apt-get install python3-pil
"""
from __future__ import annotations

import argparse
import os
from pathlib import Path

BACKGROUND = (14, 10, 34)
ACCENT = (220, 80, 110)
ACCENT2 = (150, 110, 255)
TEXT = (236, 236, 246)

TITLE = "TH01"
SUBTITLE = "open card-breaking port"
FOOTER = "bring your own data"


def _placeholder() -> "Image.Image":  # noqa: F821
    from PIL import Image, ImageDraw

    image = Image.new("RGB", (256, 256), BACKGROUND)
    draw = ImageDraw.Draw(image)

    for y in range(256):
        shade = 10 + int(38 * y / 255)
        draw.line([(0, y), (255, y)], fill=(shade, shade // 2, shade + 26))

    # yin-yang style orb drawn from primitives
    draw.ellipse((52, 40, 204, 192), fill=(248, 248, 252))
    draw.pieslice((52, 40, 204, 192), start=270, end=90, fill=(32, 24, 54))
    draw.ellipse((104, 70, 152, 118), fill=(32, 24, 54))
    draw.ellipse((104, 114, 152, 162), fill=(248, 248, 252))
    draw.ellipse((52, 40, 204, 192), outline=ACCENT, width=5)

    draw.text((18, 206), TITLE, fill=TEXT)
    draw.text((18, 224), SUBTITLE, fill=ACCENT2)
    draw.text((18, 240), FOOTER, fill=(150, 150, 180))
    return image


def create_icon(path: Path, source: str | None = None) -> dict:
    from PIL import Image, ImageOps

    path = Path(path)
    path.parent.mkdir(parents=True, exist_ok=True)

    source = source or os.environ.get("TH01_ICON_SOURCE")
    if source:
        data = Path(source).read_bytes()
        from io import BytesIO

        if len(data) > 20 * 1024 * 1024:
            raise RuntimeError("icon source exceeds the 20 MiB limit")
        with Image.open(BytesIO(data)) as original:
            original.load()
            cover = ImageOps.exif_transpose(original)
        cover = ImageOps.contain(cover.convert("RGB"), (256, 256), Image.Resampling.LANCZOS)
        image = Image.new("RGB", (256, 256), BACKGROUND)
        image.paste(cover, ((256 - cover.width) // 2, (256 - cover.height) // 2))
    else:
        image = _placeholder()

    image.save(path, "JPEG", quality=94, progressive=False, subsampling=0)
    return {"path": str(path), "size": image.size, "mode": image.mode,
            "source": "user" if source else "procedural"}


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("output", type=Path, nargs="?", default=Path("dist/th01/switch/th01-icon.jpg"))
    parser.add_argument("--source", help="local image you are allowed to use (no downloads)")
    args = parser.parse_args()

    info = create_icon(args.output, args.source)
    print(f"icon: {info['path']} ({info['size'][0]}x{info['size'][1]}, {info['mode']}, {info['source']})")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
