#ifndef UI_H
#define UI_H

#ifdef __cplusplus
extern "C"
{
#endif

#include "lvgl.h"

// Tab State for Single-Screen Architecture
typedef enum {
    TAB_DASHBOARD,
    TAB_MONITOR,
    TAB_CONTROL,
    TAB_GRAPHS,
    TAB_DIAGNOSTICS,
    TAB_SETTINGS
} ui_tab_t;

// Update lanes: ui_tick() fans out to the active screen at three rates so the
// xSPI bus is never flooded — FAST ~20Hz (gauge), SLOW 5Hz (text values),
// CHART 8Hz (each of the 4 charts gets every 4th tick => 2Hz, staggered).
typedef enum {
    UI_LANE_FAST,
    UI_LANE_SLOW,
    UI_LANE_CHART
} ui_lane_t;

extern ui_tab_t ui_current_tab;
extern lv_obj_t *ui_MainScreen;

void ui_init(void);
void ui_tick(void);
void loadScreen(lv_obj_t *target_screen);

// Request a tab switch (the tab-bar buttons call this). The heavy teardown +
// rebuild is COALESCED and THROTTLED: it runs at most once per ui_tick(), and
// no more often than ~90ms, inside ui_tick() rather than inside the click
// callback. A burst of rapid taps therefore collapses into ONE rebuild of the
// final tab instead of one rebuild per tap — the old
// clean+create-inside-the-event-callback path could, under fast repeated
// clicking, pile up allocations/frees and fragment the constrained LVGL heap until a
// build failed and the UI hung (recoverable only by resetting the MCU).
void ui_request_tab(ui_tab_t tab);

// Runs every tick lane once immediately. Call right after create_screen_XXX()
// on a tab switch so the new screen renders correct on its first frame
// instead of waiting up to 200ms for the next scheduled SLOW-lane tick.
void ui_force_refresh(void);

#ifdef __cplusplus
}
#endif

#endif /* UI_H */
