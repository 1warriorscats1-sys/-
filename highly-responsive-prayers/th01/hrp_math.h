/* hrp_math.h - deterministic integer trigonometry and helpers.
 *
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2026 contributors to the 1warriorscats1-sys/- repository.
 *
 * The simulation must produce byte-identical results on the host and on the
 * Switch, so it never calls libm: every value below is produced with integer
 * arithmetic only.
 */
#ifndef HRP_MATH_H
#define HRP_MATH_H

#include <stdint.h>

typedef int32_t hrp_fx;

#define HRP_FX_SHIFT 16
#define HRP_FX_ONE   (1 << HRP_FX_SHIFT)

/* 1 radian in Q16.16 is 205887/65536; PI/180 in Q16.16 is 1144 (approx). */
#define HRP_DEG_TO_FX(deg) (((int64_t)(deg) * 205887 + 90) / 180)

static inline int hrp_norm180(int deg)
{
    deg %= 360;
    if (deg > 180) deg -= 360;
    if (deg < -180) deg += 360;
    return deg;
}

/* sin of an angle in degrees, Q16.16 result. |error| < 2e-4 of full scale. */
static inline hrp_fx hrp_sin_fx(int deg)
{
    int d = hrp_norm180(deg);
    int sign = 1;
    if (d < 0) { d = -d; sign = -1; }
    if (d > 90) d = 180 - d; /* sin(d) == sin(180-d) */

    int64_t x = HRP_DEG_TO_FX(d);      /* radians, Q16.16 */
    int64_t x2 = (x * x) >> 16;
    int64_t term = x;
    int64_t sum = x;
    term = -(int64_t)(((term * x2) >> 16) / 6);
    sum += term;
    term = -(int64_t)(((term * x2) >> 16) / 20);
    sum += term;
    term = -(int64_t)(((term * x2) >> 16) / 42);
    sum += term;
    return (hrp_fx)(sign * sum);
}

static inline hrp_fx hrp_cos_fx(int deg)
{
    return hrp_sin_fx(deg + 90);
}

/* integer square root (Newton, 32-bit input) */
static inline int32_t hrp_isqrt(int64_t v)
{
    if (v <= 0) return 0;
    int64_t x = v;
    for (int i = 0; i < 32; i++) {
        int64_t nx = (x + v / x) / 2;
        if (nx == x) break;
        x = nx;
    }
    return (int32_t)x;
}

#define HRP_FX(x) ((hrp_fx)((double)(x) * (double)HRP_FX_ONE))

static inline hrp_fx hrp_fx_mul(hrp_fx a, hrp_fx b)
{
    return (hrp_fx)(((int64_t)a * (int64_t)b) >> HRP_FX_SHIFT);
}

static inline hrp_fx hrp_fx_div(hrp_fx a, hrp_fx b)
{
    /* multiply rather than shift: shifting a negative value is undefined */
    return (hrp_fx)(((int64_t)a * (int64_t)HRP_FX_ONE) / (int64_t)b);
}

static inline int hrp_fx_floor(hrp_fx a) { return (int)(a >> HRP_FX_SHIFT); }

static inline int hrp_fx_round(hrp_fx a) { return (int)((a + HRP_FX_ONE / 2) >> HRP_FX_SHIFT); }

static inline int hrp_abs_i(int v) { return v < 0 ? -v : v; }

#endif /* HRP_MATH_H */
