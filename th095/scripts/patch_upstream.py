#!/usr/bin/env python3
"""Apply the TH095 port source patches to a pristine reconstruction checkout.

The upstream source is an exact-match reconstruction for the 32-bit Windows
binary. Its compile-time layout assertions (``typedef char X[(...)? 1 : -1];``
and their ``C_ASSERT`` forms) document the i386 target layout; they are
meaningless for the runnable 64-bit port and cannot compile on a 64-bit
platform. This script disables them in place on the working copy.

Patches applied. P1/P2 are purely compile-time; P3-P7 are minimal
portability repairs of reconstruction artifacts (identical runtime behaviour):


P1  32-bit layout assertion typedefs
    ``typedef char X[(cond) ? 1 : -1];``  ->  ``typedef char X[1];``
    (multi-line conditions supported; the original condition is kept in a
    comment for provenance).

P2  Port-macro rename
    The reconstruction reuses the TH08 port's guard macro name
    ``TH08_MODERN_PORT`` in src/ZunMath.hpp; the port defines
    ``TH095_MODERN_PORT``, so the guard is renamed to match.

P3  Unguarded x87 inline assembly in AnmManager.cpp
    ``Float3::FromAngleMagnitude`` is the one bounded x87 asm block without
    an MSVC/i386 guard (its sibling paths in ZunMath.hpp/AnmDrawCore.cpp are
    guarded). The port wraps it in ``#if defined(_MSC_VER) && defined(_M_IX86)``
    with the same ``sinf``/``cosf`` portable path ZunMath.hpp already uses
    (x = cos(angle) * magnitude, y = sin(angle) * magnitude).

P4  zwave.cpp: loop index hoisted + <stdlib.h>
    ``CSound::GetFreeBuffer`` scopes its index ``i``
    inside the ``for`` statement but reads it after the loop — ill-formed as
    written. The index is declared before the loop and ``<stdlib.h>`` is
    included for ``rand`` (MSVC provided it transitively).

P8  Main.cpp: _beginthread called with four arguments
    The one ``_beginthread`` call site is reconstructed with three arguments;
    the MSVC shape is (start, stackSize, initFlag, arg). The port restores
    the missing ``initFlag`` (0).

P5  pbg/PbgArchive.cpp: declarations before gotos
    ``pbgReadEntry``/``pbgReadCompressedSize``/``pbgReadDecompressedSize``/
    ``pbgReadProfileIndex`` are initialized after ``goto entry_read_error;``
    statements; crossing initialized declarations is forbidden. All four are
    declared at the top of the function; assignments unchanged.

P6  Background.cpp: RenderObjects declarations before gotos
    ``resultDrawBacking096``/``resultDrawBacking157`` are initialized between
    ``goto nextInstance;`` jumps (forbidden), and ``instancePosition``
    (non-trivial constructor) is initialized between jumps. Declarations are
    hoisted above the jumps; initializations become assignments with the
    same values.

P7  PhotoStage.cpp: trailing array on 64-bit
    ``u8 trailing[0x2214 - 0x10c8 - 6 * sizeof(AnmVm)]`` underflows when the
    64-bit ``AnmVm`` is larger than the 32-bit layout assumed. The size is
    clamped at zero (the row keeps its two VM arrays; the padding shrinks).

Usage:
    python3 th095/scripts/patch_upstream.py build-inputs/th095-src

The script is idempotent and rewrites the files in place.
"""

import re
import sys
from pathlib import Path

# typedef char X[( <any expression, no nested brackets> ? 1 : -1 );
ASSERT_TYPEDEF = re.compile(
    r"typedef\s+char\s+([A-Za-z_]\w*)\s*\[((?:[^\[\]]*)\?\s*1\s*:\s*-1)\]\s*;",
    re.DOTALL,
)

REPLACEMENT = "typedef char {name}[1]; // th095-port: 32-bit layout assert disabled"

FROM_ANGLE_MAGNITUDE_ASM = """void Float3::FromAngleMagnitude(f32 angle, f32 magnitude)
{
    __asm
    {
        mov eax, this
        fld angle
        fsincos
        fmul [magnitude]
        fstp [eax]
        fmul [magnitude]
        fstp [eax + 4]
    }
}"""

