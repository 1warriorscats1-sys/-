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

## Requested Switch controls adjustment

The Switch-only mapping now exchanges physical A/B while retaining the previous
X/Y mapping: B → gp_face1, A → gp_face2, Y → gp_face3, X → gp_face4.
The right stick is entirely inert: both axes, its click (R3), and the libnx
StickR directional bits that previously leaked through `HidNpadButton_Any*`.
Left-stick axes/directions, D-pad, L/R/ZL/ZR, Plus/Minus, L3, controller enumeration
and reconnect/restart handling are unchanged. Existing in-game glyph artwork is
not relabeled by this patch.

`tests/native/sanae_switch_mapping.c` exercises the actual mapping include with
synthetic libnx inputs, including simultaneous right-stick and valid input.
The public suite passes locally: 52 total, 41 passed, 11 skipped; native mapping
and helpers also pass ASan/UBSan. Real controller verification remains necessary.

Controls build passed both CI jobs: https://github.com/1warriorscats1-sys/-/actions/runs/34241537490
Binary source `3bce43114549a0dbdac6d0681d5929214a958621`.
Download: https://github.com/1warriorscats1-sys/-/actions/runs/34241537490/artifacts/10062243168
Artifact 18,717,015 bytes, non-expired at publication. This is a compiled/tested
control-policy change, not a fresh hardware confirmation.

## Contact damage and suction: bilateral collision dispatch repair

Reproduced with unmodified private Steam data, not inferred from a compilation:

* In the old slot-major responder order, the enemy contact handler set its own
  damage flag before the player's inherited handler tested that flag. Contact
  therefore left the player's HP at 100.
* The enemy suction handler transformed/destroyed the enemy before the reverse
  notification updated Sanae's capture state. Sorting responders alone repaired
  ordinary enemies but was insufficient: a suction object created during the
  collision phase could encounter a catcher whose bucket had already run.
  Hina/Chen capture then lost the reverse notification at this second collision.
* Collision responders now have deterministic descending concrete-resource
  order. Each overlapping instance pair resolves its most-specific inherited
  handlers, invokes both once in that order, and is recorded in a frame-local
  pair set. Notification is not conditional on solidity or a second overlap
  test after movement/destruction. Both descriptors are captured before either
  handler changes state. Newly created collision participants follow the same
  pair ordering even if their partner's bucket was already visited.
* Existing precise masks, solid rollback/path handling, pending-room guards,
  game-end policy, non-collision event order and input mapping are retained.
  No HP amount, timer, capture eligibility, game script or data.win is patched.

The public GameMaker HTML5 `scripts/Events.js` in
https://github.com/YoYoGames/GameMaker-HTML5 documents bilateral notification
without a solid-only condition. That is supporting event-semantics evidence,
not proof that every platform/version uses the same resource ordering. The
ordering policy here is validated against SANAE's original script handshakes;
we do not claim universal GameMaker or complete-game parity.

### Checks with original game scripts (private diagnostic harness)

An env-gated host-only probe spawned an original enemy spawner at frame 600 near
Sanae; its original scripts created the actual enemy. No game variables or
balance values were forced. The hook and game assets are not shipped.

* Contact: HP **100 → 92** at frame 610; original hurt/invulnerability state
  finishes by frame 700. A second spawned contact gives **84** at frame 810;
  normal state returns by 900. No permanent invulnerability or manual HP edit.
* Capture: eleven spawners (`e010`, `e011`, `e020`, `e021`, `e100`, `e110`,
  `e120`, `es01`, `es02`, `es03`, `es04_tutorial`) all produce `mode_catch=1`,
  `vacuum_count=1`, `catch_count=1` at frame 610.
* Down input consumes the capture. Original scripts set the Cirno, Kogasa, Hina
  and Chen ability identifiers respectively; counters/state clear normally.
  This verifies acquisition, not every attack or later boss encounter.
* The no-enemy GLES frame-600 screenshot is byte-identical to the previously
  repaired floor/HUD screenshot: SHA-256
  `e3678e8351fd9cb3b9ef6a2c6cce876803e3fd055e6f47ae92005f82df6c2e5d`.

Public synthetic tests run the actual `Runner_step` / VM call path, with original
synthetic bytecode rather than game content. They cover inherited handler owner,
child-target precedence, exactly-once notification, both creation orders,
movement/destruction in the first handler, a new lower-index instance after its
partner's bucket, solid contacts, one-sided handlers and misses. A negative
control without the ordering repair failed the contact-order assertion.
The local Python suite remains 52 tests (41 passed, 11 skipped); native combat,
window/save/restart/draw tests and sanitized audio tests pass. These are host
checks, not confirmation on physical Switch hardware or a full playthrough.

