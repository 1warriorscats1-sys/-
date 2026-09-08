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
            const char *name = runnerProbe->dataWin->objt.objects[a->objectIndex].name;
            if (!name) continue;
            if (!strcmp(name, "obj_P01_KOCHIYASanae")) { if (!playerProbe) playerProbe = a; }
            else if (!strcmp(name, "obj_p01s02e02_gurad")) { if (!guardProbe) guardProbe = a; }
            else if (strstr(name, "KIRISAMEMarisa")) { if (!bossProbe) { bossProbe = a; bossName = name; } }
        }

        /* --- deliberate probe actions (same as earlier diagnostic runs) --- */
        if (frame == 600 && playerProbe && runnerProbe) {
            const char *spawn = NULL;
            float x = playerProbe->x, y = playerProbe->y;
            if (!strcmp(scenario, "umbrella")) { spawn = "obj_app_es02"; x += 48; }
            else if (!strncmp(scenario, "obj_b02_", 8) || !strncmp(scenario, "obj_mb00_", 9)) spawn = scenario;
            else if (!strncmp(scenario, "Room_", 5) || !strncmp(scenario, "room_", 5)) {
                const char *roomName = scenario[0] == 'R' ? scenario : scenario + 5;
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
        }
        if (!strcmp(scenario, "umbrella") && guardProbe && frame >= 1100 && frame <= 1800 && frame % 100 == 0) {
            const char *spawn = "obj_b02e03_starS_shot";
            float x = guardProbe->x + 32, y = guardProbe->y - 8;
            for (unsigned i = 0; i < runnerProbe->dataWin->objt.count; ++i)
                if (!strcmp(runnerProbe->dataWin->objt.objects[i].name, spawn)) {
                    Runner_createInstance(runnerProbe, x, y, (int)i);
                    break;
                }
        }
        if (!strncmp(scenario, "umbrella_room_", 14) && frame == 1200 && playerProbe && runnerProbe) {
            const char *roomName = scenario + 14;
            for (unsigned i = 0; i < runnerProbe->dataWin->room.count; ++i)
                if (!strcmp(runnerProbe->dataWin->room.rooms[i].name, roomName))
                    runnerProbe->pendingRoom = (int)i;
        }

        /* --- compact per-frame audit --- */
        int wantContact = (!strncmp(scenario, "room_", 5) || !strncmp(scenario, "umbrella_room_", 14) ||
                           !strncmp(scenario, "obj_b02_", 8) || !strncmp(scenario, "obj_mb00_", 9));
        int wantGuard = (!strcmp(scenario, "umbrella") || !strncmp(scenario, "umbrella_room_", 14));
        if ((wantContact || wantGuard) && runnerProbe && frame > 0) {
            static int lastHp = -1, lastBossHp = -1, lastGuardHp = -1, firstBossFrame = -1;
            float playerHp = -1, bossHp = -1, guardHp = -1;
            VMContext *vmProbe = runnerProbe->vmContext;

            if (playerProbe && shgeti(vmProbe->varNameMap, "hp_now") >= 0) {
                RValue v = VM_structGetVariableByVarId(playerProbe, shget(vmProbe->varNameMap, "hp_now"), -1);
                if (v.type == RVALUE_REAL || v.type == RVALUE_INT32) playerHp = (float)RValue_toReal(v);
                RValue_free(&v);
            }
            if (bossProbe && shgeti(vmProbe->varNameMap, "hp_now") >= 0) {
                RValue v = VM_structGetVariableByVarId(bossProbe, shget(vmProbe->varNameMap, "hp_now"), -1);
                if (v.type == RVALUE_REAL || v.type == RVALUE_INT32) bossHp = (float)RValue_toReal(v);
                RValue_free(&v);
                if (firstBossFrame < 0) firstBossFrame = frame;
            }
            if (guardProbe && shgeti(vmProbe->varNameMap, "hp_now") >= 0) {
                RValue v = VM_structGetVariableByVarId(guardProbe, shget(vmProbe->varNameMap, "hp_now"), -1);
                if (v.type == RVALUE_REAL || v.type == RVALUE_INT32) guardHp = (float)RValue_toReal(v);
                RValue_free(&v);
            }

            InstanceBBox pb = {0}, bb = {0}, gb = {0};
            if (playerProbe) pb = Collision_computeBBox(runnerProbe, playerProbe);
            if (bossProbe) bb = Collision_computeBBox(runnerProbe, bossProbe);
            if (guardProbe) gb = Collision_computeBBox(runnerProbe, guardProbe);

            int overBoss = 0, touchBoss = 0, bulletCount = 0;
            char overlapNames[160] = "";
            char guardOverlapNames[96] = "";
            if (pb.valid && bb.valid) {
                float l = bb.left - 2.0f, r = bb.right + 2.0f, t = bb.top - 2.0f, b = bb.bottom + 2.0f;
                overBoss = !(pb.left >= bb.right || bb.left >= pb.right || pb.top >= bb.bottom || bb.top >= pb.bottom);
                touchBoss = !(pb.left >= r || l >= pb.right || pb.top >= b || t >= pb.bottom);
            }
            if (pb.valid) {
                for (int i = 0; i < probeCount; ++i) {
                    Instance *o = runnerProbe->instances[i];
                    if (!o->active || o == playerProbe || o == bossProbe || o == guardProbe) continue;
                    const char *on = runnerProbe->dataWin->objt.objects[o->objectIndex].name;
                    InstanceBBox ob = Collision_computeBBox(runnerProbe, o);
                    if (!ob.valid) continue;
                    int hit = !(pb.left >= ob.right || ob.left >= pb.right || pb.top >= ob.bottom || ob.top >= pb.bottom);
                    if (!hit) continue;
                    bulletCount++;
                    if (strlen(overlapNames) < 120 && on) { strcat(overlapNames, on); strcat(overlapNames, ","); }
                }
            }
            int guardStarHits = 0;
            if (gb.valid) {
                for (int i = 0; i < probeCount; ++i) {
                    Instance *o = runnerProbe->instances[i];
                    if (!o->active || o == guardProbe || o == playerProbe || o == bossProbe) continue;
                    const char *on = runnerProbe->dataWin->objt.objects[o->objectIndex].name;
                    InstanceBBox ob = Collision_computeBBox(runnerProbe, o);
                    if (!ob.valid) continue;
                    int hit = !(gb.left >= ob.right || ob.left >= gb.right || gb.top >= ob.bottom || ob.top >= gb.bottom);
                    if (!hit) continue;
                    guardStarHits++;
                    if (strlen(guardOverlapNames) < 72 && on) { strcat(guardOverlapNames, on); strcat(guardOverlapNames, ","); }
                }
            }

            int hpChanged = (int)playerHp != lastHp || (int)bossHp != lastBossHp || (int)guardHp != lastGuardHp;
            int periodic = (frame % 200) == 0;
            int interesting = overBoss || touchBoss || hpChanged || guardStarHits > 0 || bulletCount > 0 || periodic;
            if (interesting) {
                const char *roomName = runnerProbe->currentRoom && runnerProbe->currentRoom->name ? runnerProbe->currentRoom->name : "?";
                logInfo("SANAE_AUDIT frame=%d room=%s hp=%g boss=%s bhp=%g firstboss=%d over=%d touch=%d bullets=%d ov=%s guard=%g guardstar=%d gov=%s\n",
                    frame, roomName, playerHp, bossName ? bossName : "-", bossHp, firstBossFrame,
                    overBoss, touchBoss, bulletCount, overlapNames[0] ? overlapNames : "-",
                    guardHp, guardStarHits, guardOverlapNames[0] ? guardOverlapNames : "-");
            }
            if ((int)playerHp != lastHp) lastHp = (int)playerHp;
            if ((int)bossHp != lastBossHp) lastBossHp = (int)bossHp;
            if ((int)guardHp != lastGuardHp) lastGuardHp = (int)guardHp;
        }
    }
