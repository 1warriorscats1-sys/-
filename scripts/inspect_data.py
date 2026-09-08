#!/usr/bin/env python3
"""Inspect a GameMaker data file and report everything a Switch port needs.

Prints:
  * chunk map
  * detected GameMaker version markers (the same heuristics UndertaleModTool
    uses, ported here: SOND -> 2024.6, FUNC -> 2024.8, font glyphs -> 2024.11,
    room flags/ICO -> 2024.13, AGRP path -> 2024.14)
  * which conversions scripts/convert_data.py would have to apply
  * the audio group table with the .dat file each group expects
  * external files the game opens at runtime (Included Files that have to be
    copied to the SD card next to game.win)
  * whether anything the 2024.14 runner cannot parse is present (vector
    sprites, sequences, particle system instances, room text items)

Usage: inspect_data.py <data.win>
"""

import re
import struct
import sys


class Data:
    def __init__(self, path):
        self.d = open(path, "rb").read()
        if self.d[:4] != b"FORM":
            raise SystemExit("not a GameMaker data file")
        self.chunks = {}
        off, end = 8, 8 + self.u32(4)
        while off < end:
            name = self.d[off:off + 4].decode("latin1")
            self.chunks[name] = (off + 8, self.u32(off + 4))
            off += 8 + self.u32(off + 4)
        self.tail_ok = (off == len(self.d))

    def u32(self, p):
        return struct.unpack_from("<I", self.d, p)[0]

    def i32(self, p, o=0):
        return struct.unpack_from("<i", self.d, p + o)[0]

    def u16(self, p):
        return struct.unpack_from("<H", self.d, p)[0]

    def s(self, p):
        if not p:
            return ""
        n = self.u32(p - 4)
        return self.d[p:p + n].decode("utf-8", "replace") if n < 4096 else ""

    def list_ptrs(self, chunk):
        base = self.chunks[chunk][0]
        return [self.u32(base + 4 + 4 * i) for i in range(self.u32(base))]


