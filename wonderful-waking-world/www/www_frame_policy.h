/* Original WWW presentation guard, MIT. */
#ifndef WWW_FRAME_POLICY_H
#define WWW_FRAME_POLICY_H
static inline int www_should_present(int exiting, int pending_room) {
    return !exiting && pending_room == -1;
}
#endif
