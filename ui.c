#include "ui.h"
#include "screens.h"
#if defined(UI_DEMO_SIM) && UI_DEMO_SIM
#include "demo_sim.h"
#endif

lv_obj_t *ui_MainScreen;

void ui_init(void)
{
    lv_disp_t *disp = lv_disp_get_default();
    lv_theme_t *theme =
            lv_theme_default_init(disp, COLOR_ACCENT, lv_palette_main(LV_PALETTE_BLUE), false /*light*/,
                                  LV_FONT_DEFAULT);
    lv_disp_set_theme(disp, theme);

    // SAFE POWER-ON STATE — the motor must never spin just because the UI
    // booted. The Tx command is fully idle: cmd=0 (STOP) and ctrlValRaw=0 (zero
    // setpoint) are the zero-init defaults; we only pin opMode=SPEED (a closed-
    // loop mode, harmless at setpoint 0) so Control shows SPEED instead of
    // OPEN_LOOP for the ~100ms before the first telemetry echo. The operator has
    // to dial in a setpoint AND press START before anything moves.
    // Telemetry (voltage/temps/motorConnected) is left 0 / disconnected on
    // purpose — it fills from the first real MotorStatusFast_t/Slow_t frame (or
    // demo_sim); we don't seed fake "connected"/48 V readings here. (demo_sim
    // injects a demo setpoint when it auto-starts, so the standalone sim still
    // looks alive without this boot state being unsafe.)
    motorCmd.bits.opMode        = OP_MODE_SPEED;
    motorCmd.bits.ctrlValRaw    = 0; // zero setpoint on boot — operator must set one
    motorCmd.bits.limitRaw      = 0; // zero limit on boot — operator must set one too
    motorStatusFast.bits.opMode = OP_MODE_SPEED;

    // Create top bar and tab bar on the top layer so they are shared
    create_common_ui(lv_layer_top());

    ui_MainScreen = lv_obj_create(NULL);
    lv_obj_clear_flag(ui_MainScreen, LV_OBJ_FLAG_SCROLLABLE);

    // Initialize individual screen contents
    create_screen_dashboard(ui_MainScreen);
    ui_force_refresh();

    // Load initial screen
    lv_scr_load(ui_MainScreen);

    create_splash();

#if defined(UI_DEMO_SIM) && UI_DEMO_SIM
    demo_sim_init();
#endif
}

void loadScreen(lv_obj_t *target_screen)
{
    // Deprecated: Single screen architecture uses lv_obj_clean instead
    (void) target_screen;
}

// ---- Coalesced tab switching (see ui_request_tab in ui.h) -------------------
#define UI_TAB_MIN_MS 90 // never rebuild more often than this, however fast the taps

static ui_tab_t s_pending_tab   = TAB_DASHBOARD;
static uint8_t  s_have_pending  = 0;
static uint32_t s_last_build_ms = 0;
static uint8_t  s_lowmem_active = 0;

void ui_request_tab(ui_tab_t tab)
{
    // Cheap: just record the target. Rapid taps overwrite it; only the last
    // one survives to be built by ui_service_pending() on the next tick.
    s_pending_tab  = tab;
    s_have_pending = 1;
}

// Minimum FREE heap required immediately after cleaning a screen. The heaviest
// replacement adds about 21 KB over the persistent top/tab chrome; 24 KB keeps
// a construction margin inside the validated 52 KB pool (including splash).
#define UI_HEAP_FLOOR 24000u