def main(path):
    g = Data(path)
    print("file: %s (%d bytes)%s" % (path, len(g.d), "" if g.tail_ok else "  [!] trailing data"))
    print("chunks: " + ", ".join("%s:%d" % (n, s) for n, (o, s) in g.chunks.items()))
    print()

    # ------------------------------------------------------------- version
    marks = {}
    gen8 = g.chunks["GEN8"][0]
    print("bytecode version: %d   game: %r   file: %r"
          % (g.d[gen8 + 1], g.s(g.u32(gen8 + 40)), g.s(g.u32(gen8 + 4))))

    if "CODE" not in g.chunks or "FUNC" not in g.chunks:
        print()
        print("[!] no CODE/FUNC/VARI chunks -> this is a **YYC build**: the game logic was")
        print("    compiled to native code inside the executable, not stored as VM bytecode.")
        print("    The GameMaker Switch runner only executes VM bytecode, so this data file")
        print("    cannot be ported with this pipeline.")
        print("    chunks present: " + ", ".join(sorted(g.chunks)))
        return 2

    marks["2023.2+ (PSEM present)"] = "PSEM" in g.chunks
    sptr = [p for p in g.list_ptrs("SOND") if p]
    if len(sptr) >= 2:
        marks["2024.6 (sound entry grew)"] = (sptr[0] + 4 * 9) == (sptr[1] - 4)
    fo, fsize = g.chunks["FUNC"]
    pos = fo + 4 + 12 * g.u32(fo)
    end = fo + fsize
    while pos < end and pos % 16 and g.d[pos] == 0:
        pos += 1
    marks["2024.8 (no code locals)"] = pos == end
    fonts = [p for p in g.list_ptrs("FONT") if p]
    if fonts:
        gl = fonts[0] + 56
        gp = [g.u32(gl + 4 + 4 * i) for i in range(g.u32(gl))]
        ok11 = sum(1 for i in range(min(len(gp) - 1, 12))
                   if gp[i] + 18 + 4 * g.u16(gp[i] + 16) <= gp[i + 1] <= gp[i] + 21 + 4 * g.u16(gp[i] + 16))
        marks["2024.11 (glyph UnknownAlwaysZero)"] = ok11 > 0
    rooms = [p for p in g.list_ptrs("ROOM") if p]
    marks["2024.13 (room creation order)"] = bool(rooms) and g.u32(rooms[0] + 0x24) >= 0x40000
    agrp = [p for p in g.list_ptrs("AGRP") if p]
    # 2024.14 added a second field, so consecutive entries are 8 bytes apart
    # instead of 4 (UndertaleModTool detects it the same way)
    if len(agrp) >= 2:
        marks["2024.14 (audio group path)"] = (agrp[1] - agrp[0]) != 4
    elif agrp:
        marks["2024.14 (audio group path)"] = struct.unpack_from("<II", g.d, agrp[0])[1] != 0
    marks["2024.13+ (UILR chunk)"] = "UILR" in g.chunks
    print("version markers:")
    for k, v in marks.items():
        print("   %-36s %s" % (k, "yes" if v else "no"))
    print()

    todo = []
    if not marks.get("2024.13 (room creation order)"):
        todo.append("rooms: insert InstanceCreationOrderIDs + raise the flags level to 0x40000")
    if not marks.get("2024.14 (audio group path)"):
        todo.append("audio groups: add the path strings")
    print("conversion needed for a 2024.14 runner: " + ("nothing" if not todo else ""))
    for t in todo:
        print("   -", t)
    print()

    # -------------------------------------------------- unsupported content
    problems = []
    sprites = g.list_ptrs("SPRT")
    special = types = 0
    for p in sprites:
        if p and g.i32(p, 14 * 4) == -1:
            special += 1
            if g.u32(p + 14 * 4 + 8) != 0:
                types += 1
    print("sprites: %d (%d special, %d non-normal)" % (len(sprites), special, types))
    if types:
        problems.append("%d non-normal sprites (vector/SWF/Spine) - 2024.14 changed that format" % types)

    seq = ps = ti = 0
    for p in rooms:
        layers = g.u32(p + (0x5C if marks["2024.13 (room creation order)"] else 0x58))
        for j in range(g.u32(layers)):
            L = g.u32(layers + 4 + 4 * j)
            if g.u32(L + 8) != 3:      # only Assets layers carry these lists
                continue
            q = L + 0x24 + 8
            q += 4 + 12 * g.u32(q)
            _, _, sq, pp, tt = struct.unpack_from("<IIIII", g.d, q)
            for ptr, acc in ((sq, "seq"), (pp, "ps"), (tt, "ti")):
                if 0 < ptr < len(g.d) and g.u32(ptr) < 100000:
                    if acc == "seq":
                        seq += g.u32(ptr)
                    elif acc == "ps":
                        ps += g.u32(ptr)
                    else:
                        ti += g.u32(ptr)
    print("rooms: %d   sequence instances: %d   particle systems: %d   text items: %d"
          % (len(rooms), seq, ps, ti))
    for n, what in ((seq, "sequence instances"), (ps, "particle system instances"), (ti, "room text items")):
        if n:
            problems.append("%d %s - 2024.13/14 changed that format" % (n, what))

    # ------------------------------------------------------- audio groups
    print()
    groups = [g.s(g.u32(p)) for p in agrp]
    counts = {}
    for p in g.list_ptrs("SOND"):
        if p:
            counts[g.i32(p, 28)] = counts.get(g.i32(p, 28), 0) + 1
    if groups:
        print("audio groups:")
        for i, name in enumerate(groups):
            where = "(inside the data file)" if i == 0 else "audiogroup%d.dat" % i
            n = counts.get(i, 0)
            print("   %-3d %-22s %4d sounds   %s%s"
                  % (i, name, n, where, "" if n or i == 0 else "   [no file expected]"))

    # ---------------------------------------------------- external files
    print()
    strings = [g.s(g.u32(g.chunks["STRG"][0] + 4 + 4 * i) + 4) for i in range(g.u32(g.chunks["STRG"][0]))]
    funcs = {}
    p = g.chunks["FUNC"][0] + 4
    for _ in range(g.u32(g.chunks["FUNC"][0])):
        name, occ, addr = g.s(g.u32(p)), g.i32(p + 4), g.i32(p + 8)
        p += 12
        sites = []
        for k in range(occ):
            if not (4 < addr < len(g.d) - 8):
                break
            sites.append(addr - 4)
            if k == occ - 1:
                break
            addr += g.u32(addr) & 0xFFFFFF
        funcs[name] = sites
    wanted = ("load_csv", "file_text_open_read", "file_bin_open", "buffer_load",
              "sprite_add", "audio_create_stream", "font_add", "json_load")
    found = {}
    for fn in wanted:
        for a in funcs.get(fn, []):
            for back in range(1, 8):
                w = g.u32(a - 4 * back)
                if (w >> 24) & 0xFF in (0xC0, 0xC1, 0xC2, 0xC3) and ((w >> 16) & 0xFF) == 6:
                    idx = g.u32(a - 4 * back + 4)
                    if idx < len(strings):
                        found.setdefault(fn, set()).add(strings[idx])
                    break
    if found:
        print("external files the game opens (copy these next to game.win):")
        for fn, names in found.items():
            for n in sorted(names):
                print("   %-22s %s" % (fn, n))
    else:
        print("external files: none referenced by a literal name")

    steam = sorted(n for n in funcs if n.startswith("steam_"))
    if steam:
        print("\nSteamworks calls: %d functions (need disarming, see scripts/patch_steam.py)" % len(steam))

    if problems:
        print("\n[!] content the 2024.14 runner would misparse:")
        for p_ in problems:
            print("   -", p_)
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1] if len(sys.argv) > 1 else "game.win"))
