# Open runner track: Butterscotch → SANAE NRO

**Status: research/probe build, not a released working SANAE Switch port.**

This is a concrete alternative to the legacy proprietary-runner port. Upstream
[Butterscotch](https://github.com/ButterscotchRunner/Butterscotch) is an independent
GameMaker runner reimplementation, has an AGPL-3.0 license and a native libnx Switch
backend. Its Switch entry point reads the original `data.win` directly. No commercial
GameMaker NX runtime, Nintendo SDK binary, or converted game file is required by this engine.

The upstream revision is pinned in `upstream.json`. No upstream code is relicensed by this
repository; the build helper only obtains that public, licensed source and builds it.
Any distribution of a resulting executable must comply with AGPL and dependency licenses,
including providing corresponding source. Game files remain the user's private inputs.

## Intended player experience

After compatibility work and hardware validation, the goal is:

```text
sd:/switch/butterscotch/
    butterscotch.nro          # Open engine; project source/license supplied with release
    data.win                 # ORIGINAL Steam file, not a renamed converted game.win
    audiogroup2.dat … audiogroup17.dat
    scenario_sanae.csv
    scenario_sanae_en.csv
    … other original Included Files if present
```

This uses the upstream hardcoded folder for now; a SANAE-branded path/name can be added later.
Launch from hbmenu in an appropriate full-memory application context. An NSP forwarder is
not needed for the initial target and would not itself add compatibility. No PC conversion
is part of this intended open-engine route. This is NOT the installation procedure for the
old proprietary NSP, and the two engines do not necessarily share save formats/locations.
Do not delete your working installation or reuse its saves for these experiments.

## What has actually been checked

On 2026-09-08, the unmodified upstream commit in `upstream.json` was built locally for
Linux with the **no-op graphics and no-op audio backends**. The supplied original Steam file:

- size: 22,542,558 bytes
- SHA-256: `94893a08f434e0698c2cc46bf0414b97ef033d13330efc127475c6a525a8cd07`

was loaded directly, without our convert_data or patch_steam scripts:

- Initial room: `Room_00_title`, ran through frame 300.
- With synthetic Z-key presses: switched to `Room_s00m01_tutorial`, ran through frame 600.
- Process returned 0 with LeakSanitizer disabled for the smoke test.
- SHA-256 of the original input remained unchanged; no converted data file was generated.

**This is execution of initialization/update logic, not proof of rendering, sound, correct
dialogues, collisions, saves, performance, controller behavior or complete gameplay.**
The no-op renderer does not exercise normal draw behavior. Missing external audio/CSV inputs
also mean this test is not a full-game test. The debug build reported small exit-time leaks
(192 bytes in the initial one-frame run); they are not silently counted as a clean ASAN run.
The loader reports GameMaker 2024.8 for this file, whereas the existing format inspection finds
2024.11 markers. That discrepancy needs checking, especially for font/glyph layout.

## Current concrete gaps

Observed unknown calls include `load_csv`, `audio_group_set_gain`, `audio_group_stop_all`,
`window_set_position`, `steam_update`, and `steam_get_achievement`. The whole-game static
inventory also lists `instance_furthest`, `audio_sound_loop`, `audio_sound_get_loop`, and
other functions. Not every statically listed extension call runs during gameplay; each
reachable call needs proper implementation or an explicit platform-appropriate behavior.
Unknown functions returning placeholders are not evidence of working support.

Next work:

1. Implement the game's CSV extension semantics from the user's game behavior/inputs.
2. Implement required audio-group and looping operations in the open audio backend.
3. Check version detection, shaders, QOI/BZip2 textures, fonts, layers, collision and draw events.
4. Implement explicitly supported offline Steam API behavior without pretending a Steam license.
5. Build the Switch target and verify visuals/audio, Pro/Joy-Con input, saving, restart and exit.
6. Only then publish a SANAE-labelled, hardware-tested NRO with complete corresponding source.

Upstream Switch input already uses libnx `HidNpadStyleSet_NpadStandard`, `padInitializeDefault`
and eight pad slots. This is a different input implementation from the old proprietary runner;
the unfinished IPS controller experiment in the workspace is not a patch for Butterscotch.
No Pro Controller success is claimed yet.

## Reproduce the build (developers)

Requirements for host build: Git, C compiler, Python 3.10+, CMake 3.21+.

```sh
python3 scripts/build_open_runner.py --target headless
.cache/open-runner-build-headless/butterscotch /your/private/steam/data.win \
  --headless --exit-at-frame 300 --save-folder /your/private/test-saves \
  --disable-log-colours
```

For an **upstream Switch probe**, install the open devkitPro toolchain (`devkitA64`, `libnx`,
Switch CMake tooling and the SDL2/Mesa/OpenAL portlibs used by upstream), set `DEVKITPRO`, then:

```sh
python3 scripts/build_open_runner.py --target switch
```

The helper downloads ONLY the pinned open-source engine, never game data or a closed runtime.
Switch compilation has not been completed in this workspace: devkitPro is not installed here.
Upstream's matching CI run is [34204341816](https://github.com/ButterscotchRunner/Butterscotch/actions/runs/34204341816)
and reports success, but that does not test SANAE. Artifact retrieval from its storage endpoint
failed in this sandbox. No NRO from that run has been repackaged or claimed as our finished port.
