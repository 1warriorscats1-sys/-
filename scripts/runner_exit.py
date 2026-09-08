"""Deferred exit for the verified 2024.14.3.260 SANAE runner.

The game_end epilogue only sets a private BSS flag. Both platform-loop paths
check it at the next iteration, outside the GML handler. Resolve the public SDK
ExitApplication entry via nn::ro::LookupSymbol, avoiding C exit/destructors.
No save-unmount calls, new imports, absolute runtime pointers or SDK edits.
"""
import struct

EXIT_NAME = b'_ZN2nn2oe15ExitApplicationEv\0'


def word(buf, at):
    return struct.unpack_from('<I', buf, at)[0]


def branch(pc, target, link=False):
    delta = target - pc
    if delta % 4 or not -(1 << 27) <= delta < (1 << 27):
        raise ValueError('ARM64 branch out of range')
    return (0x94000000 if link else 0x14000000) | ((delta >> 2) & 0x3ffffff)


def adrp(pc, target, rd):
    imm = (target >> 12) - (pc >> 12)
    if not -(1 << 20) <= imm < (1 << 20):
        raise ValueError('ADRP out of range')
    return 0x90000000 | ((imm & 3) << 29) | (((imm >> 2) & 0x7ffff) << 5) | rd


def add(rd, rn, imm):
    if not 0 <= imm <= 4095:
        raise ValueError('ADD immediate out of range')
    return 0x91000000 | imm << 10 | rn << 5 | rd


def install(text, raw, segs, header, commit_only):
    # Fixed sites are deliberately fail-closed, not heuristic matches.
    expected = {
        0x375214: 0xf94023f3,  # ldr x19,[sp,#0x40] in game_end epilogue
        0x375218: 0xa9437bfd,
        0x37521c: 0x910143ff,
        0x375220: 0xd65f03c0,
        0x1740: branch(0x1740, 0xaf0, True),
        0x1954: branch(0x1954, 0xaf0, True),
        0x5693f0: adrp(0x5693f0, 0x792140, 16),
        0x5693f4: 0xf940a211,  # LookupSymbol PLT: ldr x17,[x16,#0x140]
        0x5693f8: add(16, 16, 0x140),
        0x5693fc: 0xd61f0220,
    }
    for at, value in expected.items():
        if word(text, at) != value:
            raise ValueError(f'Unexpected exit hook instruction at {at:#x}')
    # Verify the relocation really imports LookupSymbol, not merely a similar PLT.
    from patch_nso import build_image
    image = build_image(segs, raw)
    mod = word(image, 4)
    if image[mod:mod+4] != b'MOD0':
        raise ValueError('Missing MOD0')
    dyn = {}; pos = mod + struct.unpack_from('<i', image, mod+4)[0]
    while True:
        tag, val = struct.unpack_from('<qQ', image, pos); pos += 16
        if not tag:
            break
        dyn[tag] = val
    found = []
    for pos in range(dyn[23], dyn[23]+dyn[2], 24):
        got, info, _ = struct.unpack_from('<QQq', image, pos)
        if got == 0x792140:
            start = dyn[5] + word(image, dyn[6] + (info >> 32)*24)
            found.append(bytes(image[start:image.index(0, start)]))
    if found != [b'_ZN2nn2ro12LookupSymbolEPmPKc']:
        raise ValueError('LookupSymbol relocation mismatch')

    # Allocate beyond the entire original BSS, not inside guessed unused globals.
    flag = segs[2][1] + len(raw[2]) + word(header, 0x3c)
    if flag != 0x11d3000:
        raise ValueError('Unexpected BSS layout')
    struct.pack_into('<I', header, 0x3c, word(header, 0x3c) + 0x1000)
    ro = bytearray(raw[1])
    struct.pack_into('<i', ro, mod + 12 - segs[1][1], flag + 0x1000 - mod)
    raw[1] = bytes(ro)

    def emit(words):
        at = len(text)
        text.extend(struct.pack('<'+'I'*len(words), *words))
        return at

    request = len(text)
    emit([adrp(request, flag, 8), add(8, 8, flag & 4095),
          0x52800029,           # mov w9,#1
          0x39000109,           # strb w9,[x8]
          expected[0x375214],    # original epilogue instruction
          branch(request+20, 0x375218)])

    poll = len(text)
    # On the normal path x0-x7 and LR remain untouched; x8 is caller-saved.
    emit([adrp(poll, flag, 8), add(8, 8, flag & 4095),
          0x39400108,           # ldrb w8,[x8]
          0x35000048,           # cbnz w8,+8
          branch(poll+16, 0xaf0),
          0xa9be7bfd,           # stp x29,x30,[sp,#-32]!
          0x910003fd,           # mov x29,sp
          branch(poll+28, commit_only, True),
          0xf9000bff,           # str xzr,[sp,#16]
          add(0, 31, 16),       # add x0,sp,#16 (out pointer)
          0, 0,                # address of symbol name in x1
          branch(poll+48, 0x5693f0, True),
          0x350000a0,           # cbnz w0,+20 -> svc fallback
          0xf9400bf0,           # ldr x16,[sp,#16]
          0xb4000070,           # cbz x16,+12 -> fallback
          0xd63f0200,           # blr x16 (ExitApplication, no return)
          0xd503201f,           # if SDK unexpectedly returns, exit process
          0xd40000e1,           # svc #7 (ExitProcess, not C exit)
          0x14000000])          # never return into the game if svc returns
    name = len(text)
    text.extend(EXIT_NAME)
    while len(text) % 4:
        text.append(0)
    struct.pack_into('<II', text, poll+40, adrp(poll+40, name, 1), add(1, 1, name & 4095))
    struct.pack_into('<I', text, 0x375214, branch(0x375214, request))
    for site in (0x1740, 0x1954):
        struct.pack_into('<I', text, site, branch(site, poll, True))
    if len(text) > segs[1][1]:
        raise ValueError('Exit stubs overlap rodata')
    print(f'  deferred exit: request={request:#x}, poll={poll:#x}, BSS flag={flag:#x}')
    return dict(request=request, poll=poll, flag=flag, name=name)
