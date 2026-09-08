#!/usr/bin/env python3
"""Diagnostic-only host probes. Never modify data.win or publish raw game dumps.

This instruments only the builder-owned disposable no-op backend, restores it
in finally, and leaves the production Switch overlay untouched. Forced spawns
and room entry are not normal progression or physical Switch verification.

Scenario families (env SANAE_PRIVATE_SCENARIO):
* baseline / obj_*            — tutorial spawn control probes.
* umbrella                    — player shield vs hand-fed Marisa stars.
* room_<RoomName>             — real Marisa boss room; the hook emits a compact
                                per-frame AUDIT line so the report can separate
                                contact damage (Sanae vs boss body, no bullet
                                overlapping) from bullet damage, and watch boss
                                HP.
* umbrella_room_<RoomName>    — capture Kogasa in the tutorial, raise the
                                shield, then carry the held action into the real
                                Marisa room and watch shield hits.

Crash visibility: any scenario that does not exit cleanly is re-run under gdb
(-batch, backtrace) and the tail of that log is embedded in the report so the
Check summary shows the abort/segfault site without publishing game data.
"""

import json
import os
from pathlib import Path
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[1]
REPORT = ROOT / '.cache/user-data-report.json'

# Inserted into the disposable no-op backend (host only; never in the Switch
# overlay). Runs before each game step. Reads engine state directly; writes no
# game files and changes no game state except the deliberate spawn/room-entry
# requests made by the existing probe scenarios.
#
# Order inside the hook matters:
#  1. the audit scan always runs against the instance list as it was at the
#     start of this platform call, and
#  2. deliberate spawns / room-entry requests are issued afterwards and only
#     once per scenario, so a freshly created instance is never scanned in the
#     same call while it is still half-initialised.
HOOK = r'''
    const char *scenario = getenv("SANAE_PRIVATE_SCENARIO");
    if (scenario) {
        Runner *runnerProbe = g_runner;
        int frame = runnerProbe ? (int)runnerProbe->frameCount : -1;
        Instance *playerProbe = NULL, *guardProbe = NULL, *bossProbe = NULL;
        const char *bossName = NULL;
        int probeCount = runnerProbe ? (int)arrlen(runnerProbe->instances) : 0;
        for (int i = 0; i < probeCount && runnerProbe; ++i) {
            Instance *a = runnerProbe->instances[i];
            if (!a->active) continue;
            if (a->objectIndex < 0 || (unsigned)a->objectIndex >= runnerProbe->dataWin->objt.count) continue;
            const char *name = runnerProbe->dataWin->objt.objects[a->objectIndex].name;
            if (!name) continue;
            if (!strcmp(name, "obj_P01_KOCHIYASanae")) { if (!playerProbe) playerProbe = a; }
            else if (!strcmp(name, "obj_p01s02e02_gurad")) { if (!guardProbe) guardProbe = a; }
            else if (strstr(name, "KIRISAMEMarisa")) { if (!bossProbe) { bossProbe = a; bossName = name; } }
        }

        /* One-shot object-name census: names that may be the ability pickup or
           the shield (only useful for designing the umbrella scenarios). */
        static int namesLogged = 0;
        if (!namesLogged && runnerProbe && frame > 0) {
            char namesBuf[1600] = "";
            for (unsigned i = 0; i < runnerProbe->dataWin->objt.count; ++i) {
                const char *on = runnerProbe->dataWin->objt.objects[i].name;
                if (!on) continue;
                if (strstr(on, "gurad") || strstr(on, "kasa") || strstr(on, "es0") ||
                    strstr(on, "spawn") || strstr(on, "Kogasa") || strstr(on, "KIRISA") ||
                    (strstr(on, "item") || strstr(on, "take")) ||
                    (!strncmp(on, "obj_app_", 8))) {
                    if (strlen(namesBuf) < 1500) { strcat(namesBuf, on); strcat(namesBuf, ","); }
                }
            }
            logInfo("SANAE_NAMES %s\n", namesBuf[0] ? namesBuf : "-");
            namesLogged = 1;
        }

        int wantContact = (!strncmp(scenario, "room_", 5) || !strncmp(scenario, "umbrella_room_", 14) ||
                           !strncmp(scenario, "obj_b02_", 8) || !strncmp(scenario, "obj_mb00_", 9) ||
                           !strncmp(scenario, "audit_", 6));
        int wantGuard = (!strcmp(scenario, "umbrella") || !strncmp(scenario, "umbrella_room_", 14) ||
                         !strncmp(scenario, "audit_", 6));

        /* --- compact per-frame audit (before any action this call) --- */
        if ((wantContact || wantGuard) && runnerProbe && frame > 0) {
            static int lastHp = -1, lastBossHp = -1, lastGuardHp = -1, firstBossFrame = -1;
            float playerHp = -1, bossHp = -1, guardHp = -1;
            VMContext *vmProbe = runnerProbe->vmContext;
            const char *roomName = runnerProbe->currentRoom && runnerProbe->currentRoom->name
                                       ? runnerProbe->currentRoom->name : "?";

            // Instance_getSelfVar reads any instance's self variable by varID
            // (no STRUCT_OBJECT_INDEX requirement) and returns a weak view, so
            // no RValue_free. VM_structGetVariableByVarId would abort on real
            // game objects (they are not GML structs).
            if (playerProbe && shgeti(vmProbe->varNameMap, "hp_now") >= 0) {
                RValue v = Instance_getSelfVar(playerProbe, shget(vmProbe->varNameMap, "hp_now"));
                if (v.type == RVALUE_REAL || v.type == RVALUE_INT32) playerHp = (float)RValue_toReal(v);
            }
            // One-time discovery: which self var actually carries boss/guard HP?
            static int bossVarsLogged = 0, guardVarsLogged = 0;
            const char *hpCands[] = {"hp_now", "hp", "hit_point", "guard_hp", "durability", "hp_max"};
            if (bossProbe && !bossVarsLogged) {
                bossVarsLogged = 1;
                for (int ci = 0; ci < (int)(sizeof(hpCands) / sizeof(hpCands[0])); ++ci) {
                    ptrdiff_t cs = shgeti(vmProbe->varNameMap, hpCands[ci]);
                    if (cs < 0) continue;
                    RValue cv = Instance_getSelfVar(bossProbe, shget(vmProbe->varNameMap, hpCands[ci]));
                    if (cv.type == RVALUE_REAL || cv.type == RVALUE_INT32)
                        logInfo("SANAE_BOSSVAR name=%s v=%g\n", hpCands[ci], (float)RValue_toReal(cv));
                }
            }
            if (guardProbe && !guardVarsLogged) {
                guardVarsLogged = 1;
                for (int ci = 0; ci < (int)(sizeof(hpCands) / sizeof(hpCands[0])); ++ci) {
                    ptrdiff_t cs = shgeti(vmProbe->varNameMap, hpCands[ci]);
                    if (cs < 0) continue;
                    RValue cv = Instance_getSelfVar(guardProbe, shget(vmProbe->varNameMap, hpCands[ci]));
                    if (cv.type == RVALUE_REAL || cv.type == RVALUE_INT32)
                        logInfo("SANAE_GUARDVAR name=%s v=%g\n", hpCands[ci], (float)RValue_toReal(cv));
                }
            }
            if (bossProbe) {
                if (shgeti(vmProbe->varNameMap, "hp_now") >= 0) {
                    RValue v = Instance_getSelfVar(bossProbe, shget(vmProbe->varNameMap, "hp_now"));
                    if (v.type == RVALUE_REAL || v.type == RVALUE_INT32) bossHp = (float)RValue_toReal(v);
                }
                if (bossHp < 0 && shgeti(vmProbe->varNameMap, "hp") >= 0) {
                    RValue v = Instance_getSelfVar(bossProbe, shget(vmProbe->varNameMap, "hp"));
                    if (v.type == RVALUE_REAL || v.type == RVALUE_INT32) bossHp = (float)RValue_toReal(v);
                }
                if (firstBossFrame < 0) firstBossFrame = frame;
            }
            if (guardProbe) {
                if (shgeti(vmProbe->varNameMap, "hp_now") >= 0) {
                    RValue v = Instance_getSelfVar(guardProbe, shget(vmProbe->varNameMap, "hp_now"));
                    if (v.type == RVALUE_REAL || v.type == RVALUE_INT32) guardHp = (float)RValue_toReal(v);
                }
                if (guardHp < 0 && shgeti(vmProbe->varNameMap, "hp") >= 0) {
                    RValue v = Instance_getSelfVar(guardProbe, shget(vmProbe->varNameMap, "hp"));
                    if (v.type == RVALUE_REAL || v.type == RVALUE_INT32) guardHp = (float)RValue_toReal(v);
                }
            }

            InstanceBBox pb = {0}, bb = {0}, gb = {0};
            if (playerProbe) pb = Collision_computeBBox(runnerProbe, playerProbe);
            if (bossProbe) bb = Collision_computeBBox(runnerProbe, bossProbe);
            if (guardProbe) gb = Collision_computeBBox(runnerProbe, guardProbe);

            int overBoss = 0, touchBoss = 0, bulletCount = 0, pbul = 0;
            char overlapNames[160] = "";
            char guardOverlapNames[96] = "";
            if (pb.valid && bb.valid) {
                float l = bb.left - 2.0f, r = bb.right + 2.0f, t = bb.top - 2.0f, b = bb.bottom + 2.0f;
                overBoss = !(pb.left >= bb.right || bb.left >= pb.right || pb.top >= bb.bottom || bb.top >= pb.bottom);
                touchBoss = !(pb.left >= r || l >= pb.right || pb.top >= b || t >= pb.bottom);
            }
            if (pb.valid) {
                int n = (int)arrlen(runnerProbe->instances);
                for (int i = 0; i < n; ++i) {
                    Instance *o = runnerProbe->instances[i];
                    if (!o->active || o == playerProbe || o == bossProbe || o == guardProbe) continue;
                    if (o->objectIndex < 0 || (unsigned)o->objectIndex >= runnerProbe->dataWin->objt.count) continue;
                    const char *on = runnerProbe->dataWin->objt.objects[o->objectIndex].name;
                    InstanceBBox ob = Collision_computeBBox(runnerProbe, o);
                    if (!ob.valid) continue;
                    int hit = !(pb.left >= ob.right || ob.left >= pb.right || pb.top >= ob.bottom || ob.top >= pb.bottom);
                    if (!hit) continue;
                    bulletCount++;
                    if (SANAE_BULLET_LIKE(on)) pbul++;
                    if (strlen(overlapNames) < 120 && on) { strcat(overlapNames, on); strcat(overlapNames, ","); }
                }
            }
            int guardStarHits = 0, gbul = 0;
            if (gb.valid) {
                int n = (int)arrlen(runnerProbe->instances);
                for (int i = 0; i < n; ++i) {
                    Instance *o = runnerProbe->instances[i];
                    if (!o->active || o == guardProbe || o == playerProbe || o == bossProbe) continue;
                    if (o->objectIndex < 0 || (unsigned)o->objectIndex >= runnerProbe->dataWin->objt.count) continue;
                    const char *on = runnerProbe->dataWin->objt.objects[o->objectIndex].name;
                    InstanceBBox ob = Collision_computeBBox(runnerProbe, o);
                    if (!ob.valid) continue;
                    int hit = !(gb.left >= ob.right || ob.left >= gb.right || gb.top >= ob.bottom || ob.top >= gb.bottom);
                    if (!hit) continue;
                    guardStarHits++;
                    if (SANAE_BULLET_LIKE(on)) gbul++;
                    if (strlen(guardOverlapNames) < 72 && on) { strcat(guardOverlapNames, on); strcat(guardOverlapNames, ","); }
                }
            }

            int playerDrop = lastHp >= 0 && playerHp >= 0 && playerHp < lastHp - 0.5f;
            int guardDrop = lastGuardHp >= 0 && guardHp >= 0 && guardHp < lastGuardHp - 0.5f;
            int hpChanged = (int)playerHp != lastHp || (int)bossHp != lastBossHp || (int)guardHp != lastGuardHp;
            int periodic = (frame % 300) == 0;
            int interesting = overBoss || touchBoss || hpChanged || guardStarHits > 0 || pbul > 0 || gbul > 0 || periodic;
            if (interesting) {
                logInfo("SANAE_AUDIT frame=%d room=%s hp=%g boss=%s bhp=%g firstboss=%d over=%d touch=%d pbul=%d bullets=%d ov=%s guard=%g gbul=%d gov=%s\n",
                    frame, roomName, playerHp, bossName ? bossName : "-", bossHp, firstBossFrame,
                    overBoss, touchBoss, pbul, bulletCount, overlapNames[0] ? overlapNames : "-",
                    guardHp, gbul, guardOverlapNames[0] ? guardOverlapNames : "-");
            }
            if (playerDrop || guardDrop) {
                logInfo("SANAE_DROP frame=%d hp=%g dhp=%g bhp=%g guard=%g dghp=%g over=%d touch=%d pbul=%d ov=%s gov=%s\n",
                    frame, playerHp, lastHp - playerHp, bossHp, guardHp, lastGuardHp - guardHp,
                    overBoss, touchBoss, pbul, overlapNames[0] ? overlapNames : "-",
                    guardOverlapNames[0] ? guardOverlapNames : "-");
            }
            if ((int)playerHp != lastHp) lastHp = (int)playerHp;
            if ((int)bossHp != lastBossHp) lastBossHp = (int)bossHp;
            if ((int)guardHp != lastGuardHp) lastGuardHp = (int)guardHp;
        }

        /* --- deliberate probe actions (once per scenario, only when in game) ---
           Environment switches (set per scenario by the Python side):
             SANAE_PROBE_CHASE      park Sanae on Marisa's body to test contact
             SANAE_PROBE_GUARDSPAWN create the shield guard next to Sanae
             SANAE_PROBE_FEED       hand-feed Marisa stars into the guard
        */
        if (strcmp(scenario, "baseline") && strncmp(scenario, "audit_", 6)) {
            const char *chaseEnv = getenv("SANAE_PROBE_CHASE");
            const char *chaseStartEnv = getenv("SANAE_PROBE_CHASE_START");
            const char *chaseEndEnv = getenv("SANAE_PROBE_CHASE_END");
            const char *guardEnv = getenv("SANAE_PROBE_GUARDSPAWN");
            const char *feedEnv = getenv("SANAE_PROBE_FEED");
            const char *captureEnv = getenv("SANAE_PROBE_CAPTURE");
            const char *entryEnv = getenv("SANAE_PROBE_ENTRY_FRAME");
            const char *guardFrameEnv = getenv("SANAE_PROBE_GUARDSPAWN_FRAME");
            int chase = chaseEnv && atoi(chaseEnv) > 0;
            int chaseStart = chaseStartEnv ? atoi(chaseStartEnv) : 1500;
            int chaseEnd = chaseEndEnv ? atoi(chaseEndEnv) : 4800;
            int entryFrame = entryEnv ? atoi(entryEnv) : 1200;
            int guardFrame = guardFrameEnv ? atoi(guardFrameEnv) : 1300;
            int guardSpawn = guardEnv && atoi(guardEnv) > 0;
            int feed = feedEnv && atoi(feedEnv) > 0;
            int capture = captureEnv && atoi(captureEnv) > 0;
            static int probeSpawnDone = 0, probeRoomEntered = 0, probeGuardSpawned = 0, probePickupSpawned = 0;
            int inGame = playerProbe != NULL && runnerProbe != NULL;
            const char *roomNow = runnerProbe && runnerProbe->currentRoom && runnerProbe->currentRoom->name
                                      ? runnerProbe->currentRoom->name : NULL;

            if (!probeSpawnDone && inGame && frame >= 600 && frame <= 2500) {
                const char *spawn = NULL;
                float x = playerProbe->x, y = playerProbe->y;
                if (!strncmp(scenario, "obj_b02_", 8) || !strncmp(scenario, "obj_mb00_", 9)) spawn = scenario;
                else if (!strncmp(scenario, "room_", 5)) {
                    const char *roomName = scenario + 5;
                    for (unsigned i = 0; i < runnerProbe->dataWin->room.count; ++i)
                        if (!strcmp(runnerProbe->dataWin->room.rooms[i].name, roomName))
                            runnerProbe->pendingRoom = (int)i;
                }
                if (spawn) {
                    for (unsigned i = 0; i < runnerProbe->dataWin->objt.count; ++i)
                        if (!strcmp(runnerProbe->dataWin->objt.objects[i].name, spawn)) {
                            Runner_createInstance(runnerProbe, x, y, (int)i);
                            break;
                        }
                }
                probeSpawnDone = 1;
            }

            /* Ability capture: drop the Kogasa-umbrella pickup in front of
               Sanae (frame 620); the input side walks her into it (700-920)
               and raises the shield with X from frame 1000 on. */
            if (capture && !probePickupSpawned && inGame && frame >= 620 && frame <= 1200 && playerProbe) {
                const char *cand[] = {"obj_ES02_TATARAKogasa", "obj_app_es02", "obj_item_es02", "obj_take_es02"};
                for (int ci = 0; ci < 4 && !probePickupSpawned; ++ci) {
                    for (unsigned i = 0; i < runnerProbe->dataWin->objt.count; ++i)
                        if (!strcmp(runnerProbe->dataWin->objt.objects[i].name, cand[ci])) {
                            Runner_createInstance(runnerProbe, playerProbe->x + 150.0f, playerProbe->y, (int)i);
                            probePickupSpawned = 1;
                            break;
                        }
                }
            }

            if (guardSpawn && !probeGuardSpawned && !guardProbe && inGame && frame >= guardFrame && frame <= guardFrame + 1000 && playerProbe) {
                for (unsigned i = 0; i < runnerProbe->dataWin->objt.count; ++i)
                    if (!strcmp(runnerProbe->dataWin->objt.objects[i].name, "obj_p01s02e02_gurad")) {
                        Runner_createInstance(runnerProbe, playerProbe->x + 40.0f, playerProbe->y, (int)i);
                        break;
                    }
                probeGuardSpawned = 1;
            }

            if (feed && guardProbe && runnerProbe && frame >= 1700 && frame <= 2500 && frame % 40 == 0) {
                const char *spawn = "obj_b02e03_starS_shot";
                float x = guardProbe->x + 55, y = guardProbe->y - 10;
                for (int k = 0; k < 3; ++k) {
                    for (unsigned i = 0; i < runnerProbe->dataWin->objt.count; ++i)
                        if (!strcmp(runnerProbe->dataWin->objt.objects[i].name, spawn)) {
                            Instance *sInst = Runner_createInstance(runnerProbe, x, y - k * 6, (int)i);
                            if (sInst) sInst->hspeed = -6.0f;
                            break;
                        }
                }
            }

            if (!strncmp(scenario, "umbrella_room_", 14) && !probeRoomEntered && inGame &&
                frame >= entryFrame && frame <= 6500) {
                const char *target = scenario + 14;
                if (roomNow && !strcmp(roomNow, target)) {
                    probeRoomEntered = 1;
                } else {
                    for (unsigned i = 0; i < runnerProbe->dataWin->room.count; ++i)
                        if (!strcmp(runnerProbe->dataWin->room.rooms[i].name, target))
                            runnerProbe->pendingRoom = (int)i;
                    probeRoomEntered = 1;
                }
            }

            if (chase && bossProbe && playerProbe && frame >= chaseStart && frame <= chaseEnd) {
                /* Walk Sanae onto Marisa's body and keep pressing: contact test.
                   Crude (teleports ~2px/frame, ignores solids) but the audit's
                   over/touch/ov columns plus SANAE_DROP lines classify any
                   resulting damage. The guard is dragged along to face the
                   real boss bullet patterns. */
                float dx = bossProbe->x - playerProbe->x;
                float dy = bossProbe->y - playerProbe->y;
                float adx = dx > 0 ? dx : -dx, ady = dy > 0 ? dy : -dy;
                if (adx > 3.0f) playerProbe->x += dx > 0 ? 3.0f : -3.0f;
                else if (ady > 3.0f) playerProbe->y += dy > 0 ? 3.0f : -3.0f;
                else if (frame % 90 < 45) playerProbe->x += 3.0f;
                else playerProbe->y += 3.0f;
                if (guardProbe) {
                    guardProbe->x = playerProbe->x + 40.0f;
                    guardProbe->y = playerProbe->y - 16.0f;
                }
            }
        }
    }
'''