FROM_ANGLE_MAGNITUDE_PORTED = """void Float3::FromAngleMagnitude(f32 angle, f32 magnitude)
{
#if defined(_MSC_VER) && defined(_M_IX86)
    __asm
    {
        mov eax, this
        fld angle
        fsincos
        fmul [magnitude]
        fstp [eax]
        fmul [magnitude]
        fstp [eax + 4]
    }
#else
    // th095-port: portable path (same sinf/cosf pair as ZunMath.hpp's sincos).
    this->x = cosf(angle) * magnitude;
    this->y = sinf(angle) * magnitude;
#endif
}"""


ZWAVE_LOOP = """    if (m_apDSBuffer == NULL)
        return FALSE;
    for (DWORD i = 0; i < m_dwNumBuffers; ++i)"""
ZWAVE_LOOP_PORTED = """    if (m_apDSBuffer == NULL)
        return FALSE;
    // th095-port: index declared before the loop; the reconstruction read it
    // after the loop, which its own scope made ill-formed.
    DWORD i;
    for (i = 0; i < m_dwNumBuffers; ++i)"""

PBG_HOIST = """    PbgArchiveEntry *pbgReadEntry = FindEntry(filename);
    if (pbgReadEntry == NULL)
        goto entry_read_error;

    i32 pbgReadCompressedSize =
        pbgReadEntry[1].dataOffset - pbgReadEntry->dataOffset;
    u32 pbgReadDecompressedSize = pbgReadEntry->decompressedSize;"""
PBG_HOIST_PORTED = """    // th095-port: declarations hoisted above the gotos (crossing initialized
    // declarations is forbidden); values unchanged.
    PbgArchiveEntry *pbgReadEntry;
    i32 pbgReadCompressedSize;
    u32 pbgReadDecompressedSize;
    u8 pbgReadProfileIndex;
    pbgReadEntry = FindEntry(filename);
    if (pbgReadEntry == NULL)
        goto entry_read_error;

    pbgReadCompressedSize =
        pbgReadEntry[1].dataOffset - pbgReadEntry->dataOffset;
    pbgReadDecompressedSize = pbgReadEntry->decompressedSize;"""
PBG_ASSIGN = """    u8 pbgReadProfileIndex =
        CalculateByteChecksum((u8 *)pbgReadEntry->filename,
                              (i32)strlen(pbgReadEntry->filename)) % 8;"""
PBG_ASSIGN_PORTED = """    pbgReadProfileIndex = CalculateByteChecksum((u8 *)pbgReadEntry->filename,
                                                (i32)strlen(pbgReadEntry->filename)) % 8;"""

BACKGROUND_OBJECT = """        BackgroundStageObject *resultDrawBacking096 =
            reinterpret_cast<BackgroundStateView *>(this)
                ->stageObjects[instance->objectId];
        if (resultDrawBacking096->mode != mode)
            goto nextInstance;"""
BACKGROUND_OBJECT_PORTED = """        // th095-port: declarations hoisted above the gotos.
        BackgroundStageObject *resultDrawBacking096;
        BackgroundStageObjectInstruction *resultDrawBacking157;
        resultDrawBacking096 =
            reinterpret_cast<BackgroundStateView *>(this)
                ->stageObjects[instance->objectId];
        if (resultDrawBacking096->mode != mode)
            goto nextInstance;"""
BACKGROUND_INSTRUCTION = """        resultDrawBacking096->flags |= 2;
        BackgroundStageObjectInstruction *resultDrawBacking157 =
            &resultDrawBacking096->firstInstruction;"""
BACKGROUND_INSTRUCTION_PORTED = """        resultDrawBacking096->flags |= 2;
        resultDrawBacking157 = &resultDrawBacking096->firstInstruction;"""

BACKGROUND_POSITION_SCOPE = """    {
        Float3 ancestralProjectSrc;
        while (instance->objectId >= 0)"""
BACKGROUND_POSITION_SCOPE_PORTED = """    {
        Float3 ancestralProjectSrc;
        Float3 instancePosition; // th095-port: declared above the gotos
        while (instance->objectId >= 0)"""
BACKGROUND_POSITION = """        Float3 instancePosition(
            instance->position.x, instance->position.y,
            instance->position.z);"""
