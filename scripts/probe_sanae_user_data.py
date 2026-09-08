#!/usr/bin/env python3
"""Diagnostic-only host probes. Never modify data.win or publish raw game dumps.

Instruments only the builder-owned disposable no-op backend, restores it in
finally, and never touches the production Switch overlay. Spawns / room entry /
motion are deliberate probe actions; original scripts and balance are unchanged.

Scenarios (env SANAE_PRIVATE_SCENARIO), all start from the tutorial room like
the ORIGINAL working harness (durability 40->0 reproduced there):

* baseline                       - menu taps only.
* obj_b02_KIRISAMEMarisa         - boss created on Sanae at frame 600, then
                                   real-motion chase (hspeed) from 700: does
                                   body contact damage repeat?
* room_Room_s02b00_marisa        - real fight via pendingRoom; Z taps advance
                                   the fight; real-motion chase onto the boss.
* umbrella                       - LEGITIMATE Kogasa capture: spawn the pickup
                                   obj_app_es02 in front of Sanae at 600, X
                                   (inhale) 580-680, Down (swallow) 720-750,
                                   X (raise umbrella) 1000+; feed Marisa stars
                                   into the shield from 1100. Watch durability.
* umbrella_room_Room_s02b00_marisa - same legit capture, umbrella stays raised,
                                   forced entry into the real Marisa room at
                                   2000, real-motion chase: shield vs real
                                   bullets + body-contact damage in one run.
"""

