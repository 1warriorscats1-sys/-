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
void platformLog(const logType type, const char *format, va_list args) {
    (void)type; vfprintf(stderr, format, args);
}

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
int main(void) {
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
