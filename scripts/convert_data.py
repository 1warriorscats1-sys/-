#!/usr/bin/env python3
"""Convert a GameMaker 2024.11/2024.12 data file to the 2024.14 layout.

SANAE's Sylphid Breeze ships a data.win built with GameMaker 2024.11, but the
only Switch runner available (and the only one new enough to run the game at
all) is 2024.14.4.268.  Two things changed in the data format in between:

  * 2024.13 - every room gained an `InstanceCreationOrderIDs` pointer, inserted
    after the `Tiles` pointer.  Without it the runner reads the room's layer
    list pointer 4 bytes too late, so no layer exists and the first
    `instance_create_layer` call kills the game.
  * 2024.14 - every audio group gained a `Path` string (the file name of the
    external audiogroup archive).  Without it the runner reads the *next*
    group's name as a path, and NULL for the last one -> crash in strlen().

Both are pointer-referenced structures, so instead of rewriting the whole file
(which would mean relocating every absolute offset in it) the converted
structures are appended at the end of the file and only the two pointer arrays
are repointed.  Nothing else moves.

Usage: convert_data.py <in.win> <out.win>
"""

import struct
import sys

ROOM_HEADER_OLD = 0x60          # name .. sequences pointer, pre-2024.13
INSTANCE_ORDER_OFFSET = 0x38    # inserted right after the tiles pointer
ROOM_FLAGS_OFFSET = 0x24
ROOM_FLAGS_LEVEL_MASK = 0xFFFF0000
ROOM_FLAGS_LEVEL_2024_13 = 0x40000   # runner: "cmp flags, #0x40000 / b.lt skip"


