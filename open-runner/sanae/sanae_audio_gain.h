/* Original SANAE group gain envelope; MIT. Independent of OpenAL. */
#ifndef SANAE_AUDIO_GAIN_H
#define SANAE_AUDIO_GAIN_H
typedef struct { float current, start, target, duration, remaining; } SanaeGain;
static void sanae_gain_init(SanaeGain *gain) {
    gain->current = gain->start = gain->target = 1.0f;
    gain->duration = gain->remaining = 0.0f;
}
static void sanae_gain_set(SanaeGain *gain, float target, unsigned time_ms) {
    if (target < 0.0f) target = 0.0f;
    gain->start = gain->current; gain->target = target;
    gain->duration = gain->remaining = (float)time_ms / 1000.0f;
    if (!time_ms) gain->current = target;
}
static void sanae_gain_update(SanaeGain *gain, float elapsed) {
    if (gain->remaining <= 0 || elapsed <= 0) return;
    gain->remaining -= elapsed;
    if (gain->remaining <= 0) { gain->remaining = 0; gain->current = gain->target; }
    else gain->current = gain->start + (gain->target - gain->start) *
        (1.0f - gain->remaining / gain->duration);
}
#endif
