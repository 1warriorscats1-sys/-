/* Synthetic fixtures only: exercise the integrated VM and filesystem. */
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#include "runner.h"
#include "vm.h"
#include "vm_builtins.h"
#include "overlay_file_system.h"
#include "stb_ds.h"
#include "log.h"
#include "gml_array.h"
#include "spatial_grid.h"
#include "noop_renderer.h"
#include "noop_audio_system.h"
#include "noop_file_system.h"
void platformLog(const logType type, const char *format, va_list args) {
    (void)type; vfprintf(stderr, format, args);
}

void Sanae_registerFixedWindow(VMContext *ctx);
static int resize_calls;
static void resize_spy(int32_t width, int32_t height) { (void)width; (void)height; ++resize_calls; }
static bool fixed_size(int32_t *width, int32_t *height) { *width = 1280; *height = 720; return true; }
static RValue string(const char *s) { return RValue_makeOwnedString(strdup(s)); }
static RValue invoke(VMContext *ctx, const char *name, RValue *args, int count) {
    BuiltinFunc f = VM_findBuiltin(ctx, name); assert(f); return f(ctx, args, count);
}
static void discard(RValue value) { RValue_free(&value); }
static void has(FileSystem *fs, const char *path, const char *text) {
    char *actual = fs->vtable->readFileText(fs, path);
    if (!actual || !strstr(actual, text)) fprintf(stderr, "File %s expected [%s], got [%s]\n", path, text, actual ? actual : "NULL");
    assert(actual && strstr(actual, text)); free(actual);
}
static void restart_gamepads(void) {
    DataWin dw = {0}; Room room = {.name="synthetic", .width=480, .height=270, .speed=60};
    dw.gen8.wadVersion=17; dw.room.count=1; dw.room.rooms=&room;
    VMContext *vm=VM_create(&dw);
    Renderer *renderer=NoopRenderer_create();
    AudioSystem *audio=(AudioSystem*)NoopAudioSystem_create();
    FileSystem *fs=NoopFileSystem_create();
    Runner *runner=Runner_create(&dw,vm,renderer,fs,audio,1);
    /* Simulate pads still physically connected when game_restart resets GML. */
    runner->gamepads->slots[0].connected=runner->gamepads->slots[0].connectedPrev=true;
    runner->gamepads->slots[7].connected=runner->gamepads->slots[7].connectedPrev=true;
    runner->sanaeRediscoverGamepads=false;
    Runner_reset(runner);
    assert(runner->sanaeRediscoverGamepads);
    assert(runner->gamepads->slots[0].connected && runner->gamepads->slots[7].connected);
    RunnerGamepad_beginFrame(runner->gamepads); /* must not erase rediscovery */
    runner->currentRoom=&room; runner->currentRoomIndex=0;
    Runner_step(runner);
    assert(!runner->sanaeRediscoverGamepads);
    assert(arrlen(runner->dsMapPool)==2); /* one async map per connected pad, not all 16 */
    RunnerGamepad_beginFrame(runner->gamepads);
    Runner_step(runner);
    assert(arrlen(runner->dsMapPool)==2); /* no duplicate discoveries next frame */
    Runner_free(runner); VM_free(vm);
    renderer->vtable->destroy(renderer);
    audio->vtable->destroy(audio); NoopFileSystem_destroy(fs);
    puts("game_restart: connected pads retained, discovery after beginFrame once per pad, no duplicates passed.");
}
static void collision_arrays(void) {
    DataWin dw = {0}; Runner runner = {0}; VMContext vm = {0};
    GameObject objects[4] = {0};
    Sprite sprite = {.width=10, .height=10, .bboxMode=1, .sepMasks=2};
    Instance caller = {.instanceId=100001, .objectIndex=0, .active=true,
        .spriteIndex=0, .maskIndex=-1, .imageXscale=1, .imageYscale=1};
    Instance wall = {.instanceId=100002, .objectIndex=1, .active=true,
        .spriteIndex=0, .maskIndex=-1, .imageXscale=1, .imageYscale=1, .x=8};
    for (int i=0; i<4; ++i) objects[i].parentId=-1;
    objects[1].parentId=2;
    dw.objt.count=4; dw.objt.objects=objects; dw.sprt.count=1; dw.sprt.sprites=&sprite;
    runner.dataWin=&dw; runner.vmContext=&vm; runner.spatialGrid=SpatialGrid_create(4,4);
    vm.runner=&runner; vm.dataWin=&dw; vm.currentInstance=&caller;
    arrput(runner.spatialGrid->grid[0], &wall);
    VMBuiltins_registerAll(&vm);
    RValue args[] = {RValue_makeReal(8), RValue_makeReal(0), RValue_makeReal(1)};
    assert(RValue_toInt32(invoke(&vm,"instance_place",args,3))==100002);
    GMLArray *targets=GMLArray_create(17,2);
    *GMLArray_slot(targets,0)=RValue_makeReal(3); /* first target misses */
    *GMLArray_slot(targets,1)=RValue_makeAssetRef(2,0); /* parent object hits */
    args[2]=RValue_makeArray(targets);
    assert(RValue_toInt32(invoke(&vm,"instance_place",args,3))==100002);
    assert(caller.x==0 && caller.y==0); /* probe cannot move the player */
    assert(GMLArray_length1D(targets)==2 && RValue_toInt32(*GMLArray_slot(targets,1))==2);
    wall.active=false;
    assert(RValue_toInt32(invoke(&vm,"instance_place",args,3))==INSTANCE_NOONE);
    wall.active=true; args[0]=RValue_makeReal(100);
    assert(RValue_toInt32(invoke(&vm,"instance_place",args,3))==INSTANCE_NOONE);
    args[0]=RValue_makeReal(8);
    *GMLArray_slot(targets,1)=RValue_makeReal(100002); /* instance IDs too */
    assert(RValue_toInt32(invoke(&vm,"instance_place",args,3))==100002);
    discard(args[2]); args[2]=RValue_makeArray(GMLArray_create(17,0));
    assert(RValue_toInt32(invoke(&vm,"instance_place",args,3))==INSTANCE_NOONE);
    assert(RValue_toInt32(invoke(&vm,"instance_place",NULL,0))==INSTANCE_NOONE);
    discard(args[2]); SpatialGrid_free(runner.spatialGrid); shfree(vm.builtinMap);
    puts("instance_place: scalar, arrays, descendants, IDs, inactive, miss, empty and position preservation passed.");
}

