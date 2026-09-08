/* OpenAL API test doubles; no device, decoding or listening test is implied. */
#include <assert.h>
#include <math.h>
#include "audio/openal/al_audio_system.c"
static float gains[8];
static int loops[8], stopped[8], generated, uploaded, parsed, fail_al, deleted_sources, deleted_buffers;
static DataWin group_data[19];
static AudioEntry group_entries[19];
static uint8_t pcm[4] = {0, 0, 0, 0};
void logWarn(const char *format, ...) { (void)format; }
void logError(const char *format, ...) { (void)format; }
DataWin *DataWin_parse(const char *path, DataWinParserOptions options) {
    int group = -1;
    assert(sscanf(path, "audiogroup%d.dat", &group) == 1 && group > 0 && group < 19);
    assert(options.parseAudo); ++parsed;
    group_entries[group].present = true; group_entries[group].data = pcm; group_entries[group].dataSize = sizeof(pcm);
    group_data[group].audo.count = 1; group_data[group].audo.entries = &group_entries[group];
    return &group_data[group];
}
void DataWin_loadAudoIfNeeded(DataWin *dw, uint32_t entry) { assert(dw && entry < dw->audo.count); }
static char *resolve_group(FileSystem *fs, const char *path) { (void)fs; return strdup(path); }
static bool exists_group(FileSystem *fs, const char *path) { (void)fs; return strcmp(path, "audiogroup3.dat") != 0; }
void alGenSources(ALsizei n, ALuint *ids) { assert(n == 1); ++generated; *ids = 4; }
void alGenBuffers(ALsizei n, ALuint *ids) { for (int i = 0; i < n; ++i) ids[i] = 5; }
ALenum alGetError(void) { if (fail_al) { fail_al = 0; return AL_OUT_OF_MEMORY; } return AL_NO_ERROR; }
void alSourcePlay(ALuint source) { assert(source < 8); }
void alBufferData(ALuint buffer, ALenum format, const ALvoid *data, ALsizei size, ALsizei frequency) {
    (void)buffer; assert(format == AL_FORMAT_STEREO16 && data && size > 0 && frequency > 0); ++uploaded;
}
void alSourceQueueBuffers(ALuint source, ALsizei n, const ALuint *ids) { (void)source; (void)n; (void)ids; }
void alGetBufferi(ALuint buffer, ALenum param, ALint *value) { (void)buffer; (void)param; *value = 1; }
void alSourcef(ALuint source, ALenum param, ALfloat value) { assert(source < 8 && param == AL_GAIN); gains[source] = value; }
void alSourcei(ALuint source, ALenum param, ALint value) { assert(source < 8); if (param == AL_LOOPING) loops[source] = value; else assert(param == AL_BUFFER); }
void alSourceStop(ALuint source) { assert(source < 8); stopped[source]++; }
void alGetSourcei(ALuint source, ALenum param, ALint *value) { (void)source; (void)param; *value = 0; }
void alSourceUnqueueBuffers(ALuint source, ALsizei count, ALuint *buffers) { (void)source; (void)count; (void)buffers; }
void alDeleteSources(ALsizei count, const ALuint *sources) { (void)sources; deleted_sources += count; }
void alDeleteBuffers(ALsizei count, const ALuint *buffers) { (void)buffers; deleted_buffers += count; }
static void group_regression(void) {
    AlAudioSystem ma = {0}; DataWin main = {0}; Sound sound = {0};
    FileSystemVtable vtable = {0}; FileSystem fs = {0};
    main.agrp.count = 19; main.sond.count = 1; main.sond.sounds = &sound;
    ma.base.dw = &main; ma.fileSystem = &fs;
    vtable.resolvePath = resolve_group; vtable.fileExists = exists_group; fs.vtable = &vtable;
    sanae_al_init_groups(&ma.base, &main);
    assert(maGroupIsLoaded(&ma.base, 0));
    assert(!maGroupIsLoaded(&ma.base, 11));
    maGroupLoad(&ma.base, 11); maGroupLoad(&ma.base, 2); maGroupLoad(&ma.base, 17);
    assert(parsed == 3);
    assert(ma.base.audioGroups[11] == &group_data[11]);
    assert(ma.base.audioGroups[2] == &group_data[2] && ma.base.audioGroups[17] == &group_data[17]);
    assert(!maGroupIsLoaded(&ma.base, 1));
    maGroupLoad(&ma.base, 11); assert(parsed == 3);
    maGroupLoad(&ma.base, 3); assert(!maGroupIsLoaded(&ma.base, 3));
    maGroupLoad(&ma.base, -1); maGroupLoad(&ma.base, 19); maGroupLoad(&ma.base, 2000000000);
    assert(!maGroupIsLoaded(&ma.base, -1) && !maGroupIsLoaded(&ma.base, 19));
    sound.audioGroup = 11; sound.audioFile = 0; sound.volume = sound.pitch = 1;
    sound.name = "synthetic sound";
    assert(maPlaySound(&ma.base, 0, 1, true) >= SOUND_INSTANCE_ID_BASE);
    assert(generated == 1 && uploaded == 1);
    assert(ma.instances[0].loop);
    assert(maGetSoundLength(&ma.base, 0) >= 0);
    sound.audioGroup = 3; assert(maPlaySound(&ma.base, 0, 1, false) == -1);
    sound.audioGroup = -1; assert(maPlaySound(&ma.base, 0, 1, false) == -1);
    sound.audioGroup = 2000000000; assert(maPlaySound(&ma.base, 0, 1, false) == -1);
    sound.audioGroup = 11; sound.audioFile = 9; assert(maPlaySound(&ma.base, 0, 1, false) == -1);
    assert(generated == 1); /* invalid input did not leak AL handles */
    releaseInstance(&ma.instances[0]);
    sound.audioGroup = -1; assert(maGetSoundLength(&ma.base, 0) == 0);
    sound.audioGroup = 11; sound.audioFile = 0;
    group_entries[11].present = false;
    assert(maPlaySound(&ma.base, 0, 1, false) == -1 && generated == 1);
    group_entries[11].present = true; group_entries[11].data = NULL;
    assert(maPlaySound(&ma.base, 0, 1, false) == -1 && generated == 1);
    group_entries[11].data = pcm;
    deleted_sources = deleted_buffers = 0; fail_al = 1;
    assert(maPlaySound(&ma.base, 0, 1, false) == -1 && generated == 2);
    assert(deleted_sources == 1 && deleted_buffers == 1);
    assert(!ma.instances[0].active && !ma.instances[0].alSource && !ma.instances[0].alBuffer);
    arrfree(ma.base.audioGroups);
    memset(stopped, 0, sizeof(stopped)); memset(loops, 0, sizeof(loops));
    puts("Reported crash regression: out-of-order groups 11/2/17, repeat/missing/negative/huge IDs, playback and length passed.");
}
int main(void) {
    group_regression();
    AlAudioSystem ma = {0}; DataWin dw = {0}; Sound sounds[3] = {{0}}; SanaeGain envelopes[2];
    int i;
    sounds[0].audioGroup = 0; sounds[1].audioGroup = sounds[2].audioGroup = 1;
    dw.sond.count = 3; dw.sond.sounds = sounds; ma.base.dw = &dw;
    ma.sanaeGroups = envelopes; ma.sanaeGroupCount = 2;
    sanae_gain_init(&envelopes[0]); sanae_gain_init(&envelopes[1]);
    for (i = 0; i < 3; ++i) {
        ma.instances[i].active = true; ma.instances[i].alSource = (ALuint)i;
        ma.instances[i].soundIndex = i; ma.instances[i].currentGain = .8f;
        ma.instances[i].instanceId = SOUND_INSTANCE_ID_BASE + i;
    }
    sanae_al_group_gain(&ma.base, 1, .5f, 0);
    assert(fabsf(gains[0] - .8f) < 1e-6f && fabsf(gains[1] - .4f) < 1e-6f && fabsf(gains[2] - .4f) < 1e-6f);
    assert(sanae_al_group_get_gain(&ma.base, 1) == .5f);
    /* The play path uses this multiplier for voices created AFTER group_set_gain. */
    assert(sanae_al_gain(&ma, 1, 1) == .5f);
    sanae_al_group_gain(&ma.base, 1, 0, 1000); sanae_gain_update(&envelopes[1], .5f);
    assert(sanae_al_gain(&ma, 1, 1) == .25f);
    sanae_al_set_loop(&ma.base, SOUND_INSTANCE_ID_BASE + 1, true);
    assert(sanae_al_get_loop(&ma.base, SOUND_INSTANCE_ID_BASE + 1) && loops[1]);
    assert(!ma.instances[0].loop && !ma.instances[2].loop);
    ma.instances[2].streaming = true;
    sanae_al_set_loop(&ma.base, 2, true); assert(ma.instances[2].loop && !loops[2]);
    sanae_al_set_loop(&ma.base, SOUND_INSTANCE_ID_BASE + 1, false); assert(!loops[1]);
    sanae_al_group_stop(&ma.base, 1);
    assert(ma.instances[0].active && !ma.instances[1].active && !ma.instances[2].active);
    assert(!stopped[0] && stopped[1] == 1 && stopped[2] == 1);
    assert(!sanae_al_get_loop(&ma.base, SOUND_INSTANCE_ID_BASE + 2));
    sanae_al_group_gain(&ma.base, -1, 0, 0); assert(envelopes[0].current == 1);
    puts("OpenAL doubles: group gain isolation/persistence/fade, voice and stream looping, group stop passed.");
    return 0;
}
