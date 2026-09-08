#!/usr/bin/env python3
"""Produce the 256x256 JPEG icon embedded in the thWWW NRO.

By default a plain procedural placeholder is generated, because no artwork from
the game is shipped in this repository. Point THWWW_ICON_SOURCE (or --source) at
a local image you are allowed to use to embed your own cover instead.
"""
from io import BytesIO
import os
from pathlib import Path

BACKGROUND = (14, 18, 42)
ACCENT = (208, 74, 110)


def _placeholder():
    """A neutral procedural emblem: night-blue field with a crescent accent."""
    from PIL import Image, ImageDraw
    image = Image.new('RGB', (256, 256), BACKGROUND)
    draw = ImageDraw.Draw(image)
    for y in range(256):
        shade = int(18 + 46 * y / 255)
        draw.line([(0, y), (255, y)], fill=(shade // 2, shade // 2, shade + 24))
    draw.ellipse((70, 54, 186, 170), fill=ACCENT)
    draw.ellipse((100, 40, 216, 156), fill=(shade // 2, shade // 2, shade + 24))
    draw.text((22, 210), 'Wonderful Waking World', fill=(232, 232, 240))
    draw.text((22, 226), 'experimental open NRO', fill=(150, 156, 176))
    return image


def create_icon(path, source=None):
    from PIL import Image, ImageOps
    source = source or os.environ.get('THWWW_ICON_SOURCE')
    if source:
        with open(source, 'rb') as stream:
            data = stream.read(20 * 1024 * 1024 + 1)
        if len(data) > 20 * 1024 * 1024:
            raise RuntimeError('Cover exceeds the 20 MiB input limit')
        with Image.open(BytesIO(data)) as original:
            original.load()
            cover = ImageOps.exif_transpose(original).convert('RGBA')
        # Preserve the whole cover, including its title: pad, never crop/stretch.
        cover = ImageOps.contain(cover, (256, 256), Image.Resampling.LANCZOS)
        image = Image.new('RGB', (256, 256), BACKGROUND)
        image.paste(cover, ((256 - cover.width) // 2, (256 - cover.height) // 2), cover)
    else:
        image = _placeholder()
    path = Path(path)
    path.parent.mkdir(parents=True, exist_ok=True)
    image.save(path, 'JPEG', quality=94, progressive=False, subsampling=0)
    return path


if __name__ == '__main__':
    import argparse
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('output', type=Path)
    parser.add_argument('--source', help='Local image file to use as the cover')
    args = parser.parse_args()
    create_icon(args.output, args.source)
