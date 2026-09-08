# SANAE compatibility investigation — 2026-09-08

This is an independent WIP interpreter, not the proprietary runner from the old
NSP. The old exit/controller binary patches cannot supply the old runner's VM,
rendering, audio and GameMaker 2024 compatibility to this engine. No complete
compatibility or hardware-fix claim is supported by the following tests.

## Confirmed causes and source repairs

* `instance_place(x,y,[object,...])`: upstream converts the target array to object
  zero. The adapter iterates scalar targets through the existing grid/precise-mask
  collision implementation. This fixes the API generally, not one particular
  tutorial floor. Public native tests cover scalar, array, parent/descendant,
  instance ID, inactive target, miss, empty input and position preservation.
* `game_restart`: game DS lists and instances are destroyed, but the physical
  controller remains connected. The replacement padmanager's list stays empty
  because no fresh discovery event fires. A one-shot rediscovery flag dispatches
  to the newly created instances without altering the Pro/Joy-Con mappings.
  Merely resetting `connectedPrev` fails because beginFrame overwrites it.

## Private original-data probes (not distributable fixtures)

Using the unchanged original Steam data.win with SHA-256
`94893a08f434e0698c2cc46bf0414b97ef033d13330efc127475c6a525a8cd07`:

* At frame 600 before the collision repair, player `(114,715.299)` was below the
  tutorial floor. After repair: `(114,174.40004)`, `mode_ground=1`.
* Keyboard playback enters gameplay, opens pause with Escape, selects title with
  Right twice, confirms with Z twice, and reaches room 0 again. Keyboard menu
  navigation works after restart; that did not prove gamepad navigation.
* A private no-op platform test supplies one continuously connected gamepad,
  then a Down edge around frame 840 and Plus around frame 900 after restart.
  With rediscovery suppressed, title remains `gamestart` at frames 800/880/1000.
  With the repair, it progresses `gamestart` → `option` → `language_jp` (`mode_opt=1`).
  This exercises original padmanager GML with simulated native input, not libnx
  radio/hardware. Test hooks are not included in the Switch source overlay.

External CSV/audio are unavailable locally. No-op rendering cannot prove pixels,
normal progression, save continuation, or later-game stability.

## Black HP and adjacent ability portrait — now reproduced and repaired on host

The owner clarified that both HP and the adjacent ability image are black. This
is not a request for a replacement UI graphic. Inspect the shared rendering path.

The original draw-GUI events call draw_self, select actual sprites, and at frame
600 the portrait has white blend, alpha 1, scale 1 and valid frame/position. The
original TXTR decoder successfully produces colored portrait pixels from page 6
and floor pixels from page 7. Neither evidence proves the Switch GL upload or
render state is correct. The subsequent GLES reproduction below identifies the draw-order defect; no replacement graphics are used.

Floor sprites are 1,070 room asset-layer sprites, not a missing tilemap. Falling
out of the camera is a possible contributor to the visual floor report; collision
repair alone does not prove rendering is fixed.

## Cover and validation

The converter uses the exact owner-requested cover URL, pads instead of cropping,
and generates a 256×256 RGB baseline JPEG. A failed download fails the build;
there is no silent fallback to another image. Tests use synthetic artwork and do
not access the network. The sandbox cannot fetch the actual CDN image directly; GitHub Actions successfully
fetched it, built the NRO and passed the mandatory embedded JPEG/NACP validation. Artwork has separate rights, see
`open-runner/ARTWORK_NOTICE.txt`.

Public Python suite: 51 tests, 40 passed, 11 skipped in the local Pillow venv.
Integrated native collision/save/window tests and ASan/UBSan OpenAL harness passed.
These are host checks, not a fully sanitized VM or a Switch gameplay test.

A public synthetic runner test also executes `Runner_reset`, `beginFrame` and two
real `Runner_step` calls: two connected pads produce exactly two async discovery
maps on the first step and no additional discoveries on the second step.


## Latest compilation, not a completed gameplay repair

Source: `926902987ab7c52b02655c37b50308f583c1dc73`.
CI: https://github.com/1warriorscats1-sys/-/actions/runs/34230377990 (success).
Experimental artifact: https://github.com/1warriorscats1-sys/-/actions/runs/34230377990/artifacts/10057572517
(18,712,918 bytes). It includes the NRO package, corresponding source, BUILD.json
and ELF symbol sidecar. That build predates the HUD repair below and is not
advertised as the requested fully compatible replacement for the working NSP.

## Scope correction: “the same fixes as the NSP”

The owner explicitly rejects an indefinite stream of individual gameplay repairs.
The following must not be conflated:

* `scripts/runner_exit.py` patches verified AArch64 instruction sites in the
  proprietary 2024.14.3.260 runner (including its `game_end` epilogue and platform
  loop). It deliberately rejects other binaries. Those sites and SDK imports do
  not exist in the independent interpreter.
