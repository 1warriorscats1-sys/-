#!/usr/bin/env python3
"""Patch the GameMaker NX runner NSO.

Three fixes, all located by instruction pattern (so they work on any 2024.14.x
runner, not just one build):

1. AGRP null path
   While reading the AGRP chunk the runner builds a `std::string` from each
   audio group's path: `ldp w10, w9, [x9]` / `cbz w9, null` / `mov x1, xzr`.
   A pre-2024.14 data file has no path field, so that branch is taken and the
   game dies in `strlen(NULL)`.  The `mov x1, xzr` becomes `add x1, x25, #0xa`,
   which points at the terminator of the "unused.dat" literal already in x25 -
   i.e. an empty string.

2. Deprecated builtins are fatal
   2024.13+ gates some builtins behind a runtime flag:
       ldrb w8, [x8]            ; "deprecated functions enabled"
       cbz  w8, error           ; -> "Calling <fn> when it is marked as deprecated"
   The flag comes from a game option that only exists in 2024.13+ data files,
   so with an older data file it reads as 0 and any call aborts the game.
   SANAE's Sylphid Breeze calls `instance_change` in 18 room controllers, which
   made the tutorial unbeatable.  The `cbz` is replaced with a `nop`.

3. Fatal abort if romfs is mounted twice
   `MountRom("rom")` is checked with `cbnz w0, abort`.  On `game_restart()` the
   init path can run again and the second mount returns "already mounted",
   killing the process instead of carrying on.  The `cbnz` becomes a `nop`.

Usage: patch_nso.py <in-nso> <out-nso>
"""

import hashlib
import os
from pathlib import Path
import struct
import sys

import capstone
import lz4.block

SRC_PATH = ""

NOP = 0xD503201F


def load_nso(path):
    d = Path(path).read_bytes()
    if d[:4] != b"NSO0":
        raise SystemExit("not an NSO")
    flags = struct.unpack_from("<I", d, 0xC)[0]
    segs = [list(struct.unpack_from("<III", d, o)) for o in (0x10, 0x20, 0x30)]
    csz = list(struct.unpack_from("<III", d, 0x60))
    raw = []
    for i, (fo, mo, sz) in enumerate(segs):
        blob = d[fo:fo + csz[i]]
        raw.append(lz4.block.decompress(blob, uncompressed_size=sz) if (flags >> i) & 1 else blob[:sz])
    return bytearray(d[:0x100]), flags, segs, raw


def build_image(segs, raw):
    end = max(mo + len(raw[i]) for i, (fo, mo, sz) in enumerate(segs))
    image = bytearray(end)
    for i, (fo, mo, sz) in enumerate(segs):
        image[mo:mo + len(raw[i])] = raw[i]
    return image


def cstring(image, addr):
    end = image.find(b"\0", addr)
    if end < 0 or end - addr > 200:
        return ""
    try:
        return image[addr:end].decode()
    except UnicodeDecodeError:
        return ""


def find_agrp_null_path(insns, by_addr, image):
    for i, ins in enumerate(insns):
        if ins.mnemonic != "ldp" or not ins.op_str.startswith("w10, w9, [x9]"):
            continue
        target = None
        for k in insns[i:i + 14]:
            if k.mnemonic == "cbz" and k.op_str.startswith("w9,"):
                target = int(k.op_str.split("#")[1], 0)
        if target is None or target not in by_addr:
            continue
        j = by_addr[target]
        if insns[j].mnemonic != "mov" or insns[j].op_str != "x1, xzr":
            continue
        literal = None
        for b in range(i - 1, max(0, i - 60), -1):
            if insns[b].mnemonic == "add" and insns[b].op_str.startswith("x25, x25,"):
                for c in range(b - 1, max(0, b - 6), -1):
                    if insns[c].mnemonic == "adrp" and insns[c].op_str.startswith("x25,"):
                        literal = (int(insns[c].op_str.split("#")[1], 0)
                                   + int(insns[b].op_str.split("#")[1], 0))
                        break
                break
        if literal is None or cstring(image, literal) != "unused.dat":
            continue
        yield target, 0x91002B21, "AGRP null path -> empty string (add x1, x25, #0xa)"


