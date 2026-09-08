/* SANAE integration, AGPL-3.0-or-later. Included after runner collision helpers.
 * A detected overlap is one bilateral event, including non-solid instances.
 * Do not retest geometry/liveness between the two notifications: movement or
 * deferred destruction in the first handler must not erase the second one.
 */
static void sanaeDispatchCollisionPair(Runner *runner, Instance *self,
                                       Instance *other, const FlattenedCollisionEvent *event) {
    FlattenedCollisionEvent forward = *event;
    FlattenedCollisionEvent *match = findSymmetricCollisionEvent(runner, other, self);
    FlattenedCollisionEvent reverse = {0};
    bool hasReverse = match != nullptr;
    if (hasReverse) reverse = *match;

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
}
