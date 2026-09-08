/* Compile the actual patched runner comparator; no game data or GL required.
 * Unused runner sections are discarded when linking this focused executable. */
#include <assert.h>
#include <limits.h>
#include "runner.c"

static void check_pair(DrawKey back, DrawKey front) {
    assert(compareDrawKeys(&back, &front) < 0);
    assert(compareDrawKeys(&front, &back) > 0);
    assert(compareDrawKeys(&back, &back) == 0);
}
int main(void) {
    /* Synthetic opaque GUI frame followed by its portrait/fill. */
    Instance frame = {.instanceId=100001, .depth=0};
    Instance portrait = {.instanceId=100002, .depth=0};
    Drawable items[] = {
        {.type=DRAWABLE_INSTANCE, .depth=0, .instance=&portrait},
        {.type=DRAWABLE_INSTANCE, .depth=0, .instance=&frame},
    };
    assert(!isDrawableArraySorted(items, 2));
    qsort(items, 2, sizeof(Drawable), compareDrawables);
    assert(isDrawableArraySorted(items, 2));
    assert(items[0].instance == &frame && items[1].instance == &portrait);
    check_pair((DrawKey){0,DRAWABLE_INSTANCE,100001}, (DrawKey){0,DRAWABLE_INSTANCE,100002});
    /* Real depth takes precedence over ID; not a reversal of the whole scene. */
    check_pair((DrawKey){100,DRAWABLE_INSTANCE,100099}, (DrawKey){-100,DRAWABLE_INSTANCE,100001});
    check_pair((DrawKey){INT_MAX,DRAWABLE_INSTANCE,1}, (DrawKey){INT_MIN,DRAWABLE_INSTANCE,1});
    check_pair((DrawKey){0,DRAWABLE_TILE,1}, (DrawKey){0,DRAWABLE_TILE,2});
    check_pair((DrawKey){0,DRAWABLE_LAYER,2}, (DrawKey){0,DRAWABLE_LAYER,1});
    check_pair((DrawKey){0,DRAWABLE_PARTICLE_SYSTEM,2}, (DrawKey){0,DRAWABLE_PARTICLE_SYSTEM,1});
    DrawKey mixed[] = {{0,DRAWABLE_TILE,1},{0,DRAWABLE_INSTANCE,1}};
    assert(compareDrawKeys(&mixed[0], &mixed[1]) ==
           (DRAWABLE_TILE < DRAWABLE_INSTANCE ? -1 : 1));
    puts("Actual draw comparator: GUI back-to-front, depth priority, cache order and unchanged tile/layer/particle ties passed.");
    return 0;
}