* Deferred exit and save flushing can be implemented equivalently at the policy
  level, but they do not implement GameMaker's VM or renderer.
* The array-target collision repair is an API-level fix, not an invisible wall
  added to one room. Restart discovery is a lifecycle fix, not a menu-key hack.
  Neither establishes compatibility with the rest of the game.
* Making an NSP forwarder for this NRO would retain the same engine bugs.
* Reusing the original runtime may retain its compatibility, but no permission
  to distribute that runtime has been established. A publicly distributable
  equivalent would require an authorized export/runtime arrangement, or an
  independently implemented engine validated against the game's required behavior.

No supported one-shot transplant of the old binary patches into this engine has
been found. Do not patch HUD pixels, inject replacement portraits, or advertise
another compilation as meeting the requested equivalence. The GLES reproduction below supplies renderer-level evidence for this HUD defect;
valid decoded source pixels alone would not have been enough. Full acceptance also requires gameplay progression, abilities, save/load,
restart/controller reconnection and clean exit on Switch, not only room entry.

## GLES pixel reproduction and HUD repair

A private Linux pbuffer test now runs the **actual modern GL renderer with a GLES
context**, using ANGLE/SwiftShader libraries from `@sparticuz/chromium` 149.0.0.
It uses the unchanged original data hash above, no-op audio, a fixed 1280×720
pbuffer/window and the same window-setter policy as Switch. No game/runtime or
third-party graphics binaries are added to the public tree. The diagnostic EGL
backend is not part of the Switch build. This is software GLES, **not Switch GPU
or controller hardware validation**.

The old comparator sorts equal-depth instances by descending instance ID.
SANAE creates its HUD back-to-front: frame 101389, portrait 101390, HP frame 101391,
HP fill 101392. All are depth zero. Consequently the opaque black frame interiors
are drawn over the already drawn portrait and red health bar. The source textures
and GL upload are not the cause of this reproduced black HUD.

The overlay changes **only the equal-depth instance tie** to ascending ID. Depth
precedence, drawable-type precedence, tile order, layer ties and particle-system
ties are retained. This is not a sprite replacement, hidden floor, color override,
or HUD-object-specific depth hack. It applies to the shared draw sorting path.

At original-data playback frame 600:

| Measurement | Before HUD repair | After HUD repair |
|---|---:|---:|
| Red HP pixels in rectangle (230,672)–(510,700) | 0 | 7,840 |
| Non-black portrait pixels in rectangle (60,580)–(180,690) | 0 | 12,140 |
| Player position / grounded | (114,174.40004), true | same |

The world crop (0,0)–(1280,490) is pixel-identical across these two runs. The floor
is visible with the array-collision repair, and the player remains on it.
Private full-frame PNG SHA-256:

* Before: `efeb2e003c8900b0fba2ded4cc8fa2f7e90244cfa3071b4245cae73bd10c4ea1`
* After: `e3678e8351fd9cb3b9ef6a2c6cce876803e3fd055e6f47ae92005f82df6c2e5d`

GLES screenshots also checked pause (550), title confirmation (650), restarted
title (800), and settings after keyboard navigation (1000). Portraits, text and
menus are visible; this does not establish later-level correctness or real audio.

`tests/native/sanae_draw_order.c` compiles the **actual patched runner.c comparator
and sorted-cache check** against synthetic instances. It verifies back-to-front
GUI ties, depth priority including extreme depths, and retained tile/layer/particle
ordering. The same fixture fails with the previous comparator (negative control).
The integrated tests pass; Python suite: 51 total, 40 passed, 11 skipped locally.
The OpenAL harness passes ASan/UBSan; full GL/VM sanitizer cleanliness is not claimed.

New Switch compilation and hardware verification must be reported separately.

## Compiled HUD/floor repair (latest)

* Binary source: `0616d04cd3139383a8a3b67760cc9f6c566b242c`.
* CI success: https://github.com/1warriorscats1-sys/-/actions/runs/34238437036
* Artifact: https://github.com/1warriorscats1-sys/-/actions/runs/34238437036/artifacts/10060951334
* Artifact metadata: `SANAE-experimental-switch`, 18,714,564 bytes, non-expired
  at publication. Both host tests and the Switch build passed; the build enforces
  embedded JPEG/NACP verification. This does not replace hardware testing.

Additional actual-GLES checks: a right/jump playback moves the player to
`(174,123.40003)`, airborne at frame 450, then `(214,174.40004)`, grounded at
frame 600. Natural title quit calls `game_end` at frame 290, exits with status 0,
and does not render a frame after that call. External audio/CSV, later levels,
real controller radio behavior and full save continuation are still not established.