// Memory-pressure fallback: a plain full-screen message. Kept tiny so it builds
// even when a real screen wouldn't. Every create is NULL-guarded — under true
// exhaustion this simply renders nothing rather than crashing.
static void ui_build_lowmem(void)
{
    lv_obj_t *scr = ui_MainScreen;
    if (!scr) return;
    lv_obj_t *lbl = lv_label_create(scr);
    if (!lbl) return; // even the label failed — bail, no deref
    lv_label_set_text_static(lbl, "LOW MEMORY\nplease reset the device");
    lv_obj_set_style_text_align(lbl, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
    lv_obj_center(lbl);
}

// The single place that tears down + rebuilds the active screen. Centralized
// here (was duplicated across every action_tab_*) so the create/refresh/
// highlight order is defined once and can't drift per tab.
static void ui_build_tab(ui_tab_t tab)
{
    ui_current_tab = tab;
    lv_obj_clean(ui_MainScreen);

    // (b) Circuit breaker: after freeing the old screen, refuse to build a full
    // one if the heap is critically low — a half-built screen full of NULL
    // widgets is worse than a clean "reset me" message on a demo unit.
    lv_mem_monitor_t mon;
    lv_mem_monitor(&mon);
    if (mon.free_size < UI_HEAP_FLOOR)
    {
        s_lowmem_active = 1;
        LV_LOG_WARN("ui_build_tab: low heap (%u free) — showing fallback", (unsigned) mon.free_size);
        ui_build_lowmem();
        ui_tabbar_set_active(tab);
        return;
    }

    s_lowmem_active = 0;

    switch (tab)
    {
        case TAB_DASHBOARD:   create_screen_dashboard(ui_MainScreen);   break;
        case TAB_MONITOR:     create_screen_monitor(ui_MainScreen);     break;
        case TAB_CONTROL:     create_screen_control(ui_MainScreen);     break;
        case TAB_GRAPHS:      create_screen_graphs(ui_MainScreen);      break;
        case TAB_DIAGNOSTICS: create_screen_diagnostics(ui_MainScreen); break;
        case TAB_SETTINGS:    create_screen_settings(ui_MainScreen);    break;
    }
    ui_force_refresh();
    ui_tabbar_set_active(tab);
}

// Called at the top of ui_tick(): build the pending tab if one is waiting, the
// throttle window has elapsed, and it isn't already the active tab. Runs
// OUTSIDE lv_timer_handler()'s event dispatch, so the clean never races the
// click event that requested it.
static void ui_service_pending(void)
{
    if (!s_have_pending) return;
    if (s_pending_tab == ui_current_tab) { s_have_pending = 0; return; }
    uint32_t now = lv_tick_get();
    if (s_last_build_ms != 0 && now - s_last_build_ms < UI_TAB_MIN_MS) return; // throttle; stay pending
    s_last_build_ms = now;
    s_have_pending  = 0;
    ui_build_tab(s_pending_tab);
}

static void dispatch_tick(ui_lane_t lane)
{
    // The fallback has no screen widgets; all per-screen static pointers still
    // contain addresses cleaned immediately before it was built.
    if (s_lowmem_active) return;

    // Runs on EVERY tick regardless of the active tab: it collects the rolling
    // graph history so the Graphs screen is pre-drawn on entry (it renders the
    // charts only while Graphs is actually active). Cheap off-lane — it
    // early-returns unless lane == CHART.
    tick_screen_graphs(lane);

    switch (ui_current_tab)
    {
        case TAB_DASHBOARD:
            tick_screen_dashboard(lane);
            break;
        case TAB_MONITOR:
            tick_screen_monitor(lane);
            break;
        case TAB_CONTROL:
            tick_screen_control(lane);
            break;
        case TAB_GRAPHS:
            break; // handled above (always-collect + render)
        case TAB_DIAGNOSTICS:
            tick_screen_diagnostics(lane);
            break;
        case TAB_SETTINGS:
            tick_screen_settings(lane);
            break;
    }
}

void ui_force_refresh(void)
{
    // create_screen_XXX() builds widgets in their *default* look (e.g. mode
    // buttons unselected) and relies on the next tick_screen_XXX() call to
    // style them to match `ms` — but that only happens on the SLOW lane's own
    // 200ms cadence, so a tab switch could show the wrong state for up to
    // 200ms. Call every lane once, right after building the screen, so it's
    // correct on the very first frame.
    tick_common_ui();
    dispatch_tick(UI_LANE_SLOW);
    dispatch_tick(UI_LANE_CHART);
}

void ui_tick(void)
{
    // Service a pending tab switch first (coalesced + throttled, see
    // ui_request_tab). At most one screen rebuild per tick.
    ui_service_pending();

    // Rate-limited lanes; call ui_tick() from the main loop as often as you
    // like — redraw work only happens at each lane's own cadence.
    //
    // FAST (50ms) was retired: it only ever drove the Dashboard/Control gauge
    // arc+needle easing, and those were replaced by plain-text tiles. Every
    // tick_screen_*() now early-returns on UI_LANE_FAST, so dispatching it was
    // 20 no-op wakeups/sec. The two live lanes are SLOW (all text) and CHART
    // (rolling history). Telemetry arrives at 100ms (motorStatusFast) / 1000ms
    // (motorStatusSlow); 200ms text is well inside that. If a future widget
    // needs sub-200ms motion, re-add a lane here rather than speeding SLOW up
    // (that would multiply the xSPI dirty-region load — the bottleneck).
    static uint32_t t_slow, t_chart;
    uint32_t now = lv_tick_get();

    if (now - t_slow >= 200)
    {
        t_slow = now;
        tick_common_ui(); // shared top bar (motor state, link dot)
        dispatch_tick(UI_LANE_SLOW);
    }
    if (now - t_chart >= 125)
    {
        t_chart = now;
        dispatch_tick(UI_LANE_CHART);
    }
}
