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

## Open: black HP and adjacent ability portrait

The owner clarified that both HP and the adjacent ability image are black. This
is not a request for a replacement UI graphic. Inspect the shared rendering path.

The original draw-GUI events call draw_self, select actual sprites, and at frame
600 the portrait has white blend, alpha 1, scale 1 and valid frame/position. The
original TXTR decoder successfully produces colored portrait pixels from page 6
and floor pixels from page 7. Neither evidence proves the Switch GL upload or
render state is correct. No speculative HUD patch has been applied.

Floor sprites are 1,070 room asset-layer sprites, not a missing tilemap. Falling
out of the camera is a possible contributor to the visual floor report; collision
repair alone does not prove rendering is fixed.

## Cover and validation

The converter uses the exact owner-requested cover URL, pads instead of cropping,
and generates a 256×256 RGB baseline JPEG. A failed download fails the build;
there is no silent fallback to another image. Tests use synthetic artwork and do
not access the network. The sandbox cannot fetch the actual CDN image; CI fetch
and NRO validation remain pending. Artwork has separate rights, see
`open-runner/ARTWORK_NOTICE.txt`.

Public Python suite: 51 tests, 40 passed, 11 skipped in the local Pillow venv.
Integrated native collision/save/window tests and ASan/UBSan OpenAL harness passed.
These are host checks, not a fully sanitized VM or a Switch gameplay test.