def _string_from(insns, j, image):
    """Resolve an adrp/add string literal starting at instruction j."""
    pages = {}
    for k in insns[j:j + 10]:
        if k.mnemonic == "adrp":
            pages[k.op_str.split(",")[0].strip()] = int(k.op_str.split("#")[1], 0)
        elif k.mnemonic == "add" and "#" in k.op_str:
            p = [x.strip() for x in k.op_str.split(",")]
            if len(p) >= 3 and p[1] in pages:
                try:
                    return cstring(image, pages[p[1]] + int(p[2].replace("#", ""), 0))
                except ValueError:
                    pass
    return ""


def find_deprecated_gates(insns, by_addr, image):
    for i, ins in enumerate(insns):
        if ins.mnemonic != "cbz" or not ins.op_str.startswith("w8, #"):
            continue
        if i < 1 or insns[i - 1].mnemonic != "ldrb":
            continue
        target = int(ins.op_str.split("#")[1], 0)
        j = by_addr.get(target)
        if j is None:
            continue
        text = _string_from(insns, j, image)
        if "marked as deprecated" not in text:
            continue
        yield ins.address, NOP, "deprecated builtin is fatal: %r" % text


def find_global_init_rerun(insns, by_addr, image):
    """game_restart() re-runs the global init list through a cached pointer.

        adrp x8, PAGE ; add x8, x8, #OFF ; ldr x0, [x8]
        adrp x8, PAGE ; add x8, x8, #OFF+8 ; ldr w1, [x8]
        b    handler                      ; handler starts with ldr wN, [x0]

    After a restart that cached pointer is NULL, so the handler dereferences
    null and the process dies with a data abort.  The thunk is rewritten to

        ldr x0, [x8] ; cbz x0, ret ; ldr w1, [x8, #8] ; b handler ; nop ; ret

    i.e. the re-run is skipped instead of crashing.
    """
    for i, ins in enumerate(insns):
        if ins.mnemonic != "ldr" or not ins.op_str.startswith("x0, [x8]"):
            continue
        w = insns[i + 1:i + 6]
        if len(w) < 5:
            continue
        if not (w[0].mnemonic == "adrp" and w[1].mnemonic == "add"
                and w[2].mnemonic == "ldr" and w[2].op_str.startswith("w1, [x8]")
                and w[3].mnemonic == "b"):
            continue
        # the two globals must be 8 bytes apart
        try:
            first = int(insns[i - 1].op_str.split("#")[1], 0)
            second = int(w[1].op_str.split("#")[1], 0)
        except (IndexError, ValueError):
            continue
        if second - first != 8:
            continue
        target = int(w[3].op_str.replace("#", ""), 0)
        j = by_addr.get(target)
        if j is None:
            continue
        # the handler must start by dereferencing x0 within a few instructions
        if not any(k.mnemonic == "ldr" and "[x0]" in k.op_str for k in insns[j:j + 24]):
            continue
        base = w[0].address                       # first instruction we may reuse
        branch_back = ((target - (base + 8)) // 4) & 0x3FFFFFF
        yield base,          0xB4000060, "global init re-run: skip when the cached list is NULL"
        yield base + 4,      0xB9400901, "  ldr w1, [x8, #8]"
        yield base + 8,      0x14000000 | branch_back, "  b handler"
        yield base + 12,     0xD65F03C0, "  ret"
        return


def find_mount_abort(insns, by_addr, image):
    for i, ins in enumerate(insns):
        if ins.mnemonic != "add" or '#' not in ins.op_str:
            continue
        # ... adrp x0, PAGE ; add x0, x0, #OFF   -> "rom" ; bl MountRom ; cbnz w0, abort
        if _string_from(insns, max(0, i - 1), image) != "rom":
            continue
        for k in range(i + 1, min(i + 6, len(insns))):
            if insns[k].mnemonic == "cbnz" and insns[k].op_str.startswith("w0, #"):
                yield insns[k].address, NOP, "MountRom failure is fatal (breaks game_restart)"
                break


def find_plt_targets(image, insns):
    """Return (CloseFile stub, CommitSaveData stub, address of the "save" string)."""
    import re as _re
    # imports are resolved through the PLT: find the stub that loads each GOT slot
    header = Path(SRC_PATH).read_bytes()
    dynstr_off = struct.unpack_from("<I", header, 0x90)[0]
    mod_off = struct.unpack_from("<I", image, 4)[0]
    if image[mod_off:mod_off + 4] != b"MOD0":
        return None
    dyn = {}
    p = mod_off + struct.unpack_from("<i", image, mod_off + 4)[0]
    while True:
        tag, val = struct.unpack_from("<qQ", image, p)
        p += 16
        if tag == 0:
            break
        dyn.setdefault(tag, val)
    strtab, symtab = dyn.get(5), dyn.get(6)
    jmprel, pltsz = dyn.get(23), dyn.get(2)
    if not (strtab and symtab and jmprel and pltsz):
        return None

    def sym_name(i):
        st_name = struct.unpack_from("<I", image, symtab + i * 24)[0]
        e = image.find(b"\0", strtab + st_name)
        return image[strtab + st_name:e].decode("latin1")

    wanted = {"_ZN2nn2fs9CloseFileENS0_10FileHandleE": None,
              "_ZN2nn2fs14CommitSaveDataEPKc": None,
              "exit": None}
    for off in range(jmprel, jmprel + pltsz, 24):
        r_offset, r_info, _ = struct.unpack_from("<QQq", image, off)
        name = sym_name(r_info >> 32)
        if name in wanted:
            wanted[name] = r_offset

    def stub_for(got):
        page, o = got & ~0xFFF, got & 0xFFF
        for i, ins in enumerate(insns):
            if ins.mnemonic == "adrp" and ("#%#x" % page) in ins.op_str:
                reg = ins.op_str.split(",")[0].strip()
                for j in range(i + 1, min(i + 4, len(insns))):
                    k = insns[j]
                    if k.mnemonic == "ldr" and reg in k.op_str and ("#%#x" % o) in k.op_str:
                        return ins.address
        return None

    close = stub_for(wanted["_ZN2nn2fs9CloseFileENS0_10FileHandleE"]) if wanted["_ZN2nn2fs9CloseFileENS0_10FileHandleE"] else None
    commit = stub_for(wanted["_ZN2nn2fs14CommitSaveDataEPKc"]) if wanted["_ZN2nn2fs14CommitSaveDataEPKc"] else None
    exitfn = stub_for(wanted["exit"]) if wanted["exit"] else None
    m = _re.search(rb"(?<=\x00)save\x00", image)
    save_str = m.start() if m else None
    if not (close and commit and save_str):
        return None
    helper = find_commit_helper(insns, commit)
    if not helper:
        return None
    return close, commit, save_str, exitfn, helper


def encode_adrp(pc, target, rd=0):
    delta = (target & ~0xFFF) - (pc & ~0xFFF)
    imm = delta >> 12
    return 0x90000000 | ((imm & 3) << 29) | (((imm >> 2) & 0x7FFFF) << 5) | rd


def encode_bl(pc, target):
    return 0x94000000 | (((target - pc) >> 2) & 0x3FFFFFF)


def find_commit_helper(insns, commit_plt):
    """The runner has its own guarded commit helper:

        ldrb w8, [flag] ; cbz w8, ret ; bl nn::fs::CommitSaveData ; bl nn::fs::Unmount ; flag = 0

    Calling nn::fs::CommitSaveData directly aborts inside the SDK when the save
    data is not mounted (fs 2002-6905), so everything goes through this helper,
    which checks the flag first.
    """
    for i, ins in enumerate(insns):
        if ins.mnemonic != "bl" or ins.op_str != ("#%#x" % commit_plt):
            continue
        page = off = None
        for k in insns[max(0, i - 12):i]:
            if k.mnemonic == "adrp":
                page = int(k.op_str.split("#")[1], 0)
            elif k.mnemonic == "add" and page is not None and "#" in k.op_str:
                try:
                    off = int(k.op_str.split("#")[1], 0)
                except ValueError:
                    off = None
            elif k.mnemonic == "ldrb" and page is not None and off is not None:
                return page + off          # the "save data needs committing" flag
    return None


def add_save_diagnostics(text, insns, image):
    """Turn the runner's silent "save data not mounted" paths into breakpoints.

    When the account save cannot be mounted the runner only writes a line to a
    log nobody can read on a retail console, and then lets the game write into
    a volatile location - which looks exactly like "the save is there until you
    close the game".  With DIAG=1 each of those three paths executes a distinct
    `brk`, so the crash report says which one fired:

        brk #0xA1  no user selected
        brk #0xA2  cannot open last user
        brk #0xA3  not enough space to create the save
    """
    messages = {b"No Save Data Mounted and No User Selected!\n": 0xA1,
                b"No Save Data Mounted and Cannot Open Last User!\n": 0xA2,
                b"No Save Data Mounted and Not Enough Space To Create it!\n": 0xA3}
    done = 0
    for i, ins in enumerate(insns):
        if ins.mnemonic != "adrp":
            continue
        nxt = insns[i + 1] if i + 1 < len(insns) else None
        if not (nxt and nxt.mnemonic == "add" and "#" in nxt.op_str):
            continue
        try:
            addr = int(ins.op_str.split("#")[1], 0) + int(nxt.op_str.split("#")[1], 0)
        except (IndexError, ValueError):
            continue
        end = image.find(b"\0", addr)
        code = messages.get(bytes(image[addr:end]))
        if code is None:
            continue
        struct.pack_into("<I", text, ins.address, 0xD4200000 | (code << 5))
        print("  diagnostic: brk #%#x at .text+%#x (%s)"
              % (code, ins.address, image[addr:end].decode()[:44]))
        done += 1
    return done


def _emit(text, words):
    while len(text) % 4:
        text.append(0)
    at = len(text)
    for w in words:
        text += struct.pack("<I", w)
    return at


def add_save_hooks(text, insns, plt):
    """Persist saves and make the quit button quit.

    * `commit_only` - the runner's own guard (a flag it sets while save data is
      mounted and dirty) followed by nn::fs::CommitSaveData.  Calling the SDK
      function unguarded aborts the process when nothing is mounted, and the
      runner's own helper additionally *unmounts* the save, which breaks every
      later write - so this stub does the guarded commit and nothing else.
    * `close_hook` - wraps nn::fs::CloseFile in the file layer, so whatever the
      game just wrote is flushed to the save data immediately.  Without it the
      data only lives in the journal and disappears when the console closes the
      application.
    Exit is installed separately by runner_exit, after the handler returns.
    """
    close_plt, commit_plt, save_str, exit_plt, flag = plt

    commit_only = _emit(text, [])
    words = [
        encode_adrp(commit_only, flag, 8),
        0x91000000 | ((flag & 0xFFF) << 10) | (8 << 5) | 8,     # add x8, x8, #off
        0x39400108,                                             # ldrb w8, [x8]
        0x34000000 | (4 << 5) | 8,                              # cbz w8, +16 (ret)
        encode_adrp(commit_only + 16, save_str, 0),
        0x91000000 | ((save_str & 0xFFF) << 10),                # add x0, x0, #off
        0,                                                      # b CommitSaveData (patched below)
        0xD65F03C0,                                             # ret
    ]
    for w in words:
        text += struct.pack("<I", w)
    struct.pack_into("<I", text, commit_only + 24,
                     0x14000000 | (((commit_plt - (commit_only + 24)) >> 2) & 0x3FFFFFF))

    close_hook = _emit(text, [
        0xA9BF7BFD,                                             # stp x29, x30, [sp, #-0x10]!
        0x910003FD,                                             # mov x29, sp
        0,                                                      # bl CloseFile
        0,                                                      # bl commit_only
        0xA8C17BFD,                                             # ldp x29, x30, [sp], #0x10
        0xD65F03C0,                                             # ret
    ])
    struct.pack_into("<I", text, close_hook + 8, encode_bl(close_hook + 8, close_plt))
    struct.pack_into("<I", text, close_hook + 12, encode_bl(close_hook + 12, commit_only))

    redirected = 0
    for ins in insns:
        if ins.mnemonic == "bl" and ins.op_str == ("#%#x" % close_plt) and 0x5a000 <= ins.address <= 0x5c000:
            struct.pack_into("<I", text, ins.address, encode_bl(ins.address, close_hook))
            redirected += 1

    print("  save commit stub at .text+%#x (guard flag %#x), %d file-close sites hooked"
          % (commit_only, flag, redirected))
    if redirected != 5:
        raise ValueError(f"Expected five save-close hooks, found {redirected}")
    return commit_only


def main(src, dst):
    global SRC_PATH
    SRC_PATH = src
    expected = "dcd5b9fc9ca50bf61f781a6a381c2f149ba97f82f395b94fe8f89d6c05ef3a2c"
    if hashlib.sha256(Path(src).read_bytes()).hexdigest() != expected:
        raise SystemExit("Unsupported runner: expected pristine 2024.14.3.260 main")
    header, flags, segs, raw = load_nso(src)
    text = bytearray(raw[0])
    image = build_image(segs, raw)

    md = capstone.Cs(capstone.CS_ARCH_ARM64, capstone.CS_MODE_ARM)
    md.skipdata = True
    from collections import namedtuple
    Instruction = namedtuple("Instruction", "address size mnemonic op_str")
    insns = [Instruction(*i) for i in md.disasm_lite(bytes(text), 0)]
    by_addr = {ins.address: i for i, ins in enumerate(insns)}

    patches = []
    patches += list(find_agrp_null_path(insns, by_addr, image))
    patches += list(find_deprecated_gates(insns, by_addr, image))
    patches += list(find_mount_abort(insns, by_addr, image))
    patches += list(find_global_init_rerun(insns, by_addr, image))

    if not any("AGRP" in why for _, _, why in patches):
        raise SystemExit("the AGRP patch site was not found - wrong runner build?")
    if not any("deprecated" in why for _, _, why in patches):
        raise SystemExit("no deprecated-builtin gate found - wrong runner build?")

    for addr, word, why in patches:
        old = struct.unpack_from("<I", text, addr)[0]
        struct.pack_into("<I", text, addr, word)
        print("  .text+%#08x  %08x -> %08x   %s" % (addr, old, word, why))

    plt = find_plt_targets(image, insns)
    if plt:
        commit_only = add_save_hooks(text, insns, plt)
        from runner_exit import install
        install(text, raw, segs, header, commit_only)
        if os.environ.get("DIAG") == "1":
            add_save_diagnostics(text, insns, image)
    else:
        raise SystemExit("Could not locate save imports; refusing incomplete patch")

    raw[0] = bytes(text)

    out = bytearray(header)
    body = b""
    file_off = 0x100
    for i in range(3):
        comp = lz4.block.compress(raw[i], mode="high_compression", store_size=False)
        struct.pack_into("<III", out, (0x10, 0x20, 0x30)[i], file_off, segs[i][1], len(raw[i]))
        struct.pack_into("<I", out, 0x60 + 4 * i, len(comp))
        out[0xA0 + 0x20 * i:0xC0 + 0x20 * i] = hashlib.sha256(raw[i]).digest()
        body += comp
        file_off += len(comp)
    struct.pack_into("<I", out, 0xC, flags | 0x7 | 0x38)
    Path(dst).write_bytes(bytes(out) + body)
    print("wrote %s (%d bytes, %d patches)" % (dst, len(out) + len(body), len(patches)))
    return 0


if __name__ == "__main__":
    if len(sys.argv) != 3:
        raise SystemExit(__doc__)
    sys.exit(main(sys.argv[1], sys.argv[2]))