'''


def summarize(path):
    state = json.loads(path.read_text())
    result = {'frame': state['frame'], 'room': state['room'],
              'game_status': state.get('globalVariables', {}).get('game_status'),
              'ability': state.get('globalVariables', {}).get('set_spell'), 'actors': []}
    for instance in state.get('instances', []):
        name = instance['objectName']
        if name not in ('obj_P01_KOCHIYASanae', 'obj_p01s02e02_gurad') and 'Marisa' not in name:
            continue
        variables = instance.get('selfVariables', {})
        result['actors'].append({'object': name, 'x': instance['x'], 'y': instance['y'],
            **{k: variables[k] for k in ('hp_now', 'hp_max', 'mode_damage', 'mode_invin',
               'mode_catch', 'next_break', 'set_action') if k in variables}})
    return result


def parse_audit(log):
    """Compress SANAE_AUDIT lines into a small report-friendly summary."""
    events = []
    for line in log.splitlines():
        if not line.startswith('SANAE_AUDIT'):
            continue
        fields = {}
        for part in line[len('SANAE_AUDIT '):].strip().split(' '):
            if '=' in part:
                key, value = part.split('=', 1)
                fields[key] = value
        events.append(fields)
    if not events:
        return {'events': 0}
    over_frames = [e for e in events if e.get('over') == '1']
    contact_frames = [e for e in over_frames if e.get('bullets') == '0']
    touch_only_frames = [e for e in events if e.get('over') == '0' and e.get('touch') == '1' and e.get('bullets') == '0']
    drops = 0
    drops_with_boss = 0
    prev = None
    for e in events:
        try:
            hp = float(e['hp'])
        except (KeyError, ValueError):
            continue
        if prev is not None and hp < prev - 0.5:
            drops += 1
            if e.get('over') == '1' and e.get('bullets') == '0':
                drops_with_boss += 1
        prev = hp
    guard_hits = []
    for e in events:
        if e.get('guardstar') not in (None, '', '0'):
            try:
                guard_hits.append((int(e['frame']),
                                   float(e['guard']) if e.get('guard') not in (None, '', '-') else -1))
            except (KeyError, ValueError):
                pass
    first_boss = None
    for e in events:
        try:
            value = int(e['firstboss'])
        except (KeyError, ValueError):
            continue
        if value >= 0:
            first_boss = value
            break
    if len(events) <= 40:
        sample = events
    else:
        sample = events[:15] + events[len(events) // 2:len(events) // 2 + 10] + events[-15:]
    return {
        'events': len(events),
        'overlap_boss_frames': len(over_frames),
        'contact_frames_no_bullets': len(contact_frames),
        'touch_frames_no_bullets': len(touch_only_frames),
        'hp_drops_total': drops,
        'hp_drops_with_boss_overlap_no_bullets': drops_with_boss,
        'guard_star_overlap_events': len(guard_hits),
        'first_boss_frame': first_boss,
        'sample': [{k: e.get(k, '') for k in ('frame', 'room', 'hp', 'boss', 'bhp', 'over', 'touch', 'bullets', 'guard', 'guardstar')} for e in sample],
    }


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
        modified = original.replace(include_anchor, include_anchor + '#include "collision.h"\n')
        modified = modified.replace(function_anchor, 'bool platformHandleEvents(void) {' + HOOK + '\n    return false;\n}')
        backend.write_text(modified)
        subprocess.run(['cmake', '--build', '.cache/sanae-build-headless', '--parallel', '4'], cwd=ROOT, check=True)
        marisa_rooms = ['Room_s02b00_marisa', 'Room_sh02b00_marisa']
        scenarios = ['baseline', 'obj_b02_KIRISAMEMarisa',
                     'room_' + marisa_rooms[0], 'room_' + marisa_rooms[1],
                     'umbrella', 'umbrella_room_' + marisa_rooms[0]]
        for index, scenario in enumerate(scenarios):
            case = work / str(index)
            case.mkdir(exist_ok=True)
            inputs = {}
            def edge(frame, key, hold=1):
                inputs[str(frame)] = {'keysPressed': [key], 'keysReleased': []}
                inputs[str(frame + hold)] = {'keysPressed': [], 'keysReleased': [key]}
            end = 900
            frames = [610, 700, 850]
            if scenario in ('baseline', 'obj_b02_KIRISAMEMarisa'):
                for frame in range(60, 361, 60):
                    edge(frame, 90)
            elif scenario == 'umbrella':
                edge(580, 88, 100); edge(720, 40, 30); edge(1000, 88, 900)
                end = 2000
                frames = [1050, 1150, 1250, 1350, 1450, 1550, 1601, 1650, 1850]
            elif scenario.startswith('room_'):
                end = 8000
                frames = [850, 1400, 2400, 4800, 7800]
                for frame in range(660, 7900, 45):
                    edge(frame, 90)
                # Sustained walking into the boss: alternate right/left so Sanae
                # repeatedly attempts contact with Marisa's body, without
                # teleporting her onto the boss.
                for frame in range(1600, 7600, 500):
                    edge(frame, 39 if (frame // 500) % 2 == 0 else 37, 460)
            elif scenario.startswith('umbrella_room_'):
                # Tutorial capture of Kogasa's ability + raised shield, then
                # carry the held action into the real Marisa room at frame 1200.
                edge(580, 88, 100); edge(720, 40, 30); edge(1000, 88, 7000)
                end = 8000
                frames = [850, 1400, 2400, 4800, 7800]
            (case / 'inputs.json').write_text(json.dumps(inputs))
            args = [str(ROOT / '.cache/sanae-build-headless/butterscotch'), str(data),
                    '--headless', '--exit-at-frame', str(end), '--playback-inputs', str(case / 'inputs.json'),
                    '--save-folder', str(case / 'saves'), '--dump-frame-json-file', str(case / 'state-%d.json'),
                    '--disable-log-colours']
            for frame in frames:
                args += ['--dump-frame-json', str(frame)]
            try:
                proc = subprocess.run(args, env=dict(os.environ, SANAE_PRIVATE_SCENARIO=scenario),
                                      stdout=subprocess.PIPE, stderr=subprocess.STDOUT, timeout=300)
                log = proc.stdout.decode(errors='replace')
                code = proc.returncode
            except subprocess.TimeoutExpired as error:
                log = (error.stdout or b'').decode(errors='replace'); code = 'timeout'
            (case / 'private.log').write_text(log)
            record = {'scenario': scenario, 'exit': code,
                      'missing_csv_count': log.count('missing/invalid CSV'),
                      'unknown_function_count': log.count('Unknown function'),
                      'audit': parse_audit(log),
                      'snapshots': [summarize(p) for p in sorted(case.glob('state-*.json'))]}
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
