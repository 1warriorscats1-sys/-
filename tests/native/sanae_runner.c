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
    RValue buffer_args[]={RValue_makeReal(1024),RValue_makeReal(0),RValue_makeReal(1)};
    RValue deleted=invoke(vm,"buffer_create",buffer_args,3);
    discard(invoke(vm,"buffer_create",buffer_args,3));
    discard(invoke(vm,"buffer_delete",&deleted,1));
    Runner_reset(runner);
    assert(runner->gmlBufferPool==NULL);
    discard(invoke(vm,"buffer_create",buffer_args,3)); /* Runner_free must release this too. */
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
 * Use inherited enemy events too, as the real combat path does.
 * The player holds two matching subscriptions (parent target 0 and child
 * target 1): both fire exactly once per pair, with the enemy's most specific
 * handler joining the first notification. */
static int combat_calls[6], combat_count, combat_hp, combat_captures, combat_subs;
static bool combat_damaged, combat_vacuum, combat_spawned;
enum { COMBAT_NORMAL, COMBAT_MOVE, COMBAT_DESTROY, COMBAT_SPAWN, COMBAT_SOLID, COMBAT_ONESIDED, COMBAT_MISS };
static int combat_variant;
static RValue combat_probe(VMContext *ctx, RValue *args, int32_t count) {
    (void)args; assert(count==0);
    int object=ctx->currentInstance->objectIndex;
    assert(ctx->currentEventType==EVENT_COLLISION && combat_count<6);
    combat_calls[combat_count++]=object;
    if (object==2) {
        assert(ctx->otherInstance->objectIndex==1);
        assert(ctx->currentEventSubtype==0 || ctx->currentEventSubtype==1);
        combat_subs|=1<<ctx->currentEventSubtype;
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
    /* call.i fixture(); pop.v local; exit.i -- grows zero-sized BC17 locals. */
    uint32_t bytecode[]={0xD9020000,0,0x4555FFF9,0xA0000000,0x9D020000};
    Variable local={.name="probe_result",.instanceType=-7,.varID=0,.occurrences=1,.firstAddress=8};
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
    dw.vari.variableCount=1; dw.vari.variables=&local;
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
    combat_count=combat_captures=combat_subs=0; combat_hp=100; combat_damaged=false; combat_vacuum=vacuum;
    Runner_step(runner);
    int pairs=(variant==COMBAT_SPAWN ? 2 : 1);
    /* First subscription notifies both sides; later same-side subscriptions
     * fire on their own visits while the partner still overlaps. Vacuum eats
     * the partner during the reverse notification, so only the first player
     * subscription fires per pair there; MOVE/DESTROY drop it via retest. */
    int calls=variant==COMBAT_MISS ? 0 : variant==COMBAT_ONESIDED ? 1 :
        !vacuum ? 3 : variant==COMBAT_SPAWN ? 4 : 2;
    int captures=vacuum && variant!=COMBAT_MISS && variant!=COMBAT_ONESIDED ? pairs : 0;
    assert(combat_count==calls);
    if (calls>=2) assert(combat_calls[0]==2 && combat_calls[1]==1);
    if (calls==1) assert(combat_calls[0]==1);
    if (calls==3) assert(combat_calls[2]==2);
    if (pairs==2) assert(combat_calls[2]==2 && combat_calls[3]==1);
    if ((!vacuum && variant==COMBAT_NORMAL) || variant==COMBAT_SPAWN)
        assert((combat_subs&3)==3); /* both subscriptions fired exactly once */
    else if (variant==COMBAT_ONESIDED || variant==COMBAT_MISS) assert(combat_subs==0);
    else assert(combat_subs==1);
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
/* Array targets across every collision/instance wrapper: scalar behaviour is
 * preserved, the first hitting element wins, *_list unions dedupe shared
 * descendants, and ordered lists sort by caller distance. */
static RValue target_array2(RValue a, RValue b) {
    GMLArray *targets=GMLArray_create(17,2);
    *GMLArray_slot(targets,0)=a; *GMLArray_slot(targets,1)=b;
    return RValue_makeArray(targets);
}
static bool target_member(int32_t id, Instance *a, Instance *b, Instance *c) {
    return id==(int32_t)a->instanceId || id==(int32_t)b->instanceId || id==(int32_t)c->instanceId;
}
static void target_arrays(void) {
    DataWin dw={0};
    Room room={.name="synthetic-targets",.width=480,.height=270,.speed=60};
    Sprite sprite={.width=10,.height=10,.bboxMode=1,.sepMasks=2};
    GameObject objects[3]={0};
    Instance *e1, *e2, *e3, *other;
    Renderer *renderer=NoopRenderer_create();
    AudioSystem *audio=(AudioSystem*)NoopAudioSystem_create();
    FileSystem *fs=NoopFileSystem_create();
    VMContext *vm;
    Runner *runner;
    RValue args[9], list, found;
    int32_t listId;
    for (int i=0;i<3;i++) {
        objects[i].present=true; objects[i].name="synthetic";
        objects[i].parentId=-1; objects[i].spriteId=0; objects[i].textureMaskId=-1;
    }
    objects[1].parentId=0;
    dw.gen8.wadVersion=17; dw.gen8.lastObj=100000;
    dw.objt.count=3; dw.objt.objects=objects;
    dw.sprt.count=1; dw.sprt.sprites=&sprite; dw.room.count=1; dw.room.rooms=&room;
    vm=VM_create(&dw);
    runner=Runner_create(&dw,vm,renderer,fs,audio,1);
    runner->spatialGrid=SpatialGrid_create(room.width,room.height);
    runner->currentRoom=&room; runner->currentRoomIndex=0;
    e1=Runner_createInstance(runner,10,10,1);
    e2=Runner_createInstance(runner,30,10,1);
    e3=Runner_createInstance(runner,40,10,1);
    other=Runner_createInstance(runner,50,10,2);
    assert(e1->instanceId==100001 && e2->instanceId==100002 && e3->instanceId==100003 && other->instanceId==100004);
    vm->currentInstance=e1; vm->otherInstance=e2;

    args[0]=RValue_makeReal(25); args[1]=RValue_makeReal(10);
    args[2]=RValue_makeReal(1);
    assert(RValue_toBool(invoke(vm,"place_meeting",args,3)));
    discard(args[2]); args[2]=target_array2(RValue_makeReal(2),RValue_makeReal(1));
    assert(RValue_toBool(invoke(vm,"place_meeting",args,3)));
    discard(args[2]); args[2]=RValue_makeReal(0); /* parent target covers the child */
    assert(RValue_toBool(invoke(vm,"place_meeting",args,3)));
    discard(args[2]); args[2]=target_array2(RValue_makeReal(2),RValue_makeReal(99));
    assert(!RValue_toBool(invoke(vm,"place_meeting",args,3)));
    discard(args[2]); args[2]=RValue_makeArray(GMLArray_create(17,0));
    assert(!RValue_toBool(invoke(vm,"place_meeting",args,3)));
    discard(args[2]);

    args[0]=RValue_makeReal(35); args[1]=RValue_makeReal(15);
    args[2]=RValue_makeReal(1);
    assert(RValue_toBool(invoke(vm,"position_meeting",args,3)));
    assert(RValue_toInt32(invoke(vm,"instance_position",args,3))==100002);
    discard(args[2]); args[2]=target_array2(RValue_makeReal(2),RValue_makeReal(0));
    assert(RValue_toBool(invoke(vm,"position_meeting",args,3)));
    assert(RValue_toInt32(invoke(vm,"instance_position",args,3))==100002);
    discard(args[2]); args[2]=RValue_makeReal(0); args[0]=RValue_makeReal(0); args[1]=RValue_makeReal(0);
    assert(!RValue_toBool(invoke(vm,"position_meeting",args,3)));
    assert(RValue_toInt32(invoke(vm,"instance_position",args,3))==INSTANCE_NOONE);
    discard(args[2]);

    args[0]=RValue_makeReal(35); args[1]=RValue_makeReal(15);
    args[2]=target_array2(RValue_makeReal(2),RValue_makeReal(0));
    args[3]=RValue_makeReal(0); args[4]=RValue_makeReal(1);
    assert(RValue_toInt32(invoke(vm,"collision_point",args,5))==100002);
    discard(args[2]);

    args[0]=RValue_makeReal(0); args[1]=RValue_makeReal(0);
    args[2]=RValue_makeReal(45); args[3]=RValue_makeReal(20);
    args[4]=target_array2(RValue_makeReal(2),RValue_makeReal(0));
    args[5]=RValue_makeReal(0); args[6]=RValue_makeReal(1);
    assert(target_member(RValue_toInt32(invoke(vm,"collision_rectangle",args,7)),e1,e2,e3));
    discard(args[4]);

    args[0]=RValue_makeReal(32); args[1]=RValue_makeReal(15); args[2]=RValue_makeReal(3);
    args[3]=target_array2(RValue_makeReal(2),RValue_makeReal(0));
    args[4]=RValue_makeReal(0); args[5]=RValue_makeReal(1);
    assert(RValue_toInt32(invoke(vm,"collision_circle",args,6))==100002);
    discard(args[3]);

    args[0]=RValue_makeReal(25); args[1]=RValue_makeReal(5);
    args[2]=RValue_makeReal(39); args[3]=RValue_makeReal(25);
    args[4]=target_array2(RValue_makeReal(2),RValue_makeReal(0));
    args[5]=RValue_makeReal(0); args[6]=RValue_makeReal(1);
    assert(RValue_toInt32(invoke(vm,"collision_ellipse",args,7))==100002);
    discard(args[4]);

    args[0]=RValue_makeReal(0); args[1]=RValue_makeReal(15);
    args[2]=RValue_makeReal(60); args[3]=RValue_makeReal(15);
    args[4]=RValue_makeReal(2);
    args[5]=RValue_makeReal(0); args[6]=RValue_makeReal(1);
    assert(RValue_toInt32(invoke(vm,"collision_line",args,7))==100004);
    discard(args[4]); args[4]=target_array2(RValue_makeReal(2),RValue_makeReal(1));
    found=invoke(vm,"collision_line",args,7);
    assert(target_member(RValue_toInt32(found),e1,e2,e3) || RValue_toInt32(found)==100004);
    discard(args[4]);

    list=invoke(vm,"ds_list_create",NULL,0); listId=RValue_toInt32(list); discard(list);
    args[0]=RValue_makeReal(35); args[1]=RValue_makeReal(10);
    args[2]=target_array2(RValue_makeReal(1),RValue_makeReal(0));
    args[3]=RValue_makeReal(listId); args[4]=RValue_makeReal(1);
    args[5]=RValue_makeReal(0);
    assert(RValue_toReal(invoke(vm,"instance_place_list",args,6))==2);
    discard(args[2]);
    { RValue get[]={RValue_makeReal(listId),RValue_makeReal(0)};
      assert(RValue_toInt32(invoke(vm,"ds_list_find_value",get,2))==100002); /* nearer first */
      get[1]=RValue_makeReal(1);
      assert(RValue_toInt32(invoke(vm,"ds_list_find_value",get,2))==100003); }
    discard(invoke(vm,"ds_list_destroy",args+3,1));

    list=invoke(vm,"ds_list_create",NULL,0); listId=RValue_toInt32(list); discard(list);
    args[0]=RValue_makeReal(0); args[1]=RValue_makeReal(0);
    args[2]=RValue_makeReal(60); args[3]=RValue_makeReal(20);
    args[4]=target_array2(RValue_makeReal(2),RValue_makeReal(1));
    args[5]=RValue_makeReal(0); args[6]=RValue_makeReal(1);
    args[7]=RValue_makeReal(listId); args[8]=RValue_makeReal(1);
    assert(RValue_toReal(invoke(vm,"collision_rectangle_list",args,9))==3); /* notme drops the caller */
    { RValue get[]={RValue_makeReal(listId),RValue_makeReal(0)};
      assert(RValue_toInt32(invoke(vm,"ds_list_find_value",get,2))==100002);
      get[1]=RValue_makeReal(1);
      assert(RValue_toInt32(invoke(vm,"ds_list_find_value",get,2))==100003);
      get[1]=RValue_makeReal(2);
      assert(RValue_toInt32(invoke(vm,"ds_list_find_value",get,2))==100004); }
    discard(args[4]);
    discard(invoke(vm,"ds_list_destroy",args+7,1));

    args[0]=RValue_makeReal(2);
    assert(RValue_toReal(invoke(vm,"distance_to_object",args,1))==30);
    discard(args[0]); args[0]=target_array2(RValue_makeReal(2),RValue_makeReal(0));
    assert(RValue_toReal(invoke(vm,"distance_to_object",args,1))==10);
    discard(args[0]);

    args[0]=RValue_makeReal(45); args[1]=RValue_makeReal(10); args[2]=RValue_makeReal(1);
    assert(RValue_toInt32(invoke(vm,"instance_nearest",args,3))==100003);
    assert(RValue_toInt32(invoke(vm,"instance_furthest",args,3))==100001);
    discard(args[2]); args[2]=target_array2(RValue_makeReal(2),RValue_makeReal(0));
    assert(RValue_toInt32(invoke(vm,"instance_nearest",args,3))==100004); /* first element wins ties */
    assert(RValue_toInt32(invoke(vm,"instance_furthest",args,3))==100001);
    discard(args[2]); args[2]=target_array2(RValue_makeReal(0),RValue_makeReal(2));
    assert(RValue_toInt32(invoke(vm,"instance_nearest",args,3))==100003);
    discard(args[2]); args[2]=RValue_makeReal(INSTANCE_SELF);
    assert(RValue_toInt32(invoke(vm,"instance_nearest",args,3))==100001);
    discard(args[2]); args[2]=RValue_makeReal(INSTANCE_OTHER);
    assert(RValue_toInt32(invoke(vm,"instance_furthest",args,3))==100002);
    discard(args[2]); args[2]=RValue_makeReal(INSTANCE_ALL);
    assert(RValue_toInt32(invoke(vm,"instance_nearest",args,3))==100003);
    assert(RValue_toInt32(invoke(vm,"instance_furthest",args,3))==100001);
    discard(args[2]);

    args[0]=RValue_makeReal(1);
    assert(RValue_toBool(invoke(vm,"instance_exists",args,1)));
    discard(args[0]); args[0]=RValue_makeReal(99);
    assert(!RValue_toBool(invoke(vm,"instance_exists",args,1)));
    discard(args[0]); args[0]=target_array2(RValue_makeReal(99),RValue_makeReal(1));
    assert(RValue_toBool(invoke(vm,"instance_exists",args,1)));
    discard(args[0]); args[0]=RValue_makeArray(GMLArray_create(17,0));
    assert(!RValue_toBool(invoke(vm,"instance_exists",args,1)));
    discard(args[0]); args[0]=RValue_makeReal(INSTANCE_NOONE);
    assert(!RValue_toBool(invoke(vm,"instance_exists",args,1)));
    discard(args[0]); args[0]=RValue_makeReal(INSTANCE_SELF);
    assert(RValue_toBool(invoke(vm,"instance_exists",args,1)));
    discard(args[0]); args[0]=RValue_makeReal(INSTANCE_ALL);
    assert(RValue_toBool(invoke(vm,"instance_exists",args,1)));
    discard(args[0]); args[0]=RValue_makeReal(100002);
    assert(RValue_toBool(invoke(vm,"instance_exists",args,1)));
    discard(invoke(vm,"instance_destroy",args,1)); /* id without event */
    assert(!e2->active);
    assert(!RValue_toBool(invoke(vm,"instance_exists",args,1)));
    discard(args[0]); args[0]=target_array2(RValue_makeReal(100003),RValue_makeReal(99));
    { RValue destroy[]={args[0],RValue_makeReal(1)};
      discard(invoke(vm,"instance_destroy",destroy,2)); } /* array tolerates unknown ids */
    assert(!e3->active);
    discard(args[0]); args[0]=RValue_makeReal(1);
    assert(RValue_toBool(invoke(vm,"instance_exists",args,1))); /* caller survives */
    discard(args[0]); args[0]=RValue_makeReal(2);
    discard(invoke(vm,"instance_destroy",args,1));
    assert(!other->active);
    assert(!RValue_toBool(invoke(vm,"instance_exists",args,1)));
    discard(invoke(vm,"instance_destroy",NULL,0)); /* no argument destroys the caller */
    assert(!e1->active);
    discard(args[0]); args[0]=RValue_makeReal(INSTANCE_ALL);
    assert(!RValue_toBool(invoke(vm,"instance_exists",args,1)));
    discard(args[0]);

    puts("Target arrays: every collision/instance wrapper merges scalar, parent, id, self/other/all and empty targets; list union dedupes and ordered results sort by distance passed.");
    Runner_free(runner); VM_free(vm);
    renderer->vtable->destroy(renderer); audio->vtable->destroy(audio); NoopFileSystem_destroy(fs);
}

static float hb_rect[8][4];
static uint32_t hb_color[8];
static bool hb_outline[8];
static int hb_count;
static void hb_spy(Renderer *renderer, float x1, float y1, float x2, float y2, uint32_t color, float alpha, bool outline) {
    (void)renderer; (void)alpha;
    if (hb_count<8) {
        hb_rect[hb_count][0]=x1; hb_rect[hb_count][1]=y1;
        hb_rect[hb_count][2]=x2; hb_rect[hb_count][3]=y2;
        hb_color[hb_count]=color; hb_outline[hb_count]=outline; ++hb_count;
    }
}
static void healthbar_directions(void) {
    DataWin dw={0}; VMContext vm={0};
    Renderer *renderer=NoopRenderer_create();
    static RendererVtable hb_vtable;
    RValue args[11];
    dw.gen8.wadVersion=17;
    vm.runner=NULL; vm.dataWin=&dw;
    { Runner runner={0}; runner.dataWin=&dw; runner.vmContext=&vm; runner.renderer=renderer; vm.runner=&runner;
      VMBuiltins_registerAll(&vm);
      hb_vtable=*renderer->vtable; hb_vtable.drawRectangle=hb_spy; renderer->vtable=&hb_vtable;
      renderer->drawAlpha=1.0f;
      args[0]=RValue_makeReal(0); args[1]=RValue_makeReal(0);
      args[2]=RValue_makeReal(100); args[3]=RValue_makeReal(20);
      args[4]=RValue_makeReal(50);
      args[5]=RValue_makeReal(0x111111); args[6]=RValue_makeReal(0x222222); args[7]=RValue_makeReal(0x222222);
      for (int direction=0;direction<4;direction++) {
          hb_count=0;
          args[8]=RValue_makeReal(direction);
          args[9]=RValue_makeReal(1); args[10]=RValue_makeReal(1);
          discard(invoke(&vm,"draw_healthbar",args,11));
          assert(hb_count==3);
          assert(hb_rect[0][0]==0 && hb_rect[0][1]==0 && hb_rect[0][2]==100 && hb_rect[0][3]==20);
          assert(hb_color[0]==0x111111 && !hb_outline[0]);
          assert(hb_color[1]==0x222222 && !hb_outline[1]);
          assert(hb_color[2]==0x000000 && hb_outline[2]);
          if (direction==0) assert(hb_rect[1][0]==0 && hb_rect[1][2]==50 && hb_rect[1][1]==0 && hb_rect[1][3]==20);
          if (direction==1) assert(hb_rect[1][0]==50 && hb_rect[1][2]==100 && hb_rect[1][1]==0 && hb_rect[1][3]==20);
          if (direction==2) assert(hb_rect[1][1]==0 && hb_rect[1][3]==10 && hb_rect[1][0]==0 && hb_rect[1][2]==100);
          if (direction==3) assert(hb_rect[1][1]==10 && hb_rect[1][3]==20 && hb_rect[1][0]==0 && hb_rect[1][2]==100);
      }
      hb_count=0; args[8]=RValue_makeReal(0);
      args[9]=RValue_makeReal(0); args[10]=RValue_makeReal(0);
      discard(invoke(&vm,"draw_healthbar",args,11));
      assert(hb_count==1 && hb_color[0]==0x222222); /* fill only */
      hb_count=0; args[4]=RValue_makeReal(150);
      discard(invoke(&vm,"draw_healthbar",args,8)); /* omitted flags behave as undefined */
      assert(hb_count==1 && hb_rect[0][2]==100);
      hb_count=0; args[4]=RValue_makeReal(-20);
      discard(invoke(&vm,"draw_healthbar",args,8));
      assert(hb_count==1 && hb_rect[0][2]==0);
      hb_count=0;
      discard(invoke(&vm,"draw_healthbar",args,7));
      assert(hb_count==0);
      shfree(vm.builtinMap); }
    renderer->vtable->destroy(renderer);
    puts("draw_healthbar: all four fill directions, clamped amount, optional background and black border passed.");
}

static void overlay_builtins(void) {
    DataWin dw={0};
    Renderer *renderer=NoopRenderer_create();
    AudioSystem *audio=(AudioSystem*)NoopAudioSystem_create();
    FileSystem *fs=NoopFileSystem_create();
    VMContext *vm;
    Runner *runner;
    RValue args[3], value, out;
    dw.gen8.wadVersion=17;
    dw.gen8.defaultWindowWidth=480; dw.gen8.defaultWindowHeight=270;
    vm=VM_create(&dw);
    runner=Runner_create(&dw,vm,renderer,fs,audio,1);
    runner->getWindowSize=NULL;
    vm->hasFixedSeed=false;

    value=invoke(vm,"display_get_width",NULL,0); assert(RValue_toReal(value)==480);
    value=invoke(vm,"display_get_height",NULL,0); assert(RValue_toReal(value)==270);
    runner->getWindowSize=fixed_size;
    value=invoke(vm,"display_get_width",NULL,0); assert(RValue_toReal(value)==1280);
    value=invoke(vm,"display_get_height",NULL,0); assert(RValue_toReal(value)==720);
    runner->getWindowSize=NULL;

    args[0]=string("{\"a\":1,\"b\":[true,null],\"c\":\"x\",\"d\":1.5}");
    value=invoke(vm,"json_parse",args,1); discard(args[0]);
    out=invoke(vm,"json_stringify",&value,1);
    assert(strstr(out.string,"\"a\":1") && strstr(out.string,"\"b\":[true,null]") &&
           strstr(out.string,"\"c\":\"x\"") && strstr(out.string,"\"d\":1.5"));
    discard(out); discard(value);
    args[0]=string("[1,2]");
    value=invoke(vm,"json_parse",args,1); discard(args[0]);
    out=invoke(vm,"json_stringify",&value,1);
    assert(!strcmp(out.string,"[1,2]"));
    discard(out); discard(value);
    args[0]=string("{oops");
    value=invoke(vm,"json_parse",args,1); discard(args[0]);
    assert(value.type==RVALUE_UNDEFINED); discard(value);
    { GMLArray *inner=GMLArray_create(17,1), *outer;
      for (int i=0;i<70;i++) {
          outer=GMLArray_create(17,1);
          *GMLArray_slot(outer,0)=RValue_makeArray(inner);
          inner=outer;
      }
      args[0]=RValue_makeArray(inner);
      out=invoke(vm,"json_stringify",args,1); /* depth cap terminates hostile input */
      assert(out.type==RVALUE_STRING && out.string!=NULL);
      discard(out); discard(args[0]); }

    args[0]=RValue_makeReal(1.5);
    value=invoke(vm,"to_string",args,1); out=invoke(vm,"string",args,1);
    assert(value.type==RVALUE_STRING && !strcmp(value.string,out.string));
    discard(value); discard(out);

    assert(RValue_toBool(invoke(vm,"texturegroup_status",NULL,0)));
    discard(invoke(vm,"texturegroup_load",NULL,0));
    discard(invoke(vm,"texturegroup_unload",NULL,0));
    discard(invoke(vm,"texture_prefetch",NULL,0));
    discard(invoke(vm,"texture_flush",NULL,0));
    discard(invoke(vm,"sprite_prefetch",NULL,0));
    discard(invoke(vm,"sprite_flush",NULL,0));
    discard(invoke(vm,"draw_texture_flush",NULL,0));

    args[0]=string("harmless"); args[1]=RValue_makeReal(0);
    discard(invoke(vm,"show_error",args,2)); discard(args[0]);
    assert(!runner->shouldExit);
    args[0]=string("fatal"); args[1]=RValue_makeReal(1);
    discard(invoke(vm,"show_error",args,2)); discard(args[0]);
    assert(runner->shouldExit);
    runner->shouldExit=false;

    args[0]=RValue_makeReal(777);
    discard(invoke(vm,"random_set_seed",args,1)); /* single argument must not read past it */
    assert(RValue_toReal(invoke(vm,"random_get_seed",NULL,0))==777);
    discard(invoke(vm,"randomize",NULL,0));
    assert(RValue_toReal(invoke(vm,"random_get_seed",NULL,0))==(GMLReal)runner->random.lastSeed);

    args[0]=RValue_makeReal(0);
    discard(invoke(vm,"draw_enable_drawevent",args,1));
    assert(!runner->sanaeDrawEventsEnabled);
    args[0]=RValue_makeReal(1);
    discard(invoke(vm,"draw_enable_drawevent",args,1));
    assert(runner->sanaeDrawEventsEnabled);

    Runner_free(runner); VM_free(vm);
    renderer->vtable->destroy(renderer); audio->vtable->destroy(audio); NoopFileSystem_destroy(fs);
    puts("Overlay builtins: display fallback, JSON roundtrip/depth cap, to_string, texture shims, show_error, random seed and drawevent flag passed.");
}

static void string_width_fixture(void) {
    DataWin dw={0}; Runner runner={0}; VMContext vm={0};
    Renderer *renderer=NoopRenderer_create();
    Font font={0};
    FontGlyph glyphs[128]={0};
    RValue args[3];
    dw.gen8.wadVersion=17;
    for (int i=0;i<128;i++) { glyphs[i].character=(uint16_t)i; glyphs[i].shift=10; font.glyphLUT[i]=&glyphs[i]; }
    font.scaleX=1.0f; font.glyphs=glyphs; font.glyphCount=128;
    dw.font.count=1; dw.font.fonts=&font;
    renderer->dataWin=&dw; renderer->drawFont=0;
    runner.dataWin=&dw; runner.vmContext=&vm; runner.renderer=renderer;
    vm.runner=&runner; vm.dataWin=&dw;
    VMBuiltins_registerAll(&vm);
    args[0]=string("AB");
    assert(RValue_toReal(invoke(&vm,"string_width",args,1))==20);
    args[1]=RValue_makeReal(-1); args[2]=RValue_makeReal(1000);
    assert(RValue_toReal(invoke(&vm,"string_width_ext",args,3))==20);
    discard(args[0]); args[0]=string("A B"); args[2]=RValue_makeReal(10);
    assert(RValue_toReal(invoke(&vm,"string_width_ext",args,3))==10);
    renderer->drawFont=-1;
    assert(RValue_toReal(invoke(&vm,"string_width_ext",args,3))==0);
    renderer->drawFont=0;
    assert(RValue_toReal(invoke(&vm,"string_width_ext",args,1))==0);
    discard(args[0]);
    shfree(vm.builtinMap);
    renderer->vtable->destroy(renderer);
    puts("string_width_ext: matches string_width unwrapped, wraps at word boundaries, guards missing fonts passed.");
}

static int drawgate_normal, drawgate_gui;
static RValue drawgate_probe(VMContext *ctx, RValue *args, int32_t count) {
    (void)args; (void)ctx; assert(count==0);
    if (ctx->currentEventSubtype==DRAW_NORMAL) ++drawgate_normal;
    else if (ctx->currentEventSubtype==DRAW_GUI) ++drawgate_gui;
    else assert(!"unexpected draw subtype");
    return RValue_makeReal(0);
}
static void drawgate_events(void) {
    DataWin dw={0};
    Room room={.name="synthetic-drawgate",.width=480,.height=270,.speed=60};
    Sprite sprite={.width=10,.height=10,.bboxMode=1,.sepMasks=2};
    GameObject objects[1]={0};
    EventAction action={.codeId=0};
    ObjectEvent draw_events[]={
        {.eventSubtype=DRAW_NORMAL,.actionCount=1,.actions=&action},
        {.eventSubtype=DRAW_GUI,.actionCount=1,.actions=&action}
    };
    uint32_t bytecode[]={0xD9020000,0,0x4555FFF9,0xA0000000,0x9D020000};
    Variable local={.name="probe_result",.instanceType=-7,.varID=0,.occurrences=1,.firstAddress=8};
    CodeEntry code={.name="synthetic_draw_probe",.length=sizeof(bytecode)};
    Function function={.name="synthetic_drawgate_probe",.occurrences=1};
    Renderer *renderer;
    AudioSystem *audio;
    FileSystem *fs;
    VMContext *vm;
    Runner *runner;
    RValue toggle;
    objects[0].present=true; objects[0].name="synthetic";
    objects[0].parentId=-1; objects[0].spriteId=0; objects[0].textureMaskId=-1;
    objects[0].visible=true;
    objects[0].eventLists[EVENT_DRAW]=(ObjectEventList){.eventCount=2,.events=draw_events};
    dw.gen8.wadVersion=17; dw.objt.count=1; dw.objt.objects=objects;
    dw.sprt.count=1; dw.sprt.sprites=&sprite; dw.room.count=1; dw.room.rooms=&room;
    dw.code.count=1; dw.code.entries=&code;
    dw.func.functionCount=1; dw.func.functions=&function;
    dw.bytecodeBuffer=(uint8_t*)bytecode;
    dw.vari.variableCount=1; dw.vari.variables=&local;
    vm=VM_create(&dw);
    renderer=NoopRenderer_create();
    audio=(AudioSystem*)NoopAudioSystem_create();
    fs=NoopFileSystem_create();
    runner=Runner_create(&dw,vm,renderer,fs,audio,1);
    VM_registerBuiltin(vm,"synthetic_drawgate_probe",drawgate_probe);
    vm->funcCallCache[0].builtin=(void*)drawgate_probe;
    runner->spatialGrid=SpatialGrid_create(room.width,room.height);
    runner->currentRoom=&room; runner->currentRoomIndex=0;
    Runner_createInstance(runner,10,10,0);
    assert(runner->sanaeDrawEventsEnabled);
    drawgate_normal=drawgate_gui=0;
    Runner_step(runner); Runner_drawViews(runner,480,270,false); Runner_drawGUI(runner,480,270,480,270);
    assert(drawgate_normal==1 && drawgate_gui==1);
    toggle=RValue_makeReal(0);
    discard(invoke(vm,"draw_enable_drawevent",&toggle,1));
    Runner_step(runner); Runner_drawViews(runner,480,270,false); Runner_drawGUI(runner,480,270,480,270);
    assert(drawgate_normal==1 && drawgate_gui==2); /* GUI drawing is unaffected */
    toggle=RValue_makeReal(1);
    discard(invoke(vm,"draw_enable_drawevent",&toggle,1));
    Runner_step(runner); Runner_drawViews(runner,480,270,false); Runner_drawGUI(runner,480,270,480,270);
    assert(drawgate_normal==2 && drawgate_gui==3);
    Runner_free(runner); VM_free(vm);
    renderer->vtable->destroy(renderer); audio->vtable->destroy(audio); NoopFileSystem_destroy(fs);
    puts("draw_enable_drawevent: normal Draw events gate on/off while GUI drawing continues passed.");
}

int main(void) {
    struct { int32_t key; int value; } *signed_keys=NULL;
    int32_t negative=-1, minimum=INT32_MIN;
    hmput(signed_keys,negative,42); hmput(signed_keys,minimum,7);
    assert(hmget(signed_keys,negative)==42 && hmget(signed_keys,minimum)==7);
    hmfree(signed_keys);
    for (int creation=0;creation<2;creation++) {
        combat_events(false,creation,COMBAT_NORMAL);
        for (int variant=COMBAT_NORMAL;variant<=COMBAT_MISS;variant++)
            combat_events(true,creation,variant);
    }
    puts("Combat dispatch: inherited contact damage and capture before destruction; both creation orders, multi-subscription pairs, motion, first-handler destruction, late spawning, solids, one-sided handlers and misses passed.");
    collision_arrays();
    target_arrays();
    healthbar_directions();
    overlay_builtins();
    string_width_fixture();
    drawgate_events();
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
