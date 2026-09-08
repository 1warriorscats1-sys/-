#!/usr/bin/env python3
"""Neutralise the Steamworks calls that run during normal play.

The game ships the Steamworks extension: 149 `steam_*` functions.  On Switch
none of them exist, and `obj_achieve_steam` - which is placed in 75 rooms,
including the title screen - calls `steam_update()` and `steam_get_achievement()`
every single frame, plus `steam_initialised()` in its Create/Game Start/Room
Start events.  `obj_title_main` calls `steam_shutdown()` right after `game_end()`.

Rather than deleting the calls (which would break GameMaker's function
reference chains - the FUNC chunk threads every call site of a function into a
linked list through the call's *operand* word, and the runner walks those lists
at load time), only the call's **opcode word** is replaced with a branch that
jumps over it.  The operand word stays exactly where it is, so every chain
stays intact; the branch just means the instruction is never executed.

Effect: `flg_steam_api` stays 0 (which is what the game already does when Steam
is unavailable), the achievement flag resolves to "not unlocked", and quitting
from the title screen no longer calls into Steam.

Usage: patch_steam.py <in.win> <out.win>
"""

import struct
import sys

B_OPCODE = 0xB6


def main(src, dst):
    data = bytearray(open(src, "rb").read())
    u32 = lambda p: struct.unpack_from("<I", data, p)[0]
    i32 = lambda p: struct.unpack_from("<i", data, p)[0]

    def gstr(p):
        return data[p:p + u32(p - 4)].decode("utf-8", "replace") if p else ""

    chunks, off, end = {}, 8, 8 + u32(4)
    while off < end:
        chunks[data[off:off + 4].decode("latin1")] = (off + 8, u32(off + 4))
        off += 8 + u32(off + 4)

    # ------------------------------------------------------------ code entries
    code = chunks["CODE"][0]
    entries = {}
    for i in range(u32(code)):
        e = u32(code + 4 + 4 * i)
        start = e + 12 + i32(e + 12)
        entries[gstr(u32(e))] = (start, u32(e + 4))

    # ------------------------------------------- call site -> function name map
    func = chunks["FUNC"][0]
    call_names = {}
    p = func + 4
    for _ in range(u32(func)):
        name, occ, addr = gstr(u32(p)), i32(p + 4), i32(p + 8)
        p += 12
        for k in range(occ):
            if not (4 < addr < len(data) - 8):
                break
            call_names[addr - 4] = name
            if k == occ - 1:
                break
            addr += u32(addr) & 0xFFFFFF

    # --------------------------------------------------------------- decoding
    def decode(start, length):
        """Yield (address, opcode, size) for one code entry."""
        pos = start
        while pos < start + length:
            word = u32(pos)
            op = (word >> 24) & 0xFF
            size = 4
            if op in (0xC0, 0xC1, 0xC2, 0xC3):          # push variants
                t = (word >> 16) & 0xFF
                size = {0: 12, 2: 8, 3: 12, 5: 8, 6: 8}.get(t, 4)
            elif op == 0x45:                            # pop
                size = 8 if ((word >> 16) & 0xF) != 0x0F else 4
            elif op == 0xD9:                            # call
                size = 8
            elif op == 0xFF and ((word >> 16) & 0xFF) == 2:   # break with int arg
                size = 8
            yield pos, op, word, size
            pos += size

    patches = []

    def branch(at, target, why):
        delta = (target - at) // 4
        if not (0 < delta < 0x800000) or (target - at) % 4:
            raise SystemExit("bad branch %#x -> %#x" % (at, target))
        patches.append((at, (B_OPCODE << 24) | delta, why))

    def first_call(entry, fname):
        start, length = entries[entry]
        for addr, op, word, _ in decode(start, length):
            if op == 0xD9 and call_names.get(addr) == fname:
                return addr
        raise SystemExit("no call to %s in %s" % (fname, entry))

    # 1-3. guarded "if (steam_initialised() && ...)" blocks: jump to event end
    for entry, fname in (
        ("gml_Object_obj_achieve_steam_Create_0", "steam_initialised"),
        ("gml_Object_obj_achieve_steam_Other_2", "steam_update"),
        ("gml_Object_obj_achieve_steam_Other_4", "steam_initialised"),
    ):
        start, length = entries[entry]
        branch(first_call(entry, fname), start + length, "%s: skip Steam block" % entry)

    # 4. per-frame achievement poll: jump to the "not unlocked" branch, which is
    #    the target of the last conditional branch in the event
    entry = "gml_Object_obj_achieve_steam_Step_0"
    start, length = entries[entry]
    else_target = None
    for addr, op, word, _ in decode(start, length):
        if op in (0xB7, 0xB8):                        # bt / bf
            delta = word & 0xFFFFFF
            if delta & 0x800000:
                delta -= 0x1000000
            else_target = addr + delta * 4
    if else_target is None or not (start < else_target < start + length):
        raise SystemExit("could not locate the else branch of %s" % entry)
    branch(first_call(entry, "steam_update"), else_target, "%s: skip Steam poll" % entry)

    # 5. steam_shutdown() right after game_end() on the title screen
    entry = "gml_Object_obj_title_main_Step_0"
    addr = first_call(entry, "steam_shutdown")
    branch(addr, addr + 12, "%s: skip steam_shutdown" % entry)   # call(8) + popz(4)

    for at, word, why in patches:
        old = u32(at)
        if (old >> 24) & 0xFF != 0xD9:
            raise SystemExit("%#x is not a call (%08x)" % (at, old))
        struct.pack_into("<I", data, at, word)
        print("  %#010x  call -> b +%d   (%s)" % (at, (word & 0xFFFFFF) * 4, why))

    open(dst, "wb").write(bytes(data))
    print("patched %d Steam call sites -> %s" % (len(patches), dst))
    return 0


if __name__ == "__main__":
    if len(sys.argv) != 3:
        raise SystemExit(__doc__)
    sys.exit(main(sys.argv[1], sys.argv[2]))
