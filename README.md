> **Compatibility status (2026-09-08):** floor collision and restart input have
> source repairs; the black HP/ability HUD is now reproduced and repaired in a host
> GLES pixel test. The new Switch build compiled successfully and requires a hardware retest. This is not
> a full-game/no-bugs guarantee. See [the investigation](docs/SANAE_GAMEPLAY_COMPATIBILITY.md).

# SANAE's Sylphid Breeze — Switch Port Tools

![Platform](https://img.shields.io/badge/Platform-Nintendo_Switch-e60012?style=for-the-badge)
![Distribution](https://img.shields.io/badge/Distribution-Source_Only-238636?style=for-the-badge)
![Data](https://img.shields.io/badge/Game_Data-Bring_Your_Own-218bff?style=for-the-badge)

**Prepare your own Steam game files for a compatible SANAE Switch port.**
Local conversion, checked SD-card layout, and source for the save/exit compatibility patches.

[Source releases](../../releases) · [Installation (English)](docs/INSTALL.md) · [Установка на русском](docs/INSTALL.ru.md) · [Technical notes](docs/TECHNICAL.md) · [Distribution policy](docs/DISTRIBUTION.md)

---

## Experimental route: an open NRO that reads original Steam files

**[SANAE open NRO integration](open-runner/README.md)** is a compatibility experiment,
not a demonstrated replacement for the working NSP. It uses Butterscotch as its independent engine. SANAE-specific save/exit, CSV
and audio additions are implemented and host-tested; the build recipe names the output
`sanae.nro` and uses `sd:/switch/sanae/`. Original `data.win` is read without conversion.
**New experimental NRO 01.01 compiled successfully: array floor collisions, equal-depth HUD drawing and controller rediscovery repaired.**
[Download the build artifact](https://github.com/1warriorscats1-sys/-/actions/runs/34238437036/artifacts/10060951334)
(GitHub sign-in may be required). Original CSV/audio validation, rendering and real
Pro/Joy-Con/save/exit tests remain outstanding; this is not a hardware-verified release.
See [project comparison and licensing rationale](docs/OPEN_RUNNER_RESEARCH.md).

## Third port scaffolding: TH095 — Shoot the Bullet

**[東方文花帖 ~ Shoot the Bullet (th095) Switch port](th095/README.md)**
uses a different base than the two projects above: the TH095 *native
Win32* engine from the [N0zoM1z0/th095 source reconstruction](https://github.com/N0zoM1z0/th095),
ported with the TH06/TH07/TH08-recipe (saekaze): untouched game code plus a
Win32/D3D8/DirectSound compatibility layer (`th095/port/`), fake DX headers,
and a pinned-upstream build (`th095/scripts/`). Phase 1 (full 64-bit
compile of all 88 upstream TUs + complete platform-symbol coverage + host
CI) is done; the Switch runtime/NRO is in progress. Source-only, bring your
own 1.02a game data — see [th095/README.md](th095/README.md).

## Second port scaffolding: Wonderful Waking World

**[Touhou Nemuri Sekai ~ Wonderful Waking World (thWWW)](wonderful-waking-world/README.md)**
reuses the same open-engine recipe in its own folder: integration overlay, build
script, NRO asset verifier, host tests and CI producing `thwww.nro` for
`sd:/switch/thwww/`. Both CI jobs pass and the Switch job produces a
verified `thwww.nro`, but it has **never been run on hardware**. No game data or artwork is included; the player supplies the
original files from their own copy.

The instructions below describe the **existing legacy proprietary-runtime toolkit**, not
installation of a finished open NRO. Do not mix the two engines' data preparation or saves.

## Legacy toolkit: what it is — and isn't

This is a **source-only compatibility toolkit**, not a standalone open-source game engine.
Unlike the TH06 native port, this implementation depends on a **proprietary GameMaker Switch runtime**.
Buying SANAE on Steam provides the PC game data, **not** that runtime or permission to redistribute it.

**No game files, Nintendo/GameMaker runtime, SDK, keys, NSP, NRO, or patched executable are shipped or downloaded.**
A legally obtained, compatible Switch application/runtime is a separate prerequisite. If you only have
Steam files, you can prepare the data, but **this toolkit alone cannot make a bootable Switch application**.
We do not provide a way to obtain the proprietary runtime.

## Features

- **Bring your own Steam copy.** Select the folder opened by Steam → Manage → Browse local files.
- **One local preparation step.** The tool converts `data.win`, adapts optional Steamworks calls for the
  Switch environment, verifies the converted structures, and copies the required audio archives and dialogue CSV files.
- **No manual renaming or hex editing.** The output is a ready-to-copy `atmosphere` directory.
- **Original files stay untouched.** Preparation uses a temporary directory, checks hashes, and refuses
  to overwrite an existing output folder. It never writes to your SD card automatically.
- **Save and exit patch source.** Deferred platform exit and guarded save commits are retained.
  The user who tested the prior binary patch reported that quitting now works on their Switch.
- **Offline operation.** Data preparation uses only Python's standard library. No account login,
  telemetry, game download, runtime download, or key download.

## Quick start

**Prerequisites:** your own SANAE PC game files, Python **3.10+**, and — for actual Switch play — an
independently and lawfully obtained compatible SANAE Switch application (runtime **2024.14.3.260**,
Title ID **`010000000005A1E1`**). This project does not supply that application.

1. Download the source with GitHub's **Code → Download ZIP**, then extract it.
2. In Steam, open SANAE → **Manage → Browse local files**. Keep Steam/game updates closed while preparing.
3. **Windows:** double-click `Prepare-SANAE.cmd` and choose that folder. You can also drag the folder onto
   the `.cmd` file. **Linux/macOS:**
   ```sh
   ./prepare-sanae.sh "/path/to/SANAE's Sylphid Breeze"
   ```
4. After successful preparation, close the Switch application via **HOME → X**, back up existing SD files,
   then merge `sdcard-pack/atmosphere` into the SD root.
5. Launch your compatible application. Confirm audio, saving, return to title, and quitting.

```text
sdcard-pack/                        # Generated locally; do NOT share
├── atmosphere/contents/010000000005A1E1/romfs/
│   ├── game.win                    # Converted from your data.win
│   ├── audiogroup2.dat              # Copied from your Steam installation
│   ├── …                           # Required external audio groups
│   ├── scenario_sanae.csv           # Original Japanese dialogue
│   └── scenario_sanae_en.csv        # Original English dialogue
├── INSTALL.txt
├── manifest.json                   # Input/output SHA-256; no PC folder paths
└── prepare.log
```

All available `audiogroup1.dat` through `audiogroup18.dat` are copied. Only groups referenced by sounds
are mandatory; the known build has an empty group 1. There is **no executable** in this output.
Do not replace saves or delete the existing `exefs` directory. Both dialogue CSV files are required;
`scenaorio_sanae.csv` (the game's spelling) and `item.txt` are also copied if present.

**Original Steam data without PC conversion:** see the [loader design and remaining blockers](docs/ORIGINAL_DATA.md).
This is a planned runtime feature, not functionality provided by the current release.

## Why `data.win` cannot just be copied

The PC build uses an older GameMaker data layout. The Switch runner expects additional room creation-order
and audio-path fields. Renaming `data.win` to `game.win` does **not** add them.
The preparation tool performs the conversion **on your computer**, then checks the result.
It also adapts optional Steam achievement/status/shutdown calls because those APIs are unavailable on Switch;
it does not patch the PC executable, acquire a license, or implement a DRM bypass.

Supported data: the known SANAE VM layout (bytecode 17, 104 rooms, 19 audio groups, 1,568 sprites).
Unknown layouts or failed conversion steps stop with an error, rather than produce an unchecked pack.
An updated Steam build may require an update to these tools. See [compatibility limits](docs/TECHNICAL.md).

## Testing & development

```sh
# Public, synthetic-data tests — no game or SDK required
python3 -m unittest discover -s tests -v
python3 scripts/audit_distribution.py

# Optional private runner tests; never upload the input or generated output
python3 -m venv .venv
.venv/bin/pip install -r requirements-patch.txt
SANAE_RUNTIME_MAIN="/your/lawful/runtime/main" .venv/bin/python -m unittest discover -s tests -v
```

The private integration tests skip when their dependencies or user-supplied runtime are absent.
No CI job downloads commercial data. The SANAE workflow is installed in `.github/workflows/`;
its first run passed the host tests and compiled the Switch NRO. See the open-runner guide
for the artifact, exact source commit and remaining hardware validation.

## Credits & rights

- **sorehodoh** — original SANAE's Sylphid Breeze game.
- **ZUN / Team Shanghai Alice** — Touhou Project.
- **GameMaker and Nintendo** — their respective technology and trademarks; no affiliation or endorsement.
- **saekaze's TH06 Switch repository** — inspiration for the documentation structure, not the engine used here.
- Existing port/conversion scripts were imported from the SANAE work in `saekaze/th8test`; see [NOTICE](NOTICE.md).

Please purchase and support the original game. Keep all generated packs and runtime patches private.
This policy is not a legal opinion about your local jurisdiction or a grant of rights to third-party software.
See [licensing scope](LICENSE.md) and [historical cleanup limitations](docs/DISTRIBUTION.md).