BACKGROUND_POSITION_PORTED = """        // th095-port: declared above the gotos; assigned here (C++ forbids
        // jumping over a non-trivial initialization).
        instancePosition.x = instance->position.x;
        instancePosition.y = instance->position.y;
        instancePosition.z = instance->position.z;"""

MAIN_BEGINTHREAD = """            _beginthread((void (__cdecl *)(void *))Supervisor::ScreenshotThread,
                         0, NULL);"""
MAIN_BEGINTHREAD_PORTED = """            _beginthread((void (__cdecl *)(void *))Supervisor::ScreenshotThread,
                         0, 0, NULL); // th095-port: 4-arg MSVC shape"""

PHOTOSTAGE_TRAILING = "    u8 trailing[0x2214 - 0x10c8 - 6 * sizeof(AnmVm)];"
PHOTOSTAGE_TRAILING_PORTED = (
    "    // th095-port: the 64-bit AnmVm exceeds the 32-bit row padding;\n"
    "    // clamp the residual size at zero.\n"
    "    u8 trailing[(0x2214 - 0x10c8) > 6 * (i32)sizeof(AnmVm)\n"
    "                ? 0x2214 - 0x10c8 - 6 * sizeof(AnmVm)\n"
    "                : 0];"
)

PORT_PATCHES = [
    (ZWAVE_LOOP, ZWAVE_LOOP_PORTED),
    (PBG_HOIST, PBG_HOIST_PORTED),
    (PBG_ASSIGN, PBG_ASSIGN_PORTED),
    (BACKGROUND_OBJECT, BACKGROUND_OBJECT_PORTED),
    (BACKGROUND_INSTRUCTION, BACKGROUND_INSTRUCTION_PORTED),
    (BACKGROUND_POSITION_SCOPE, BACKGROUND_POSITION_SCOPE_PORTED),
    (BACKGROUND_POSITION, BACKGROUND_POSITION_PORTED),
    (PHOTOSTAGE_TRAILING, PHOTOSTAGE_TRAILING_PORTED),
    (MAIN_BEGINTHREAD, MAIN_BEGINTHREAD_PORTED),
]


def patch_file(path: Path) -> int:
    text = path.read_text(encoding="utf-8", errors="surrogateescape")
    changed = False

    if str(path).endswith("zwave.cpp"):
        if ZWAVE_LOOP in text:
            text = text.replace(ZWAVE_LOOP, ZWAVE_LOOP_PORTED)
            changed = True
        if "#include <stdlib.h>" not in text and "#include <stddef.h>" in text:
            text = text.replace("#include <stddef.h>", "#include <stddef.h>\n#include <stdlib.h>")
            changed = True

    for original, ported in PORT_PATCHES:
        if original in text:
            text = text.replace(original, ported)
            changed = True

    if "th095-port: 32-bit layout assert disabled" not in text:
        patched, count = ASSERT_TYPEDEF.subn(lambda m: REPLACEMENT.format(name=m.group(1)), text)
        if count:
            text = patched
            changed = True

    if "TH08_MODERN_PORT" in text:
        text = text.replace("TH08_MODERN_PORT", "TH095_MODERN_PORT")
        changed = True

    if FROM_ANGLE_MAGNITUDE_ASM in text:
        text = text.replace(FROM_ANGLE_MAGNITUDE_ASM, FROM_ANGLE_MAGNITUDE_PORTED)
        changed = True

    if changed:
        path.write_text(text, encoding="utf-8", errors="surrogateescape")
    return 1 if changed else 0


def main() -> None:
    if len(sys.argv) != 2:
        raise SystemExit(__doc__)
    root = Path(sys.argv[1])
    if not (root / "src").is_dir():
        raise SystemExit(f"not a TH095 checkout (missing src/): {root}")

    files = 0
    for path in sorted(root.rglob("*.hpp")) + sorted(root.rglob("*.cpp")) + sorted(root.rglob("*.h")) + sorted(root.rglob("*.inl")):
        if ".git" in path.parts:
            continue
        if patch_file(path):
            files += 1
            print(f"[patch_upstream] patched {path.relative_to(root)}")
    print(f"[patch_upstream] done: {files} file(s) patched.")


if __name__ == "__main__":
    main()
