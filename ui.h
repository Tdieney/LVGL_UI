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

// Update lanes: ui_tick() schedules SLOW at 5Hz (text values) and CHART at 8Hz
// (each of the 4 charts gets every 4th tick => 2Hz, staggered) so the xSPI bus
// is never flooded. FAST is retained as a compatibility enum for screen tick
// functions but is currently not dispatched.
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

// Request a tab switch (the tab-bar buttons call this). The transition is
// COALESCED and THROTTLED, then runs in two ui_tick() phases: (1) flush only the
// old/new nav cells + page title, (2) teardown/build the content on the next
// tick. A burst of rapid taps therefore collapses into ONE rebuild of the final
// tab instead of one rebuild per tap — the old
// clean+create-inside-the-event-callback path could, under fast repeated
// clicking, pile up allocations/frees and fragment the constrained LVGL heap until a
// build failed and the UI hung (recoverable only by resetting the MCU).
void ui_request_tab(ui_tab_t tab);

// Request a deferred rebuild of the currently active tab. This is used by a
// screen when a rare structural state change (currently Control LEVEL <->
// POSITION) needs a different widget tree. Like ui_request_tab(), the rebuild
// is serviced from ui_tick(), never from the input event callback.
void ui_request_current_rebuild(void);

// Runs both active tick lanes once immediately. Call right after
// create_screen_XXX() on a tab switch so the new screen renders correctly on
// its first frame instead of waiting up to 200ms for the next SLOW-lane tick.
void ui_force_refresh(void);

#ifdef __cplusplus
}
#endif

#endif /* UI_H */
