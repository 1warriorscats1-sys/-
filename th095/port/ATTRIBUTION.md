# Attribution

## Upstream game source

`N0zoM1z0/th095` — "Source reconstruction of 東方文花帖 ~ Shoot the Bullet
1.02a", MIT-licensed repository-authored code (no game data). Pinned commit
recorded in `../upstream.json`. Fetched by `../scripts/fetch_upstream.py`,
patched in place by `../scripts/patch_upstream.py` (every patch is
documented in that file's docstring).

## Compatibility layer lineage

The port layer in this directory is adapted from the TH08 Switch port:

* `N0zoM1z0/th08` — `src/modern/linux/` (fake Win32/D3D8 headers,
  `linux_compat.cpp`, `d3d8_compat.cpp`, `d3dx8_compat.cpp`,
  `linux_runtime.cpp`), MIT.
* `saekaze/th08-switch` — the Switch-side layer and packaging approach
  (NACP, devkitA64 CMake branch, data-folder discovery, crash reporter),
  MIT.

Adaptation notes (th08 → th095):

* All `th08_*` / `TH08*` identifiers renamed to `th095_*` / `TH095*`.
* `d3d8_compat.cpp`: the TH08 dialogue-snapshot hook in `BeginScene()`
  (which reached into `th08::g_Gui`) is disabled — TH095's GUI state lives
  in different structs; dialogues render over live scenes for now.
* `win32_compat.cpp`: added `_beginthread`/`_beginthreadex` (pthread
  backends), `SetTextColor`, `sincosf` (absent from newlib),
  `c_dfDIJoystick2`, `DI8DEVCLASS_GAMECTRL`; `EnumDevices` takes the
  device-class GUID by value to match the reconstruction's call shape.
* `d3dx8_compat.cpp`: added `D3DXLoadSurfaceFromMemory` (raw-pixel upload
  with format decode and scaling, per the reconstruction's 10-argument
  call shape) and `D3DXVec4Transform`.
* `port/include/`: extended fake headers — `D3DXVECTOR4`, `D3DX_PI`,
  D3DX filter constants, `DIK_L`, `VK_NUMPAD4/5`, `BI_BITFIELDS`,
  `IShellLinkA` + shell-link CLSIDs/IIDs, `<process.h>`/`<direct.h>`
  shims, `C_ASSERT` compiled out (32-bit layout checks).

The host `linux_runtime.cpp` is a fresh, smaller take on TH08's
(`--data-dir` discovery, crash reporter, no target-data aliases — TH095
defines its own production owners in `Main.cpp` under `#ifndef DIFFBUILD`).
