# SANAE — experimental open NRO integration

**Status (2026-09-08): first SANAE NRO successfully compiled in GitHub Actions.
Host tests passed. Switch hardware compatibility is NOT verified.**

- [Download experimental build artifact](https://github.com/1warriorscats1-sys/-/actions/runs/34224440055/artifacts/10055121900)
  (GitHub sign-in may be required; Actions artifacts expire).
- [Successful build and both jobs](https://github.com/1warriorscats1-sys/-/actions/runs/34224440055)
- Built source commit: `d9011335c3bf612165ce91c6783e91aedffcdb86`.

The artifact contains the Switch zip, matching corresponding-source archive and BUILD.json.
Its existence, size (5,867,275 bytes) and non-expired status were confirmed through the
GitHub API. Downloading it back into this sandbox failed at GitHub's blob-storage endpoint,
so no local binary inspection or hardware test is claimed.

This is a port of **SANAE's Sylphid Breeze**, not WWW or another game.
[Butterscotch](https://github.com/ButterscotchRunner/Butterscotch) is the independent
open engine used underneath it. It reads the user's original Steam `data.win` without
conversion. The engine revision is pinned in `upstream.json`; our additions are in `sanae/`.
No proprietary GameMaker/Nintendo runtime or game payload is supplied.

## Latest follow-up build: 01.01

[Download 01.01](https://github.com/1warriorscats1-sys/-/actions/runs/34225607505/artifacts/10055608060)
— [successful CI run](https://github.com/1warriorscats1-sys/-/actions/runs/34225607505),
source `388a460857a9c84521df7a210a2b75dfe3dd9dea`. Host and Switch jobs passed;
artifact existence/non-expiry confirmed through GitHub API. This build disables window
controls and updates metadata; it does **not** claim to fix the new-game crash, missing
icon or exit-transition flash. Only replace `switch/sanae/sanae.nro`; preserve game data
and saves. `sanae-symbols.zip` is for debugging, not SD installation.

## Hardware feedback and follow-up

The user tested the first NRO: **Nintendo Bluetooth Pro Controller works and the exit
returns**, but a brief unwanted frame appears during the exit transition. Starting a new
game crashes before gameplay. A save file exists afterward, which does **not** establish
correct progress persistence. Reported module ID:
`8562093015BBCBCD089A6523D7FFC5862E637B8F000000000000000000000000`;
PC `+0xbe908`, LR `+0xbe8c8`, Data Abort. Root cause is not yet symbolicated or fixed.
The first artifact did not retain its ELF; future builds package the exact ELF separately
in `sanae-symbols.zip` with line information. These new symbols cannot be used as though
they matched the old build's offsets.

Follow-up: Switch window size/rectangle/position/fullscreen/region-size/centering calls
are no-ops. Physical size getters remain intact and fullscreen reports true. Surface and
GUI sizing remain untouched. A host regression test verifies the same registration policy
blocks backend resize calls. This is **not yet a hardware-verified flicker fix**, nor a fix
for the new-game crash or the exit transition. Menu labels/settings may still change in
game logic; the runtime ignores the window manipulation itself.
NACP version is now `01.01`, author credit `sorehodoh` (original game developer).
The missing icon remains outstanding.

## SANAE-specific changes

- Switch entry point and NACP name identify SANAE; application directory is
  `sdmc:/switch/sanae`, with a separate `saves/` directory and `sanae.log` diagnostics.
- Entry point checks the original data file, both dialogue CSVs and audio archives 2–17.
  A missing-file/error screen allows return with Plus instead of continuing blindly.
- INI/text replacement uses checked write, `fflush`, `fsync`, checked close, and a retained
  `.bak` generation. An interrupted replacement with no current file recovers the backup.
  Deliberate deletion also deletes that backup to prevent resurrection.
- A failed INI/text close retains pending data for retry rather than caching/discarding it.
  Implicit INI close writes pending changes. Shutdown flushes pending INI/text buffers;
  write errors are logged and cause an error return, not a fabricated save-success result.
- Game End events are dispatched once before audio/renderer teardown, including when
  `game_end` already set the exit flag. Normal execution then returns through libnx.
  **This is new source logic, not the user-confirmed legacy machine-code exit patch.**
- Bounded CSV-to-DS-grid loader supports quoted/escaped fields, multiline fields, BOM,
  CRLF and ragged rows, preserving strings. The game's real CSV format and extension
  typing semantics still need checking with the missing original files.
- Fixed an upstream `ds_grid_set` argument guard that rejected normal four-argument calls.
- Added OpenAL group gain with timed fades and persistence for future voices, group stop,
  and looping controls for active buffered/streaming voices. Tests use OpenAL doubles,
  **not an audio device or the game's real archives**.
- Optional non-Steam availability/achievement queries return false; update/shutdown do
  nothing on this non-Steam port. Switch desktop-window positioning is a no-op.

The existing libnx input backend uses `NpadStandard`, the default handheld/player-one pad
and eight player slots. **Bluetooth Pro and Joy-Con behavior remains unverified on hardware.**
The legacy controller IPS draft is unrelated and is not applied to this engine.

### Save and exit limits

Backup rotation is not a guarantee of atomic SD metadata updates under power loss.
HOME force-close, crashes, removal of the SD card and power failure cannot run orderly
shutdown code. Native binary file handles do not use the INI/text backup scheme.
The upstream filesystem still allows some explicit absolute-path operations; it is not
advertised here as a hardened security sandbox. The host tests exercise relative game
save names, including SANAE's observed `game_setting.ini` and `save_data01.ini`.
Keep the old installation/saves untouched; automatic legacy-save migration is not provided.
A write error is conservatively remembered for the current run even if a later retry works.

## Player layout (experimental build)

```text
sd:/switch/sanae/
    sanae.nro
    data.win                         # ORIGINAL Steam file, unchanged
    audiogroup2.dat … audiogroup17.dat
    scenario_sanae.csv
    scenario_sanae_en.csv
    … other original Included Files if supplied
    saves/                           # Created by the port, do not reuse old NSP saves
```

See `SANAE_INSTALL.txt`. Launch via hbmenu with application/full-memory access.
No PC conversion or newly generated `data.win` is part of this route. An NSP forwarder
would not itself fix compatibility and is not needed for this initial NRO target.

## What has actually passed

Local Linux tests on 2026-09-08:

1. Public suite: **45 tests, 34 passed, 11 private-input tests skipped**. The new native
   helper harness also passed with AddressSanitizer and UndefinedBehaviorSanitizer:
   CSV parsing/bounds, gain envelopes and injected allocation, rename, flush and close failures.
2. Full Release headless build with the SANAE overlay, using no-op graphics/audio.
3. Synthetic fixtures through the actual VM/filesystem: INI reopen, implicit close,
   failed-close retry with old file preserved, backup recovery/deletion, pending INI/text
   flush after `game_end`, CSV grid contents and four-argument `ds_grid_set`.
4. OpenAL doubles: group gain isolation/persistence/fade, loop changes and group stop.
   The full patched OpenAL translation unit also passed host syntax checking.
5. Two launches of the supplied **unchanged** original Steam data: title room → tutorial
   (`Room_s00m01_tutorial`, 82 instances), 600 frames, normal process status 0.
   Original SHA-256 remains
   `94893a08f434e0698c2cc46bf0414b97ef033d13330efc127475c6a525a8cd07`
   (22,542,558 bytes). The save directory contains game settings and completion-rate INIs.

**These are NOT proof of playable rendering, dialogue, sound, collision correctness,
performance, real game-menu exit, Pro Controller support, or Switch save persistence.**
CSV loading reports the missing `scenario_sanae.csv` during the original-data host probe;
continuing to the tutorial despite that error is not counted as working dialogue support.
Desktop headless probes still report unsupported window positioning (the no-op is Switch-only).
No assertion is made that all unknown chunks/functions or other engine bugs are resolved.
The earlier unmodified Debug runner also reported small exit-time leaks; helper sanitizer
success must not be confused with a clean sanitizer run of the entire engine.

## Build and reproduce (developers)

Host requirements: Linux, Git, Python 3.10+, C compiler, Make, CMake 3.21+.

```sh
SANAE_SANITIZE=1 python3 -m unittest discover -s tests -v
python3 scripts/build_sanae.py --target headless
.cache/sanae-build-headless/butterscotch /your/private/Steam/data.win \
  --headless --exit-at-frame 300 --save-folder /your/private/test-saves \
  --disable-log-colours
```

The builder obtains a clean pinned upstream checkout, makes a disposable source copy,
checks every source-edit anchor, applies SANAE integration and builds/tests it. It never
modifies that upstream checkout or downloads game files. `--source` can select an already
cached clean checkout. `--cmake` selects a CMake executable.

For Switch, install devkitPro/devkitA64/libnx, Switch CMake tooling and the upstream
SDL2/Mesa/OpenAL portlibs, set `DEVKITPRO`, then:

```sh
python3 scripts/build_sanae.py --target switch
```

Expected outputs **only after successful compilation**:

```text
dist/sanae/switch/sanae.nro
dist/sanae/switch/SANAE-experimental-switch.zip
dist/sanae/switch/sanae-source.tar.gz
dist/sanae/switch/BUILD.json
```

`--prepare-only` prepares the SANAE source without claiming to compile anything.
`ci/sanae-switch.yml` is an Actions workflow template; copying it to
`.github/workflows/` requires workflow-write permission on the GitHub connection.
It runs on an activation push changing that workflow on this session branch, not
on ordinary source pushes. Manual dispatch is also declared; GitHub may require
the workflow to exist on the default branch before enabling that dispatch.
The template builds/tests the headless integration before building the NRO. Its devkitPro
container uses `latest`, so only the engine source is pinned, not the entire toolchain.

Local Switch compilation remains unavailable (no devkitPro; package/registry TLS failures),
but GitHub workflow-write permission was subsequently granted. The workflow is now installed
at `.github/workflows/sanae-switch.yml`; its activation push successfully built SANAE.
The integration still cannot dispatch workflows through the API (HTTP 403 for the attempted
manual dispatch); the successful run above was triggered by the push, not a manual dispatch.

`build_open_runner.py` remains available for **unmodified upstream comparison probes**;
use `build_sanae.py` for the SANAE-specific target.

## Licensing / distribution

The fetched engine is AGPL-3.0; original integration additions have the toolkit's MIT
notice and are combined with that AGPL engine, not used to relicense it. Preserve all
upstream/dependency notices. Publish the **matching exact patched source archive alongside
any binary zip**. The archive contains the patched `engine/`, integration sources, tests,
build helpers and `BUILD.json`. One can build `engine/` directly using the recorded CMake
options and the required platform toolchain; no game file is needed to compile it.

No private original/converted Steam data, proprietary executable, key or Nintendo SDK
binary is packaged. The current source tree is separate from the repository's older Git
history, which still contains previous uploads and is not made clean by deleting a file
from HEAD. For a clean public repository, export audited source rather than that history.