def summarize(path):
    state = json.loads(path.read_text())
    room = state.get('room') or {}
    result = {'frame': state['frame'], 'room': room.get('name'),
              'game_status': state.get('globalVariables', {}).get('game_status'),
              'ability': state.get('globalVariables', {}).get('set_spell'), 'actors': []}
    for instance in state.get('instances', []):
        name = instance['objectName']
        if name not in ('obj_P01_KOCHIYASanae', 'obj_p01s02e02_gurad') and 'Marisa' not in name and 'Kogasa' not in name:
            continue
        variables = instance.get('selfVariables', {})
        actor = {'object': name, 'x': round(instance['x']), 'y': round(instance['y'])}
        for k in ('hp_now', 'hp_max', 'mode_damage', 'mode_invin', 'mode_catch',
                  'vacuum_count', 'catch_count', 'next_break', 'set_action'):
            if k in variables and variables[k] is not None:
                actor[k] = round(variables[k]) if isinstance(variables[k], float) else variables[k]
        result['actors'].append(actor)
    return result


def merge_snapshots(snaps, cap=6):
    """Deduplicate repeated states; keep the widest spread of frames."""
    groups = {}
    for s in snaps:
        sig = (s['room'], s['game_status'], s['ability'],
               tuple((a['object'], a.get('x'), a.get('y'), a.get('hp_now'),
                      a.get('mode_damage'), a.get('mode_invin'), a.get('set_action'))
                     for a in s['actors']))
        groups.setdefault(sig, []).append(s['frame'])
    merged = []
    for sig, frames in groups.items():
        frames = sorted(frames)
        keep = [frames[0]]
        if len(frames) > 1:
            keep.append(frames[-1])
        entry = {'room': sig[0], 'game_status': sig[1], 'ability': sig[2],
                 'actors': [{'object': a[0], 'x': a[1], 'y': a[2],
                             'hp_now': a[3], 'mode_damage': a[4], 'mode_invin': a[5],
                             'set_action': a[6]} for a in sig[3]],
                 'frames': keep}
        merged.append(entry)
    merged.sort(key=lambda m: m['frames'][0])
    return merged[:min(cap, 3)]