def main(src_path: str, dst_path: str) -> int:
    data = bytearray(open(src_path, "rb").read())
    if data[:4] != b"FORM":
        raise SystemExit("not a GameMaker data file")

    def u32(pos: int) -> int:
        return struct.unpack_from("<I", data, pos)[0]

    # ---------------------------------------------------------------- chunks
    chunks = {}
    order = []
    off, end = 8, 8 + u32(4)
    while off < end:
        name = data[off:off + 4].decode("latin1")
        size = u32(off + 4)
        chunks[name] = (off, off + 8, size)      # header, content, size
        order.append(name)
        off += 8 + size
    last = order[-1]
    print("chunks: %d, last: %s, file: %d bytes" % (len(order), last, len(data)))

    if len(data) != end:
        raise SystemExit("trailing data after FORM, refusing to convert")

    # appended data goes into the tail of the last chunk (pointer-indexed, so
    # extra bytes at the end are never looked at by the runner)
    append = bytearray()
    base = len(data)

    def emit(blob: bytes) -> int:
        """Append blob (4-byte aligned) and return its absolute file offset."""
        while len(append) % 4:
            append.append(0)
        pos = base + len(append)
        append.extend(blob)
        return pos

    def emit_string(text: str) -> int:
        """Append a GameMaker string, return the pointer to its characters."""
        raw = text.encode("utf-8")
        pos = emit(struct.pack("<I", len(raw)) + raw + b"\0")
        return pos + 4

    # ------------------------------------------------- what is already there
    room_probe = u32(chunks["ROOM"][1] + 4)
    rooms_done = u32(room_probe + ROOM_FLAGS_OFFSET) >= ROOM_FLAGS_LEVEL_2024_13
    agrp_base = chunks["AGRP"][1]
    agrp_done = (u32(agrp_base + 8) - u32(agrp_base + 4)) != 4 if u32(agrp_base) >= 2 else False
    _s = chunks["SOND"][1]
    _p = [u32(_s + 4 + 4 * i) for i in range(u32(_s))]
    _real = [x for x in _p if x]
    pre_2024_6 = len(_real) >= 2 and (_real[1] - _real[0]) < 40
    print("already in 2024.14 form: rooms=%s audio groups=%s (pre-2024.6 data: %s)"
          % (rooms_done, agrp_done, pre_2024_6))

    # ------------------------------------------------- audio groups (2024.14)
    agrp_off = chunks["AGRP"][1]
    group_count = u32(agrp_off) if not agrp_done else 0
    for i in range(group_count):
        slot = agrp_off + 4 + 4 * i
        name_ptr = u32(u32(slot))
        # group 0 is the built-in group: its audio lives inside the data file
        path = "" if i == 0 else "audiogroup%d.dat" % i
        path_ptr = emit_string(path)
        struct.pack_into("<I", data, slot, emit(struct.pack("<II", name_ptr, path_ptr)))
    print("audio groups converted: %d" % group_count)

    # ------------------------------------------------------ sounds (2024.6)
    # UndertaleSound gained a trailing `AudioLength` float.
    sond_off = chunks["SOND"][1]
    sond_count = u32(sond_off)
    sond_ptrs = [u32(sond_off + 4 + 4 * i) for i in range(sond_count)]
    real = [p for p in sond_ptrs if p]
    sound_len = (real[1] - real[0]) if len(real) >= 2 else 0
    if sond_count and sound_len and sound_len < 40:
        for i in range(sond_count):
            slot = sond_off + 4 + 4 * i
            old = u32(slot)
            if not old:
                continue
            struct.pack_into("<I", data, slot,
                             emit(bytes(data[old:old + sound_len]) + struct.pack("<f", 0.0)))
        print("sounds converted: %d (entry %d -> %d bytes)" % (sond_count, sound_len, sound_len + 4))
    else:
        print("sounds already in 2024.6 form (entry %d bytes)" % sound_len)

    # ------------------------------------------- collision masks (2024.6)
    # Before 2024.6 a sprite's collision mask covered the whole sprite; from
    # 2024.6 it only covers the bounding box.  The runner computes the mask
    # length itself, so an old mask block desynchronises the rest of the sprite
    # entry (the symptom is an absurd allocation while loading).  Every mask is
    # re-packed into the bounding box here.
    sprt_off = chunks["SPRT"][1]
    sprite_count = u32(sprt_off)
    masks_done = 0
    if pre_2024_6:
        for i in range(sprite_count):
            slot = sprt_off + 4 + 4 * i
            old = u32(slot)
            if not old:
                continue
            width, height = u32(old + 4), u32(old + 8)
            left, right, bottom, top = (struct.unpack_from("<i", data, old + 12 + 4 * k)[0] for k in range(4))
            if struct.unpack_from("<i", data, old + 56)[0] != -1:
                continue                                   # not a special sprite
            sver = u32(old + 60)
            if u32(old + 64) != 0:
                continue                                   # only normal sprites carry masks here
            q = old + 68 + 8                               # playback speed + type
            if sver >= 2:
                q += 4
            if sver >= 3:
                q += 4
            tex_count = u32(q)
            q += 4 + 4 * tex_count                          # texture entry list
            mask_count = u32(q)
            q += 4
            if not (0 <= mask_count < 10000) or width == 0 or height == 0:
                continue

            old_stride = (width + 7) // 8
            old_len = old_stride * height
            bw = right - left + 1
            bh = bottom - top + 1
            new_stride = (bw + 7) // 8
            new_len = new_stride * bh if bw > 0 and bh > 0 else 0

            blocks = []
            for m in range(mask_count):
                src = q + m * old_len
                out = bytearray(new_len)
                for y in range(bh):
                    row = src + (top + y) * old_stride
                    for x in range(bw):
                        sx = left + x
                        if data[row + (sx >> 3)] & (0x80 >> (sx & 7)):
                            out[y * new_stride + (x >> 3)] |= 0x80 >> (x & 7)
                blocks.append(bytes(out))
            block = struct.pack("<I", mask_count) + b"".join(blocks)
            while len(block) % 4:
                block += b"\0"

            prefix = bytes(data[old:q - 4])                 # up to (not including) the mask count
            struct.pack_into("<I", data, slot, emit(prefix + block))
            masks_done += 1
        print("sprite collision masks re-packed: %d" % masks_done)
    else:
        print("collision masks already in 2024.6 form")

    # ------------------------------------------ texture groups (2023.1)
    # UndertaleTextureGroupInfo gained Directory / Extension / LoadType
    # strings right after the name.  They only matter for texture groups that
    # are loaded from disk at runtime, so empty strings are written.
    tgin_off = chunks["TGIN"][1]
    tgin_count = u32(tgin_off + 4)              # the chunk starts with a version word
    tgin_ptrs = [u32(tgin_off + 8 + 4 * i) for i in range(tgin_count)]
    chunk_end = tgin_off + chunks["TGIN"][2] if len(chunks["TGIN"]) > 2 else None
    if tgin_ptrs:
        in_chunk = lambda v: tgin_off <= v < tgin_off + chunks["TGIN"][2]
        after_name = [u32(tgin_ptrs[0] + 4 + 4 * k) for k in range(3)]
        needs = all(in_chunk(v) for v in after_name)      # lists start right after the name
        if needs:
            empty = emit_string("")
            for i in range(tgin_count):
                slot = tgin_off + 8 + 4 * i
                old = u32(slot)
                if not old:
                    continue
                # everything from the first list pointer to the next entry
                nxt = u32(tgin_off + 8 + 4 * (i + 1)) if i + 1 < tgin_count else None
                lists = bytes(data[old + 4:nxt]) if nxt else bytes(data[old + 4:old + 4 + 20])
                struct.pack_into("<I", data, slot,
                                 emit(bytes(data[old:old + 4])
                                      + struct.pack("<III", empty, empty, empty)
                                      + lists))
            print("texture groups converted: %d" % tgin_count)
        else:
            print("texture groups already in 2023.1 form")

    # -------------------------------------------------- textures (2022.9)
    # UndertaleEmbeddedTexture gained TextureWidth / TextureHeight /
    # IndexInGroup between the block size and the data pointer.  The size is
    # taken from the texture blob itself (the "2zoq" and PNG headers both
    # carry it), the group index is left at 0.
    txtr_off = chunks["TXTR"][1]
    txtr_count = u32(txtr_off)
    tx_ptrs = [u32(txtr_off + 4 + 4 * i) for i in range(txtr_count)]
    tx_stride = (tx_ptrs[1] - tx_ptrs[0]) if txtr_count >= 2 else 0
    if txtr_count >= 2 and tx_stride == 16:
        for i in range(txtr_count):
            slot = txtr_off + 4 + 4 * i
            old = u32(slot)
            head = bytes(data[old:old + 12])          # scaled, mips, block size
            blob = u32(old + 12)
            w = h = 0
            magic = bytes(data[blob:blob + 4])
            if magic == b"2zoq":
                w, h = struct.unpack_from("<HH", data, blob + 4)
            elif magic == b"\x89PNG":
                w, h = struct.unpack_from(">II", data, blob + 16)
            struct.pack_into("<I", data, slot,
                             emit(head + struct.pack("<iiI", w, h, 0) + struct.pack("<I", blob)))
        print("textures converted: %d (entry 16 -> 28 bytes)" % txtr_count)
    else:
        print("textures already in 2022.9 form (entry %d bytes)" % tx_stride)

    # ------------------------------------------- fonts (2023.2 / 2023.6 / 2024.11)
    # header gained SDFSpread + LineHeight before the glyph list, and every
    # glyph gained a 2-byte field before its kerning list.
    font_off = chunks["FONT"][1]
    font_count = u32(font_off)
    font_ptrs = [u32(font_off + 4 + 4 * i) for i in range(font_count)]
    glyph_base = None
    if font_ptrs:
        # locate the glyph list: try the old (48) and new (56) header sizes
        for cand in (48, 56):
            n = u32(font_ptrs[0] + cand)
            if 0 < n < 65536 and 0 < u32(font_ptrs[0] + cand + 4) < len(data):
                glyph_base = cand
                break
    if glyph_base == 48:
        for i in range(font_count):
            slot = font_off + 4 + 4 * i
            old = u32(slot)
            if not old:
                continue
            count = u32(old + 48)
            glyphs = [u32(old + 52 + 4 * k) for k in range(count)]
            # relocate every glyph with the extra 2-byte field
            new_glyphs = []
            line_height = 0
            for gp in glyphs:
                head = bytes(data[gp:gp + 14])              # char..offset
                line_height = max(line_height, struct.unpack_from("<H", head, 8)[0])
                kern_count = struct.unpack_from("<H", data, gp + 14)[0]
                kerning = bytes(data[gp + 14:gp + 16 + 4 * kern_count])
                new_glyphs.append(emit(head + struct.pack("<h", 0) + kerning))
            header = bytes(data[old:old + 48])
            body = (header
                    + struct.pack("<II", 0, line_height)     # SDFSpread, LineHeight
                    + struct.pack("<I", count)
                    + b"".join(struct.pack("<I", g) for g in new_glyphs))
            struct.pack_into("<I", data, slot, emit(body))
        print("fonts converted: %d" % font_count)
    else:
        print("fonts already in 2024.11 form")

    # ------------------------------- room asset layers (2023.2 / 2024.6)
    # Assets layers gained a ParticleSystems and a TextItems pointer.
    empty_list = emit(struct.pack("<I", 0))
    converted_layers = 0
    for i in range(u32(chunks["ROOM"][1])):
        room = u32(chunks["ROOM"][1] + 4 + 4 * i)
        layers_ptr = u32(room + (0x5C if rooms_done else 0x58))
        for j in range(u32(layers_ptr)):
            slot = layers_ptr + 4 + 4 * j
            lp = u32(slot)
            if u32(lp + 8) != 3:                 # only Assets layers
                continue
            q = lp + 0x24 + 8                     # after visible/effectEnabled/effectType
            q += 4 + 12 * u32(q)                  # effect properties
            type_data = q - lp
            first = u32(q)
            # already converted if there are five pointers before the first list
            if first == q + 20:
                continue
            if first != q + 12:
                continue                          # unexpected shape - leave alone
            body = (bytes(data[lp:lp + type_data + 12])
                    + struct.pack("<II", empty_list, empty_list))
            struct.pack_into("<I", data, slot, emit(body))
            converted_layers += 1
    print("asset layers converted: %d" % converted_layers)

    # ------------------------------------------------------- rooms (2024.13)
    # The new field holds the order in which the room's pre-placed instances
    # get their Create event run.  Before 2024.13 that order was implicit: the
    # order of the room's flat GameObjects list.  Writing an empty list here
    # makes the runner skip every Create event (objects exist but none of their
    # variables are initialised), so the list is rebuilt from that flat list.
    room_off = chunks["ROOM"][1]
    room_count = u32(room_off) if not rooms_done else 0
    total_instances = 0
    for i in range(room_count):
        slot = room_off + 4 + 4 * i
        old = u32(slot)

        game_objects = u32(old + 0x30)
        instance_count = u32(game_objects)
        ids = [u32(u32(game_objects + 4 + 4 * j) + 0xC) for j in range(instance_count)]
        total_instances += instance_count
        order_list = emit(struct.pack("<I%dI" % instance_count, instance_count, *ids))

        header = bytearray(data[old:old + ROOM_HEADER_OLD])
        # The runner only reads the new field when the room's flags say the
        # room is in the 2024.13+ format (flags >= 0x40000); the file says
        # 0x30000 ("GMS 2.3"), so the level has to be raised as well.
        flags = struct.unpack_from("<I", header, ROOM_FLAGS_OFFSET)[0]
        flags = (flags & ~ROOM_FLAGS_LEVEL_MASK) | ROOM_FLAGS_LEVEL_2024_13
        struct.pack_into("<I", header, ROOM_FLAGS_OFFSET, flags)

        new_header = (bytes(header[:INSTANCE_ORDER_OFFSET])
                      + struct.pack("<I", order_list)
                      + bytes(header[INSTANCE_ORDER_OFFSET:]))
        struct.pack_into("<I", data, slot, emit(new_header))
    print("rooms converted: %d (%d instances ordered)" % (room_count, total_instances))

    # ------------------------------------------------- resize last chunk/FORM
    data.extend(append)
    hdr, _, size = chunks[last]
    struct.pack_into("<I", data, hdr + 4, size + len(append))
    struct.pack_into("<I", data, 4, len(data) - 8)

    open(dst_path, "wb").write(bytes(data))
    print("written %s (%d bytes, +%d)" % (dst_path, len(data), len(append)))
    return 0


if __name__ == "__main__":
    if len(sys.argv) != 3:
        raise SystemExit(__doc__)
    sys.exit(main(sys.argv[1], sys.argv[2]))
