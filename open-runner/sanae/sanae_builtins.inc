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

static RValue sanae_nearest_furthest(VMContext *ctx, RValue *args, int32_t count, bool furthest);
static RValue sanae_instance_furthest(VMContext *ctx, RValue *args, int32_t count) {
    return sanae_nearest_furthest(ctx, args, count, true);
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
static RValue sanae_fullscreen(MAYBE_UNUSED VMContext *ctx, MAYBE_UNUSED RValue *args, MAYBE_UNUSED int32_t count) {
    return RValue_makeBool(true);
}
/* Kept callable on the host for a regression test of the exact Switch policy.
 * Do not disable surface_resize or display_set_gui_size: those are game drawing,
 * not desktop window controls. Getters retain the real platform dimensions. */
void Sanae_registerFixedWindow(VMContext *ctx) {
    sanae_register(ctx, "window_set_size", sanae_noop);
    sanae_register(ctx, "window_set_rectangle", sanae_noop);
    sanae_register(ctx, "window_set_position", sanae_noop);
    sanae_register(ctx, "window_set_fullscreen", sanae_noop);
    sanae_register(ctx, "window_set_region_size", sanae_noop);
    sanae_register(ctx, "window_center", sanae_noop);
    sanae_register(ctx, "window_get_fullscreen", sanae_fullscreen);
}
/* ---- Array-capable collision/instance targets (GMS 2022+ parity) ----
 * Upstream coerces an array target to object 0 through RValue_toInt32, which
 * silently retargets every multi-object floor, enemy or bullet check at once.
 * The wrappers below iterate scalar elements through the existing scalar
 * builtins and merge the results the way the native runner does. Nested
 * arrays and undefined elements carry no target meaning and are skipped. */
typedef RValue (*SanaeTargetBuiltin)(VMContext *ctx, RValue *args, int32_t count);
#define SANAE_TARGET_ARGS 9
static bool sanae_target_usable(RValue *target) {
    return target != nullptr && target->type != RVALUE_ARRAY && target->type != RVALUE_UNDEFINED;
}

/* Boolean queries (place_meeting, position_meeting): true when ANY target hits. */
static RValue sanae_bool_any(VMContext *ctx, RValue *args, int32_t count, int32_t targetIndex,
                             int32_t needed, SanaeTargetBuiltin scalar) {
    if (count < needed) return RValue_makeBool(false);
    if (args[targetIndex].type != RVALUE_ARRAY) return scalar(ctx, args, count);
    {
        GMLArray *targets = args[targetIndex].array;
        int32_t length = GMLArray_length1D(targets);
        RValue scalarArgs[SANAE_TARGET_ARGS];
        int32_t passed = count > SANAE_TARGET_ARGS ? SANAE_TARGET_ARGS : count;
        int32_t i, j;
        for (j = 0; j < passed; ++j) scalarArgs[j] = args[j];
        for (i = 0; i < length; ++i) {
            RValue *target = GMLArray_slot(targets, i);
            if (!sanae_target_usable(target)) continue;
            scalarArgs[targetIndex] = *target;
            if (RValue_toBool(scalar(ctx, scalarArgs, passed))) return RValue_makeBool(true);
        }
    }
    return RValue_makeBool(false);
}

/* First-hit queries (instance_place/position, collision_*): first element hit wins. */
static RValue sanae_first_hit(VMContext *ctx, RValue *args, int32_t count, int32_t targetIndex,
                              int32_t needed, SanaeTargetBuiltin scalar) {
    if (count < needed) return RValue_makeReal(INSTANCE_NOONE);
    if (args[targetIndex].type != RVALUE_ARRAY) return scalar(ctx, args, count);
    {
        GMLArray *targets = args[targetIndex].array;
        int32_t length = GMLArray_length1D(targets);
        RValue scalarArgs[SANAE_TARGET_ARGS];
        int32_t passed = count > SANAE_TARGET_ARGS ? SANAE_TARGET_ARGS : count;
        int32_t i, j;
        for (j = 0; j < passed; ++j) scalarArgs[j] = args[j];
        for (i = 0; i < length; ++i) {
            RValue *target = GMLArray_slot(targets, i);
            RValue result;
            if (!sanae_target_usable(target)) continue;
            scalarArgs[targetIndex] = *target;
            result = scalar(ctx, scalarArgs, passed);
            if (RValue_toInt32(result) != INSTANCE_NOONE) return result;
        }
    }
    return RValue_makeReal(INSTANCE_NOONE);
}

/* distance_to_object(obj): minimum over the union. */
static RValue sanae_min_distance(VMContext *ctx, RValue *args, int32_t count, SanaeTargetBuiltin scalar) {
    if (count < 1) return RValue_makeReal(0.0);
    if (args[0].type != RVALUE_ARRAY) return scalar(ctx, args, count);
    {
        GMLArray *targets = args[0].array;
        int32_t length = GMLArray_length1D(targets);
        GMLReal best = 0;
        bool have = false;
        int32_t i;
        for (i = 0; i < length; ++i) {
            RValue *target = GMLArray_slot(targets, i);
            RValue one, result;
            GMLReal distance;
            if (!sanae_target_usable(target)) continue;
            one = *target;
            result = scalar(ctx, &one, 1);
            distance = RValue_toReal(result);
            if (!have || distance < best) { best = distance; have = true; }
        }
        if (!have) {
            RValue none = RValue_makeReal(INSTANCE_NOONE);
            return scalar(ctx, &none, 1);
        }
        return RValue_makeReal(best);
    }
}

/* instance_nearest/furthest over a scalar-or-array target, with self/other/all
 * resolved the way the collision builtins resolve them. First instance wins
 * distance ties, matching the scalar builtins. */
static RValue sanae_nearest_furthest(VMContext *ctx, RValue *args, int32_t count, bool furthest) {
    Runner *runner = ctx->runner;
    GMLReal x, y, best = 0;
    int32_t bestId = INSTANCE_NOONE;
    bool have = false;
    int32_t i, elements;
    if (count < 3) return RValue_makeReal(INSTANCE_NOONE);
    x = RValue_toReal(args[0]); y = RValue_toReal(args[1]);
    elements = args[2].type == RVALUE_ARRAY ? GMLArray_length1D(args[2].array) : 1;
    for (i = 0; i < elements; ++i) {
        RValue *target = args[2].type == RVALUE_ARRAY ? GMLArray_slot(args[2].array, i) : &args[2];
        int32_t resolved, base, end, k;
        if (!sanae_target_usable(target)) continue;
        resolved = VM_resolveInstanceTarget(ctx, RValue_toInt32(*target));
        if (resolved == INSTANCE_NOONE) continue;
        base = Runner_pushInstancesForTarget(runner, resolved);
        end = (int32_t)arrlen(runner->instanceSnapshots);
        for (k = base; k < end; ++k) {
            Instance *inst = runner->instanceSnapshots[k];
            GMLReal dx, dy, d2;
            if (!inst->active) continue;
            dx = inst->x - x; dy = inst->y - y; d2 = dx * dx + dy * dy;
            if (!have || (furthest ? d2 > best : d2 < best)) { best = d2; bestId = (int32_t)inst->instanceId; have = true; }
        }
        Runner_popInstanceSnapshot(runner, base);
    }
    return RValue_makeReal(bestId);
}
static RValue sanae_instance_nearest(VMContext *ctx, RValue *args, int32_t count) {
    return sanae_nearest_furthest(ctx, args, count, false);
}

/* Ordered *_list results sort by distance from the calling instance (shape
 * point/center when there is no caller), nearest first with id tiebreaks.
 * Array targets additionally union without duplicating shared descendants. */
static GMLReal sanae_list_distance2(Runner *runner, int32_t id, GMLReal refX, GMLReal refY) {
    Instance *inst = hmget(runner->instancesById, id);
    GMLReal dx, dy;
    if (inst == nullptr) return (GMLReal)1e30;
    dx = inst->x - refX; dy = inst->y - refY;
    return dx * dx + dy * dy;
}
static void sanae_list_sort_slice(Runner *runner, DsList *list, int32_t base, GMLReal refX, GMLReal refY) {
    int32_t end = (int32_t)arrlen(list->items);
    int32_t i;
    for (i = base + 1; i < end; ++i) {
        RValue key = list->items[i];
        int32_t keyId = RValue_toInt32(key);
        GMLReal keyD = sanae_list_distance2(runner, keyId, refX, refY);
        int32_t j = i - 1;
        while (j >= base) {
            int32_t id = RValue_toInt32(list->items[j]);
            GMLReal d = sanae_list_distance2(runner, id, refX, refY);
            if (d > keyD || (d == keyD && id > keyId)) { list->items[j + 1] = list->items[j]; --j; }
            else break;
        }
        list->items[j + 1] = key;
    }
}
static void sanae_list_dedup_slice(DsList *list, int32_t base, int32_t before) {
    int32_t i = before;
    while (i < (int32_t)arrlen(list->items)) {
        int32_t id = RValue_toInt32(list->items[i]);
        int32_t j, tail;
        bool dup = false;
        for (j = base; j < before; ++j) {
            if (RValue_toInt32(list->items[j]) == id) { dup = true; break; }
        }
        if (!dup) { ++i; continue; }
        tail = (int32_t)arrlen(list->items) - i - 1;
        RValue_free(&list->items[i]);
        if (tail > 0) memmove(&list->items[i], &list->items[i + 1], (size_t)tail * sizeof(RValue));
        arrsetlen(list->items, arrlen(list->items) - 1);
    }
}
static RValue sanae_list_union(VMContext *ctx, RValue *args, int32_t count, int32_t targetIndex,
                               int32_t listIndex, int32_t orderedIndex, int32_t needed,
                               GMLReal fallbackX, GMLReal fallbackY, SanaeTargetBuiltin scalar) {
    Runner *runner = ctx->runner;
    bool ordered;
    DsList *list;
    int32_t base, total;
    GMLReal refX, refY;
    if (count < needed) return RValue_makeReal(0.0);
    if (ctx->currentInstance != nullptr) { refX = ctx->currentInstance->x; refY = ctx->currentInstance->y; }
    else { refX = fallbackX; refY = fallbackY; }
    if (args[targetIndex].type != RVALUE_ARRAY) {
        RValue result;
        list = dsListGet(runner, RValue_toInt32(args[listIndex]));
        base = list != nullptr ? (int32_t)arrlen(list->items) : 0;
        result = scalar(ctx, args, count);
        ordered = count > orderedIndex && RValue_toBool(args[orderedIndex]);
        if (ordered && list != nullptr && (int32_t)arrlen(list->items) > base + 1)
            sanae_list_sort_slice(runner, list, base, refX, refY);
        return result;
    }
    {
        GMLArray *targets = args[targetIndex].array;
        int32_t length = GMLArray_length1D(targets);
        RValue scalarArgs[SANAE_TARGET_ARGS];
        int32_t passed = count > SANAE_TARGET_ARGS ? SANAE_TARGET_ARGS : count;
        int32_t i, j;
        for (j = 0; j < passed; ++j) scalarArgs[j] = args[j];
        list = dsListGet(runner, RValue_toInt32(args[listIndex]));
        base = list != nullptr ? (int32_t)arrlen(list->items) : 0;
        ordered = count > orderedIndex && RValue_toBool(args[orderedIndex]);
        for (i = 0; i < length; ++i) {
            RValue *target = GMLArray_slot(targets, i);
            int32_t before;
            if (!sanae_target_usable(target)) continue;
            before = list != nullptr ? (int32_t)arrlen(list->items) : 0;
            scalarArgs[targetIndex] = *target;
            (void)scalar(ctx, scalarArgs, passed);
            if (list != nullptr) sanae_list_dedup_slice(list, base, before);
        }
        total = list != nullptr ? (int32_t)arrlen(list->items) - base : 0;
        if (ordered && list != nullptr && total > 1)
            sanae_list_sort_slice(runner, list, base, refX, refY);
    }
    return RValue_makeReal((GMLReal)total);
}

static RValue sanae_place_meeting(VMContext *ctx, RValue *args, int32_t count) {
    return sanae_bool_any(ctx, args, count, 2, 3, builtin_place_meeting);
}
static RValue sanae_position_meeting(VMContext *ctx, RValue *args, int32_t count) {
    return sanae_bool_any(ctx, args, count, 2, 3, builtin_position_meeting);
}
static RValue sanae_instance_position(VMContext *ctx, RValue *args, int32_t count) {
    return sanae_first_hit(ctx, args, count, 2, 3, builtin_instance_position);
}
static RValue sanae_collision_point(VMContext *ctx, RValue *args, int32_t count) {
    return sanae_first_hit(ctx, args, count, 2, 5, builtin_collision_point);
}
static RValue sanae_collision_rectangle(VMContext *ctx, RValue *args, int32_t count) {
    return sanae_first_hit(ctx, args, count, 4, 7, builtin_collision_rectangle);
}
static RValue sanae_collision_circle(VMContext *ctx, RValue *args, int32_t count) {
    return sanae_first_hit(ctx, args, count, 3, 6, builtin_collision_circle);
}
static RValue sanae_collision_ellipse(VMContext *ctx, RValue *args, int32_t count) {
    return sanae_first_hit(ctx, args, count, 4, 7, builtin_collision_ellipse);
}
static RValue sanae_collision_line(VMContext *ctx, RValue *args, int32_t count) {
    return sanae_first_hit(ctx, args, count, 4, 7, builtin_collision_line);
}
static RValue sanae_instance_place_list(VMContext *ctx, RValue *args, int32_t count) {
    GMLReal x = count > 0 ? RValue_toReal(args[0]) : 0;
    GMLReal y = count > 1 ? RValue_toReal(args[1]) : 0;
    return sanae_list_union(ctx, args, count, 2, 3, 4, 5, x, y, builtin_instance_place_list);
}
static RValue sanae_collision_rectangle_list(VMContext *ctx, RValue *args, int32_t count) {
    GMLReal cx = count > 3 ? (RValue_toReal(args[0]) + RValue_toReal(args[2])) / 2 : 0;
    GMLReal cy = count > 3 ? (RValue_toReal(args[1]) + RValue_toReal(args[3])) / 2 : 0;
    return sanae_list_union(ctx, args, count, 4, 7, 8, 8, cx, cy, builtin_collision_rectangle_list);
}
static RValue sanae_collision_circle_list(VMContext *ctx, RValue *args, int32_t count) {
    GMLReal x = count > 0 ? RValue_toReal(args[0]) : 0;
    GMLReal y = count > 1 ? RValue_toReal(args[1]) : 0;
    return sanae_list_union(ctx, args, count, 3, 6, 7, 8, x, y, builtin_collision_circle_list);
}
static RValue sanae_collision_ellipse_list(VMContext *ctx, RValue *args, int32_t count) {
    GMLReal cx = count > 3 ? (RValue_toReal(args[0]) + RValue_toReal(args[2])) / 2 : 0;
    GMLReal cy = count > 3 ? (RValue_toReal(args[1]) + RValue_toReal(args[3])) / 2 : 0;
    return sanae_list_union(ctx, args, count, 4, 7, 8, 8, cx, cy, builtin_collision_ellipse_list);
}
static RValue sanae_collision_line_list(VMContext *ctx, RValue *args, int32_t count) {
    GMLReal cx = count > 3 ? (RValue_toReal(args[0]) + RValue_toReal(args[2])) / 2 : 0;
    GMLReal cy = count > 3 ? (RValue_toReal(args[1]) + RValue_toReal(args[3])) / 2 : 0;
    return sanae_list_union(ctx, args, count, 4, 7, 8, 9, cx, cy, builtin_collision_line_list);
}
static RValue sanae_distance_to_object(VMContext *ctx, RValue *args, int32_t count) {
    return sanae_min_distance(ctx, args, count, builtin_distance_to_object);
}

static bool sanae_exists_target(VMContext *ctx, RValue *target) {
    Runner *runner = ctx->runner;
    int32_t id, base, end, i;
    if (target->type == RVALUE_ARRAY) {
        GMLArray *elements = target->array;
        int32_t length = GMLArray_length1D(elements);
        for (i = 0; i < length; ++i) {
            RValue *element = GMLArray_slot(elements, i);
            if (!sanae_target_usable(element)) continue;
            if (sanae_exists_target(ctx, element)) return true;
        }
        return false;
    }
    id = VM_resolveInstanceTarget(ctx, RValue_toInt32(*target));
    if (id == INSTANCE_NOONE) return false;
    if (id == INSTANCE_ALL) {
        int32_t total = (int32_t)arrlen(runner->instances);
        for (i = 0; i < total; ++i) {
            if (runner->instances[i]->active) return true;
        }
        return false;
    }
    if (id >= 0 && runner->dataWin->objt.count > (uint32_t)id) {
        bool found = false;
        base = Runner_pushInstancesOfObject(runner, id);
        end = (int32_t)arrlen(runner->instanceSnapshots);
        for (i = base; i < end; ++i) {
            if (runner->instanceSnapshots[i]->active) { found = true; break; }
        }
        Runner_popInstanceSnapshot(runner, base);
        return found;
    }
    {
        Instance *inst = hmget(runner->instancesById, id);
        return inst != nullptr && inst->active;
    }
}
static RValue sanae_instance_exists(VMContext *ctx, RValue *args, int32_t count) {
    if (count < 1) return RValue_makeBool(false);
    return RValue_makeBool(sanae_exists_target(ctx, &args[0]));
}

static void sanae_destroy_target(VMContext *ctx, RValue *target, bool performEvents) {
    Runner *runner = ctx->runner;
    int32_t id, base, end, i;
    if (target->type == RVALUE_ARRAY) {
        GMLArray *elements = target->array;
        int32_t length = GMLArray_length1D(elements);
        for (i = 0; i < length; ++i) {
            RValue *element = GMLArray_slot(elements, i);
            if (!sanae_target_usable(element)) continue;
            sanae_destroy_target(ctx, element, performEvents);
        }
        return;
    }
    id = VM_resolveInstanceTarget(ctx, RValue_toInt32(*target));
    if (id == INSTANCE_NOONE) return;
    if (id == INSTANCE_ALL || (id >= 0 && runner->dataWin->objt.count > (uint32_t)id)) {
        /* Destroy events run user code that can spawn/destroy/instance_change;
         * iterate a snapshot so those mutations cannot corrupt the loop. */
        base = Runner_pushInstancesOfObject(runner, id);
        end = (int32_t)arrlen(runner->instanceSnapshots);
        for (i = base; i < end; ++i) {
            Instance *inst = runner->instanceSnapshots[i];
            if (inst->active) Runner_destroyInstance(runner, inst, performEvents);
        }
        Runner_popInstanceSnapshot(runner, base);
        return;
    }
    {
        Instance *inst = hmget(runner->instancesById, id);
        if (inst != nullptr && inst->active) Runner_destroyInstance(runner, inst, performEvents);
    }
}
static RValue sanae_instance_destroy(VMContext *ctx, RValue *args, int32_t count) {
    if (count < 1) {
        if (ctx->currentInstance != nullptr)
            Runner_destroyInstance(ctx->runner, ctx->currentInstance, true);
        return RValue_makeUndefined();
    }
    sanae_destroy_target(ctx, &args[0], count >= 2 ? RValue_toBool(args[1]) : true);
    return RValue_makeUndefined();
}

/* draw_healthbar with fill direction, clamped amount and an optional black
 * border. Upstream always filled left-to-right and never drew the border;
 * direction is 0 = left-to-right, 1 = right-to-left, 2 = top-to-bottom,
 * 3 = bottom-to-top. Omitted trailing arguments behave as undefined. */
static RValue sanae_draw_healthbar(VMContext *ctx, RValue *args, int32_t count) {
    Runner *runner = ctx->runner;
    float x1, y1, x2, y2, amount, f, fx1, fy1, fx2, fy2;
    uint32_t backCol, minCol, maxCol, fill;
    int32_t direction;
    bool showBack, showBorder;
    if (runner->renderer == nullptr) return RValue_makeUndefined();
    if (count < 8) return RValue_makeUndefined();
    x1 = (float)RValue_toReal(args[0]);
    y1 = (float)RValue_toReal(args[1]);
    x2 = (float)RValue_toReal(args[2]);
    y2 = (float)RValue_toReal(args[3]);
    amount = (float)RValue_toReal(args[4]);
    backCol = RValue_toColour(args[5]);
    minCol = RValue_toColour(args[6]);
    maxCol = RValue_toColour(args[7]);
    direction = count > 8 ? RValue_toInt32(args[8]) : 0;
    showBack = count > 9 ? RValue_toBool(args[9]) : false;
    showBorder = count > 10 ? RValue_toBool(args[10]) : false;
    if (direction < 0 || direction > 3) direction = 0;
    f = amount / 100.0f;
    if (f < 0.0f) f = 0.0f;
    if (f > 1.0f) f = 1.0f;
    fill = (uint32_t)Color_lerp((int32_t)minCol, (int32_t)maxCol, f);
    if (showBack)
        runner->renderer->vtable->drawRectangle(runner->renderer, x1, y1, x2, y2, backCol, runner->renderer->drawAlpha, false);
    fx1 = x1; fy1 = y1; fx2 = x2; fy2 = y2;
    switch (direction) {
        case 0: fx2 = x1 + (x2 - x1) * f; break;
        case 1: fx1 = x2 + (x1 - x2) * f; break;
        case 2: fy2 = y1 + (y2 - y1) * f; break;
        default: fy1 = y2 + (y1 - y2) * f; break;
    }
    runner->renderer->vtable->drawRectangle(runner->renderer, fx1, fy1, fx2, fy2, fill, runner->renderer->drawAlpha, false);
    if (showBorder)
        runner->renderer->vtable->drawRectangle(runner->renderer, x1, y1, x2, y2, 0x000000u, runner->renderer->drawAlpha, true);
    return RValue_makeUndefined();
}

/* display_get_width/height report the real platform window (a fixed console
 * has no separate monitor size), falling back to the game's defaults. */
static RValue sanae_display_width(VMContext *ctx, MAYBE_UNUSED RValue *args, MAYBE_UNUSED int32_t count) {
    Runner *runner = ctx->runner;
    if (runner != nullptr && runner->getWindowSize != nullptr) {
        int32_t w = 0, h = 0;
        if (runner->getWindowSize(&w, &h)) return RValue_makeReal((GMLReal)w);
    }
    return RValue_makeReal((GMLReal)ctx->dataWin->gen8.defaultWindowWidth);
}
static RValue sanae_display_height(VMContext *ctx, MAYBE_UNUSED RValue *args, MAYBE_UNUSED int32_t count) {
    Runner *runner = ctx->runner;
    if (runner != nullptr && runner->getWindowSize != nullptr) {
        int32_t w = 0, h = 0;
        if (runner->getWindowSize(&w, &h)) return RValue_makeReal((GMLReal)h);
    }
    return RValue_makeReal((GMLReal)ctx->dataWin->gen8.defaultWindowHeight);
}

/* Modern struct/array JSON. json_decode/json_encode cover only legacy
 * ds_map/ds_list containers; games using json_parse/json_stringify for saves
 * or configuration otherwise hit unknown-function calls. Depth is capped so
 * cyclic or hostile input cannot overflow the C stack. */
#define SANAE_JSON_MAX_DEPTH 64
static RValue sanae_json_parse_value(VMContext *ctx, JsonValue *json, int depth) {
    int i, length;
    if (json == nullptr || depth > SANAE_JSON_MAX_DEPTH) return RValue_makeUndefined();
    switch (json->type) {
        case JSON_NULL: return RValue_makeUndefined();
        case JSON_BOOL: return RValue_makeBool(json->boolValue);
        case JSON_NUMBER: return RValue_makeReal((GMLReal)json->numberValue);
        case JSON_STRING:
            return RValue_makeOwnedString(safeStrdup(json->stringValue != nullptr ? json->stringValue : ""));
        case JSON_ARRAY: {
            GMLArray *elements = GMLArray_create(ctx->dataWin->gen8.wadVersion, 0);
            length = JsonReader_arrayLength(json);
            for (i = 0; i < length; ++i) {
                RValue element = sanae_json_parse_value(ctx, JsonReader_getArrayElement(json, i), depth + 1);
                GMLArray_add(elements, element);
                RValue_free(&element);
            }
            return RValue_makeArray(elements);
        }
        case JSON_OBJECT: {
            Instance *members = Runner_createStruct(ctx->runner);
            length = JsonReader_objectLength(json);
            for (i = 0; i < length; ++i) {
                const char *key = JsonReader_getJsonKeyByIndex(json, i);
                RValue element = sanae_json_parse_value(ctx, JsonReader_getJsonValueByIndex(json, i), depth + 1);
                VM_structSetAndFreeVal(ctx, members, key != nullptr ? key : "", element, -1);
            }
            return RValue_makeStructAndIncRef(members);
        }
        default: return RValue_makeUndefined();
    }
}
static RValue sanae_json_parse(VMContext *ctx, RValue *args, int32_t count) {
    JsonValue *json;
    RValue result;
    if (count < 1 || args[0].type != RVALUE_STRING || args[0].string == nullptr)
        return RValue_makeUndefined();
    json = JsonReader_parse(args[0].string);
    if (json == nullptr) return RValue_makeUndefined();
    result = sanae_json_parse_value(ctx, json, 0);
    JsonReader_free(json);
    return result;
}
static void sanae_json_stringify_value(VMContext *ctx, JsonWriter *writer, RValue val,
                                       bool floatMarkers, int depth) {
    if (depth > SANAE_JSON_MAX_DEPTH) { JsonWriter_null(writer); return; }
    if (val.type == RVALUE_STRUCT && val.structInst != nullptr) {
        Instance *members = val.structInst;
        uint32_t i;
        JsonWriter_beginObject(writer);
        for (i = 0; i < members->selfVars.capacity; ++i) {
            IntRValueEntry entry = members->selfVars.entries[i];
            char *name;
            if (entry.key == INT_RVALUE_HASHMAP_EMPTY_KEY) continue;
            name = VM_getVariableNameByVarId(ctx, entry.key);
            JsonWriter_key(writer, name != nullptr ? name : "");
            sanae_json_stringify_value(ctx, writer, entry.value, floatMarkers, depth + 1);
        }
        JsonWriter_endObject(writer);
        return;
    }
    if (val.type == RVALUE_ARRAY && val.array != nullptr) {
        int32_t length = GMLArray_length1D(val.array);
        int32_t i;
        JsonWriter_beginArray(writer);
        for (i = 0; i < length; ++i) {
            RValue *slot = GMLArray_slot(val.array, i);
            sanae_json_stringify_value(ctx, writer, slot != nullptr ? *slot : RValue_makeUndefined(), floatMarkers, depth + 1);
        }
        JsonWriter_endArray(writer);
        return;
    }
    jsonEncodeValue(writer, val, floatMarkers, ctx->dataWin);
}
static RValue sanae_json_stringify(VMContext *ctx, RValue *args, int32_t count) {
    JsonWriter writer;
    char *result;
    bool floatMarkers;
    if (count < 1) return RValue_makeOwnedString(safeStrdup("null"));
    floatMarkers = DataWin_isVersionAtLeast(ctx->dataWin, 2023, 2, 0, 0);
    writer = JsonWriter_create();
    sanae_json_stringify_value(ctx, &writer, args[0], floatMarkers, 0);
    result = JsonWriter_copyOutput(&writer);
    JsonWriter_free(&writer);
    return RValue_makeOwnedString(result != nullptr ? result : safeStrdup("null"));
}

/* string_width_ext mirrors string_width over wrapped lines. Upstream leaves it
 * a zero stub, which collapses any dialogue or menu layout that measures
 * wrapped text. */
static RValue sanae_string_width_ext(VMContext *ctx, RValue *args, int32_t count) {
    Runner *runner = ctx->runner;
    Renderer *renderer = runner->renderer;
    int32_t fontIndex = renderer->drawFont;
    Font *font;
    char *str;
    PreprocessedText processed, wrapped;
    int32_t textLen, lineStart;
    float maxWidth = 0;
    if (count < 3) return RValue_makeReal(0.0);
    if (0 > fontIndex || renderer->dataWin->font.count <= (uint32_t)fontIndex) return RValue_makeReal(0.0);
    font = &renderer->dataWin->font.fonts[fontIndex];
    str = RValue_toString(args[0], ctx->runner->dataWin);
    processed = TextUtils_preprocessGmlTextIfNeeded(runner, str);
    wrapped = TextUtils_wrapText(font, processed.text, RValue_toInt32(args[2]));
    textLen = (int32_t)strlen(wrapped.text);
    lineStart = 0;
    while (textLen >= lineStart) {
        int32_t lineEnd = lineStart;
        int32_t lineLen;
        float lineWidth;
        while (textLen > lineEnd && !TextUtils_isNewlineChar(wrapped.text[lineEnd])) ++lineEnd;
        lineLen = lineEnd - lineStart;
        lineWidth = TextUtils_measureLineWidth(font, wrapped.text + lineStart, lineLen);
        if (lineWidth > maxWidth) maxWidth = lineWidth;
        if (textLen > lineEnd) lineStart = TextUtils_skipNewline(wrapped.text, lineEnd, textLen);
        else break;
    }
    PreprocessedText_free(wrapped);
    PreprocessedText_free(processed);
    free(str);
    return RValue_makeReal((GMLReal)(maxWidth * font->scaleX));
}

/* show_error surfaces the game's own fatal path instead of silently running
 * past it; aborting requests a clean exit like the native dialog's abort. */
static RValue sanae_show_error(VMContext *ctx, RValue *args, int32_t count) {
    char *message;
    bool abort;
    if (count < 1) return RValue_makeUndefined();
    message = RValue_toString(args[0], ctx->runner->dataWin);
    abort = count > 1 && RValue_toBool(args[1]);
    logError("SANAE: show_error: %s%s\n", message != nullptr ? message : "", abort ? " (aborting)" : "");
    free(message);
    if (abort && ctx->runner != nullptr) ctx->runner->shouldExit = true;
    return RValue_makeUndefined();
}

/* All texture pages stay resident, so group loads are already satisfied and
 * cache prefetch/flush calls keep working by doing nothing. */
static RValue sanae_texturegroup_status(MAYBE_UNUSED VMContext *ctx, MAYBE_UNUSED RValue *args, MAYBE_UNUSED int32_t count) {
    return RValue_makeBool(true);
}
static RValue sanae_random_get_seed(VMContext *ctx, MAYBE_UNUSED RValue *args, MAYBE_UNUSED int32_t count) {
    return RValue_makeReal((GMLReal)ctx->runner->random.lastSeed);
}
static RValue sanae_draw_enable_drawevent(VMContext *ctx, RValue *args, int32_t count) {
    if (count >= 1 && ctx->runner != nullptr)
        ctx->runner->sanaeDrawEventsEnabled = RValue_toBool(args[0]);
    return RValue_makeUndefined();
}
/* GMS 2024 instance_place accepts an array of collision targets. Do not coerce
 * the array to object zero: that silently removes floors and all other checks. */
static RValue sanae_instance_place(VMContext *ctx, RValue *args, int32_t count) {
    return sanae_first_hit(ctx, args, count, 2, 3, builtin_instance_place);
}
static void sanae_register_builtins(VMContext *ctx) {
    sanae_register(ctx, "audio_group_set_gain", sanae_group_gain);
    sanae_register(ctx, "audio_group_get_gain", sanae_group_get_gain);
    sanae_register(ctx, "audio_group_stop_all", sanae_group_stop);
    sanae_register(ctx, "audio_sound_loop", sanae_sound_loop);
    sanae_register(ctx, "audio_sound_get_loop", sanae_sound_get_loop);
    sanae_register(ctx, "load_csv", sanae_load_csv);
    sanae_register(ctx, "instance_place", sanae_instance_place);
    sanae_register(ctx, "instance_position", sanae_instance_position);
    sanae_register(ctx, "instance_place_list", sanae_instance_place_list);
    sanae_register(ctx, "place_meeting", sanae_place_meeting);
    sanae_register(ctx, "position_meeting", sanae_position_meeting);
    sanae_register(ctx, "collision_point", sanae_collision_point);
    sanae_register(ctx, "collision_rectangle", sanae_collision_rectangle);
    sanae_register(ctx, "collision_circle", sanae_collision_circle);
    sanae_register(ctx, "collision_ellipse", sanae_collision_ellipse);
    sanae_register(ctx, "collision_line", sanae_collision_line);
    sanae_register(ctx, "collision_rectangle_list", sanae_collision_rectangle_list);
    sanae_register(ctx, "collision_circle_list", sanae_collision_circle_list);
    sanae_register(ctx, "collision_ellipse_list", sanae_collision_ellipse_list);
    sanae_register(ctx, "collision_line_list", sanae_collision_line_list);
    sanae_register(ctx, "distance_to_object", sanae_distance_to_object);
    sanae_register(ctx, "instance_nearest", sanae_instance_nearest);
    sanae_register(ctx, "instance_furthest", sanae_instance_furthest);
    sanae_register(ctx, "instance_exists", sanae_instance_exists);
    sanae_register(ctx, "instance_destroy", sanae_instance_destroy);
    sanae_register(ctx, "draw_healthbar", sanae_draw_healthbar);
    sanae_register(ctx, "display_get_width", sanae_display_width);
    sanae_register(ctx, "display_get_height", sanae_display_height);
    sanae_register(ctx, "json_parse", sanae_json_parse);
    sanae_register(ctx, "json_stringify", sanae_json_stringify);
    sanae_register(ctx, "string_width_ext", sanae_string_width_ext);
    sanae_register(ctx, "show_error", sanae_show_error);
    sanae_register(ctx, "to_string", builtin_string);
    sanae_register(ctx, "texturegroup_load", sanae_noop);
    sanae_register(ctx, "texturegroup_status", sanae_texturegroup_status);
    sanae_register(ctx, "texturegroup_unload", sanae_noop);
    sanae_register(ctx, "texture_prefetch", sanae_noop);
    sanae_register(ctx, "texture_flush", sanae_noop);
    sanae_register(ctx, "sprite_prefetch", sanae_noop);
    sanae_register(ctx, "sprite_flush", sanae_noop);
    sanae_register(ctx, "draw_texture_flush", sanae_noop);
    sanae_register(ctx, "draw_enable_drawevent", sanae_draw_enable_drawevent);
    sanae_register(ctx, "random_get_seed", sanae_random_get_seed);
    sanae_register(ctx, "steam_initialised", sanae_false);
    sanae_register(ctx, "steam_get_achievement", sanae_false);
    sanae_register(ctx, "steam_is_overlay_enabled", sanae_false);
    sanae_register(ctx, "steam_update", sanae_noop);
    sanae_register(ctx, "steam_shutdown", sanae_noop);
#ifdef __SWITCH__
    Sanae_registerFixedWindow(ctx);
#endif
}
