#!/usr/bin/env python3
"""Write the Japanese language slot into the NACP.

The NACP language-records array is NOT indexed by SetLanguage! The real
order (libnx, nx/source/runtime/nacp.c, g_nacpLanguageTable; matches
SwitchBrew "Control.nacp Language"):

    slot 0  = AmericanEnglish
    slot 1  = BritishEnglish
    slot 2  = Japanese             <- here
    slot 3  = French
    slot 4  = German
    slot 5  = LatinAmericanSpanish
    slot 6  = Spanish
    slot 7  = Italian
    slot 8  = Dutch
    slot 9  = CanadianFrench
    slot 10 = Portuguese
    slot 11 = Russian
    slot 12 = Korean
    slot 13 = TraditionalChinese
    slot 14 = SimplifiedChinese
    slot 15 = BrazilianPortuguese

Record layout: name 0x200 UTF-8 NUL-terminated + author 0x100, 0x300 total,
16 records starting at offset 0. nacptool fills ALL slots with the same
(English) strings, so it is enough to overwrite slot 2 with the Japanese
name/author.

Usage: nacp_lang.py <input.nacp> <output.nacp>
"""

import sys

ENTRY_SIZE = 0x300
NAME_SIZE = 0x200
AUTHOR_SIZE = 0x100

# (slot index, language, name, author)
SLOTS = [
    (2, "Japanese", "\u6771\u65b9\u6587\u82b1\u5e14 \uff5e Shoot the Bullet", "\u4e0a\u6d77\u30a2\u30ea\u30b9\u5e7b\u6a02\u5718"),
]


def put(dest: bytearray, offset: int, size: int, text: str) -> None:
    raw = text.encode("utf-8")
    if len(raw) + 1 > size:
        raise SystemExit(f"string does not fit the slot: {text!r}")
    dest[offset:offset + size] = raw + b"\x00" * (size - len(raw))


def main() -> None:
    if len(sys.argv) != 3:
        raise SystemExit(__doc__)
    with open(sys.argv[1], "rb") as handle:
        nacp = bytearray(handle.read())
    if len(nacp) < 16 * ENTRY_SIZE:
        raise SystemExit(f"suspiciously small NACP file: {len(nacp)} bytes")

    for slot, language, name, author in SLOTS:
        base = slot * ENTRY_SIZE
        put(nacp, base, NAME_SIZE, name)
        put(nacp, base + NAME_SIZE, AUTHOR_SIZE, author)
        print(f"nacp_lang: slot {slot} ({language}) <- '{name}' / '{author}'")

    with open(sys.argv[2], "wb") as handle:
        handle.write(nacp)


if __name__ == "__main__":
    main()
