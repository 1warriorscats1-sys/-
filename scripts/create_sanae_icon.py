#!/usr/bin/env python3
"""Convert the user-requested SANAE cover into a baseline 256x256 NRO JPEG.

The converter is MIT; the cover is third-party artwork, not MIT/AGPL artwork.
A local cover can be supplied with SANAE_ICON_SOURCE (or the source argument).
Never silently substitute a different cover or the old procedural emblem.
"""
from io import BytesIO
import os
from pathlib import Path
from urllib.request import Request, urlopen

COVER_URL = ('https://lambda.vgmtreasurechest.com/soundtracks/'
             'sanaes-sylphid-breeze-unofficial-soundtrack-windows-gamerip-2025/cover.png')
MAX_BYTES = 20 * 1024 * 1024


def create_icon(path, source=None):
    from PIL import Image, ImageOps
    source = str(source or os.environ.get('SANAE_ICON_SOURCE') or COVER_URL)
    if source.startswith('https://'):
        request = Request(source, headers={'User-Agent': 'SANAE-icon-converter/01.01'})
        with urlopen(request, timeout=60) as response:
            data = response.read(MAX_BYTES + 1)
    else:
        with open(source, 'rb') as stream:
            data = stream.read(MAX_BYTES + 1)
    if len(data) > MAX_BYTES:
        raise RuntimeError('Cover exceeds the 20 MiB input limit')
    with Image.open(BytesIO(data)) as original:
        original.load()
        cover = ImageOps.exif_transpose(original).convert('RGBA')
    # Preserve the whole cover, including its title. Pad rather than crop/stretch.
    cover = ImageOps.contain(cover, (256, 256), Image.Resampling.LANCZOS)
    image = Image.new('RGB', (256, 256), (15, 25, 29))
    image.paste(cover, ((256-cover.width)//2, (256-cover.height)//2), cover)
    path = Path(path)
    path.parent.mkdir(parents=True, exist_ok=True)
    image.save(path, 'JPEG', quality=94, progressive=False, subsampling=0)


if __name__ == '__main__':
    import argparse
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('output', type=Path)
    parser.add_argument('--source', help='Local cover file or HTTPS URL')
    args = parser.parse_args()
    create_icon(args.output, args.source)
