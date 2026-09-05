#include "ui.h"
#include "screens.h"
#if defined(UI_DEMO_SIM) && UI_DEMO_SIM
#include "demo_sim.h"
#endif

lv_obj_t *ui_MainScreen;
static lv_obj_t *ui_ContentHost;

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
    // reserved opMode value 0 for the ~100ms before the first telemetry echo. The operator has
    // to dial in a setpoint AND press START before anything moves.
    // Telemetry (voltage/temps/motorConnected) is left 0 / disconnected on
    // purpose — it fills from the first real MotorStatusFast_t/Slow_t frame (or
    // demo_sim); we don't seed fake "connected"/48 V readings here. Headless
    // simulator runs may explicitly request a non-zero demo command, but the
    // product/demo UI itself still boots in this safe idle state.
    motorCmd.bits.opMode        = OP_MODE_SPEED;
    motorCmd.bits.ctrlValRaw    = 0; // zero setpoint on boot — operator must set one
    motorCmd.bits.limitRaw      = 0; // zero limit on boot — operator must set one too
    motorStatusFast.bits.opMode = OP_MODE_SPEED;

    // Load persisted RS-485 parameters before building the top pill and the
    // Settings dropdowns. Firmware registers its storage hooks before ui_init().
    ui_rs485_load_config();

    // Create top bar and tab bar on the top layer so they are shared
    create_common_ui(lv_layer_top());

    ui_MainScreen = lv_obj_create(NULL);
    lv_obj_clear_flag(ui_MainScreen, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_bg_color(ui_MainScreen, COLOR_BG, LV_PART_MAIN);
    lv_obj_set_style_bg_opa(ui_MainScreen, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_pad_all(ui_MainScreen, 0, LV_PART_MAIN);

    // Persistent, transparent rebuild boundary. Cleaning the full screen made
    // LVGL invalidate/send all 800x480 pixels on every tab switch even though
    // the display GRAM already held the unchanged header and navigation rail.
    // Keeping the host at the exact content rectangle limits teardown redraw to
    // 712x414; child cards still use their original host-relative coordinates.
    ui_ContentHost = lv_obj_create(ui_MainScreen);
    lv_obj_remove_style_all(ui_ContentHost);
    lv_obj_set_size(ui_ContentHost, UI_CONTENT_W, UI_CONTENT_H);
    lv_obj_set_pos(ui_ContentHost, UI_CONTENT_X, UI_CONTENT_Y);
    lv_obj_clear_flag(ui_ContentHost, LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(ui_ContentHost, LV_OBJ_FLAG_OVERFLOW_VISIBLE);

    // Initialize individual screen contents
    create_screen_dashboard(ui_ContentHost);
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
static uint8_t  s_force_pending = 0;
static uint8_t  s_chrome_ready  = 0;
static ui_tab_t s_chrome_tab    = TAB_DASHBOARD;
static uint32_t s_last_build_ms = 0;
static uint8_t  s_lowmem_active = 0;

void ui_request_tab(ui_tab_t tab)
{
    // Cheap: just record the target. Rapid taps overwrite it; only the last
    // one survives to be built by ui_service_pending() on the next tick.
    s_pending_tab  = tab;
    s_have_pending = 1;
    s_force_pending = 0;
}

void ui_request_current_rebuild(void)
{
    s_pending_tab   = ui_current_tab;
    s_have_pending  = 1;
    s_force_pending = 1;
}

// Minimum FREE heap required immediately after cleaning a screen. The 24 KB
// gate preserves construction headroom inside the validated 42 KB pool while
// the persistent chrome remains resident (including a possible splash).
#define UI_HEAP_FLOOR 24000u

// Memory-pressure fallback: a plain full-screen message. Kept tiny so it builds
// even when a real screen wouldn't. Every create is NULL-guarded — under true
// exhaustion this simply renders nothing rather than crashing.
static void ui_build_lowmem(void)
{
    lv_obj_t *scr = ui_ContentHost;
    if (!scr) return;
    lv_obj_t *lbl = lv_label_create(scr);
    if (!lbl) return; // even the label failed — bail, no deref
    lv_label_set_text_static(lbl, "LOW MEMORY\nplease reset the device");
    lv_obj_set_style_text_align(lbl, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
    lv_obj_center(lbl);
}

// The single place that tears down + rebuilds the active screen. Centralized
// here (was duplicated across every action_tab_*) so the create/refresh order
// is defined once and can't drift per tab. Normal tab chrome is committed by
// ui_service_pending() in the preceding refresh phase.
static void ui_build_tab(ui_tab_t tab)
{
    ui_current_tab = tab;
    lv_obj_clean(ui_ContentHost);

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
        return;
    }

    s_lowmem_active = 0;

    switch (tab)
    {
        case TAB_DASHBOARD:   create_screen_dashboard(ui_ContentHost);   break;
        case TAB_MONITOR:     create_screen_monitor(ui_ContentHost);     break;
        case TAB_CONTROL:     create_screen_control(ui_ContentHost);     break;
        case TAB_GRAPHS:      create_screen_graphs(ui_ContentHost);      break;
        case TAB_DIAGNOSTICS: create_screen_diagnostics(ui_ContentHost); break;
        case TAB_SETTINGS:    create_screen_settings(ui_ContentHost);    break;
    }

    ui_force_refresh();
}

// Service at most one transition phase per ui_tick(). A normal tab change first
// commits only the two nav cells + page title and forces that small dirty set to
// the display. The following ui_tick() tears down/builds content. This keeps the
// responsive chrome out of the 600+ KB content transfer while preserving the
// rule that object teardown never runs from an input callback.
//
// Return true after either phase so ui_tick() does not enqueue periodic screen
// updates into the same refresh batch. A same-tab structural rebuild (Control
// LEVEL <-> POSITION) has no chrome transition and goes straight to phase 2.
static bool ui_service_pending(void)
{
    if (!s_have_pending) return false;

    // A normal request back to the content which is still resident cancels an
    // in-flight transition. Restore its chrome in its own small refresh.
    if (s_pending_tab == ui_current_tab && !s_force_pending)
    {
        s_have_pending = 0;
        s_chrome_ready = 0;
        if (s_chrome_tab != ui_current_tab)
        {
            s_chrome_tab = ui_current_tab;
            ui_tabbar_set_active(s_chrome_tab);
            lv_refr_now(NULL);
            return true;
        }
        return false;
    }

    uint32_t now = lv_tick_get();

    if (!s_force_pending)
    {
        // Phase 1. Keep the old content resident for this refresh; only chrome
        // changes. Re-check the target so rapid taps still collapse to the last
        // requested tab before any expensive rebuild happens.
        if (!s_chrome_ready || s_chrome_tab != s_pending_tab)
        {
            if (s_last_build_ms != 0 && now - s_last_build_ms < UI_TAB_MIN_MS) return false;
            s_chrome_tab   = s_pending_tab;
            s_chrome_ready = 1;
            ui_tabbar_set_active(s_chrome_tab);
            lv_refr_now(NULL);
            return true;
        }
    }
    else if (s_chrome_tab != ui_current_tab)
    {
        // A Control structural rebuild can supersede a just-requested tab. Put
        // the chrome back on the still-resident Control screen before rebuilding.
        s_chrome_tab   = ui_current_tab;
        s_chrome_ready = 0;
        ui_tabbar_set_active(s_chrome_tab);
        lv_refr_now(NULL);
        return true;
    }

    // Phase 2. For a normal switch phase 1 has already crossed the throttle
    // gate. Same-tab rebuilds retain the original 90 ms allocation-churn guard.
    if (s_force_pending && s_last_build_ms != 0 && now - s_last_build_ms < UI_TAB_MIN_MS)
        return false;

    s_last_build_ms = now;
    s_have_pending  = 0;
    s_force_pending = 0;
    s_chrome_ready  = 0;
    ui_build_tab(s_pending_tab);
    return true;
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
    // 200ms. Call both active lanes once, right after building the screen, so
    // it is correct on the very first frame. FAST remains only as a compatible
    // enum value and is not scheduled.
    tick_common_ui();
    dispatch_tick(UI_LANE_SLOW);
    dispatch_tick(UI_LANE_CHART);
}

void ui_tick(void)
{
    // Service a pending tab switch first (coalesced + throttled, see
    // ui_request_tab). Chrome and content are deliberately separate refreshes;
    // skip periodic lanes on either transition phase.
    if (ui_service_pending()) return;

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
