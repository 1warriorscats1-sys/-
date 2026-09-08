#!/usr/bin/env python3
"""Validate the actual NRO's embedded icon and NACP, not just CMake arguments."""
import io
from pathlib import Path
import struct


def verify(path):
    from PIL import Image
    data = Path(path).read_bytes()
    if data[0x10:0x14] != b'NRO0':
        raise RuntimeError('Not an NRO')
    size, = struct.unpack_from('<I', data, 0x18)
    if data[size:size+4] != b'ASET':
        raise RuntimeError('NRO is missing embedded assets')
    _, version, icon_offset, icon_size, nacp_offset, nacp_size, _, _ = struct.unpack_from('<IIQQQQQQ', data, size)
    if version != 0 or icon_offset < 56 or not icon_size or nacp_offset < 56 or nacp_size != 0x4000:
        raise RuntimeError('Invalid NRO asset table')
    if size + icon_offset + icon_size > len(data) or size + nacp_offset + nacp_size > len(data):
        raise RuntimeError('Truncated NRO assets')
    icon = data[size+icon_offset:size+icon_offset+icon_size]
    with Image.open(io.BytesIO(icon)) as image:
        image.load()
        if image.format != 'JPEG' or image.size != (256,256) or image.mode != 'RGB':
            raise RuntimeError('NRO icon is not a 256x256 RGB JPEG')
    nacp = data[size+nacp_offset:size+nacp_offset+nacp_size]
    author = nacp[0x200:0x300].split(b'\0',1)[0].decode('utf-8')
    display_version = nacp[0x3060:0x3070].split(b'\0',1)[0].decode('utf-8')
    title = nacp[:0x200].split(b'\0',1)[0].decode('utf-8')
    if author != 'sorehodoh' or display_version != '01.01' or title != 'SANAE - Sylphid Breeze':
        raise RuntimeError(f'Unexpected NACP: {title!r} / {author!r} / {display_version!r}')
    return {'icon_bytes': icon_size, 'author': author, 'version': display_version,
            'title': title, 'module_id': data[0x40:0x60].hex().upper()}

if __name__ == '__main__':
    import argparse, json
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('nro', type=Path)
    print(json.dumps(verify(parser.parse_args().nro), indent=2))
