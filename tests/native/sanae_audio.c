/* OpenAL API test doubles; no device, decoding or listening test is implied. */
#include <assert.h>
#include <math.h>
#include "audio/openal/al_audio_system.c"
static float gains[8];
static int loops[8], stopped[8];
void alSourcef(ALuint source, ALenum param, ALfloat value) { assert(source < 8 && param == AL_GAIN); gains[source] = value; }
void alSourcei(ALuint source, ALenum param, ALint value) { assert(source < 8 && param == AL_LOOPING); loops[source] = value; }
void alSourceStop(ALuint source) { assert(source < 8); stopped[source]++; }
void alGetSourcei(ALuint source, ALenum param, ALint *value) { (void)source; (void)param; *value = 0; }
void alSourceUnqueueBuffers(ALuint source, ALsizei count, ALuint *buffers) { (void)source; (void)count; (void)buffers; }
void alDeleteSources(ALsizei count, const ALuint *sources) { (void)count; (void)sources; }
void alDeleteBuffers(ALsizei count, const ALuint *buffers) { (void)count; (void)buffers; }
int main(void) {
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
