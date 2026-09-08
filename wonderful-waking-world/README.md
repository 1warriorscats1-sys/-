# Touhou Nemuri Sekai ~ Wonderful Waking World — experimental open NRO

Port scaffolding for **東方眠世界 ~ Wonderful Waking World** (thWWW) by Oligarchomp,
built the same way as the SANAE port in [`open-runner/`](../open-runner/README.md):
the open [Butterscotch](https://github.com/ButterscotchRunner/Butterscotch) GameMaker
runner is compiled for Switch with a small, game-specific integration overlay.

> **Status (2026-09-08): compiles, untested on hardware.** Both CI jobs pass:
> the engine builds on Linux with the integration applied, and the Switch job
> produces a `thwww.nro` whose embedded icon and NACP are verified. The overlay
> applies cleanly to the pinned upstream commit, and the save/exit code passes
> native tests under ASan/UBSan. Nothing has been run on a Switch, and no
> gameplay, rendering or audio behaviour has been observed.

## What this is — and isn't

- **Is:** an integration overlay (entry point, controller map, checked saves,
  clean-exit guard), a reproducible build script, an NRO asset verifier and CI.
- **Isn't:** a finished port, a bug-free build, or anything containing game data.
  No `data.win`, artwork, music or proprietary runtime is stored or downloaded.
  The game itself is free from Oligarchomp; you supply your own copy's files.

## Layout

| Path | Purpose |
| --- | --- |
| `www/main.c` | libnx entry point; checks `sdmc:/switch/thwww/data.win`, sets up saves and logging |
| `www/www_switch_mapping.inc` | Touhou-convention pad layout; right stick disabled |
| `www/www_save.h` | write–fsync–rename save replacement with one retained generation |
| `www/www_frame_policy.h` | suppress presenting a partial frame during `game_end` |
| `THWWW_INSTALL.txt` | end-user SD-card instructions, shipped inside the zip |
| `../scripts/apply_www_overlay.py` | applies the overlay to a disposable engine copy; fails on upstream drift |
| `../scripts/build_thwww.py` | fetches the pinned engine, patches, builds, packages source + symbols |
| `../scripts/create_www_icon.py` | 256×256 NRO icon (procedural placeholder by default) |
| `../scripts/verify_www_nro.py` | validates the real NRO's embedded icon and NACP |
| `../tests/test_thwww.py` | host tests for the icon, NACP verifier and overlay wiring |
| `../tests/native/www_save.c` | functional tests: save rotation, crash recovery, frame policy |
| `../scripts/test_thwww_native.py` | compiles and runs those tests, optionally sanitized |

The engine revision is pinned in [`../open-runner/upstream.json`](../open-runner/upstream.json),
shared with the SANAE port so both builds track one audited upstream commit.

## Build

```bash
python3 -m unittest tests.test_thwww -v            # host checks, no toolchain needed
python3 scripts/test_thwww_native.py --sanitize    # save/exit tests under ASan+UBSan
python3 scripts/build_thwww.py --target headless   # engine + integration on Linux
python3 scripts/build_thwww.py --target switch     # needs devkitPro/devkitA64/libnx
```

Output lands in `dist/thwww/switch/`: `thwww.nro`, the installable zip,
`thwww-source.tar.gz` (exact patched AGPL source) and `thwww-symbols.zip`.
CI runs both targets — see [`../.github/workflows/thwww-switch.yml`](../.github/workflows/thwww-switch.yml).

## Icon

`create_www_icon.py` draws a neutral procedural emblem, because no game artwork is
redistributed here. To embed the real cover on your own build, point it at a local
file you are permitted to use:

```bash
THWWW_ICON_SOURCE=/path/to/cover.png python3 scripts/build_thwww.py --target switch
```

## Licensing

The integration code in this folder is MIT (see [`../LICENSE.md`](../LICENSE.md)).
The linked result is **AGPL-3.0** because Butterscotch is AGPL: anyone distributing
a binary must ship the corresponding `thwww-source.tar.gz`. Wonderful Waking World
is a Touhou Project derivative work by Oligarchomp and is not covered by either
license; nothing of it is included here.

## Open work

- **Never run on hardware.** A green build is not a compatibility result.
- Audio-group indexing and draw ordering carry no thWWW-specific repairs yet —
  the SANAE port needed several before it behaved, and this game may need its own.
- The artifact could not be inspected from this sandbox (Azure blob storage is
  firewalled), so only CI's own NRO icon/NACP verification vouches for it.
- Controller layout is a reasonable default, not one confirmed against the
  game's own input handling.
- No rendering, performance or full-playthrough verification on hardware.
