#ifndef CTRL_POS_MATH_H
#define CTRL_POS_MATH_H

/* Pure geometry for the Control POSITION rotary knob — no LVGL dependency, so
 * it can be unit-tested on the host (see tests/test_pos_angle.c).
 *
 * WHY THIS EXISTS: the knob used to rely on lv_arc's built-in touch handling.
 * For a gapless full-circle arc (bg_angles 0..360) lv_arc applies an anti-wrap
 * heuristic (lv_arc.c: `if (LV_ABS(delta_angle) > 280) angle = 0 or deg_range`)
 * that SNAPS the value to min/max whenever the finger crosses the top seam —
 * the erratic jump. We instead compute the angle straight from the
 * pointer here: continuous everywhere, and crossing the top just wraps
 * 360°->0° cleanly (which is correct for an angle), never an interior jump.
 *
 * Convention: (dx,dy) is the pointer offset from the ring centre in SCREEN
 * pixels (+x right, +y down). Output is the raw setpoint 0..max where 0 sits at
 * the TOP (12 o'clock) and the value increases CLOCKWISE — matching the ring's
 * lv_arc_set_rotation(270) so value 0 renders at the top.
 */

#include <stdint.h>

// Integer atan2 approximation (within about one degree), adapted from the
// same Roman Black algorithm used by LVGL v8's lv_atan2(). Keeping a copy here
// preserves this header's LVGL-free host test while avoiding atan2f/soft-float
// in production. Inputs from the 236 px control are far inside its safe range.
static inline uint16_t ctrl_pos_atan2_deg(int x, int y)
{
    uint8_t flags = 0;
    if (x < 0) { flags |= 0x01u; x = -x; }
    if (y < 0) { flags |= 0x02u; y = -y; }
    uint32_t ux = (uint32_t) x;
    uint32_t uy = (uint32_t) y;
    uint32_t degree;
    if (ux > uy) { degree = (uy * 45u + ux / 2u) / ux; flags |= 0x10u; }
    else         { degree = (ux * 45u + uy / 2u) / uy; }

    uint8_t d = (uint8_t) degree;
    uint8_t correction = 0;
    if (d > 22u)
    {
        if (d <= 44u) correction++;
        if (d <= 41u) correction++;
        if (d <= 37u) correction++;
        if (d <= 32u) correction++;
    }
    else
    {
        if (d >= 2u) correction++;
        if (d >= 6u) correction++;
        if (d >= 10u) correction++;
        if (d >= 15u) correction++;
    }
    degree += correction;
    if (flags & 0x10u) degree = 90u - degree;
    if (flags & 0x02u) degree = (flags & 0x01u) ? 180u + degree : 180u - degree;
    else if (flags & 0x01u) degree = 360u - degree;
    return (uint16_t) degree;
}

static inline int32_t ctrl_pos_angle_to_raw(int dx, int dy, int32_t max)
{
    if (dx == 0 && dy == 0) return 0; /* angle undefined at the exact centre */

    /* ctrl_pos_atan2_deg(dy,dx): 0 = right, clockwise on screen. */
    uint16_t deg = (uint16_t) ((ctrl_pos_atan2_deg(dy, dx) + 90u) % 360u);
    int32_t v = (int32_t) (((uint32_t) deg * (uint32_t) max + 180u) / 360u);
    if (v < 0) v = 0;
    if (v > max) v = max;
    return v;
}

#endif /* CTRL_POS_MATH_H */