/* Original synthetic bytecode calls a fixture builtin through the real VM.
 * Lower object is an enemy, higher is a player/vacuum. Slot-major ordering
 * visits the enemy first because of its unrelated earlier target slot.
 * Use inherited enemy events too, as the real combat path does. */
static int combat_calls[4], combat_count, combat_hp, combat_captures;
static bool combat_damaged, combat_vacuum, combat_spawned;
enum { COMBAT_NORMAL, COMBAT_MOVE, COMBAT_DESTROY, COMBAT_SPAWN, COMBAT_SOLID, COMBAT_ONESIDED, COMBAT_MISS };
static int combat_variant;
static RValue combat_probe(VMContext *ctx, RValue *args, int32_t count) {
    (void)args; assert(count==0);
    int object=ctx->currentInstance->objectIndex;
    assert(ctx->currentEventType==EVENT_COLLISION && combat_count<4);
    combat_calls[combat_count++]=object;
    if (object==2) {
        assert(ctx->otherInstance->objectIndex==1);
        assert(ctx->currentEventSubtype==1); /* child target overrides parent */
        if (!combat_damaged || combat_variant==COMBAT_SPAWN) {
            if (combat_vacuum) ++combat_captures;
            else combat_hp-=8;
        }
        if (combat_variant==COMBAT_MOVE) ctx->currentInstance->x=200;
        if (combat_variant==COMBAT_DESTROY) Runner_destroyInstance(ctx->runner,ctx->currentInstance,false);
        if (combat_variant==COMBAT_SPAWN && !combat_spawned) {
            combat_spawned=true;
            Runner_createInstance(ctx->runner,10,10,1);
        }
    } else {
        assert(object==1 && ctx->otherInstance->objectIndex==2);
        assert(ctx->currentEventObjectIndex==0); /* inherited handler owner */
        combat_damaged=true;
        if (combat_vacuum) Runner_destroyInstance(ctx->runner,ctx->currentInstance,false);
    }
    return RValue_makeReal(0);
}
static void combat_events(bool vacuum, bool reversed_creation, int variant) {
    DataWin dw={0};
    Room room={.name="synthetic-combat",.width=480,.height=270,.speed=60};
    Sprite sprite={.width=10,.height=10,.bboxMode=1,.sepMasks=2};
    GameObject objects[3]={0};
    EventAction action={.codeId=0};
    ObjectEvent enemy_events[]={
        {.eventSubtype=0,.actionCount=1,.actions=&action}, /* earlier unrelated target slot */
        {.eventSubtype=2,.actionCount=1,.actions=&action}
    };
    ObjectEvent player_events[]={
        {.eventSubtype=0,.actionCount=1,.actions=&action},
        {.eventSubtype=1,.actionCount=1,.actions=&action}
    };
    /* call.i fixture(); popz.v; exit.i -- entirely synthetic, no game code */
    uint32_t bytecode[]={0xD9020000,0,0x9E050000,0x9D020000};
    CodeEntry code={.name="synthetic_collision_probe",.length=sizeof(bytecode)};
    Function function={.name="synthetic_combat_probe",.occurrences=1};
    for (int i=0;i<3;i++) {
        objects[i].present=true; objects[i].name="synthetic";
        objects[i].parentId=-1; objects[i].spriteId=0; objects[i].textureMaskId=-1;
    }
    objects[1].parentId=0;
    objects[1].solid=(variant==COMBAT_SOLID);
    objects[0].eventLists[EVENT_COLLISION]=(ObjectEventList){.eventCount=2,.events=enemy_events};
    objects[2].eventLists[EVENT_COLLISION]=(ObjectEventList){.eventCount=2,.events=player_events};
    if (variant==COMBAT_ONESIDED) objects[2].eventLists[EVENT_COLLISION].eventCount=0;
    dw.gen8.wadVersion=17; dw.objt.count=3; dw.objt.objects=objects;
    dw.sprt.count=1; dw.sprt.sprites=&sprite; dw.room.count=1; dw.room.rooms=&room;
    dw.code.count=1; dw.code.entries=&code;
    dw.func.functionCount=1; dw.func.functions=&function;
    dw.bytecodeBuffer=(uint8_t*)bytecode;
    VMContext *vm=VM_create(&dw);
    Renderer *renderer=NoopRenderer_create();
    AudioSystem *audio=(AudioSystem*)NoopAudioSystem_create();
    FileSystem *fs=NoopFileSystem_create();
    Runner *runner=Runner_create(&dw,vm,renderer,fs,audio,1);
    VM_registerBuiltin(vm,"synthetic_combat_probe",combat_probe);
    vm->funcCallCache[0].builtin=(void*)combat_probe;
    runner->spatialGrid=SpatialGrid_create(room.width,room.height);
    runner->currentRoom=&room; runner->currentRoomIndex=0;
    Runner_createInstance(runner,10,10,reversed_creation ? 2 : 1);
    Runner_createInstance(runner,variant==COMBAT_MISS ? 100 : 10,10,reversed_creation ? 1 : 2);
    combat_variant=variant; combat_spawned=false;
    combat_count=combat_captures=0; combat_hp=100; combat_damaged=false; combat_vacuum=vacuum;
    Runner_step(runner);
    int pairs=(variant==COMBAT_SPAWN ? 2 : 1);
    int calls=variant==COMBAT_MISS ? 0 : variant==COMBAT_ONESIDED ? 1 : 2*pairs;
    int captures=vacuum && variant!=COMBAT_MISS && variant!=COMBAT_ONESIDED ? pairs : 0;
    assert(combat_count==calls);
    if (calls>=2) assert(combat_calls[0]==2 && combat_calls[1]==1);
    if (calls==1) assert(combat_calls[0]==1);
    if (pairs==2) assert(combat_calls[2]==2 && combat_calls[3]==1);
    assert(combat_damaged==(variant!=COMBAT_MISS));
    assert(combat_hp==(vacuum ? 100 : 92));
    assert(combat_captures==captures);
    if (vacuum) {
        Runner_step(runner);
        assert(combat_count==calls && combat_captures==captures); /* no destroyed-enemy replay */
    }
    Runner_free(runner); VM_free(vm);
    renderer->vtable->destroy(renderer); audio->vtable->destroy(audio); NoopFileSystem_destroy(fs);
}
int main(void) {
    for (int creation=0;creation<2;creation++) {
        combat_events(false,creation,COMBAT_NORMAL);
        for (int variant=COMBAT_NORMAL;variant<=COMBAT_MISS;variant++)
            combat_events(true,creation,variant);
    }
    puts("Combat dispatch: inherited contact damage and capture before destruction; both creation orders, target specificity, motion, first-handler destruction, late spawning, solids, one-sided handlers and misses passed.");
    collision_arrays();
    restart_gamepads();
    DataWin dw = {0}; Runner runner = {0}; VMContext vm = {0};
    RValue ini = string("progress.ini"), second = string("next.ini");
    RValue fields[] = {string("progress"), string("stage"), RValue_makeReal(3)};
    RValue value, textfile = string("pending.txt"), csvfile = string("fixture.csv");
    OverlayFileSystem *overlay;
    FileSystem *fs;
    assert(mkdir("bundle", 0700) == 0 && mkdir("saves", 0700) == 0);
    overlay = OverlayFileSystem_create("bundle", "saves"); fs = &overlay->base;
    runner.dataWin = &dw; runner.vmContext = &vm; runner.fileSystem = fs;
    vm.runner = &runner; vm.dataWin = &dw;
    VMBuiltins_registerAll(&vm);
    {
        const char *blocked[] = {"window_set_size", "window_set_rectangle", "window_set_position",
            "window_set_fullscreen", "window_set_region_size", "window_center"};
        RValue dimensions[] = {RValue_makeReal(640), RValue_makeReal(480), RValue_makeReal(320), RValue_makeReal(240)};
        BuiltinFunc surface_resize = VM_findBuiltin(&vm, "surface_resize");
        BuiltinFunc gui_size = VM_findBuiltin(&vm, "display_set_gui_size");
        runner.setWindowSize = resize_spy; runner.getWindowSize = fixed_size;
        /* Positive control: the unmodified desktop handler really calls the backend. */
        discard(invoke(&vm, "window_set_size", dimensions, 2)); assert(resize_calls == 1);
        Sanae_registerFixedWindow(&vm); resize_calls = 0;
        for (unsigned i = 0; i < sizeof(blocked)/sizeof(blocked[0]); ++i) {
            discard(invoke(&vm, blocked[i], dimensions, 4));
            discard(invoke(&vm, blocked[i], NULL, 0));
        }
        assert(resize_calls == 0);
        value = invoke(&vm, "window_get_width", NULL, 0); assert(RValue_toReal(value) == 1280);
        value = invoke(&vm, "window_get_height", NULL, 0); assert(RValue_toReal(value) == 720);
        value = invoke(&vm, "window_get_fullscreen", NULL, 0); assert(RValue_toBool(value));
        assert(VM_findBuiltin(&vm, "surface_resize") == surface_resize);
        assert(VM_findBuiltin(&vm, "display_set_gui_size") == gui_size);
        puts("Switch window policy: resize blocked, real size getters retained, drawing APIs unchanged.");
    }
    discard(invoke(&vm, "ini_open", &ini, 1));
    discard(invoke(&vm, "ini_write_real", fields, 3));
    discard(invoke(&vm, "ini_open", &second, 1)); /* implicit close persists first */
    has(fs, "progress.ini", "stage=\"3\"");
    discard(invoke(&vm, "ini_close", NULL, 0));
    discard(invoke(&vm, "ini_open", &ini, 1));
    value = invoke(&vm, "ini_read_real", fields, 3); assert(RValue_toReal(value) == 3);
    fields[2] = RValue_makeReal(4); discard(invoke(&vm, "ini_write_real", fields, 3));
    assert(mkdir("saves/progress.ini.tmp", 0700) == 0);
    discard(invoke(&vm, "ini_close", NULL, 0));
    assert(runner.currentIni && runner.currentIniDirty && runner.sanaeSaveFailed);
    has(fs, "progress.ini", "stage=\"3\"");
    assert(rmdir("saves/progress.ini.tmp") == 0);
    discard(invoke(&vm, "ini_close", NULL, 0));
    assert(!runner.currentIni); has(fs, "progress.ini", "stage=\"4\""); has(fs, "progress.ini.bak", "stage=\"3\"");
    assert(remove("saves/progress.ini") == 0);
    assert(fs->vtable->fileExists(fs, "progress.ini")); /* interrupted replacement recovery */
    has(fs, "progress.ini", "stage=\"3\"");
    discard(invoke(&vm, "ini_open", &second, 1));
    discard(invoke(&vm, "ini_write_real", fields, 3));
    value = invoke(&vm, "file_text_open_write", &textfile, 1);
    { RValue textargs[] = {value, string("pending data")};
      discard(invoke(&vm, "file_text_write_string", textargs, 2)); discard(textargs[1]); }
    discard(invoke(&vm, "game_end", NULL, 0)); assert(runner.shouldExit);
    assert(Runner_sanaeFlushSaves(&runner));
    has(fs, "next.ini", "stage=\"4\""); has(fs, "pending.txt", "pending data");
    discard(invoke(&vm, "file_text_close", &value, 1));
    discard(invoke(&vm, "ini_close", NULL, 0));
    assert(fs->vtable->writeFileText(fs, "fixture.csv", "001,\"a,b\"\r\nlast,"));
    value = invoke(&vm, "load_csv", &csvfile, 1);
    { int id = RValue_toInt32(value); DsGrid *grid = &runner.dsGridPool[id];
      RValue args[] = {value, RValue_makeReal(1), RValue_makeReal(0), RValue_makeReal(42)};
      assert(grid->width == 2 && grid->height == 2);
      assert(!strcmp(grid->items[0].string, "001") && !strcmp(grid->items[1].string, "a,b"));
      discard(invoke(&vm, "ds_grid_set", args, 4)); assert(RValue_toReal(grid->items[1]) == 42);
      discard(invoke(&vm, "ds_grid_destroy", &value, 1)); }
    assert(fs->vtable->writeFileText(fs, "delete.txt", "one"));
    assert(fs->vtable->writeFileText(fs, "delete.txt", "two"));
    assert(fs->vtable->deleteFile(fs, "delete.txt"));
    assert(!fs->vtable->fileExists(fs, "delete.txt"));
    Ini_free(runner.cachedIni); free(runner.cachedIniPath);
    arrfree(runner.dsGridPool); shfree(vm.builtinMap);
    discard(ini); discard(second); discard(textfile); discard(csvfile);
    discard(fields[0]); discard(fields[1]); OverlayFileSystem_destroy(overlay);
    puts("SANAE VM integration: INI reopen/implicit close/failure retry, pending saves on game_end, CSV grid/set and backup deletion passed.");
    return 0;
}
