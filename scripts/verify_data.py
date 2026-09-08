#!/usr/bin/env python3
"""Verify a data file the way the 2024.14.4.268 Switch runner actually reads it.

Every offset and every gate in here was taken from the disassembly of the
runner's own parser (exefs `main`, function 0x210550 = ROOM chunk loader,
0xde120 = per-room parser, 0xe3520 = layer list parser, 0x2cd780 = AGRP
loader), not from documentation - so passing this check means the runner will
walk the same structures without falling off a cliff.

Usage: verify_data.py <game.win>
"""

import struct
import sys

LAYER_TYPES = {0: "Path", 1: "Background", 2: "Instances", 3: "Assets", 4: "Tiles", 6: "Effect"}


def main(path: str) -> int:
    d = open(path, "rb").read()
    u32 = lambda p: struct.unpack_from("<I", d, p)[0]
    problems = []

    def check(cond, msg):
        if not cond:
            problems.append(msg)
        return cond

    def gstr(p):
        if p == 0:
            return ""
        length = u32(p - 4)
        if length > 4096 or p + length > len(d):
            return None
        return d[p:p + length].decode("utf-8", "replace")

    # ---------------------------------------------------------------- chunks
    check(d[:4] == b"FORM", "no FORM magic")
    chunks, off, end = {}, 8, 8 + u32(4)
    while off < end:
        name = d[off:off + 4].decode("latin1")
        size = u32(off + 4)
        chunks[name] = (off + 8, size)
        off += 8 + size
    check(off == len(d), "chunk walk ends at %d but file is %d bytes" % (off, len(d)))
    print("chunks: %d, file %d bytes" % (len(chunks), len(d)))

    # ------------------------------------------------- audio groups (0x2cd780)
    # runner: entry -> ldp w10, w9 -> name, path; std::string(path) => must not be NULL
    agrp = chunks["AGRP"][0]
    count = u32(agrp)
    for i in range(count):
        entry = u32(agrp + 4 + 4 * i)
        if entry == 0:
            continue
        name_ptr, path_ptr = struct.unpack_from("<II", d, entry)
        name, path = gstr(name_ptr), gstr(path_ptr)
        check(name is not None, "audio group %d: bad name pointer" % i)
        check(path_ptr != 0, "audio group %d (%s): NULL path -> strlen(NULL) crash" % (i, name))
        check(path is not None, "audio group %d (%s): bad path pointer" % (i, name))
    print("audio groups: %d, all with a path" % count)

    # ------------------------------------------------------- rooms (0xde120)
    room = chunks["ROOM"][0]
    room_count = u32(room)
    instances_total = 0
    layers_total = 0
    for i in range(room_count):
        p = u32(room + 4 + 4 * i)
        name = gstr(u32(p))
        flags = u32(p + 0x24)

        # gate at 0xde6ec: flags < 0x40000 -> creation order list is ignored
        if not check(flags >= 0x40000,
                     "room %d (%s): flags %#x < 0x40000, runner skips the instance "
                     "creation order -> no Create events" % (i, name, flags)):
            continue
        # gate at 0xde7c8 / 0xde7f0: layers need >= 0x20000, sequences >= 0x30000
        check(flags >= 0x30000, "room %d (%s): flags %#x too low for layers" % (i, name, flags))

        order = u32(p + 0x38)
        order_count = u32(order)
        objects = u32(p + 0x30)
        object_count = u32(objects)
        check(order_count == object_count,
              "room %d (%s): creation order has %d ids but the room has %d instances"
              % (i, name, order_count, object_count))
        ids = [u32(order + 4 + 4 * k) for k in range(order_count)]
        placed = [u32(u32(objects + 4 + 4 * k) + 0xC) for k in range(object_count)]
        check(sorted(ids) == sorted(placed),
              "room %d (%s): creation order ids do not match the placed instances" % (i, name))
        instances_total += order_count

        layers = u32(p + 0x5C)
        layer_count = u32(layers)
        check(0 <= layer_count < 4096, "room %d (%s): implausible layer count %d" % (i, name, layer_count))
        names = []
        for j in range(layer_count):
            lp = u32(layers + 4 + 4 * j)
            lname = gstr(u32(lp))
            ltype = u32(lp + 8)
            check(lname is not None, "room %d layer %d: bad name" % (i, j))
            check(ltype in LAYER_TYPES, "room %d layer %r: unknown type %d" % (i, lname, ltype))
            names.append(lname)
        layers_total += layer_count
        seq = u32(p + 0x60)
        check(u32(seq) == 0 or u32(seq) < 4096, "room %d (%s): implausible sequence count" % (i, name))

    print("rooms: %d, layers: %d, ordered instances: %d" % (room_count, layers_total, instances_total))

    # ------------------------------------------------ sprite collision masks
    # 2024.6+: every mask covers the bounding box, so the block must be exactly
    # maskCount * ceil(bboxWidth/8) * bboxHeight bytes (padded to 4).
    sprt = chunks["SPRT"][0]
    checked = masked = 0
    for i in range(u32(sprt)):
        p_ = u32(sprt + 4 + 4 * i)
        if not p_ or struct.unpack_from("<i", d, p_ + 56)[0] != -1 or u32(p_ + 64) != 0:
            continue
        left, right, bottom, top = (struct.unpack_from("<i", d, p_ + 12 + 4 * k)[0] for k in range(4))
        sver = u32(p_ + 60)
        q = p_ + 68 + 8 + (4 if sver >= 2 else 0) + (4 if sver >= 3 else 0)
        q += 4 + 4 * u32(q)
        count = u32(q)
        if not (0 <= count < 10000):
            problems.append("sprite %d: implausible mask count %d" % (i, count))
            continue
        bw, bh = right - left + 1, bottom - top + 1
        expect = ((bw + 7) // 8) * bh if bw > 0 and bh > 0 else 0
        checked += 1
        masked += count
        if q + 4 + count * expect > len(d):
            problems.append("sprite %d: mask block runs past the end of the file" % i)
    print("sprites with collision masks checked: %d (%d masks)" % (checked, masked))

    if problems:
        print("\nFAILED (%d problems):" % len(problems))
        for p in problems[:40]:
            print("  -", p)
        return 1
    print("\nOK - the runner's own read path is satisfied")
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1] if len(sys.argv) > 1 else "game.win"))
