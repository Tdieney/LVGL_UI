#define main submitted_simulator_main
#include "../../../sim_pc/main.c"
#undef main
#include "../../../ui.c"
#define REQUIRE(x) do { if (!(x)) { fprintf(stderr,"FAIL %d: %s\n",__LINE__,#x); return 1; } } while(0)
static DWORD WINAPI watchdog(LPVOID p) { (void)p; Sleep(15000); ExitProcess(124); return 0; }
static void pulse(uint32_t ms, bool fresh) {
    lv_tick_inc(ms);
    if (fresh) { lora_last_rx_tick_ms=lv_tick_get(); lora_rx_revision++; }
    ui_tick(); lv_timer_handler(); lv_refr_now(NULL);
}
static unsigned heap(const char *name) {
    lv_mem_monitor_t m; lv_mem_monitor(&m);
    printf("HEAP %s: used=%u free=%u biggest=%u frag=%u\n", name, m.used_pct,
           (unsigned)m.free_size, (unsigned)m.free_biggest_size, m.frag_pct);
    return (unsigned)m.free_size;
}

/* Review-only host probe; no production changes or MCU export. */
int main(int argc, char **argv)
{
    if (argc != 2) return 2;
    setvbuf(stdout, NULL, _IONBF, 0);
    CreateThread(NULL, 0, watchdog, NULL, 0, NULL);
    REQUIRE(run_regression() == 0);
    unsigned minimum = heap("after suite");
    ui_hub_device_config_t cfg;
    puts("TRACE configure Auto");
    ui_auto_get_defaults(0, UI_PRESET_PURIFIER, &cfg);
    cfg.mode = UI_MODE_AUTO;
    REQUIRE(ui_set_device_config(0, &cfg));
    puts("TRACE post healthy frame");
    sim_post_node_status(true, 700, true, 180, true, 235, true, 48, true, 0, 0, 0, 0);
    pulse(40, true);
    puts("TRACE hold and qualify");
    for (unsigned n = 0; n < 12; n++) pulse(1000, true);
    REQUIRE(s_device_cmds[0].phase == UI_CMD_PHASE_PENDING);
    puts("TRACE open modal");
    ui_navigate_to_page(UI_PAGE_DEVICES);
    pulse(20, true);
    heap("Devices pending before modal");
    open_device_dialog(0);
    heap("Device settings pending before tick");
    pulse(20, true);
    heap("Device settings pending settled");
    lv_dropdown_open(s_dialog_preset_dd);
    heap("Dropdown pending before tick");
    pulse(20, true);
    unsigned f = heap("Auto pending + dropdown"); if (f < minimum) minimum = f;
    pulse(3100, true);
    REQUIRE(s_device_cmds[0].phase == UI_CMD_PHASE_ERROR && s_auto_states[0].paused_error);
    REQUIRE(!lv_obj_has_flag(s_toast_obj, LV_OBJ_FLAG_HIDDEN));
    f = heap("Auto timeout + dropdown + toast"); if (f < minimum) minimum = f;
    lv_dropdown_close(s_dialog_preset_dd);
    pulse(20, true);
    on_dialog_open_limits_click(NULL);
    pulse(20, true);
    for (unsigned n = 0; n < 10; n++) ui_dialog_step_thresh(false, 1);
    pulse(20, true);
    REQUIRE(lv_obj_has_state(s_dialog_btn_save_auto, LV_STATE_DISABLED));
    f = heap("Invalid limits + Auto error + toast"); if (f < minimum) minimum = f;
    ui_dialog_limits_back();
    pulse(20, true);
    lv_dropdown_set_selected(s_dialog_preset_dd, 3);
    lv_event_send(s_dialog_preset_dd, LV_EVENT_VALUE_CHANGED, NULL);
    pulse(20, true);
    REQUIRE(s_dialog_mode_helper_lbl != NULL);
    lv_dropdown_open(s_dialog_preset_dd);
    pulse(20, true);
    f = heap("Unsupported preset + dropdown + Auto error + toast"); if (f < minimum) minimum = f;
    printf("COMBINED minimum=%u threshold>4096\n", minimum);
    REQUIRE(minimum > 4096);
    return 0;
}
