/* Original SANAE compatibility builtins; incorporated into an AGPL runner.
 * No copyrighted game bytecode or closed-runtime binary is included. */
#include "sanae_csv.h"

static RValue sanae_load_csv(VMContext *ctx, RValue *args, int32_t count) {
    SanaeCsv csv;
    RValue dimensions[2], result;
    DsGrid *grid;
    FileSystem *fs = ctx->runner->fileSystem;
    char *contents;
    size_t i;
    if (count != 1 || args[0].type != RVALUE_STRING) return RValue_makeReal(-1);
    contents = fs->vtable->readFileText(fs, args[0].string);
    if (!contents || !sanae_csv_parse(contents, &csv)) {
        logError("SANAE: missing/invalid CSV: %s\n", args[0].string);
        free(contents); return RValue_makeReal(-1);
    }
    free(contents);
    dimensions[0] = RValue_makeReal((GMLReal)csv.columns);
    dimensions[1] = RValue_makeReal((GMLReal)csv.rows);
    result = builtin_ds_grid_create(ctx, dimensions, 2);
    grid = dsGridGet(ctx->runner, RValue_toInt32(result));
    if (!grid) { sanae_csv_free(&csv); return RValue_makeReal(-1); }
    /* Preserve text, including leading zeros and empty cells. Game-side real()
       conversions retain responsibility for numeric interpretation. */
    for (i = 0; i < csv.rows * csv.columns; ++i)
        grid->items[i] = RValue_makeOwnedString(safeStrdup(""));
    for (i = 0; i < csv.count; ++i) {
        SanaeCsvCell *cell = &csv.cells[i];
        RValue *value = &grid->items[cell->row * csv.columns + cell->column];
        RValue_free(value);
        *value = RValue_makeOwnedString(safeStrdup(cell->text));
    }
    sanae_csv_free(&csv);
    return result;
}

static RValue sanae_instance_furthest(VMContext *ctx, RValue *args, int32_t count) {
    Runner *runner = ctx->runner;
    int32_t id = INSTANCE_NOONE, base, end, i;
    GMLReal x, y, best = 0;
    if (count < 3) return RValue_makeReal(id);
    x = RValue_toReal(args[0]); y = RValue_toReal(args[1]);
    base = Runner_pushInstancesOfObject(runner, RValue_toInt32(args[2]));
    end = (int32_t)arrlen(runner->instanceSnapshots);
    for (i = base; i < end; ++i) {
        Instance *instance = runner->instanceSnapshots[i];
        GMLReal dx, dy, distance;
        if (!instance->active) continue;
        dx = instance->x - x; dy = instance->y - y;
        distance = dx*dx + dy*dy;
        if (id == INSTANCE_NOONE || distance > best) {
            best = distance; id = instance->instanceId;
        }
    }
    Runner_popInstanceSnapshot(runner, base);
    return RValue_makeReal(id);
}

/* On this non-Steam platform, optional availability/achievement queries are
 * false; no Steam identity, entitlement, achievements or cloud save is forged. */
static RValue sanae_false(MAYBE_UNUSED VMContext *ctx, MAYBE_UNUSED RValue *args, MAYBE_UNUSED int32_t count) {
    return RValue_makeBool(false);
}
static RValue sanae_noop(MAYBE_UNUSED VMContext *ctx, MAYBE_UNUSED RValue *args, MAYBE_UNUSED int32_t count) {
    return RValue_makeUndefined();
}
static RValue sanae_group_gain(VMContext *ctx, RValue *args, int32_t count) {
    AudioSystem *audio = ctx->runner->audioSystem;
    int32_t time;
    if (count < 3 || !audio || !audio->vtable->sanaeGroupGain) return RValue_makeUndefined();
    time = RValue_toInt32(args[2]);
    audio->vtable->sanaeGroupGain(audio, RValue_toInt32(args[0]), (float)RValue_toReal(args[1]), time < 0 ? 0 : (uint32_t)time);
    return RValue_makeUndefined();
}
static RValue sanae_group_get_gain(VMContext *ctx, RValue *args, int32_t count) {
    AudioSystem *audio = ctx->runner->audioSystem;
    if (count < 1 || !audio || !audio->vtable->sanaeGroupGetGain) return RValue_makeReal(1);
    return RValue_makeReal(audio->vtable->sanaeGroupGetGain(audio, RValue_toInt32(args[0])));
}
static RValue sanae_group_stop(VMContext *ctx, RValue *args, int32_t count) {
    AudioSystem *audio = ctx->runner->audioSystem;
    if (count >= 1 && audio && audio->vtable->sanaeGroupStop) audio->vtable->sanaeGroupStop(audio, RValue_toInt32(args[0]));
    return RValue_makeUndefined();
}
static RValue sanae_sound_loop(VMContext *ctx, RValue *args, int32_t count) {
    AudioSystem *audio = ctx->runner->audioSystem;
    if (count >= 2 && audio && audio->vtable->sanaeSetLoop) audio->vtable->sanaeSetLoop(audio, RValue_toInt32(args[0]), RValue_toBool(args[1]));
    return RValue_makeUndefined();
}
static RValue sanae_sound_get_loop(VMContext *ctx, RValue *args, int32_t count) {
    AudioSystem *audio = ctx->runner->audioSystem;
    return RValue_makeBool(count >= 1 && audio && audio->vtable->sanaeGetLoop && audio->vtable->sanaeGetLoop(audio, RValue_toInt32(args[0])));
}
static void sanae_register(VMContext *ctx, const char *name, BuiltinFunc function) {
    ptrdiff_t index = shgeti(ctx->builtinMap, name);
    if (index >= 0) ctx->builtinMap[index].value = function;
    else VM_registerBuiltin(ctx, name, function);
}
static void sanae_register_builtins(VMContext *ctx) {
    sanae_register(ctx, "audio_group_set_gain", sanae_group_gain);
    sanae_register(ctx, "audio_group_get_gain", sanae_group_get_gain);
    sanae_register(ctx, "audio_group_stop_all", sanae_group_stop);
    sanae_register(ctx, "audio_sound_loop", sanae_sound_loop);
    sanae_register(ctx, "audio_sound_get_loop", sanae_sound_get_loop);
    sanae_register(ctx, "load_csv", sanae_load_csv);
    sanae_register(ctx, "instance_furthest", sanae_instance_furthest);
    sanae_register(ctx, "steam_initialised", sanae_false);
    sanae_register(ctx, "steam_get_achievement", sanae_false);
    sanae_register(ctx, "steam_is_overlay_enabled", sanae_false);
    sanae_register(ctx, "steam_update", sanae_noop);
    sanae_register(ctx, "steam_shutdown", sanae_noop);
#ifdef __SWITCH__
    /* Fullscreen console has no desktop window position. */
    sanae_register(ctx, "window_set_position", sanae_noop);
#endif
}
