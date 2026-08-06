/* Host unit test for the Control POSITION knob angle mapping (ctrl_pos_math.h).
 *
 * Build + run (from repo root):
 *   gcc -I. tests/test_pos_angle.c -o pos_test -lm && ./pos_test
 *
 * Purpose: the old knob relied on lv_arc's built-in drag, which SNAPS the value
 * to min/max when the finger crosses the top seam of a gapless 360° arc
 * (lv_arc.c: `if (LV_ABS(delta_angle) > 280) angle = 0 or deg_range`) — that
 * was the erratic jump. This test proves the replacement mapping is
 * continuous the whole way around (max interior step ~= one pointer step,
 * never a ~MAX snap), and that the top is a clean value-wrap, not a jump.
 */
#include <math.h>
#include <stdio.h>
#include <stdlib.h>

#include "ctrl_pos_math.h"

#define MAX 3600 /* UI_POSITION_MAX_RAW: 0.1 deg/LSB, full turn */

static int failures = 0;

static void expect(const char *what, int got, int want, int tol)
{
    if (abs(got - want) > tol)
    {
        printf("  FAIL %-14s got=%d want=%d (tol %d)\n", what, got, want, tol);
        failures++;
    }
}

/* Pointer offset for a clockwise-from-top angle theta (deg), at radius r px. */
static void ptr_at(float theta_deg, int r, int *dx, int *dy)
{
    float a = (theta_deg - 90.0f) * 3.14159265f / 180.0f;
    *dx     = (int) lroundf(r * cosf(a));
    *dy     = (int) lroundf(r * sinf(a));
}

int main(void)
{
    /* 1) centre is undefined -> 0 (guard). */
    expect("centre", ctrl_pos_angle_to_raw(0, 0, MAX), 0, 0);

    /* 2) cardinal points: 0 at top, increasing clockwise. */
    expect("top",   ctrl_pos_angle_to_raw(0, -100, MAX), 0, 0);
    expect("right", ctrl_pos_angle_to_raw(100, 0, MAX), 900, 1);
    expect("down",  ctrl_pos_angle_to_raw(0, 100, MAX), 1800, 1);
    expect("left",  ctrl_pos_angle_to_raw(-100, 0, MAX), 2700, 1);

    /* 3) round-trip accuracy + NO interior jump across a full 0..359 sweep. */
    int r = 110, prev = 0, maxstep = 0;
    for (int t = 0; t <= 359; t++)
    {
        int dx, dy;
        ptr_at((float) t, r, &dx, &dy);
        int v    = ctrl_pos_angle_to_raw(dx, dy, MAX);
        int want = (int) lroundf(t * (MAX / 360.0f));
        if (abs(v - want) > 20)
        {
            printf("  FAIL sweep t=%d got=%d want=%d\n", t, v, want);
            failures++;
        }
        if (t > 0)
        {
            int step = abs(v - prev);
            if (step > maxstep) maxstep = step;
        }
        prev = v;
    }
    if (maxstep > 25)
    {
        printf("  FAIL interior jump: maxstep=%d raw (a seam-snap bug would be ~%d)\n", maxstep, MAX);
        failures++;
    }
    printf("  max interior step over 0..359 = %d raw units (expect ~10)\n", maxstep);

    /* 4) the top is a clean value-wrap (dot moves ~2deg), not an arbitrary snap. */
    int dxa, dya, dxb, dyb;
    ptr_at(359.0f, r, &dxa, &dya); /* just CCW of the top */
    ptr_at(1.0f, r, &dxb, &dyb);   /* just CW of the top  */
    int va = ctrl_pos_angle_to_raw(dxa, dya, MAX);
    int vb = ctrl_pos_angle_to_raw(dxb, dyb, MAX);
    printf("  near-seam: 359deg->%d, 1deg->%d (value wraps at top; dot moves ~2deg)\n", va, vb);
    expect("seam-ccw", va, 3590, 20);
    expect("seam-cw", vb, 10, 20);

    printf(failures ? "%d FAILURE(S)\n" : "ALL PASS\n", failures);
    return failures ? 1 : 0;
}
