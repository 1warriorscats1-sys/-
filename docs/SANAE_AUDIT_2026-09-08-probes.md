# SANAE host probe audit — 2026-09-08 (runs 7–16)

Private CI probes (`sanae-private-probe.yml`, workflow runs 7–16) drove the
**patched headless engine + the user's original `data.win`** (SHA-256
`94893a08…cd07`, 22 542 558 bytes, unchanged in every run; no game file was
ever modified). This page records what the probes could and could not prove
about the three open Switch reports. The tooling is
`scripts/probe_sanae_user_data.py` (host-only; instruments only the disposable
no-op backend and restores it in `finally`; never the Switch overlay).

Runs 7–9 crashed inside the probe hook itself (struct API misuse:
`VM_structGetVariableByVarId` aborts on non-struct instances; fixed with
`Instance_getSelfVar`; later a census buffer overflow). Runs 10+ are valid.
The raw per-scenario JSON check payloads for runs 9–16 are kept out of git in
`.cache/user-data-report-run*.json`.

## Method (valid runs)

Each scenario is one headless process with an `inputs.json` script, `--exit-at-frame`,
frame dumps, and a per-frame engine-side audit printed as `SANAE_AUDIT` /
`SANAE_DROP` lines. Deliberate actions (spawns, room entry, chase, shield-guard
spawn, star feed) only ever ran host-side via env switches; original scripts
and balance were never patched.

Audit columns relevant below:
`hp` player HP, `bhp` boss HP, `over` player AABB overlapping the boss body,
`touch` within +2 px, `pbul` number of *bullet-like* objects (name contains
shot/star/laser/e001/e013/e02) overlapping the player, `gbul` same for the
shield guard, `guard` = shield durability read from its self var (`hp_now`,
=40/40 when present).

## Results

### 1. Marisa boss body contact (report: "no contact damage from Marisa")

| run | scenario | boss-body overlap frames | clean overlaps (pbul=0) | HP drops | drops while overlapping boss, pbul=0 |
|----|----|----|----|----|----|
| 13/15/16 | `room_Room_s02b00_marisa` (chase parks Sanae on Marisa, whole fight to boss HP 4) | 1055 | 669 | 5 | **0** |
| 16 | `umbrella_room_…` same | 1041 | 766 | 3 | **0** |
| 15/16 | `obj_b02_KIRISAMEMarisa` (boss created overlapping Sanae in empty tutorial) | 3 | — | 3 | **1** (frame 601, −16, one-shot) |

The fight runs cover the whole boss life (bhp 100 → 4), so several phases
occurred. Body overlap without any bullet-like object present is clean and
frequent, yet HP never drops in those frames. The **only** body-contact damage
ever seen from this boss object was the single frame the boss was *created*
inside Sanae (its start/create path), i.e. contact is not applied continuously
by the boss body.

The engine dispatches collision events every frame from current positions
(`runner.c::dispatchCollisionEvents` iterates AABB overlaps for objects with
collision events), so this absence means the boss chain has no event that
damages a player whose AABB merely overlaps its body, or the damage is gated
by a boss state the chase never produced.

Caveat: the chase moves Sanae by direct x/y writes (no velocity), which is
irrelevant for position-based collision-event dispatch but bypasses any
"moved into enemy" semantics a script might use (distance-based checks still
see the positions). A real-input approach test remains desirable.

### 2. Shield umbrella durability/break (report: "cannot be broken / no durability loss")

Every guard that existed in these runs was **directly spawned** by the probe
(`obj_p01s02e02_gurad`, `SANAE_PROBE_GUARDSPAWN`); it persists, follows Sanae,
and exposes `hp_now=40, hp_max=40`.

| run | context | guard durability | bullet-like overlaps on guard |
|----|----|----|----|
| 11 | tutorial, stars spawned overlapping guard | 40 → 40 | 1699 (mostly non-bullet; polluted) |
| 15 | real Marisa fight, chase, whole fight | 40 → 40 | — |
| 16 | real Marisa fight (orrery/laser/bomb objects) | 40 → 40 | **890** clean bullet-like overlap frames |

Sanae took damage from the same orrery objects the guard overlapped, while the
guard durability never decreased and it never broke. This matches the user
report but is not conclusive proof of an engine defect, because the guard was
never obtained through the real ability flow:

* Capturing the Kogasa umbrella was **not** reproduced. `obj_ES02_TATARAKogasa`
  is present/overlaps the tutorial spawn; touching her deals 16 HP contact
  damage in ticks, she flees right with `hp_now` 8, and `mode_catch` was never
  observed on Sanae in any run. The earlier public docs
  (`SANAE_GAMEPLAY_COMPATIBILITY.md`) obtained `mode_catch=1`/`catch_count=1`
  via the game's **spawner** objects (e0xx family) and consumed the capture
  with Down; our capture attempts (spawn Kogasa + Z vacuum + Down + X raise)
  all left ability `00_normal` and produced no ability-granted guard.
* A direct-spawned guard may lack the "ability active" state that gates its
  parry/durability code, so a 40→40 result on a directly spawned guard cannot
  distinguish "bug reproduces" from "guard not actually armed".

### 3. HP-bar/durability-bar rendering (report: purple on PC, black on Switch)

Not testable with the no-op audio/render host backend (no pixels). Needs a GLES
screenshot comparison on the Switch NRO or the GLES host backend.

## Open items / recommended next steps

1. Obtain a legitimately ability-granted shield. Most reliable path is a real
   save that already has the Kogasa ability (physical Switch or a save the
   user can provide), then rerun the shield scenarios.
2. Re-run the boss-contact scenario with **real input movement** (fly to the
   boss height, jitter against the body) instead of the chase teleport, to
   rule out movement-dependent contact semantics.
3. A/B the same scenarios against the **unpatched upstream engine** (build
   `butterscotch-open` at the pinned `e92a2e45…` with the same hook) to
   separate base-engine behavior from overlay regressions.
4. Rendering: capture GLES frames of the shield durability bar on Switch NRO
   and compare against the PC reference screenshot (purple vs black).
