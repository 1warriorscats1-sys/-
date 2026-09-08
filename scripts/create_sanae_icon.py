#!/usr/bin/env python3
"""Draw an original SANAE wind emblem, not extracted game artwork (MIT).

Requires Pillow. The generated 256x256 baseline JPEG is embedded in the NRO;
no Steam assets, downloaded art or third-party fonts are used.
"""
from pathlib import Path

def create_icon(path):
    from PIL import Image, ImageDraw
    scale = 3
    image = Image.new('RGB', (256 * scale, 256 * scale))
    draw = ImageDraw.Draw(image)
    for y in range(256 * scale):
        t = y / (256 * scale - 1)
        draw.line((0, y, 256 * scale, y), fill=(int(13+10*t), int(48+48*t), int(58+28*t)))
    def line(points, colour, width):
        draw.line([(int(x*scale), int(y*scale)) for x,y in points], fill=colour, width=width*scale, joint='curve')
    def polygon(points, colour):
        draw.polygon([(int(x*scale), int(y*scale)) for x,y in points], fill=colour)
    mint = (125, 239, 189)
    white = (235, 255, 244)
    # Wind curls and a folded leaf, drawn with simple original geometry.
    draw.arc(tuple(v*scale for v in (41, 31, 213, 167)), 190, 352, fill=mint, width=7*scale)
    draw.arc(tuple(v*scale for v in (67, 49, 213, 153)), 340, 520, fill=white, width=5*scale)
    line([(43,132),(81,132),(100,123)], mint, 7)
    line([(35,153),(104,153),(134,138)], white, 4)
    polygon([(83,115),(174,57),(153,122),(111,133)], mint)
    polygon([(83,115),(174,57),(122,117)], white)
    line([(83,141),(122,117),(166,68)], (33,112,99), 3)
    for x,y,r in [(50,64,4),(210,174,3),(31,104,2)]:
        polygon([(x,y-r),(x+r,y),(x,y+r),(x-r,y)], white)
    # Original 5x7 letter patterns keep the icon independent of font files.
    glyphs = {
        'S': ['01111','10000','10000','01110','00001','00001','11110'],
        'A': ['01110','10001','10001','11111','10001','10001','10001'],
        'N': ['10001','11001','11001','10101','10011','10011','10001'],
        'E': ['11111','10000','10000','11110','10000','10000','11111'],
    }
    unit, left, top = 6, 41, 190
    for letter in 'SANAE':
        for row, bits in enumerate(glyphs[letter]):
            for column, bit in enumerate(bits):
                if bit == '1':
                    x,y = left+column*unit, top+row*unit
                    draw.rectangle((x*scale,y*scale,(x+unit)*scale-1,(y+unit)*scale-1), fill=white)
        left += unit*6
    image = image.resize((256,256), Image.Resampling.LANCZOS)
    path = Path(path); path.parent.mkdir(parents=True, exist_ok=True)
    image.save(path, 'JPEG', quality=94, progressive=False, subsampling=0)

if __name__ == '__main__':
    import argparse
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('output', type=Path)
    create_icon(parser.parse_args().output)
