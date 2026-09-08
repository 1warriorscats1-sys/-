# SANAE's Sylphid Breeze - Nintendo Switch port

> Exit fix: see [SANAE_EXIT_FIX.md](SANAE_EXIT_FIX.md). The NSP checksum below
> describes the imported old package; install the new ZIP over it.

A standalone installable Switch title (own icon, own title ID) built around
GameMaker's Switch runner, plus the game's own data file converted to the
format that runner expects. Not a LayeredFS override of another game.

| | |
|---|---|
| Title | SANAE's Sylphid Breeze |
| Publisher | sorehodoh |
| Title ID | `010000000005A1E1` |
| Runner | GameMaker NX **2024.14.3.260** |
| Game build | GameMaker **2024.11** (converted, see below) |
| `SANAEs_Sylphid_Breeze.nsp` | 34,670,816 B - sha256 `2efe49ed2d4050643f2fa76e15138a71a3a3ce1b030171c1541b4ba6588c3248` (build 4) |
| `game.win` | 22,575,962 B - sha256 `f8b816d0bea0cf35b6f6eb793fc49cf0aa948806a646abd245310ee2f727097f` |

## Install

1. Install `SANAEs_Sylphid_Breeze.nsp` with any NSP installer on Atmosphère
   (DBI, Goldleaf, Awoo, Tinfoil). Sigpatches must be active, as for any
   homebrew NSP.
2. Copy the data onto the SD card:

   ```
   sdcard:/atmosphere/contents/010000000005A1E1/romfs/
       game.win           <- from this repository
       audiogroup1.dat
       audiogroup2.dat
       ...                <- audiogroup1.dat ... audiogroup18.dat from the PC game folder
   ```

   `game.win` is your `data.win` after the conversion described below - a plain
   rename of `data.win` will not work. The audiogroup files are copied as-is.
   Nothing else from the PC folder is needed.
3. Launch it from the home menu (a user profile is required, so pick one).

## Why the data file has to be converted

The game is built with GameMaker **2024.11**. The newest Switch runner that
exists in the wild is **2024.14**, and the data format changed twice in
between. Both changes were confirmed by disassembling the runner itself, not
guessed:

| Change | Runner code | Symptom with an unconverted file |
|---|---|---|
| 2024.14: audio groups gained a `Path` string | AGRP loader reads `ldp w10, w9, [x9]` and builds a `std::string` from the second word | data abort in `strlen`, before the first frame |
| 2024.13: rooms gained an `InstanceCreationOrderIDs` pointer after `Tiles`, and the room flags level moved from `0x30000` to `0x40000` | `cmp w27, #0x40000 / b.lt` gates the new field; the layer pointer is read at a fixed `room+0x5c` | `instance_create_layer :: specified layer "Instances" does not exist`, and - with the field present but the flags left alone - no Create event runs at all, so every object reads unset variables |

`scripts/convert_data.py` performs both upgrades without moving a single
existing byte: the new room headers, the per-room creation order lists (5508
instances across 104 rooms, taken from each room's flat GameObjects list, which
is exactly the order pre-2024.13 runners used implicitly), the 19 audio-group
records and their `audiogroup1.dat` ... `audiogroup18.dat` path strings are
appended at the end of the file, and only the ROOM and AGRP pointer arrays are
repointed.

Everything else that changed between 2024.11 and 2024.14 was checked against
UndertaleModTool's format definitions and against this game's data, and does
not apply: no backgrounds (2024.14.1), no sequences (2024.13/14), no particle
system instances or room text items (2024.14), no vector sprites (2024.14),
1568/1568 sprites are ordinary, and the font/chunk padding changes only affect
trailing bytes that the runner never reads. Textures are QOI+BZip2 (`2zoq`) and
the shader is GLSL ES - both supported by the runner.

## Steam calls

