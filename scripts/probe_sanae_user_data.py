#!/usr/bin/env python3
"""Diagnostic-only host probes. Never modify data.win or publish raw game dumps.

This instruments only the builder-owned disposable no-op backend, restores it
in finally, and leaves the production Switch overlay untouched. Forced spawns
and room entry are not normal progression or physical Switch verification.
"""
import json
import os
from pathlib import Path
import subprocess

ROOT = Path(__file__).resolve().parents[1]
REPORT = ROOT / '.cache/user-data-report.json'
HOOK = r'''
    const char *scenario = getenv("SANAE_PRIVATE_SCENARIO");
    if (scenario) {
        Instance *player = NULL, *guard = NULL;
        for (int i = 0; i < arrlen(g_runner->instances); ++i) {
            Instance *a = g_runner->instances[i];
            if (!a->active) continue;
            const char *name = g_runner->dataWin->objt.objects[a->objectIndex].name;
            if (!strcmp(name, "obj_P01_KOCHIYASanae")) player = a;
            if (!strcmp(name, "obj_p01s02e02_gurad")) guard = a;
        }
        int frame = g_runner->frameCount;
        const char *spawn = NULL;
        float x = player ? player->x : 0, y = player ? player->y : 0;
        if (frame == 600 && player) {
            if (!strcmp(scenario, "umbrella")) { spawn = "obj_app_es02"; x += 48; }
            else if (!strncmp(scenario, "obj_", 4)) spawn = scenario;
            else if (!strncmp(scenario, "Room_", 5)) {
                for (unsigned i = 0; i < g_runner->dataWin->room.count; ++i)
                    if (!strcmp(g_runner->dataWin->room.rooms[i].name, scenario))
                        g_runner->pendingRoom = (int)i;
            }
        }
        if (!strcmp(scenario, "umbrella") && guard && frame >= 1100 && frame <= 1800 && frame % 100 == 0) {
            spawn = "obj_b02e03_starS_shot";
            x = guard->x + 32; y = guard->y - 8;
        }
        if (spawn) {
            for (unsigned i = 0; i < g_runner->dataWin->objt.count; ++i)
                if (!strcmp(g_runner->dataWin->objt.objects[i].name, spawn)) {
                    Runner_createInstance(g_runner, x, y, (int)i);
                    break;
                }
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


def main():
    data, = (ROOT / '.cache/user-game').rglob('data.win')
    report = json.loads(REPORT.read_text())
    stage = ROOT / '.cache/sanae-source-headless'
    if not (stage / '.sanae-generated').is_file():
        raise RuntimeError('Not a disposable builder-owned source tree')
    backend = stage / 'src/backends/noop.c'
    original = backend.read_text()
    anchor = 'bool platformHandleEvents(void) {\n    return false;\n}'
    if original.count(anchor) != 1:
        raise RuntimeError('Diagnostic hook source drift')
    work = ROOT / '.cache/user-probe'
    work.mkdir(parents=True, exist_ok=True)
    report['probes'] = []
    try:
        backend.write_text(original.replace(anchor, 'bool platformHandleEvents(void) {' + HOOK + '\n    return false;\n}'))
        subprocess.run(['cmake', '--build', '.cache/sanae-build-headless', '--parallel', '4'], cwd=ROOT, check=True)
        scenarios = ['baseline', 'obj_b02_KIRISAMEMarisa', 'obj_mb00_KIRISAMEMarisa_tutorial',
                     'obj_mb00_KIRISAMEMarisa', 'obj_b02_KIRISAMEMarisa_hs02',
                     'obj_b02_KIRISAMEMarisa_exs01', 'obj_b02_KIRISAMEMarisa_exs02',
                     'obj_b02_KIRISAMEMarisa_exs03', 'umbrella',
                     'Room_s02b00_marisa', 'Room_sh02b00_marisa', 'Room_sh01m00_marisa']
        for index, scenario in enumerate(scenarios):
            case = work / str(index)
            case.mkdir(exist_ok=True)
            inputs = {}
            def edge(frame, key, hold=1):
                inputs[str(frame)] = {'keysPressed': [key], 'keysReleased': []}
                inputs[str(frame + hold)] = {'keysPressed': [], 'keysReleased': [key]}
            for frame in range(60, 361, 60):
                edge(frame, 90)
            end = 900
            frames = [610, 700, 850]
            if scenario == 'umbrella':
                edge(580, 88, 100); edge(720, 40, 30); edge(1000, 88, 900)
                end = 2000
                frames = [1050, 1150, 1250, 1350, 1450, 1550, 1601, 1650, 1850]
            if scenario.startswith('Room_'):
                end = 8000
                frames = [850, 1400, 2400, 4800, 7800]
                for frame in range(660, 7900, 45):
                    edge(frame, 90)
                # Scripted walking/turning, not teleporting the player into the boss.
                for frame in range(1600, 7600, 800):
                    edge(frame, 39 if (frame // 800) % 2 == 0 else 37, 360)
            (case / 'inputs.json').write_text(json.dumps(inputs))
            args = [str(ROOT / '.cache/sanae-build-headless/butterscotch'), str(data),
                    '--headless', '--exit-at-frame', str(end), '--playback-inputs', str(case / 'inputs.json'),
                    '--save-folder', str(case / 'saves'), '--dump-frame-json-file', str(case / 'state-%d.json'),
                    '--disable-log-colours']
            for frame in frames:
                args += ['--dump-frame-json', str(frame)]
            try:
                proc = subprocess.run(args, env=dict(os.environ, SANAE_PRIVATE_SCENARIO=scenario),
                                      stdout=subprocess.PIPE, stderr=subprocess.STDOUT, timeout=90)
                log = proc.stdout.decode(errors='replace')
                code = proc.returncode
            except subprocess.TimeoutExpired as error:
                log = (error.stdout or b'').decode(errors='replace'); code = 'timeout'
            (case / 'private.log').write_text(log)
            record = {'scenario': scenario, 'exit': code,
                      'missing_csv_count': log.count('missing/invalid CSV'),
                      'unknown_function_count': log.count('Unknown function'),
                      'snapshots': [summarize(p) for p in sorted(case.glob('state-*.json'))]}
            report['probes'].append(record)
            REPORT.write_text(json.dumps(report, indent=2))
        report.update(status='probes completed', completed=True,
                      caveat='Host no-op audio/render probes; forced room entry and spawns are not Switch verification.')
    finally:
        backend.write_text(original)
        REPORT.write_text(json.dumps(report, indent=2))

if __name__ == '__main__':
    main()