import json
import os
from pathlib import Path
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[1]
REPORT = ROOT / '.cache/user-data-report.json'

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

        /* ---- per-frame audit (before any action this call) ---- */
        if (runnerProbe && frame > 0 && strcmp(scenario, "baseline")) {
            static int lastHp = -1, lastBossHp = -1, lastGuardHp = -1, firstBossFrame = -1;
            float playerHp = -1, bossHp = -1, guardHp = -1;
            int pbul = 0, gbul = 0, overBoss = 0, touchBoss = 0, bulletCount = 0;
            char overlapNames[160] = "", guardOverlapNames[96] = "";
            VMContext *vmProbe = runnerProbe->vmContext;
            const char *roomName = runnerProbe->currentRoom && runnerProbe->currentRoom->name
                                       ? runnerProbe->currentRoom->name : "?";

            /* One-time discovery of the var that really holds boss/guard HP. */
            static int guardVarLogged = 0;
            const char *hpCands[] = {"hp_now", "hp", "guard_hp", "durability"};
            if (guardProbe && !guardVarLogged) {
                guardVarLogged = 1;
                for (int ci = 0; ci < 4; ++ci) {
                    ptrdiff_t cs = shgeti(vmProbe->varNameMap, hpCands[ci]);
                    if (cs < 0) continue;
                    RValue cv = Instance_getSelfVar(guardProbe, shget(vmProbe->varNameMap, hpCands[ci]));
                    if (cv.type == RVALUE_REAL || cv.type == RVALUE_INT32)
                        logInfo("SANAE_GUARDVAR name=%s v=%g\n", hpCands[ci], (float)RValue_toReal(cv));
                }
            }
            if (playerProbe && shgeti(vmProbe->varNameMap, "hp_now") >= 0) {
                RValue v = Instance_getSelfVar(playerProbe, shget(vmProbe->varNameMap, "hp_now"));
                if (v.type == RVALUE_REAL || v.type == RVALUE_INT32) playerHp = (float)RValue_toReal(v);
            }
            if (bossProbe) {
                if (shgeti(vmProbe->varNameMap, "hp_now") >= 0) {
                    RValue v = Instance_getSelfVar(bossProbe, shget(vmProbe->varNameMap, "hp_now"));
                    if (v.type == RVALUE_REAL || v.type == RVALUE_INT32) bossHp = (float)RValue_toReal(v);
                }
                if (firstBossFrame < 0) firstBossFrame = frame;
            }
            if (guardProbe) {
                if (shgeti(vmProbe->varNameMap, "hp_now") >= 0) {
                    RValue v = Instance_getSelfVar(guardProbe, shget(vmProbe->varNameMap, "hp_now"));
                    if (v.type == RVALUE_REAL || v.type == RVALUE_INT32) guardHp = (float)RValue_toReal(v);
                }
            }

            InstanceBBox pb = {0}, bb = {0}, gb = {0};
            if (playerProbe) pb = Collision_computeBBox(runnerProbe, playerProbe);
            if (bossProbe) bb = Collision_computeBBox(runnerProbe, bossProbe);
            if (guardProbe) gb = Collision_computeBBox(runnerProbe, guardProbe);
            if (pb.valid && bb.valid) {
                float l = bb.left - 2.0f, r = bb.right + 2.0f, t = bb.top - 2.0f, b = bb.bottom + 2.0f;
                overBoss = !(pb.left >= bb.right || bb.left >= pb.right || pb.top >= bb.bottom || bb.top >= pb.bottom);
                touchBoss = !(pb.left >= r || l >= pb.right || pb.top >= b || t >= pb.bottom);
            }
            int n = probeCount;
            if (pb.valid) {
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
                    if (on && (strstr(on, "shot") || strstr(on, "star") || strstr(on, "laser") ||
                               strstr(on, "e001") || strstr(on, "e013") || strstr(on, "e02"))) pbul++;
                    if (strlen(overlapNames) < 120 && on) { strcat(overlapNames, on); strcat(overlapNames, ","); }
                }
            }
            if (gb.valid) {
                for (int i = 0; i < n; ++i) {
                    Instance *o = runnerProbe->instances[i];
                    if (!o->active || o == guardProbe || o == playerProbe || o == bossProbe) continue;
                    if (o->objectIndex < 0 || (unsigned)o->objectIndex >= runnerProbe->dataWin->objt.count) continue;
                    const char *on = runnerProbe->dataWin->objt.objects[o->objectIndex].name;
                    InstanceBBox ob = Collision_computeBBox(runnerProbe, o);
                    if (!ob.valid) continue;
                    int hit = !(gb.left >= ob.right || ob.left >= gb.right || gb.top >= ob.bottom || ob.top >= gb.bottom);
                    if (!hit) continue;
                    if (on && (strstr(on, "shot") || strstr(on, "star") || strstr(on, "laser") ||
                               strstr(on, "e001") || strstr(on, "e013") || strstr(on, "e02"))) gbul++;
                    if (strlen(guardOverlapNames) < 72 && on) { strcat(guardOverlapNames, on); strcat(guardOverlapNames, ","); }
                }
            }

            int playerDrop = lastHp >= 0 && playerHp >= 0 && playerHp < lastHp - 0.5f;
            int guardDrop = lastGuardHp >= 0 && guardHp >= 0 && guardHp < lastGuardHp - 0.5f;
            int hpChanged = (int)playerHp != lastHp || (int)bossHp != lastBossHp || (int)guardHp != lastGuardHp;
            int periodic = (frame % 300) == 0;
            if (overBoss || touchBoss || hpChanged || gbul > 0 || pbul > 0 || periodic) {
                logInfo("SANAE_AUDIT frame=%d room=%s hp=%g boss=%s bhp=%g firstboss=%d over=%d touch=%d pbul=%d ov=%s guard=%g gbul=%d gov=%s\n",
                    frame, roomName, playerHp, bossName ? bossName : "-", bossHp, firstBossFrame,
                    overBoss, touchBoss, pbul, overlapNames[0] ? overlapNames : "-",
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

        /* ---- deliberate probe actions (one-shot; then real motion) ---- */
        const char *chaseEnv = getenv("SANAE_PROBE_HS_CHASE");
        const char *entryEnv = getenv("SANAE_PROBE_ENTRY_FRAME");
        int hsChase = chaseEnv && atoi(chaseEnv) > 0;
        int entryFrame = entryEnv ? atoi(entryEnv) : 2000;
        static int probeSpawnDone = 0, probeRoomEntered = 0;
        int inGame = playerProbe != NULL && runnerProbe != NULL;
        int isUmbrellaRoom = !strncmp(scenario, "umbrella_room_", 14);

        if (!probeSpawnDone && inGame && frame >= 600 && frame <= 1200 && playerProbe) {
            const char *spawn = NULL;
            float x = playerProbe->x, y = playerProbe->y;
            if (!strcmp(scenario, "umbrella") || isUmbrellaRoom) { spawn = "obj_app_es02"; x += 48; }
            else if (!strncmp(scenario, "obj_", 4)) spawn = scenario;
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

        /* Umbrella (tutorial only): feed Marisa stars into the raised shield. */
        if (!strcmp(scenario, "umbrella") && guardProbe && runnerProbe &&
            frame >= 1100 && frame <= 2000 && frame % 100 == 0) {
            const char *spawn = "obj_b02e03_starS_shot";
            float x = guardProbe->x + 32, y = guardProbe->y - 8;
            for (unsigned i = 0; i < runnerProbe->dataWin->objt.count; ++i)
                if (!strcmp(runnerProbe->dataWin->objt.objects[i].name, spawn)) {
                    Runner_createInstance(runnerProbe, x, y, (int)i);
                    break;
                }
        }

        /* umbrella_room: after the tutorial capture is done, enter the boss room. */
        if (isUmbrellaRoom && !probeRoomEntered && inGame && frame >= entryFrame && frame <= 6500) {
            const char *target = scenario + 14;
            for (unsigned i = 0; i < runnerProbe->dataWin->room.count; ++i)
                if (!strcmp(runnerProbe->dataWin->room.rooms[i].name, target))
                    runnerProbe->pendingRoom = (int)i;
            probeRoomEntered = 1;
        }

        /* Real-motion chase: drive the player with hspeed/vspeed toward the
           boss so the engine's own movement/collision path (xprevious, solid
           restore, collision events on entry) runs, unlike an x/y teleport.
           The guard is dragged along by hspeed/vspeed as well. */
        if (hsChase && bossProbe && playerProbe && frame >= 1300) {
            float dx = bossProbe->x - playerProbe->x;
            float dy = bossProbe->y - playerProbe->y;
            float dist = dx * dx + dy * dy;
            if (dist > 25.0f) {
                float inv = 2.0f / (dist > 0.0f ? sqrt(dist) : 1.0f);
                playerProbe->hspeed = dx * inv;
                playerProbe->vspeed = dy * inv;
            } else {
                playerProbe->hspeed = 0.0f;
                playerProbe->vspeed = 0.0f;
            }
            if (guardProbe) {
                float gdx = playerProbe->x + 40.0f - guardProbe->x;
                float gdy = playerProbe->y - 16.0f - guardProbe->y;
                float gdist = gdx * gdx + gdy * gdy;
                if (gdist > 36.0f) {
                    float ginv = 2.0f / (gdist > 0.0f ? sqrt(gdist) : 1.0f);
                    guardProbe->hspeed = gdx * ginv;
                    guardProbe->vspeed = gdy * ginv;
                } else {
                    guardProbe->hspeed = 0.0f;
                    guardProbe->vspeed = 0.0f;
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
                  'next_break', 'set_action', 'vacuum_count'):
            if k in variables and variables[k] is not None:
                v = variables[k]
                actor[k] = round(v) if isinstance(v, float) else v
        result['actors'].append(actor)
    return result


def merge_snapshots(snaps, cap=4):
    groups = {}
    for s in snaps:
        sig = (s['room'], s['game_status'], s['ability'],
               tuple((a['object'], a.get('x'), a.get('y'), a.get('hp_now'),
                      a.get('mode_invin'), a.get('next_break'), a.get('set_action'))
                     for a in s['actors']))
        groups.setdefault(sig, []).append(s['frame'])
    merged = []
    for sig, frames in groups.items():
        frames = sorted(frames)
        entry = {'room': sig[0], 'game_status': sig[1], 'ability': sig[2],
                 'actors': [{'object': a[0], 'x': a[1], 'y': a[2], 'hp_now': a[3],
                             'mode_invin': a[4], 'next_break': a[5], 'set_action': a[6]} for a in sig[3]],
                 'frames': [frames[0], frames[-1]] if len(frames) > 1 else [frames[0]]}
        merged.append(entry)
    merged.sort(key=lambda m: m['frames'][0])
    return merged[:cap]


def fields_of(line, prefix):
    fields = {}
    for part in line[len(prefix):].strip().split(' '):
        if '=' in part:
            key, value = part.split('=', 1)
            fields[key] = value
    return fields


def parse_audit(log):
    events = []
    drops = []
    guard_vars = []
    for line in log.splitlines():
        if line.startswith('SANAE_AUDIT'):
            events.append(fields_of(line, 'SANAE_AUDIT'))
        elif line.startswith('SANAE_DROP'):
            drops.append(fields_of(line, 'SANAE_DROP'))
        elif line.startswith('SANAE_GUARDVAR'):
            f = fields_of(line, 'SANAE_GUARDVAR')
            guard_vars.append('%s=%s' % (f.get('name'), f.get('v')))
    if not events:
        return {'events': 0, 'guard_vars': guard_vars}
    over_frames = [e for e in events if e.get('over') == '1']
    contact_frames = [e for e in over_frames if e.get('pbul') == '0']
    drops_with_boss = [d for d in drops if d.get('over') == '1']
    drops_with_boss_no_bullets = [d for d in drops_with_boss if d.get('pbul') == '0']
    guard_drops = [d for d in drops if d.get('dghp') not in (None, '', '0', '-')]
    guard_hits = 0
    guard_first = None
    guard_hp_min = None
    guard_hp_end = None
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
            guard_hp_end = g
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
    return {
        'events': len(events),
        'overlap_boss_frames': len(over_frames),
        'contact_frames_no_bullets': len(contact_frames),
        'hp_drops_total': len(drops),
        'drops_with_boss_overlap': len(drops_with_boss),
        'drops_with_boss_overlap_no_bullets': len(drops_with_boss_no_bullets),
        'guard_hp_drops': len(guard_drops),
        'guard_bullet_overlap_frames': guard_hits,
        'guard_first_frame': guard_first,
        'guard_hp_min_seen': guard_hp_min,
        'guard_hp_end': guard_hp_end,
        'first_boss_frame': first_boss,
        'guard_vars': guard_vars,
        'drop_details': [{**{k: d.get(k, '') for k in ('frame', 'hp', 'dhp', 'bhp', 'guard', 'dghp', 'over', 'touch', 'pbul')},
                          'ov': (d.get('ov') or '')[:90], 'gov': (d.get('gov') or '')[:90]} for d in drops[:10]],
        'sample': [{k: e.get(k, '') for k in ('frame', 'room', 'hp', 'over', 'touch', 'pbul', 'guard', 'gbul')} for e in sample],
    }


def start_keys(inputs, edge):
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
        modified = original.replace(include_anchor, include_anchor + '#include "collision.h"\n#include <math.h>\n')
        modified = modified.replace(function_anchor, 'bool platformHandleEvents(void) {' + HOOK + '\n    return false;\n}')
        backend.write_text(modified)
        subprocess.run(['cmake', '--build', '.cache/sanae-build-headless', '--parallel', '4'], cwd=ROOT, check=True)
        marisa_rooms = ['Room_s02b00_marisa', 'Room_sh02b00_marisa']
        scenarios = [('baseline', {}),
                     ('obj_b02_KIRISAMEMarisa', {'SANAE_PROBE_HS_CHASE': '1'}),
                     ('room_' + marisa_rooms[0], {'SANAE_PROBE_HS_CHASE': '1'}),
                     ('umbrella', {}),
                     ('umbrella_room_' + marisa_rooms[0],
                      {'SANAE_PROBE_HS_CHASE': '1', 'SANAE_PROBE_ENTRY_FRAME': '2000'})]
        for index, (scenario, probe_env) in enumerate(scenarios):
            case = work / str(index)
            case.mkdir(exist_ok=True)
            inputs = {}
            def edge(frame, key, hold=1):
                inputs[str(frame)] = {'keysPressed': [key], 'keysReleased': []}
                inputs[str(frame + hold)] = {'keysPressed': [], 'keysReleased': [key]}
            start_keys(inputs, edge)
            end = 900
            frames = [610, 850]
            if scenario == 'obj_b02_KIRISAMEMarisa':
                # Contact: boss lands on Sanae at 600, real-motion chase 1300+.
                end = 2600
                frames = [640, 1000, 1500, 2100, 2550]
                edge(700, 90, 150)   # brief Z (attack), then chase drives motion
            elif scenario.startswith('room_'):
                end = 8000
                frames = [1100, 1800, 2800, 4600, 6200, 7900]
                for frame in range(660, 7950, 45):
                    edge(frame, 90)  # Z taps: dialog + shots
            elif scenario == 'umbrella':
                # Original working recipe: inhale pickup (X), swallow (Down),
                # raise umbrella (X); stars are fed by the hook from 1100.
                end = 2000
                frames = [950, 1150, 1300, 1500, 1750, 1950]
                edge(580, 88, 100)
                edge(720, 40, 30)
                edge(1000, 88, 950)
            elif scenario.startswith('umbrella_room_'):
                end = 8000
                frames = [1300, 1900, 2500, 3400, 4800, 6500, 7900]
                edge(580, 88, 100)
                edge(720, 40, 30)
                edge(1000, 88, 7000)  # raise umbrella and keep it up
                for frame in range(2050, 7950, 45):
                    edge(frame, 90)   # advance the boss-fight dialog
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
            record = {'scenario': scenario, 'exit': code,
                      'missing_csv_count': log.count('missing/invalid CSV'),
                      'unknown_function_count': log.count('Unknown function'),
                      'audit': parse_audit(log),
                      'snapshots': merge_snapshots([summarize(p) for p in sorted(case.glob('state-*.json'))])}
            if code not in (0,):
                record['log_tail'] = log.splitlines()[-12:]
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
