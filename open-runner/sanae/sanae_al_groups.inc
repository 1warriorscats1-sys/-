/* Indexed audio group storage: loading order must not change group IDs. */
static DataWin *sanae_al_group(AudioSystem *audio, int32_t group) {
    if (group < 0 || group >= arrlen(audio->audioGroups)) return nullptr;
    return audio->audioGroups[group];
}
static void sanae_al_init_groups(AudioSystem *audio, DataWin *dw) {
    uint32_t count = dw->agrp.count ? dw->agrp.count : 1;
    arrsetlen(audio->audioGroups, count);
    memset(audio->audioGroups, 0, count * sizeof(*audio->audioGroups));
    audio->audioGroups[0] = dw;
}
static void maGroupLoad(AudioSystem *audio, int32_t groupIndex) {
    AlAudioSystem *ma = (AlAudioSystem *)audio;
    FileSystem *fs = ma->fileSystem;
    char name[64];
    char *path;
    DataWin *group;
    DataWinParserOptions options = {0};
    if (groupIndex <= 0 || !audio->dw || (uint32_t)groupIndex >= audio->dw->agrp.count) return;
    if (sanae_al_group(audio, groupIndex)) return; /* idempotent; no duplicate loads */
    if (!fs) return;
    snprintf(name, sizeof(name), "audiogroup%d.dat", groupIndex);
    path = fs->vtable->resolvePath(fs, name);
    if (!path || !fs->vtable->fileExists(fs, path)) {
        logWarn("SANAE: audio group %d is missing (%s)\n", groupIndex, name);
        free(path); return;
    }
    options.parseAudo = true;
    options.lazyLoadAudio = audio->dw->lazyLoadAudio;
    if (audio->dw->mappedFile) options.loadType = DATAWINLOADTYPE_MAP_FILE;
    group = DataWin_parse(path, options);
    free(path);
    if (!group) { logWarn("SANAE: cannot parse audio group %d\n", groupIndex); return; }
    /* maInit allocated every declared slot, including unloaded holes. */
    audio->audioGroups[groupIndex] = group;
}
static bool maGroupIsLoaded(AudioSystem *audio, int32_t groupIndex) {
    return sanae_al_group(audio, groupIndex) != nullptr;
}
