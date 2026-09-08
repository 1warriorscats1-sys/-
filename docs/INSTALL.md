# Install using your own game files

## Before starting

This toolkit is not a replacement game engine. You need:

- Your legally owned **SANAE's Sylphid Breeze** Steam installation, containing `data.win`, audio archives, `scenario_sanae.csv` and `scenario_sanae_en.csv`.
- Python **3.10 or newer** from python.org. On Windows, enable **Add Python to PATH**.
  The folder picker uses Tkinter (included with typical Windows Python installers).
  Linux users without Tkinter can pass the folder in a command instead.
- For playing, an independently and lawfully obtained compatible SANAE Switch application:
  GameMaker runtime **2024.14.3.260**, Title ID **010000000005A1E1**, with the relevant compatibility patches.
  It is **not included**, and ownership of a Steam game does not provide a Switch runtime license.
- An environment supporting the application's SD RomFS overrides. No protection-bypass files are supplied here.

If you do not have the compatible Switch application, stop before the launch step. The source kit does not
build a free replacement runtime, NRO, or NSP. Installing only the generated data pack will not create a HOME icon.

## Prepare

1. Extract this repository's current source ZIP to a writable folder.
2. Steam → SANAE → Manage → Browse local files. Verify the folder contains `data.win`.
3. Close the PC game and pause updates. Do not move or rename the original files.
4. Windows: open `Prepare-SANAE.cmd`, select the game folder, and read the completion/error message.
   Alternatively drag the game folder onto that launcher.
5. Linux/macOS:
   ```sh
   ./prepare-sanae.sh "/path/to/Steam/steamapps/common/SANAE"
   ```
   All platforms also support:
   ```sh
   python scripts/prepare_sd.py "/path/to/SANAE" --output "/path/to/new-sdcard-pack"
   ```

Preparation is offline and may take a while when copying audio. Leave the terminal open.
The destination must not already exist or overlap the game folder. For a second run, choose a new destination,
e.g. `--output sdcard-pack-2`; do not discard a working pack before verifying the new one.

Only supported VM data is accepted. A known previously converted file is recognized by SHA-256 and not patched
twice. For other inputs, every conversion and validation step must succeed. There is no silent fallback.

## Copy and test

- Close the Switch game with HOME → X.
- Back up the current `atmosphere/contents/010000000005A1E1/romfs` files.
- Merge **only the generated `atmosphere` folder** into the SD root. Do not copy the enclosing `sdcard-pack` folder.
- Keep the existing installed application, `exefs`, and saves. This preparation does not update the executable.
- Launch, check music and voices, change a setting/save, quit, and relaunch. Also check return-to-title.

If you already use the working exit fix, leave it in place. Source for private reproduction is described in
[EXIT_FIX.md](EXIT_FIX.md). No patched runner binary is distributed anymore.

## Troubleshooting

| Message/symptom | What to do |
|---|---|
| Missing `data.win` | Select the game folder, not the Steam executable, shortcut or downloaded ZIP. |
| Missing `scenario_sanae.csv` / `scenario_sanae_en.csv` | Verify your Steam installation; these external dialogue files are not inside `data.win`. |
| Missing `audiogroupN.dat` | Verify your own Steam installation files; do not download audio packs from third parties. |
| Unsupported revision / parser rejection | Keep your old working pack. Report the tool error and game version; do not attach game data. |
| Output already exists | Choose another `--output` folder. Existing files are intentionally never replaced. |
| No HOME icon / cannot launch | The pack is data only; it does not supply or install the proprietary application. |
| Game crashes / no voices | Confirm the exact application/runtime and that all output files reached the correct SD folder. |
| No Python / no folder picker | Install Python 3.10+ or use the command-line invocation. |

You may share the error message, game version and hashes. Inspect logs/crash reports for personal information
before posting them. Do not attach `data.win`, generated `game.win`, audio, EXE, NSP, SDK, keys or ExeFS binaries.
