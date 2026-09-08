#!/usr/bin/env python3
"""Turn host-build PPM frames into an animated GIF or a PNG contact sheet.

The host target of the TH01 port writes raw P6 PPM frames (see
highly-responsive-prayers/th01/main_host.c). This tool packs them into an
animated GIF so a build can be watched without a Switch, a windowing toolkit or
any third-party Python package: the GIF/LZW/PNG encoders below are implemented
with the standard library only.

Usage:
    python3 scripts/render_th01_preview.py --frames dist/th01/host --gif preview.gif
    python3 scripts/render_th01_preview.py --frames dist/th01/host --sheet sheet.png
    python3 scripts/render_th01_preview.py --selftest
"""
from __future__ import annotations

import argparse
import struct
import sys
import zlib
from pathlib import Path


# --------------------------------------------------------------------- GIF


class BitWriter:
    def __init__(self) -> None:
        self.data = bytearray()
        self.acc = 0
        self.bits = 0

    def write(self, code: int, width: int) -> None:
        self.acc |= code << self.bits
        self.bits += width
        while self.bits >= 8:
            self.data.append(self.acc & 0xFF)
            self.acc >>= 8
            self.bits -= 8

    def flush(self) -> bytes:
        if self.bits:
            self.data.append(self.acc & 0xFF)
            self.acc = 0
            self.bits = 0
        return bytes(self.data)


def lzw_encode(indices: list[int], min_code_size: int = 8) -> bytes:
    """GIF LZW with periodic dictionary resets.

    The code width is kept at min_code_size + 1 bits for the whole stream: the
    encoder emits a Clear code before the dictionary could ever need a wider
    code. That removes the usual encoder/decoder "which side grows the table
    first" ambiguity, at the cost of a slightly larger file.
    """
    clear_code = 1 << min_code_size
    end_code = clear_code + 1
    code_size = min_code_size + 1
    reset_at = (1 << code_size) - 12  # keep every emitted code inside the width

    writer = BitWriter()

    def reset_table() -> tuple[dict[tuple[int, ...], int], int]:
        return {tuple([i]): i for i in range(clear_code)}, end_code + 1

    table, next_code = reset_table()
    writer.write(clear_code, code_size)
    buffer: tuple[int, ...] = ()

    for pixel in indices:
        candidate = buffer + (pixel,)
        if candidate in table:
            buffer = candidate
            continue
        writer.write(table[buffer], code_size)
        if next_code < reset_at:
            table[candidate] = next_code
            next_code += 1
        else:
            writer.write(clear_code, code_size)
            table, next_code = reset_table()
        buffer = (pixel,)

    if buffer:
        writer.write(table[buffer], code_size)
    writer.write(end_code, code_size)
    return writer.flush()


def lzw_decode(data: bytes, min_code_size: int = 8) -> list[int]:
    """Reference decoder used by --selftest (and to validate real output)."""
    clear_code = 1 << min_code_size
    end_code = clear_code + 1

    def reset():
        return {i: (i,) for i in range(clear_code)}, end_code + 1

    table, next_code = reset()
    code_size = min_code_size + 1
    out: list[int] = []
    previous: tuple[int, ...] | None = None

    bit_pos = 0
    total_bits = len(data) * 8
    while bit_pos + code_size <= total_bits:
        byte = bit_pos // 8
        offset = bit_pos % 8
        chunk = int.from_bytes(data[byte:byte + 3].ljust(3, b"\0"), "little")
        code = (chunk >> offset) & ((1 << code_size) - 1)
        bit_pos += code_size

        if code == clear_code:
            table, next_code = reset()
            code_size = min_code_size + 1
            previous = None
            continue
        if code == end_code:
            break
        if code in table:
            entry = table[code]
        elif previous is not None:
            entry = previous + (previous[0],)
        else:
            raise ValueError("corrupt LZW stream")

        out.extend(entry)
        if previous is not None and next_code < 4096:
            table[next_code] = previous + (entry[0],)
            next_code += 1
            if next_code > (1 << code_size) - 1 and code_size < 12:
                code_size += 1
        previous = entry
    return out


