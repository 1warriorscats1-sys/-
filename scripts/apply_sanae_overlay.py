#!/usr/bin/env python3
"""Apply SANAE integration to a disposable copy of the pinned upstream sources.

Fails on source drift. Never run against the toolkit or your upstream checkout.
Original integration script: MIT; resulting linked runner: AGPL-3.0.
"""
from pathlib import Path
import shutil

ROOT = Path(__file__).resolve().parents[1]
OVERLAY = ROOT / 'open-runner/sanae'


def apply(source: Path):
    def replace(name, before, after):
        p = source / name
        text = p.read_text()
        if text.count(before) != 1:
            raise RuntimeError(f'Upstream drift / already patched: {name}: {before[:90]!r}')
        p.write_text(text.replace(before, after, 1))

    for p in OVERLAY.iterdir():
        if p.suffix in ('.h', '.inc'):
            shutil.copy2(p, source / 'src' / p.name)
    shutil.copy2(OVERLAY / 'main.c', source / 'src/switch/main.c')
    # Swap only A/B; suppress every right-stick path without changing enumeration.
    replace('src/switch/switch_input.c', '''static void mapLibnxToGml(GamepadSlot* slot, PadState* pad, u64 cur) {
    if (cur & HidNpadButton_A) slot->buttonDown[0] = true;
    if (cur & HidNpadButton_B) slot->buttonDown[1] = true;
    if (cur & HidNpadButton_Y) slot->buttonDown[2] = true;
    if (cur & HidNpadButton_X) slot->buttonDown[3] = true;
    if (cur & HidNpadButton_L) slot->buttonDown[4] = true;
    if (cur & HidNpadButton_R) slot->buttonDown[5] = true;
    slot->buttonValue[6] = (cur & HidNpadButton_ZL) ? 1.0f : 0.0f;
    slot->buttonValue[7] = (cur & HidNpadButton_ZR) ? 1.0f : 0.0f;
    if (cur & HidNpadButton_Minus) slot->buttonDown[8] = true;
    if (cur & HidNpadButton_Plus) slot->buttonDown[9] = true;
    if (cur & HidNpadButton_StickL) slot->buttonDown[10] = true;
    if (cur & HidNpadButton_StickR) slot->buttonDown[11] = true;
    if (cur & HidNpadButton_AnyUp) slot->buttonDown[12] = true;
    if (cur & HidNpadButton_AnyDown) slot->buttonDown[13] = true;
    if (cur & HidNpadButton_AnyLeft) slot->buttonDown[14] = true;
    if (cur & HidNpadButton_AnyRight) slot->buttonDown[15] = true;

    HidAnalogStickState l = padGetStickPos(pad, 0);
    HidAnalogStickState r = padGetStickPos(pad, 1);
    slot->axisValue[0] = l.x / 32767.0f;
    slot->axisValue[1] = -l.y / 32767.0f;
    slot->axisValue[2] = r.x / 32767.0f;
    slot->axisValue[3] = -r.y / 32767.0f;
}
''',
            '#include "sanae_switch_mapping.inc"\n')
    replace('CMakeLists.txt', 'NAME "Butterscotch" AUTHOR "Butterscotch" VERSION "1.0.0"',
            'NAME "SANAE - Sylphid Breeze" AUTHOR "sorehodoh" VERSION "01.01"')

    replace('src/vm_builtins.c', '// ===[ REGISTRATION ]===',
            '#include "sanae_builtins.inc"\n\n// ===[ REGISTRATION ]===')
    replace('src/vm_builtins.c', '    VM_registerBuiltin(ctx, "sprite_get_info", builtin_sprite_get_info);',
            '    VM_registerBuiltin(ctx, "sprite_get_info", builtin_sprite_get_info);\n    sanae_register_builtins(ctx);')

    replace('src/vm_builtins.c', 'static RValue builtin_ds_grid_set(VMContext* ctx, MAYBE_UNUSED RValue* args, MAYBE_UNUSED int32_t argCount) {\n    if (argCount > 3)',
            'static RValue builtin_ds_grid_set(VMContext* ctx, MAYBE_UNUSED RValue* args, MAYBE_UNUSED int32_t argCount) {\n    if (argCount < 4)')

    # Preserve dirty state after failed close so shutdown / a subsequent close can retry.
    replace('src/vm_builtins.c', '        fs->vtable->writeFileText(fs, runner->currentIniPath, serialized);', '''        if (!fs->vtable->writeFileText(fs, runner->currentIniPath, serialized)) {
            runner->sanaeSaveFailed = true;
            logError("SANAE: INI write failed: %s\\n", runner->currentIniPath);
            return RValue_makeOwnedString(serialized);
        }
        runner->currentIniDirty = false;''')
    replace('src/vm_builtins.c', '        fs->vtable->writeFileText(fs, file->filePath, file->writeBuffer);', '''        if (!fs->vtable->writeFileText(fs, file->filePath, file->writeBuffer)) {
            runner->sanaeSaveFailed = true;
            logError("SANAE: text write failed: %s\\n", file->filePath);
            return RValue_makeUndefined();
        }''')
    replace('src/vm_builtins.c', 'static RValue builtin_ini_open(VMContext* ctx, RValue* args, int32_t argCount) {', '''static RValue builtin_ini_close(VMContext* ctx, RValue* args, int32_t argCount);
static RValue builtin_ini_open(VMContext* ctx, RValue* args, int32_t argCount) {''')
    for comment in ('// Close any previously open INI (implicit close, no disk write)',
                    '// Implicit close of any open INI (no disk write)'):
        replace('src/vm_builtins.c', comment + '''
    if (runner->currentIni != nullptr) {
        Ini_free(runner->currentIni);
        runner->currentIni = nullptr;
    }''', '''// Flush an implicit close too; do not discard an unsuccessful write.
    if (runner->currentIni != nullptr) {
        RValue closed = builtin_ini_close(ctx, nullptr, 0);
        RValue_free(&closed);
        if (runner->currentIni != nullptr) return RValue_makeUndefined();
    }''')

    # Sanitizer-confirmed undefined behavior on empty containers/BC17 locals.
    replace('src/runner.c',
            '        qsort(dst->events, dst->eventCount, sizeof(FlattenedCollisionEvent), compareTargetObjectIndexAscending);',
            '        if (dst->eventCount > 1)\n'
            '            qsort(dst->events, dst->eventCount, sizeof(FlattenedCollisionEvent), compareTargetObjectIndexAscending);')
    replace('src/vm.c',
            '        memcpy(resizedLocalVars, ctx->localVars, sizeof(RValue) * ctx->localVarCount);',
            '        if (ctx->localVarCount > 0)\n'
            '            memcpy(resizedLocalVars, ctx->localVars, sizeof(RValue) * ctx->localVarCount);')
    replace('src/event_table.c',
            '    memset(slotCursor, 0, (size_t) slotCount * sizeof(uint32_t));',
            '    if (slotCount > 0) memset(slotCursor, 0, (size_t) slotCount * sizeof(uint32_t));')
    # Negative/high-bit integer keys must not shift a signed promoted byte.
    for prefix in ('    data = ', '    unsigned int hash = ', '    size_t hash = '):
        before = prefix + 'd[0] | (d[1] << 8) | (d[2] << 16) | (d[3] << 24);'
        replace('vendor/stb/ds/stb_ds.h', before,
                before.replace('(d[3] << 24)', '((unsigned int)d[3] << 24)'))
    replace('vendor/stb/ds/stb_ds.h', 'case 4: data |= (d[3] << 24);',
            'case 4: data |= ((unsigned int)d[3] << 24);')
    # Buffer storage survives ordinary room changes, but not game_restart/free.
    replace('src/runner.c', '    runner->dsGridPool = nullptr;', """    runner->dsGridPool = nullptr;

    for (ptrdiff_t i = 0; i < arrlen(runner->gmlBufferPool); ++i) {
        free(runner->gmlBufferPool[i].data);
    }
    arrfree(runner->gmlBufferPool);
    runner->gmlBufferPool = nullptr;""")
    # Headless tooling: avoid allocating empty hash maps in a by-value args copy.
    for field in ('dumpFrames', 'dumpJsonFrames', 'screenshotFrames', 'screenshotSurfacesFrames'):
        replace('src/loop.c', f'hmget(args.{field}, runner->frameCount)',
                f'(args.{field} != nullptr && hmget(args.{field}, runner->frameCount))')
    replace('src/noop_audio_system.c',
            'static void noopDestroy(AudioSystem* audio) {\n    free(audio);',
            'static void noopDestroy(AudioSystem* audio) {\n    arrfree(audio->audioGroups);\n    free(audio);')

    # The slot-major dedup order depends on collision target IDs. SANAE's
    # bilateral contact/capture scripts require later concrete objects first:
    # player/vacuum must observe the enemy before its damage/destroy handler.
    # Build the collision responder list in descending resource order once,
    # without changing subtype resolution, instance order or other events.
    replace('src/runner.c', """                arrput(runner->objectsWithAnyEventOfType[t], obj);
            }
        }
    }

    free(seen);""", """                arrput(runner->objectsWithAnyEventOfType[t], obj);
            }
        }
        if (t == EVENT_COLLISION) {
            // SANAE: resource order, not the incidental first target slot.
            arrsetlen(runner->objectsWithAnyEventOfType[t], 0);
            for (int32_t obj = objectCount; obj-- > 0;) {
                if (seen[obj]) arrput(runner->objectsWithAnyEventOfType[t], obj);
            }
        }
    }

    free(seen);""")

    replace('src/runner.c', 'static void dispatchCollisionEvents(Runner* runner) {',
            '#include "sanae_collision_dispatch.inc"\n\n'
            'static void dispatchCollisionEvents(Runner* runner) {\n'
            '    struct { uint64_t key; uint64_t value; } *sanaePairs = nullptr;')
    # Every matching subscription fires exactly once per pair: the first hit
    # notifies both sides (bilateral, resource-ordered), later visits by the
    # same side fire only their own additional subscription, and the reverse
    # bucket fires only subscriptions the reverse pass did not already run.
    replace('src/runner.c', '                    if (other == self) continue;', """                    if (other == self) continue;
                    if (!self->active) break;
                    uint32_t lo = self->instanceId < other->instanceId ? self->instanceId : other->instanceId;
                    uint32_t hi = self->instanceId < other->instanceId ? other->instanceId : self->instanceId;
                    uint64_t sanaePairKey = ((uint64_t)lo << 32) | hi;
                    ptrdiff_t sanaePairIndex = hmgeti(sanaePairs, sanaePairKey);
                    if (sanaePairIndex >= 0) {
                        uint64_t sanaePairValue = sanaePairs[sanaePairIndex].value;
                        if ((uint32_t)(sanaePairValue >> 32) != self->instanceId &&
                            (uint32_t)sanaePairValue == evt->targetObjectIndex) continue;
                    }""")
    replace('src/runner.c', '                    // Collision detected! If either instance is solid, restore both to xprevious/yprevious.',
            '                    bool sanaePairFresh = sanaePairIndex < 0;\n'
            '                    uint32_t sanaeReverseTarget = SANAE_COLLISION_NO_REVERSE;\n\n'
            '                    // Collision detected! If either instance is solid, restore both to xprevious/yprevious.')
    # Replace the pinned solid-only reverse notification block, retaining the
    # existing solid rollback/path/precise-mask logic on either side of it.
    collision_source = (source / 'src/runner.c').read_text()
    first = '#ifdef ENABLE_VM_TRACING\n                    if (traceThisPair) logInfo("  fire self->other:'
    last = '                    // Native parity for solids: collision event can alter path state'
    if collision_source.count(first) != 1 or collision_source.count(last) != 1:
        raise RuntimeError('Upstream collision dispatch block drift')
    begin = collision_source.index(first)
    end = collision_source.index(last, begin)
    replace('src/runner.c', collision_source[begin:end],
            '                    if (sanaePairFresh) {\n'
            '                        sanaeReverseTarget = sanaeDispatchCollisionPair(runner, self, other, evt);\n'
            '                        hmput(sanaePairs, sanaePairKey, ((uint64_t)self->instanceId << 32) | sanaeReverseTarget);\n'
            '                    } else if (!runner->shouldExit) {\n'
            '                        executeCollisionEvent(runner, self, other, (int32_t)evt->targetObjectIndex,\n'
            '                                              evt->codeId, evt->ownerObjectIndex);\n'
            '                    }\n\n')
    replace('src/runner.c', """        arrsetlen(runner->instanceSnapshots, selfSnapBase);
    }
}

// ===[ View Following + Clamping ]===""", """        arrsetlen(runner->instanceSnapshots, selfSnapBase);
    }
    hmfree(sanaePairs);
}

// ===[ View Following + Clamping ]===""")

    # SANAE's equal-depth GUI instances are created back-to-front. The pinned
    # runner sorts their IDs descending, so opaque frame backgrounds cover the
    # portrait and HP fill. Keep depth/type and non-instance ordering unchanged.
    replace('src/runner.c',
            '    if (a->type == DRAWABLE_TILE)\n'
            '        return (a->order > b->order) - (a->order < b->order); // tiles: higher index later',
            '    if (a->type == DRAWABLE_TILE || a->type == DRAWABLE_INSTANCE)\n'
            '        return (a->order > b->order) - (a->order < b->order); // instances: newer IDs draw on top')
    replace('src/runner.c', '// instance/layer: higher first',
            '// layers/particle systems: higher first')
    replace('src/runner.h', '#define OTHER_GAME_START     2', '#define OTHER_GAME_START     2\n#define OTHER_GAME_END       3')
    replace('src/runner.h', '    bool shouldExit;', '    bool shouldExit;\n    bool sanaeSaveFailed;\n    bool sanaeDrawEventsEnabled;')
    # draw_enable_drawevent(false) suppresses only the normal Draw event; the
    # instance still draws its sprite, and Begin/End/GUI drawing is unaffected.
    replace('src/runner.c', '    runner->appSurfaceEnabled = true;',
            '    runner->appSurfaceEnabled = true;\n    runner->sanaeDrawEventsEnabled = true;')
    replace('src/runner.c',
            '            int32_t codeId = findEventCodeIdAndOwner(runner, inst->objectIndex, EVENT_DRAW, DRAW_NORMAL, &ownerObjectIndex);\n'
            '            if (codeId >= 0) {',
            '            int32_t codeId = findEventCodeIdAndOwner(runner, inst->objectIndex, EVENT_DRAW, DRAW_NORMAL, &ownerObjectIndex);\n'
            '            if (!runner->sanaeDrawEventsEnabled) codeId = -1;\n'
            '            if (codeId >= 0) {')
    # random_get_seed reports the seed random_set_seed/randomize installed.
    replace('src/random.h', '    uint32_t state[16];\n    uint32_t index;',
            '    uint32_t state[16];\n    uint32_t index;\n    uint32_t lastSeed;')
    replace('src/random.c', 'void Random_setSeed(Random* m, uint32_t seed) {\n    m->state[0] = seed;',
            'void Random_setSeed(Random* m, uint32_t seed) {\n    m->lastSeed = seed;\n    m->state[0] = seed;')
    # random_set_seed takes one argument; do not read past it.
    replace('src/vm_builtins.c', '    bool fixRangeBug = RValue_toBool(args[1]); ',
            '    bool fixRangeBug = argCount > 1 && RValue_toBool(args[1]);')
    # Slot 0 follows the system default pad while slots 1-7 bind No2-No8, so
    # player 1 can hop between physical controllers. Bind every slot to its
    # own No1-No8 controller instead; enumeration is unchanged.
    replace('src/switch/switch_input.c', '''        padInitializeDefault(&pads[0]);
        for (int i = 1; SWITCH_NPAD_COUNT > i; i++) {
            padInitialize(&pads[i], HidNpadIdType_No1 + i);
        }''', '''        for (int i = 0; SWITCH_NPAD_COUNT > i; i++) {
            padInitialize(&pads[i], HidNpadIdType_No1 + i);
        }''')
    # game_restart destroys the game's controller DS lists, not the physical pads.
    # Re-emit discovery once after the new room's Create events. connectedPrev is
    # overwritten by beginFrame, so resetting that field alone cannot work.
    replace('src/runner.h', '    RunnerGamepadState* gamepads;',
            '    RunnerGamepadState* gamepads;\n    bool sanaeRediscoverGamepads;')
    replace('src/runner.c', '    runner->gameStartFired = false;',
            '    runner->gameStartFired = false;\n    runner->sanaeRediscoverGamepads = true;')
    replace('src/runner.c', '    for (int i = 0; MAX_GAMEPADS > i; i++) {',
            '    bool rediscover = runner->sanaeRediscoverGamepads;\n'
            '    runner->sanaeRediscoverGamepads = false;\n'
            '    for (int i = 0; MAX_GAMEPADS > i; i++) {')
    replace('src/runner.c', '        if (slot->connected != slot->connectedPrev) {',
            '        if ((rediscover && slot->connected) || slot->connected != slot->connectedPrev) {')
    replace('src/runner.h', 'void Runner_free(Runner* runner);',
            'bool Runner_sanaeFlushSaves(Runner* runner);\nvoid Runner_free(Runner* runner);')
    replace('src/runner.c', 'if (isEventBlockedByPendingRoom(runner, instance, eventType) || runner->shouldExit)',
            'if ((isEventBlockedByPendingRoom(runner, instance, eventType) || runner->shouldExit) &&\n        !(eventType == EVENT_OTHER && eventSubtype == OTHER_GAME_END))')
    replace('src/runner.c', 'static void cleanupState(Runner* runner) {', '''bool Runner_sanaeFlushSaves(Runner* runner) {
    bool ok = true;
    FileSystem *fs = runner->fileSystem;
    if (fs == nullptr) return true;
    if (runner->currentIni && runner->currentIniPath && runner->currentIniDirty) {
        char *text = Ini_serialize(runner->currentIni, INI_SERIALIZE_DEFAULT_INITIAL_CAPACITY);
        if (!fs->vtable->writeFileText(fs, runner->currentIniPath, text)) {
            logError("SANAE: shutdown INI save failed: %s\\n", runner->currentIniPath);
            ok = false;
        } else runner->currentIniDirty = false;
        free(text);
    }
    repeat(MAX_OPEN_TEXT_FILES, i) {
        OpenTextFile *file = &runner->openTextFiles[i];
        if (file->isOpen && file->isWriteMode && file->writeBuffer && file->filePath) {
            if (!fs->vtable->writeFileText(fs, file->filePath, file->writeBuffer)) {
                logError("SANAE: shutdown text save failed: %s\\n", file->filePath);
                ok = false;
            } else file->isWriteMode = false;
        }
    }
    if (!ok) runner->sanaeSaveFailed = true;
    return ok;
}

static void cleanupState(Runner* runner) {
    (void)Runner_sanaeFlushSaves(runner);''')
    replace('src/loop.c', '        // Snapshot any pending game_change request before we tear the runner down', '''        // Deliver Game End with audio/renderer/filesystem still alive, exactly once.
        if (actuallyShuttingDown) Runner_executeEventForAll(runner, EVENT_OTHER, OTHER_GAME_END);
        bool sanaeSaveFailed = !Runner_sanaeFlushSaves(runner) || runner->sanaeSaveFailed;

        // Snapshot any pending game_change request before we tear the runner down''')
    replace('src/loop.c', '''            return 0;
        }

        // game_change was called''', '''            return sanaeSaveFailed ? 1 : 0;
        }

        // game_change was called''')

    replace('src/overlay_file_system.c', '#include "overlay_file_system.h"',
            '#include "overlay_file_system.h"\n#include "sanae_save.h"')
    replace('src/overlay_file_system.c', '''    char* saveFull = joinPath(ofs->savePath, normalized);
    if (pathExists(saveFull))''', '''    char* saveFull = joinPath(ofs->savePath, normalized);
    sanae_save_recover(saveFull);
    if (pathExists(saveFull))''')
    # Recover explicitly resolved save filenames too, without changing bundled reads.
    p = source / 'src/overlay_file_system.c'
    text = p.read_text()
    start = text.index('static char* resolveForRead(')
    end = text.index('// Returns a heap-allocated full path for writes.', start)
    body = text[start:end].replace('    if (isAbsolute(normalized)) return normalized;\n', '')
    body = body.replace('if (strncmp(normalized, ofs->savePath, strlen(ofs->savePath)) == 0) return normalized;',
                        'if (strncmp(normalized, ofs->savePath, strlen(ofs->savePath)) == 0) {\n        sanae_save_recover(normalized); return normalized;\n    }\n    if (isAbsolute(normalized)) return normalized;')
    p.write_text(text[:start] + body + text[end:])
    replace('src/overlay_file_system.c', '''    FILE* f = fopen(fullPath, "wb");
    free(fullPath);
    if (f == nullptr) return false;

    size_t len = strlen(contents);
    size_t written = fwrite(contents, 1, len, f);
    fclose(f);
    return written == len;''', '''    bool ok = sanae_save_text(fullPath, contents) != 0;
    if (!ok) logError("SANAE: save failed: %s\\n", fullPath);
    free(fullPath);
    return ok;''')
    replace('src/overlay_file_system.c', '''    int result = remove(fullPath);
    free(fullPath);
    return result == 0;''', '''    // A deliberate delete must not resurrect the previous generation on read.
    size_t n = strlen(fullPath);
    char *backup = (char *)safeMalloc(n + 5);
    memcpy(backup, fullPath, n); memcpy(backup + n, ".bak", 5);
    if (sanae_save_exists(backup) && remove(backup) != 0) {
        free(backup); free(fullPath); return false;
    }
    free(backup);
    int result = remove(fullPath);
    free(fullPath);
    return result == 0;''')

    # Audio API extension: persistent per-group gain/fades also affect future voices.
    replace('src/audio_system.h', '    bool (*groupIsLoaded)(AudioSystem* audio, int32_t groupIndex);', '''    bool (*groupIsLoaded)(AudioSystem* audio, int32_t groupIndex);
    void (*sanaeGroupGain)(AudioSystem*, int32_t, float, uint32_t);
    float (*sanaeGroupGetGain)(AudioSystem*, int32_t);
    void (*sanaeGroupStop)(AudioSystem*, int32_t);
    void (*sanaeSetLoop)(AudioSystem*, int32_t, bool);
    bool (*sanaeGetLoop)(AudioSystem*, int32_t);''')
    replace('src/audio/openal/al_audio_system.h', '#include "audio_system.h"',
            '#include "audio_system.h"\n#include "sanae_audio_gain.h"')
    replace('src/audio/openal/al_audio_system.h', '    AudioSystem base;',
            '    AudioSystem base;\n    SanaeGain *sanaeGroups;\n    uint32_t sanaeGroupCount;')
    audio = 'src/audio/openal/al_audio_system.c'
    replace(audio, 'static void maInit(AudioSystem* audio, DataWin* dataWin, FileSystem* fileSystem) {',
            '#include "sanae_al.inc"\n\nstatic void maInit(AudioSystem* audio, DataWin* dataWin, FileSystem* fileSystem) {')
    replace(audio, '    ma->base.dw = dataWin;', '''    ma->base.dw = dataWin;
    ma->sanaeGroupCount = dataWin->agrp.count;
    ma->sanaeGroups = (SanaeGain *)safeCalloc(ma->sanaeGroupCount, sizeof(SanaeGain));
    for (uint32_t group = 0; group < ma->sanaeGroupCount; ++group) sanae_gain_init(&ma->sanaeGroups[group]);''')
    replace(audio, '    arrfree(ma->base.audioGroups);', '    arrfree(ma->base.audioGroups);\n    free(ma->sanaeGroups);')
    replace(audio, '''static void maUpdate(AudioSystem* audio, float deltaTime) {
    AlAudioSystem* ma = (AlAudioSystem*) audio;''', '''static void maUpdate(AudioSystem* audio, float deltaTime) {
    AlAudioSystem* ma = (AlAudioSystem*) audio;
    for (uint32_t group = 0; group < ma->sanaeGroupCount; ++group) sanae_gain_update(&ma->sanaeGroups[group], deltaTime);''')
    replace(audio, '''        if (inst->streaming) {
            // Recycle''', '''        alSourcef(inst->alSource, AL_GAIN, sanae_al_gain(ma, inst->soundIndex, inst->currentGain));
        if (inst->streaming) {
            // Recycle''')
    p = source / audio
    text = p.read_text().replace('AL_GAIN, inst->currentGain);', 'AL_GAIN, sanae_al_gain(ma, inst->soundIndex, inst->currentGain));')
    text = text.replace('alSourcef(inst->alSource, AL_GAIN, gain);', 'alSourcef(inst->alSource, AL_GAIN, sanae_al_gain(ma, inst->soundIndex, gain));')
    text = text.replace('AL_GAIN, volume);', 'AL_GAIN, sanae_al_gain(ma, soundIndex, volume));')
    p.write_text(text)
    replace(audio, '    AlAudioSystemVtable.init = maInit;', '''    AlAudioSystemVtable.sanaeGroupGain = sanae_al_group_gain;
    AlAudioSystemVtable.sanaeGroupGetGain = sanae_al_group_get_gain;
    AlAudioSystemVtable.sanaeGroupStop = sanae_al_group_stop;
    AlAudioSystemVtable.sanaeSetLoop = sanae_al_set_loop;
    AlAudioSystemVtable.sanaeGetLoop = sanae_al_get_loop;
    AlAudioSystemVtable.init = maInit;''')

    # Hardware crash +0xbe908: audio group 11 was looked up in an append-order array.
    replace(audio, '#include "sanae_al.inc"', '#include "sanae_al_groups.inc"\n#include "sanae_al.inc"')
    replace(audio, '    arrput(ma->base.audioGroups, dataWin);', '    sanae_al_init_groups(audio, dataWin);')
    p = source / audio
    text = p.read_text()
    start = text.index('static void maGroupLoad(AudioSystem* audio, int32_t groupIndex) {')
    end = text.index('// ===[ Audio Streams ]===', start)
    text = text[:start] + text[end:]
    text = text.replace('DataWin* dw = ma->base.audioGroups[0];', 'DataWin* dw = ma->base.dw;')
    text = text.replace('if (0 > soundIndex || (uint32_t) soundIndex >= dw->sond.count)',
                        'if (!dw || 0 > soundIndex || (uint32_t) soundIndex >= dw->sond.count)')
    text = text.replace('if (0 > sound->audioFile || (uint32_t) sound->audioFile >= ma->base.audioGroups[sound->audioGroup]->audo.count)',
                        'if (!sanae_al_group(audio, sound->audioGroup) || 0 > sound->audioFile || (uint32_t) sound->audioFile >= sanae_al_group(audio, sound->audioGroup)->audo.count)')
    # Validate before allocating AL objects, including unloaded/nonexistent groups.
    text = text.replace('        alGenSources(1, &slot->alSource);\n        alGenBuffers(1, &slot->alBuffer);', '        bool needsGroup = (sound->flags & AUDIO_ENTRY_FLAG_REGULAR) != AUDIO_ENTRY_FLAG_REGULAR ||\n            (sound->flags & (AUDIO_ENTRY_FLAG_IS_EMBEDDED | AUDIO_ENTRY_FLAG_IS_COMPRESSED)) != 0;\n        DataWin *group = sanae_al_group(audio, sound->audioGroup);\n        if (needsGroup && (!group || sound->audioFile < 0 || (uint32_t)sound->audioFile >= group->audo.count)) {\n            logWarn("SANAE: refusing sound %d: group %d unloaded/invalid or audio entry %d missing\\n", soundIndex, sound->audioGroup, sound->audioFile);\n            return -1;\n        }\n        alGenSources(1, &slot->alSource);\n        alGenBuffers(1, &slot->alBuffer);')
    p.write_text(text)

    # game_end during Step must not draw a frame without the game's Draw/GUI fade.
    replace('src/loop.c', '#include "loop.h"', '#include "loop.h"\n#include "sanae_frame_policy.h"')
    replace('src/loop.c', '                Runner_step(runner);', '                Runner_step(runner);\n                if (runner->shouldExit) {\n                    actuallyShuttingDown = true;\n                    break; /* Keep the last fully presented frame, not a partial menu. */\n                }')
    replace('src/loop.c', '                if (runner->pendingRoom == -1)\n                    platformSwapBuffers();',
            '                if (sanae_should_present(runner->shouldExit, runner->pendingRoom))\n                    platformSwapBuffers();')
    replace('src/loop.c', '                Runner_handlePendingRoomChange(runner);',
            '                if (!runner->shouldExit) Runner_handlePendingRoomChange(runner);')
    # The entry point redirects stderr; upstream printed all diagnostics to stdout.
    replace('src/switch/log.c', '    printf("%s%s%s", colourPrefix, buffer, ANSI_COLOUR_CODE_RESET);',
            '    (void)colourPrefix;\n    fputs(buffer, stderr);\n    fflush(stderr);')

    replace(audio, '    slot->streaming = false;\n    slot->vorbis = nullptr;', '    slot->loop = loop;\n    slot->streaming = false;\n    slot->vorbis = nullptr;')

    replace('CMakeLists.txt', 'nx_create_nro(butterscotch NACP Butterscotch.nacp)',
            'nx_create_nro(butterscotch NACP Butterscotch.nacp ICON "${CMAKE_CURRENT_SOURCE_DIR}/sanae-icon.jpg")')

    replace(audio, '    slot->loop = loop;\n    slot->streaming = false;',
            '    slot->alSource = slot->alBuffer = 0;\n    slot->loop = loop;\n    slot->streaming = false;')
    replace(audio, '        alGenSources(1, &slot->alSource);\n        alGenBuffers(1, &slot->alBuffer);', '''        if (needsGroup) {
            DataWin_loadAudoIfNeeded(group, (uint32_t)sound->audioFile);
            AudioEntry *entry = &group->audo.entries[sound->audioFile];
            if (!entry->present || !entry->data || !entry->dataSize) {
                logWarn("SANAE: missing audio payload for sound %d in group %d\\n", soundIndex, sound->audioGroup);
                return -1;
            }
        }
        alGenSources(1, &slot->alSource);
        alGenBuffers(1, &slot->alBuffer);''')
    p = source / audio
    text = p.read_text()
    start = text.index('        bool needsGroup =')
    end = text.index('    // Apply properties', start)
    body = text[start:end].replace('return -1;', 'sanae_al_discard_pending(slot); return -1;')
    p.write_text(text[:start] + body + text[end:])
