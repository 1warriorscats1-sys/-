# TH095 (東方文花帖 ~ Shoot the Bullet) — Switch Port

Native homebrew port of **Touhou 9.5** (東方文花帖 ～ Shoot the Bullet, 1.02a)
for the **Nintendo Switch (Horizon OS)**, built on the source reconstruction
[N0zoM1z0/th095](https://github.com/N0zoM1z0/th095).

This project follows the same recipe as the TH06/TH07/TH08 Switch ports
(saekaze): the untouched decompiled game code keeps calling Win32/D3D8/
DirectSound/DirectInput/GDI by their original names, and the platform
difference is confined to the compatibility layer in `port/`.

> ⚠️ **Status: bring-up (Phase 1 complete, Phase 2 in progress).**
> Phase 1 — the pinned upstream reconstruction (88 translation units) now
> compiles cleanly against the port layer for 64-bit platforms, and every
> platform symbol the game references is implemented by `port/`. The host
> CI job builds the full x86-64 Linux binary.
> Phase 2 — the Switch runtime (`port/runtime/switch_runtime.cpp`), the
> GLES3 renderer path, NACP/icon, and the CI NRO job are in progress.
> **There is no runnable Switch NRO in this folder yet.**

---

## How it works

| Layer | File(s) | Purpose |
| :-- | :-- | :-- |
| Fake Win32/DX headers | `port/include/` | `windows.h`, `d3d8.h`, `d3dx8.h`, `dsound.h`, `dinput.h`, `mmsystem.h`, `shobjidl.h`, ... — the exact API surface the reconstruction uses, declared for the port. Extended from the TH08 port's set (MIT). |
| Force-include | `port/compat/th095_compat.hpp` | Maps MSVC spellings (`__fastcall`, `__forceinline`), disables 32-bit `C_ASSERT` layout checks, and pulls in the fake headers for every game TU. |
| Win32 platform layer | `port/compat/win32_compat.cpp` | Files, windows/messages, keyboard, timers, threads (incl. `_beginthread`/`_beginthreadex`), GDI text with CP932 + Japanese fonts, DirectInput, winmm, **DirectSound on SDL audio**. Adapted from the TH08 port (MIT). |
| D3D8 renderer | `port/compat/d3d8_compat.cpp`, `d3dx8_compat.cpp` | FBO-backed D3D8 device, textures, surfaces, D3DX math + `D3DXLoadSurfaceFromMemory`, GPU `CopyRects`. Adapted from the TH08 port (MIT). |
| Runtime | `port/runtime/linux_runtime.cpp` | Host entry point, crash reporter (`th095-crash.txt`), data-directory discovery. |
| Upstream source | fetched, not vendored | `scripts/fetch_upstream.py` pins `upstream.json` → `build-inputs/th095-src`; `scripts/patch_upstream.py` applies the documented port patches (see that file: 32-bit asserts, x87 asm guards, goto/declaration fixes). |

## Data (bring your own)

Original game files of a legally obtained TH095 1.02a installation:
`th095.dat`, `thbgm.dat`, `msgothic.ttc` (font), plus `th095.cfg`,
`scoreth095.dat` and the `replays/` folder as generated. No game data,
fonts or artwork are distributed here.

```text
sd:/switch/th095/
    ├── th095.nro          # (Phase 2) the homebrew executable
    ├── th095.dat          # main game archive (your copy)
    ├── thbgm.dat          # BGM archive (your copy)
    └── msgothic.ttc       # Japanese font (ships with the Windows release)
```

MIDI BGM is not available on Horizon (no system synthesizer); the game's
WAV BGM path is supported (see the TH06 port's `bgm/` folder approach).

## Controls (planned, fixed in code)

| Button | Action |
| :-- | :-- |
| Left stick / D-pad | Move |
| A | Shoot / Confirm |
| B | Bomb |
| L | Focus (slow-motion) |
| Z | Pause / In-game menu |

## Building (host, Phase 1)

```sh
python3 th095/scripts/fetch_upstream.py
python3 th095/scripts/patch_upstream.py build-inputs/th095-src
cmake -S th095 -B th095/build-host -DCMAKE_BUILD_TYPE=Release \
  -DTH095_UPSTREAM_SRC=$PWD/build-inputs/th095-src
cmake --build th095/build-host -j"$(nproc)"
```

Requires: gcc/g++, cmake, ninja, SDL2 + SDL2_ttf + SDL2_image, OpenGL,
fontconfig (see `.github/workflows/th095-build.yml`).

Run (with game data in `build-inputs/th095-data`):

```sh
th095/build-host/th095-host --data-dir build-inputs/th095-data
```

## Layout

```text
th095/
├── CMakeLists.txt          # host + (Phase 2) NINTENDO_SWITCH branches
├── README.md
├── TH095_INSTALL.txt       # user-facing installation instructions
├── upstream.json           # pinned N0zoM1z0/th095 commit
├── ci/                     # (legacy mirror; live workflow in .github/)
├── port/
│   ├── ATTRIBUTION.md
│   ├── include/            # fake Win32/DX headers
│   ├── compat/             # Win32/D3D8/DirectSound/GDI compatibility
│   └── runtime/            # linux_runtime.cpp (+ switch_runtime.cpp, Phase 2)
└── scripts/
    ├── fetch_upstream.py   # pinned upstream fetch → build-inputs/
    └── patch_upstream.py   # documented port source patches
```

## Credits & rights

- **ZUN / Team Shanghai Alice** — original game; all rights to the game and
  its assets remain with the original holders. No license is granted to the
  game data by this repository.
- **[N0zoM1z0](https://github.com/N0zoM1z0/th095)** — the TH095 source
  reconstruction (MIT for repository-authored code).
- **[saekaze](https://github.com/saekaze/th08-switch)** and
  **[N0zoM1z0](https://github.com/N0zoM1z0/th08)** — the TH08 port whose
  compatibility layer this project adapts (MIT).
- **Switchbrew & devkitPro** — libnx SDK and toolchain.
