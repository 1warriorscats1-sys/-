/* th01_frame_policy.c - native tests for the present/exit frame policy.
 *
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2026 contributors to the 1warriorscats1-sys/- repository.
 */
#include <stdio.h>

#include "hrp_frame_policy.h"

static int failures = 0;

#define CHECK(cond, message)                                                   \
    do {                                                                       \
        if (!(cond)) {                                                         \
            printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, (message));        \
            failures++;                                                        \
        }                                                                      \
    } while (0)

int main(void)
{
    hrp_frame_policy policy;
    hrp_frame_policy_init(&policy);

    CHECK(hrp_frame_policy_should_present(&policy) == 0,
          "nothing may be presented before a frame is ready");

    policy.frame_ready = 1;
    CHECK(hrp_frame_policy_should_present(&policy) == 1, "a complete frame may be presented");
    hrp_frame_policy_commit(&policy, 1);
    CHECK(policy.presented == 1, "a presented frame is counted");
    CHECK(policy.frame_ready == 0, "the frame is consumed after presenting");

    /* a skipped frame is not counted as presented */
    policy.frame_ready = 1;
    hrp_frame_policy_commit(&policy, 0);
    CHECK(policy.presented == 1, "a skipped frame is not counted");

    /* once shutdown starts, no further frame may be presented */
    policy.frame_ready = 1;
    policy.exiting = 1;
    CHECK(hrp_frame_policy_should_present(&policy) == 0,
          "no frame may be presented while exiting");
    hrp_frame_policy_commit(&policy, 0);
    CHECK(policy.presented == 1, "the exiting frame is not presented");

    if (failures) {
        printf("th01_frame_policy: %d failure(s)\n", failures);
        return 1;
    }
    printf("th01_frame_policy: all checks passed\n");
    return 0;
}
