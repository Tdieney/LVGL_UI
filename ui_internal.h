#ifndef UI_INTERNAL_H
#define UI_INTERNAL_H

#include "lvgl.h"
#include "ui_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Touch handling on the parameter pages:
 * - the first 10 px of travel lock the axis;
 * - a horizontal travel of 48 px or more changes one page on release;
 * - on Light, a vertical drag dims (2 px per percent, relative to the start);
 * - buttons act only on a tap (no axis locked). */
#define UI_AXIS_LOCK_PX     10
#define UI_PAGE_SWIPE_PX    48
#define UI_DIM_PX_PER_STEP  2
#define UI_DRAG_EMIT_MS     100u
#define UI_TIMER_PERIOD_MS  20u

typedef enum {
    UI_AXIS_NONE = 0,
    UI_AXIS_HORIZONTAL,
    UI_AXIS_VERTICAL
} ui_axis_t;

typedef enum {
    UI_PANEL_NONE = 0,
    UI_PANEL_SCENES,
    UI_PANEL_CONNECTION
} ui_panel_t;

typedef struct {
    bool active;
    bool canceled;
    bool dragging;
    bool tap;          /* last release was a tap: buttons may act */
    bool emitted;      /* a LIGHT event has been sent during this drag */
    bool pending;      /* a newer draft waits for the 100 ms window */
    ui_axis_t axis;
    lv_point_t start;
    int start_level;
    int draft;
    uint32_t last_emit;
} ui_gesture_t;

#ifdef __cplusplus
}
#endif

#endif /* UI_INTERNAL_H */