### Published combat build

Source `d650385e2785d701e980f4f3727ea71fdc562f0f`.
Both Linux/native checks and Switch compilation passed:
https://github.com/1warriorscats1-sys/-/actions/runs/34244356105

Artifact `SANAE-experimental-switch`, ID `10063396009`, 18,721,350 bytes,
non-expired at publication:
https://github.com/1warriorscats1-sys/-/actions/runs/34244356105/artifacts/10063396009

The Switch CI includes the mandatory NACP/embedded-cover verification. The
sandbox could not independently download the completed artifact (blob download
returned EOF); its publication and size were confirmed through the GitHub API.
This is the combat build, not the earlier controls-only artifact.

## Subsequent broad audit

See [the full audit](SANAE_AUDIT_2026-09-08.md) for additional sanitizer-confirmed
VM/hash undefined behavior and buffer cleanup fixes, 104 room-entry probes,
96,000 input-stress frames, death/restart/exit and GLES smoke tests. Full-engine
sanitizers now gate the build. The audit explicitly does not establish full-game
or physical Switch parity.

Latest audited binary source: `70f0eef8edfc96956e50998c4033ca624ba38677`.
Successful CI: https://github.com/1warriorscats1-sys/-/actions/runs/34246955442
Artifact: https://github.com/1warriorscats1-sys/-/actions/runs/34246955442/artifacts/10064457261
(18,722,289 bytes, non-expired at publication).

## Multi-subscription collisions, umbrella HP bar, full target-array parity

Two owner reports stayed open after the combat repair: the second (Marisa)
playable character deals no contact damage, and the umbrella durability bar
renders black/invisible instead of the purple HP fill. Both now have confirmed
engine causes and source repairs; hardware confirmation still requires the
Switch artifact below.

* Missing contact damage: when one instance held several collision handlers
  matching the same partner (for example the partner's object and its parent),
  dispatch kept only the most specific subscription and skipped the rest, so a
  contact-damage handler could never run. Pairs are now booked once per frame
  while every matching subscription on both sides fires exactly once; the
  reverse side still joins the first notification, preserving capture-before-
  destruction. Native tests cover both orders, motion/self-destruction during
  the first handler, late spawning, solids, one-sided handlers and misses.
* Umbrella HP bar: `draw_healthbar` always filled left-to-right, ignored the
  fill direction, never drew the black border, and read the optional
  background/border flags out of bounds. The replacement implements all four
  directions, clamped amount, optional background and border; native tests
  record every rectangle through a renderer spy.
* Target arrays generalized from `instance_place` to all nineteen
  collision/instance builtins (`place/meeting`, `collision_*`,
  `*_list`, `distance_to_object`, `instance_nearest/furthest/exists/destroy`):
  scalar behavior is unchanged, first hitting element wins, `*_list` unions
  dedupe shared descendants, and `ordered=true` sorts by caller distance
  (previously ignored, silently returning grid order).
* `instance_exists`/`instance_destroy` previously crashed or mis-resolved
  `self`/`other`/`all`/array targets (`exists(all)` reported whether object
  -3 exists; `destroy` dereferenced small ids as object indices). Both are
  reimplemented with snapshot iteration so destroy events that spawn or
  destroy cannot corrupt the loop.
* New builtins: `json_parse`/`json_stringify` (struct/array JSON with a depth
  cap; the legacy `json_decode`/`json_encode` cover only ds containers),
  `string_width_ext` (was a zero stub collapsing wrapped-text layout),
  `show_error` (logs; abort requests a clean exit instead of running past a
  fatal game path), `to_string`, `texturegroup_*`, texture/sprite
  prefetch/flush shims, `draw_texture_flush`, `draw_enable_drawevent`
  (suppresses only normal Draw events; Begin/End/GUI unaffected),
  `random_get_seed` (backed by a new `Random.lastSeed`), and
  `display_get_width/height` reporting the real window with game-default
  fallback.
* `random_set_seed(seed)` read a second argument unconditionally; guarded.
* Switch pad 0 followed the system default pad while slots 1-7 bound fixed
  controllers, so player 1 could hop between physical pads; every slot now
  binds its own controller with unchanged enumeration.
* A planned sprite-margin adjustment was dropped: re-audit shows the
  inclusive-margin/half-open-interval math is self-consistent, and no failing
  case could be reproduced. It must not be reinvented without a reproducer.

Headless build and the full native suite pass locally
(`scripts/build_sanae.py --target headless`, then
`scripts/test_sanae_runner.py`). Switch compilation and the NRO artifact come
from CI on push; this section must not claim hardware parity until the owner
confirms the purple umbrella fill and Marisa contact damage on device.
