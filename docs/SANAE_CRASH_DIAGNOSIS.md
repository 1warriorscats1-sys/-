# SANAE first-build crash: diagnosis and repairs

## Exact identification (2026-09-08)

User reported a new-game Data Abort before gameplay, PC `+0xbe908`, LR `+0xbe8c8`.
First NRO module ID:

```text
8562093015BBCBCD089A6523D7FFC5862E637B8F000000000000000000000000
```

[Diagnostic Actions run](https://github.com/1warriorscats1-sys/-/actions/runs/34225995053)
rebuilt source `d9011335c3bf612165ce91c6783e91aedffcdb86` with the same toolchain image.
**The rebuilt NRO's module ID matched the report exactly.** The job emitted build-ID,
addr2line and objdump evidence in check-run annotations (check run `102060140494`).
This is not symbolication against a differently linked follow-up executable.

```text
+0xbe908  maPlaySound, al_audio_system.c:462
+0xbe8c8  maPlaySound, al_audio_system.c:451
be8fc: ldr   x0, [x20, #16]          ; audioGroups array
be900: ldrsw x2, [x24, #56]          ; sound.audioGroup
be904: ldr   x0, [x0, x2, lsl #3]    ; group pointer
be908: ldr   w2, [x0, #664]          ; group->audo.count, faulting access
```

Upstream `maGroupLoad` appended parsed archives with `arrput`, but playback and duration
lookup indexed that array by the original group ID. Loading groups out of order or
skipping an empty/missing group produced wrong entries or out-of-bounds reads.
`maGroupIsLoaded` also incorrectly treated array length as proof of loading.

## Implemented repair

- Allocate the declared group-index table, explicitly initialize unloaded slots to null,
  and keep the main data file at index zero.
- Store each archive at its actual group ID, independently of load order.
- Make repeat loads idempotent; check the requested slot for loaded status.
- Reject invalid/negative/out-of-range/unloaded groups and invalid audio entries before
  allocating playback handles. Duration queries also check group availability.
- Check payload availability before playback; clean up buffered OpenAL handles on failure.
- Preserve buffered sound loop state in the voice record as well as OpenAL.
- Do not disable audio or rewrite the original game data to hide the error.

The regression harness calls the patched OpenAL functions with synthetic parser/AL doubles:
loads groups 11, 2, 17, repeats 11, requests missing group 3 and invalid IDs, then plays an
embedded sample in group 11. It checks loaded-state accuracy, successful upload, no extra
allocations on invalid input and cleanup after injected AL failure. The harness passed
with address/undefined-behavior sanitizers. It is not a real audio device test.

## Exit-transition repair

The loop previously continued through rendering after `Runner_step` set `shouldExit`.
The event dispatcher skips normal events after that flag, including Draw/GUI fading, but
the renderer could still clear/composite the scene and swap it: a partial/background frame.
The patch exits the frame loop before drawing after a Step exit, prevents presentation if
exit is requested during Draw, and avoids a pending room transition after exit. The last
complete frame remains until normal teardown. Game End/save flushing still runs normally.
A truth-table test covers the presentation guard. Hardware rechecking of the reported
flash remains necessary; no artificial delay or permanently black screen is substituted.

## Icon, metadata and diagnostics

- Original procedural 256x256 RGB JPEG wind emblem, drawn from MIT source without Steam
  art or third-party fonts, embedded with `nx_create_nro ICON`.
- NACP: `SANAE - Sylphid Breeze`, author `sorehodoh`, display version `01.01`.
- Builder parses the produced NRO's ASET/NACP and decodes its JPEG to verify the icon,
  actual metadata and bounds before allowing publication.
- Switch logs now go to stderr (the stream redirected to `sanae.log`), rather than
  stdout. The previous logger bypassed that redirection.
- Exact ELF remains in `sanae-symbols.zip` for later crash analysis. Do not use a newer
  ELF to decode the first-build offsets without verifying its build ID.

Bluetooth Pro Controller and return to hbmenu were confirmed by the user for the first
build. Seeing a save file after a crash is not proof of successful gameplay/progress saves.
No claim of complete gameplay, correct original CSV semantics, or absence of other bugs
is made until the repaired build is tested on the user's Switch with all original files.
