/* Original SANAE presentation guard, MIT. */
#ifndef SANAE_FRAME_POLICY_H
#define SANAE_FRAME_POLICY_H
static inline int sanae_should_present(int exiting, int pending_room) {
    return !exiting && pending_room == -1;
}
#endif
