/* hrp_frame_policy.h - decide when a frame may be presented.
 *
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2026 contributors to the 1warriorscats1-sys/- repository.
 *
 * Presenting a half-finished frame while the application is tearing down is a
 * known source of flicker or a stuck image on the Switch. The rule is shared by
 * the host and Switch backends and covered by tests.
 */
#ifndef HRP_FRAME_POLICY_H
#define HRP_FRAME_POLICY_H

#include <stdint.h>

typedef struct {
    int exiting;       /* set once shutdown has been requested */
    int frame_ready;   /* set once the frame buffer holds a complete frame */
    int presented;     /* number of frames actually presented */
} hrp_frame_policy;

static void hrp_frame_policy_init(hrp_frame_policy *p)
{
    p->exiting = 0;
    p->frame_ready = 0;
    p->presented = 0;
}

/* A frame is presented only when it is complete and we are not shutting down. */
static int hrp_frame_policy_should_present(const hrp_frame_policy *p)
{
    return (!p->exiting) && p->frame_ready;
}

/* Called after a successful present (or a deliberate skip). */
static void hrp_frame_policy_commit(hrp_frame_policy *p, int presented)
{
    if (presented) p->presented++;
    p->frame_ready = 0;
}

#endif /* HRP_FRAME_POLICY_H */
