/* SANAE integration, AGPL-3.0-or-later. Included after runner collision helpers.
 * A detected overlap is one bilateral event, including non-solid instances.
 * Do not retest geometry/liveness between the two notifications: movement or
 * deferred destruction in the first handler must not erase the second one.
 *
 * Pair bookkeeping (sanaePairs, keyed by instance pair) additionally preserves
 * GameMaker's per-subscription behavior: when one side holds several collision
 * subscriptions that all match the same partner (e.g. a handler for the
 * partner's object AND one for its parent), every matching handler fires
 * exactly once. The reverse side contributes its most specific handler to the
 * first notification; any remaining subscription fires when its own bucket
 * visit finds the pair already recorded. The recorded value packs the
 * first-notified self instance id (high 32 bits) and the reverse handler's
 * target object (low 32 bits, SANAE_COLLISION_NO_REVERSE when one-sided).
 */
#define SANAE_COLLISION_NO_REVERSE 0xFFFFFFFFu
static uint32_t sanaeDispatchCollisionPair(Runner *runner, Instance *self,
                                           Instance *other, const FlattenedCollisionEvent *event) {
    FlattenedCollisionEvent forward = *event;
    FlattenedCollisionEvent *match = findSymmetricCollisionEvent(runner, other, self);
    FlattenedCollisionEvent reverse = {0};
    bool hasReverse = match != nullptr;
    uint32_t reverseTarget = SANAE_COLLISION_NO_REVERSE;
    if (hasReverse) {
        reverse = *match;
        reverseTarget = reverse.targetObjectIndex;
    }

    /* Resource order also applies to instances created by a collision handler
     * after their partner's object bucket has already been visited. */
    bool otherFirst = hasReverse && other->objectIndex > self->objectIndex;
    if (otherFirst) {
        executeCollisionEvent(runner, other, self, (int32_t)reverse.targetObjectIndex,
                              reverse.codeId, reverse.ownerObjectIndex);
    }
    if (!runner->shouldExit) {
        executeCollisionEvent(runner, self, other, (int32_t)forward.targetObjectIndex,
                              forward.codeId, forward.ownerObjectIndex);
    }
    if (hasReverse && !otherFirst && !runner->shouldExit) {
        executeCollisionEvent(runner, other, self, (int32_t)reverse.targetObjectIndex,
                              reverse.codeId, reverse.ownerObjectIndex);
    }
    return reverseTarget;
}