def build_palette(frames: list[tuple[int, int, bytes]], max_colors: int = 255) -> tuple[list[tuple[int, int, int]], dict[bytes, int]]:
    """Quantise to a 6x7x6 cube; the frames use few distinct colours anyway."""
    counts: dict[bytes, int] = {}
    for _, _, pixels in frames:
        for i in range(0, len(pixels), 3):
            key = pixels[i:i + 3]
            counts[key] = counts.get(key, 0) + 1

    def quantise(rgb: bytes) -> bytes:
        r, g, b = rgb[0], rgb[1], rgb[2]
        return bytes(((r * 5 + 127) // 255 * 51, (g * 6 + 127) // 255 * 42, (b * 5 + 127) // 255 * 51))

    buckets: dict[bytes, int] = {}
    for colour, count in counts.items():
        q = quantise(colour)
        buckets[q] = buckets.get(q, 0) + count

    ordered = sorted(buckets.items(), key=lambda item: -item[1])[:max_colors]
    palette = [tuple(entry[0]) for entry in ordered]  # type: ignore[misc]
    lookup: dict[bytes, int] = {}
    for index, colour in enumerate(palette):
        lookup[bytes(colour)] = index
    quantised: dict[bytes, int] = {}
    for colour in counts:
        quantised[colour] = lookup[quantise(colour)]
    while len(palette) < 2:
        palette.append((0, 0, 0))
    return palette, quantised


def write_gif(path: Path, frames: list[tuple[int, int, bytes]], delay_cs: int = 8) -> None:
    if not frames:
        raise SystemExit("no frames to write")
    width, height, _ = frames[0]
    palette, quantised = build_palette(frames)

    # pad the palette to a power of two (GIF wants 2..256 entries)
    size = 2
    while size < len(palette):
        size *= 2
    palette += [(0, 0, 0)] * (size - len(palette))

    out = bytearray()
    out += b"GIF89a"
    out += struct.pack("<HHBBB", width, height, 0xF0 | 0x07, 0, 0)  # 256-entry GCT
    for colour in palette:
        out += bytes(colour)
    # Netscape looping extension
    out += b"\x21\xFF\x0BNETSCAPE2.0\x03\x01\x00\x00\x00"

    for _, _, pixels in frames:
        out += b"\x21\xF9\x04\x00" + struct.pack("<H", delay_cs) + b"\x00\x00"  # GCE
        out += b"\x2C" + struct.pack("<HHHHB", 0, 0, width, height, 0)
        indices = [quantised[pixels[i:i + 3]] for i in range(0, len(pixels), 3)]
        data = lzw_encode(indices, 8)
        out += b"\x08"
        for start in range(0, len(data), 255):
            block = data[start:start + 255]
            out += bytes([len(block)]) + block
        out += b"\x00"
    out += b"\x3B"
    path.write_bytes(bytes(out))


# --------------------------------------------------------------------- PNG


def write_png(path: Path, width: int, height: int, rgb: bytes, scale: int = 1) -> None:
    if scale > 1:
        scaled = bytearray()
        stride = width * 3
        for y in range(height):
            row = rgb[y * stride:(y + 1) * stride]
            expanded = bytearray()
            for x in range(width):
                expanded += row[x * 3:x * 3 + 3] * scale
            for _ in range(scale):
                scaled += expanded
        width *= scale
        height *= scale
        rgb = bytes(scaled)

    raw = bytearray()
    stride = width * 3
    for y in range(height):
        raw.append(0)
        raw += rgb[y * stride:(y + 1) * stride]

    def chunk(tag: bytes, payload: bytes) -> bytes:
        return (struct.pack(">I", len(payload)) + tag + payload +
                struct.pack(">I", zlib.crc32(tag + payload) & 0xFFFFFFFF))

    png = b"\x89PNG\r\n\x1a\n"
    png += chunk(b"IHDR", struct.pack(">IIBBBBB", width, height, 8, 2, 0, 0, 0))
    png += chunk(b"IDAT", zlib.compress(bytes(raw), 9))
    png += chunk(b"IEND", b"")
    path.write_bytes(png)


def write_sheet(path: Path, frames: list[tuple[int, int, bytes]], columns: int, scale: int) -> None:
    if not frames:
        raise SystemExit("no frames to write")
    width, height, _ = frames[0]
    rows = (len(frames) + columns - 1) // columns
    canvas = bytearray(b"\x10\x08\x20" * (width * columns * height * rows))
    for index, (_, _, pixels) in enumerate(frames):
        cx = (index % columns) * width
        cy = (index // columns) * height
        for y in range(height):
            src = pixels[y * width * 3:(y + 1) * width * 3]
            dst = (cy + y) * width * columns * 3 + cx * 3
            canvas[dst:dst + len(src)] = src
    write_png(path, width * columns, height * rows, bytes(canvas), scale)


# ------------------------------------------------------------------ frames


def read_ppm(path: Path) -> tuple[int, int, bytes]:
    data = path.read_bytes()
    if not data.startswith(b"P6"):
        raise SystemExit(f"{path}: not a binary PPM (P6) file")
    pos = 2
    values = []
    while len(values) < 3:
        while data[pos:pos + 1].isspace():
            pos += 1
        if data[pos:pos + 1] == b"#":
            while data[pos:pos + 1] not in (b"\n", b""):
                pos += 1
            continue
        start = pos
        while data[pos:pos + 1].isdigit():
            pos += 1
        values.append(int(data[start:pos]))
    pos += 1  # single whitespace after the header
    width, height, maxval = values
    if maxval != 255:
        raise SystemExit(f"{path}: unsupported maxval {maxval}")
    pixels = data[pos:pos + width * height * 3]
    if len(pixels) != width * height * 3:
        raise SystemExit(f"{path}: truncated frame")
    return width, height, pixels


def load_frames(directory: Path, step: int, limit: int) -> list[tuple[int, int, bytes]]:
    files = sorted(directory.glob("frame_*.ppm"))
    if not files:
        raise SystemExit(f"no frame_*.ppm files in {directory}")
    files = files[::step]
    if limit > 0:
        files = files[:limit]
    return [read_ppm(path) for path in files]


def selftest() -> int:
    import random

    rng = random.Random(20260908)
    for trial in range(20):
        indices = [rng.randrange(256) for _ in range(rng.randrange(1, 4000))]
        encoded = lzw_encode(indices, 8)
        if lzw_decode(encoded, 8) != indices:
            print(f"selftest: FAIL at trial {trial}")
            return 1
    print("selftest: LZW round-trip OK (20 trials)")
    return 0


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__,
                                     formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--frames", type=Path, help="directory with frame_*.ppm files")
    parser.add_argument("--gif", type=Path, help="write an animated GIF")
    parser.add_argument("--sheet", type=Path, help="write a PNG contact sheet")
    parser.add_argument("--columns", type=int, default=4)
    parser.add_argument("--scale", type=int, default=1)
    parser.add_argument("--step", type=int, default=1, help="use every Nth frame")
    parser.add_argument("--limit", type=int, default=0)
    parser.add_argument("--delay", type=int, default=8, help="GIF frame delay in 1/100 s")
    parser.add_argument("--selftest", action="store_true", help="verify the LZW codec")
    args = parser.parse_args()

    if args.selftest:
        return selftest()
    if not args.frames or (not args.gif and not args.sheet):
        parser.error("--frames and at least one of --gif/--sheet are required")

    frames = load_frames(args.frames, args.step, args.limit)
    print(f"loaded {len(frames)} frames ({frames[0][0]}x{frames[0][1]})")

    if args.gif:
        write_gif(args.gif, frames, args.delay)
        print(f"wrote {args.gif}")
    if args.sheet:
        write_sheet(args.sheet, frames, max(1, args.columns), max(1, args.scale))
        print(f"wrote {args.sheet}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