The game ships the Steamworks extension (149 `steam_*` functions, none of which
exist on Switch) and `obj_achieve_steam` - present in 75 rooms including the
title screen - polls `steam_update()` and `steam_get_achievement()` every frame.
`scripts/patch_steam.py` disarms the five call sites that run during normal
play by replacing only the call's **opcode word** with a branch over it. The
operand word is left untouched, because GameMaker threads every call site of a
function into a linked list through exactly that word and the runner walks
those lists at load time - all 15,425 function and 79,286 variable references
were re-walked after patching and are intact.

Result: `flg_steam_api` stays 0, which is the same state the game reaches on a
PC without Steam running.

## The title itself

* ExeFS - the 2024.14.3.260 runner (`main`, `main.npdm`, `rtld`, `sdk`,
  `subsdk0`, `subsdk1`). `main.npdm` is retargeted to `010000000005A1E1`
  (ACI0 program id + ACID program-id range) and hacPack re-signs the ACID.
  `main` carries three patches from `scripts/patch_nso.py`, each located by
  instruction pattern rather than a fixed offset:
  * an audio group with a NULL path becomes an empty string instead of
    crashing in `strlen`;
  * the "deprecated builtin" gate (`ldrb w8,[x8]` / `cbz w8, error`) is
    nop'ed. 2024.13+ refuses to run `instance_change` and `position_change`
    unless a flag that only exists in 2024.13+ data files is set, and this game
    calls `instance_change` from 18 room controllers - which made the tutorial
    boss unbeatable ("Calling instance_change when it is marked as
    deprecated");
  * `MountRom` failure is no longer fatal, so `game_restart()` (the pause
    menu's "return to title") cannot be killed by a second romfs mount;
  * the global-init re-run on `game_restart()` is skipped when its cached list
    pointer is NULL, instead of dereferencing null;
  * **the quit button really quits.** `game_end()` only clears the runner's
    "running" flag; the main loop then spins forever, which on console is a
    black screen. The old patch called C `exit(0)` from the handler. The new patch sets
    a private flag, returns through the original epilogue, and requests
    `nn::oe::ExitApplication` at the next platform-loop iteration, after a
    guarded save commit. See `SANAE_EXIT_FIX.md` for validation limits.
  * **saves survive closing the game.** The runner commits save data only from
    a few GML-facing helpers the game never calls, so everything it wrote
    stayed in the journal and was dropped when the console closed the
    application. A stub wraps `nn::fs::CloseFile` in the file layer and
    commits right after it. Two traps had to be avoided: calling
    `nn::fs::CommitSaveData` directly aborts the process when nothing is
    mounted (fs 2002-6905, crashes at boot while reading romfs), and the
    runner's own commit helper *unmounts* the save afterwards, which kills
    every later write. The stub therefore reuses the runner's guard flag and
    commits without unmounting.

* RomFS - `options.ini` and `preselected_user`; the game data is layered on top
  from the SD card.
* Logo - blank, so no third-party boot logo appears.
* Control - EN/JA NACP (`scripts/control.xml`: required user account, 4 MiB
  account save with a 1.5 MiB journal, CERO 12 / ESRB 10) and a 256x256 icon
  cropped from the game's Steam library capsule.
* 2024.14.3.260 was chosen over the newer 2024.14.4.268 because the latter is
  flagged as having broken saving; both parse the data identically (verified by
  diffing their parsers - same flag gates, same field offsets).

## Reproducing

```sh
scripts/build.sh path/to/data.win      # -> SANAEs_Sylphid_Breeze.nsp + game.win
```

It builds hacPack/hactool from source, assembles and signs the NSP, converts
and patches the data file, and finally runs `scripts/verify_data.py`, which
re-reads the finished file the way the runner does - same offsets, same flag
gates - and fails if anything the runner touches is off.

External inputs (runner binaries, artwork) live in `build-inputs/` and are
fetched by `.github/workflows/fetch-inputs.yml`, because the build sandbox can
only reach GitHub.