def tail(lines, count=22, width=240):
    out = []
    for line in lines[-count:]:
        out.append(line[:width])
    return out


def crash_section(log, max_lines=34, width=170):
    """Signal line + backtrace frames from a gdb -batch log (skip register dump)."""
    lines = log.splitlines()
    start = None
    for i, line in enumerate(lines):
        if 'signal' in line.lower() or line.startswith('#'):
            start = i
            break
    if start is None:
        return tail(lines, count=18, width=width)
    out = []
    for line in lines[start:]:
        if out and (line.startswith('rax ') or line.startswith('rip ') or line.startswith('eflags')):
            break
        out.append(line[:width])
        if len(out) >= max_lines:
            break
    return out


def parse_audit(log):
    """Compress SANAE_AUDIT / SANAE_DROP lines into a report-friendly summary."""
    def fields_of(line, prefix):
        fields = {}
        for part in line[len(prefix):].strip().split(' '):
            if '=' in part:
                key, value = part.split('=', 1)
                fields[key] = value
        return fields
    events = []
    drops = []
    boss_vars = []
    guard_vars = []
    names = ''
    for line in log.splitlines():
        if line.startswith('SANAE_AUDIT'):
            events.append(fields_of(line, 'SANAE_AUDIT'))
        elif line.startswith('SANAE_DROP'):
            drops.append(fields_of(line, 'SANAE_DROP'))
        elif line.startswith('SANAE_BOSSVAR'):
            f = fields_of(line, 'SANAE_BOSSVAR')
            boss_vars.append('%s=%s' % (f.get('name'), f.get('v')))
        elif line.startswith('SANAE_GUARDVAR'):
            f = fields_of(line, 'SANAE_GUARDVAR')
            guard_vars.append('%s=%s' % (f.get('name'), f.get('v')))
        elif line.startswith('SANAE_NAMES'):
            names = line[len('SANAE_NAMES '):].strip()[:420]
    if not events:
        return {'events': 0, 'boss_vars': boss_vars, 'guard_vars': guard_vars, 'names': names}
    over_frames = [e for e in events if e.get('over') == '1']
    contact_frames = [e for e in over_frames if e.get('pbul') == '0']
    touch_only_frames = [e for e in events if e.get('over') == '0' and e.get('touch') == '1' and e.get('pbul') == '0']
    drops_with_boss = [d for d in drops if d.get('over') == '1']
    drops_with_boss_no_bullets = [d for d in drops_with_boss if d.get('pbul') == '0']
    guard_drops = [d for d in drops if d.get('dghp') not in (None, '', '0', '-')]
    guard_hits = 0
    guard_first = None
    guard_hp_min = None
    for e in events:
        if e.get('gbul') not in (None, '', '0'):
            guard_hits += 1
        try:
            g = float(e['guard'])
        except (KeyError, ValueError):
            continue
        if g >= 0:
            if guard_first is None:
                guard_first = int(e['frame'])
            guard_hp_min = g if guard_hp_min is None else min(guard_hp_min, g)
    first_boss = None
    for e in events:
        try:
            value = int(e['firstboss'])
        except (KeyError, ValueError):
            continue
        if value >= 0:
            first_boss = value
            break
    if len(events) <= 12:
        sample = events
    else:
        sample = events[:4] + events[len(events) // 2:len(events) // 2 + 4] + events[-4:]
    drop_sample = drops[:10]
    return {
        'events': len(events),
        'overlap_boss_frames': len(over_frames),
        'contact_frames_no_bullets': len(contact_frames),
        'touch_frames_no_bullets': len(touch_only_frames),
        'hp_drops_total': len(drops),
        'drops_with_boss_overlap': len(drops_with_boss),
        'drops_with_boss_overlap_no_bullets': len(drops_with_boss_no_bullets),
        'guard_hp_drops': len(guard_drops),
        'guard_star_overlap_events': guard_hits,
        'guard_first_frame': guard_first,
        'guard_hp_min_seen': guard_hp_min,
        'first_boss_frame': first_boss,
        'names': names,
        'boss_vars': boss_vars,
        'guard_vars': guard_vars,
        'drop_details': [{**{k: d.get(k, '') for k in ('frame', 'hp', 'dhp', 'bhp', 'guard', 'dghp', 'over', 'touch', 'pbul')},
                           'ov': (d.get('ov') or '')[:80], 'gov': (d.get('gov') or '')[:80]} for d in drop_sample],
        'sample': [{k: e.get(k, '') for k in ('frame', 'room', 'hp', 'over', 'touch', 'pbul', 'guard', 'gbul')} for e in sample],
    }


def start_keys(inputs, edge):
    # Same menu/confirm taps baseline uses to reach the tutorial reliably.
    for frame in range(60, 361, 60):
        edge(frame, 90)


def main():
    data, = (ROOT / '.cache/user-game').rglob('data.win')
    report = json.loads(REPORT.read_text())
    stage = ROOT / '.cache/sanae-source-headless'
    if not (stage / '.sanae-generated').is_file():
        raise RuntimeError('Not a disposable builder-owned source tree')
    backend = stage / 'src/backends/noop.c'
    original = backend.read_text()
    function_anchor = 'bool platformHandleEvents(void) {\n    return false;\n}'
    include_anchor = '#include "runner_mouse.h"\n'
    if original.count(function_anchor) != 1 or original.count(include_anchor) != 1:
        raise RuntimeError('Diagnostic hook source drift')
    work = ROOT / '.cache/user-probe'
    work.mkdir(parents=True, exist_ok=True)
    report['probes'] = []
    try:
        macro = '#define SANAE_BULLET_LIKE(n) ((n) && (strstr((n), "shot") || strstr((n), "star") || strstr((n), "laser") || strstr((n), "e001") || strstr((n), "e013") || strstr((n), "e02")))\n'
        modified = original.replace(include_anchor, include_anchor + '#include "collision.h"\n' + macro)
        modified = modified.replace(function_anchor, 'bool platformHandleEvents(void) {' + HOOK + '\n    return false;\n}')
        backend.write_text(modified)
        subprocess.run(['cmake', '--build', '.cache/sanae-build-headless', '--parallel', '4'], cwd=ROOT, check=True)
        marisa_rooms = ['Room_s02b00_marisa', 'Room_sh02b00_marisa']
        scenarios = [('baseline', {}),
                     ('obj_b02_KIRISAMEMarisa', {'SANAE_PROBE_CHASE': '1',
                                                 'SANAE_PROBE_CHASE_START': '850',
                                                 'SANAE_PROBE_CHASE_END': '1650'}),
                     ('room_' + marisa_rooms[0], {'SANAE_PROBE_CHASE': '1',
                                                  'SANAE_PROBE_CHASE_END': '7900'}),
                     ('umbrella', {'SANAE_PROBE_CAPTURE': '1', 'SANAE_PROBE_FEED': '1',
                                   'SANAE_PROBE_GUARDSPAWN': '1', 'SANAE_PROBE_GUARDSPAWN_FRAME': '2250'}),
                     ('umbrella_room_' + marisa_rooms[0],
                      {'SANAE_PROBE_CAPTURE': '1', 'SANAE_PROBE_ENTRY_FRAME': '2400',
                       'SANAE_PROBE_GUARDSPAWN': '1', 'SANAE_PROBE_GUARDSPAWN_FRAME': '2700',
                       'SANAE_PROBE_CHASE': '1', 'SANAE_PROBE_CHASE_START': '2900',
                       'SANAE_PROBE_CHASE_END': '7900'})]
        for index, (scenario, probe_env) in enumerate(scenarios):
            case = work / str(index)
            case.mkdir(exist_ok=True)
            inputs = {}
            def edge(frame, key, hold=1):
                inputs[str(frame)] = {'keysPressed': [key], 'keysReleased': []}
                inputs[str(frame + hold)] = {'keysPressed': [], 'keysReleased': [key]}
            def z_taps(step, first, last):
                for frame in range(first, last, step):
                    edge(frame, 90)
            end = 900
            frames = [610, 850]
            if scenario == 'obj_b02_KIRISAMEMarisa':
                # Contact test: boss spawns on Sanae at >=600, chase from 850.
                start_keys(inputs, edge)
                end = 1700
                frames = [640, 900, 1300, 1650]
            elif scenario == 'baseline':
                start_keys(inputs, edge)
            elif scenario.startswith('room_'):
                # pendingRoom at >=600; Z taps dismiss the boss intro dialog
                # and fire shots; chase parks Sanae on Marisa for ~6400 frames
                # so several boss phases (incl. any melee/dash moves) occur.
                start_keys(inputs, edge)
                end = 8000
                frames = [1100, 1800, 2800, 4200, 6200, 7900]
                z_taps(30, 660, 7950)
            elif scenario == 'umbrella':
                # Capture: hook drops obj_ES02_TATARAKogasa 150px ahead at 620;
                # hold Z (attack/vacuum) to catch her, Down consumes the capture
                # (sets Kogasa ability), X raises the shield, stars are then
                # hand-fed with leftward velocity (durability/break test).
                start_keys(inputs, edge)
                end = 2600
                frames = [900, 1300, 1700, 2100, 2450]
                edge(700, 90, 800)         # attack/vacuum 700-1500
                for down in range(1120, 1520, 80):
                    edge(down, 40)         # consume the capture
                edge(1600, 88, 850)        # raise the shield 1600-2450
            elif scenario.startswith('umbrella_room_'):
                # Capture in the tutorial (vacuum + Down + X raise by 2000),
                # enter the Marisa room at 2400, Z taps dismiss the fight
                # dialog, chase drags guard+Sanae through the boss patterns.
                # Direct guard spawn at 2700 remains if capture produced none.
                start_keys(inputs, edge)
                end = 8000
                frames = [1300, 2000, 2600, 3400, 4800, 6500, 7900]
                edge(700, 90, 800)         # attack/vacuum 700-1500
                for down in range(1120, 1520, 80):
                    edge(down, 40)         # consume the capture
                edge(1800, 88, 6100)       # hold X: raise + keep the shield
                z_taps(30, 2500, 7950)     # dismiss fight dialog, keep firing
            (case / 'inputs.json').write_text(json.dumps(inputs))
            args = [str(ROOT / '.cache/sanae-build-headless/butterscotch'), str(data),
                    '--headless', '--exit-at-frame', str(end), '--playback-inputs', str(case / 'inputs.json'),
                    '--save-folder', str(case / 'saves'), '--dump-frame-json-file', str(case / 'state-%d.json'),
                    '--disable-log-colours']
            for frame in frames:
                args += ['--dump-frame-json', str(frame)]
            env = dict(os.environ, SANAE_PRIVATE_SCENARIO=scenario, **probe_env)
            log = ''
            code = 0
            try:
                proc = subprocess.run(args, env=env, stdout=subprocess.PIPE,
                                      stderr=subprocess.STDOUT, timeout=420)
                log = proc.stdout.decode(errors='replace')
                code = proc.returncode
            except subprocess.TimeoutExpired as error:
                log = (error.stdout or b'').decode(errors='replace')
                code = 'timeout'
            (case / 'private.log').write_text(log)
            audit = parse_audit(log)
            if index != 0:
                audit['names'] = ''
            record = {'scenario': scenario, 'exit': code,
                      'missing_csv_count': log.count('missing/invalid CSV'),
                      'unknown_function_count': log.count('Unknown function'),
                      'audit': audit,
                      'snapshots': merge_snapshots([summarize(p) for p in sorted(case.glob('state-*.json'))])}
            if code not in (0,):
                # Reproduce under gdb so the abort/segv site is visible in the report.
                gdb_log = ''
                try:
                    gdb = subprocess.run(['gdb', '-batch', '-nx', '-ex', 'run', '-ex', 'bt 40',
                                          '-ex', 'info registers', '--args'] + args,
                                         env=env, stdout=subprocess.PIPE,
                                         stderr=subprocess.STDOUT, timeout=420)
                    gdb_log = gdb.stdout.decode(errors='replace')
                except subprocess.TimeoutExpired as error:
                    gdb_log = (error.stdout or b'').decode(errors='replace')
                    gdb_log += '\n[gdb timed out]'
                (case / 'crash.log').write_text(gdb_log)
                record['crash_section'] = crash_section(gdb_log)
                record['log_tail'] = tail(log.splitlines(), count=25)
            report['probes'].append(record)
            REPORT.write_text(json.dumps(report, indent=2))
        report.update(status='probes completed', completed=True,
                      caveat='Host no-op audio/render probes; forced room entry and spawns are not Switch verification.')
    finally:
        backend.write_text(original)
        REPORT.write_text(json.dumps(report, indent=2))


if __name__ == '__main__':
    try:
        main()
    except Exception as error:
        REPORT.write_text(json.dumps({'status': 'probe failure', 'error': repr(error), 'completed': False}, indent=2))
        print(f'PROBE FAILURE: {error!r}', file=sys.stderr)
        raise
