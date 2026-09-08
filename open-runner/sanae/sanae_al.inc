/* SANAE-specific additions to the AGPL OpenAL backend. */
static float sanae_al_gain(AlAudioSystem *ma, int32_t sound, float gain) {
    if (sound >= 0 && (uint32_t)sound < ma->base.dw->sond.count) {
        int32_t group = ma->base.dw->sond.sounds[sound].audioGroup;
        if (group >= 0 && (uint32_t)group < ma->sanaeGroupCount)
            return gain * ma->sanaeGroups[group].current;
    }
    return gain;
}
static void sanae_al_group_gain(AudioSystem *audio, int32_t group, float gain, uint32_t time) {
    AlAudioSystem *ma = (AlAudioSystem *)audio;
    int i;
    if (group < 0 || (uint32_t)group >= ma->sanaeGroupCount) return;
    sanae_gain_set(&ma->sanaeGroups[group], gain, time);
    for (i = 0; i < MAX_SOUND_INSTANCES; ++i) {
        SoundInstance *instance = &ma->instances[i];
        if (instance->active)
            alSourcef(instance->alSource, AL_GAIN, sanae_al_gain(ma, instance->soundIndex, instance->currentGain));
    }
}
static float sanae_al_group_get_gain(AudioSystem *audio, int32_t group) {
    AlAudioSystem *ma = (AlAudioSystem *)audio;
    return group >= 0 && (uint32_t)group < ma->sanaeGroupCount ? ma->sanaeGroups[group].current : 0;
}
static void sanae_al_group_stop(AudioSystem *audio, int32_t group) {
    AlAudioSystem *ma = (AlAudioSystem *)audio;
    int i;
    for (i = 0; i < MAX_SOUND_INSTANCES; ++i) {
        SoundInstance *instance = &ma->instances[i];
        int32_t sound = instance->soundIndex;
        if (instance->active && sound >= 0 && (uint32_t)sound < audio->dw->sond.count &&
            audio->dw->sond.sounds[sound].audioGroup == group) releaseInstance(instance);
    }
}
static void sanae_al_set_loop(AudioSystem *audio, int32_t sound, bool loop) {
    AlAudioSystem *ma = (AlAudioSystem *)audio;
    int i;
    for (i = 0; i < MAX_SOUND_INSTANCES; ++i) {
        SoundInstance *instance = &ma->instances[i];
        if (!instance->active) continue;
        if (isValidSoundInstanceId(sound) ? instance->instanceId != sound : instance->soundIndex != sound) continue;
        instance->loop = loop;
        if (!instance->streaming) alSourcei(instance->alSource, AL_LOOPING, loop ? AL_TRUE : AL_FALSE);
    }
}
static bool sanae_al_get_loop(AudioSystem *audio, int32_t sound) {
    AlAudioSystem *ma = (AlAudioSystem *)audio;
    int i;
    for (i = 0; i < MAX_SOUND_INSTANCES; ++i) {
        SoundInstance *instance = &ma->instances[i];
        if (!instance->active) continue;
        if (isValidSoundInstanceId(sound) ? instance->instanceId != sound : instance->soundIndex != sound) continue;
        return instance->loop;
    }
    return false;
}
