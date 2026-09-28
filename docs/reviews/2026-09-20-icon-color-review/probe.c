/* Review-only diagnostic; does not modify production or simulator sources. */
#define main submitted_simulator_main
#include "../../../sim_pc/main.c"
#undef main

static void snap(const char *name)
{
    char path[512];
    advance_ui(50u);
    snprintf(path, sizeof(path), "docs/reviews/2026-09-20-icon-color-review/%s.raw", name);
    save_framebuffer(path);
    printf("%s: submitted containment audit = %s\n", name,
           audit_visible_glyph_containment(lv_scr_act()) ? "PASS" : "FAIL");
}

int main(void)
{
    lvgl_setup(); ui_init(); populate_node_status_baseline();
    advance_ui(2200u);
    ui_navigate_to_page(UI_PAGE_HOME);
    sim_post_node_status(true, 10000, true, 500, true, -32768, true, 100, true, 1, 0, 0, 1);
    snap("probe_home_limits");
    ui_navigate_to_page(UI_PAGE_TRENDS);
    ui_select_trend_metric(UI_METRIC_CO2);
    int32_t vals[16]; bool valid[16];
    for (int i=0; i<16; ++i) { vals[i]=10000; valid[i]=true; }
    ui_feed_history(UI_METRIC_CO2, vals, valid, 16, 1000);
    snap("probe_trends_10000");
    ui_feed_history(UI_METRIC_CO2, vals, valid, 2, 1439);
    snap("probe_trends_two_samples");
    ui_select_trend_metric(UI_METRIC_TEMP);
    for (int i=0; i<16; ++i) vals[i]=-32768;
    ui_feed_history(UI_METRIC_TEMP, vals, valid, 16, 1000);
    snap("probe_trends_negative");
    ui_open_device_settings(1); ui_dialog_open_limits();
    snap("probe_auto_live_max");
    ui_close_device_settings(); ui_navigate_to_page(UI_PAGE_DEVICES);
    ui_hub_device_config_t cfg; ui_auto_get_defaults(1, UI_PRESET_FAN, &cfg);
    cfg.mode=UI_MODE_AUTO;
    ui_test_set_boot_staging(true); ui_set_device_config(1, &cfg); ui_test_set_boot_staging(false);
    snap("probe_devices_auto");
    ui_navigate_to_page(UI_PAGE_HOME);
    snap("probe_home_auto");
    ui_close_device_settings(); ui_test_reset_device_commands();
    ui_auto_get_defaults(1, UI_PRESET_FAN, &cfg);
    cfg.auto_on_thresh=10000; cfg.auto_off_thresh=9950;
    ui_test_set_boot_staging(true); ui_set_device_config(1, &cfg); ui_test_set_boot_staging(false);
    ui_open_device_settings(1); snap("probe_settings_max_summary");
    ui_close_device_settings(); ui_open_device_settings(0); ui_dialog_open_limits();
    for (int i=0; i<10; ++i) ui_dialog_step_thresh(false, 1);
    ui_dialog_limits_back(); snap("probe_settings_invalid_back");
    ui_close_device_settings(); ui_test_reset_device_commands();
    sim_set_tx_sink(fake_node_receive_cmd);
    s_fake_node_cmd_in_flight=false;
    ui_auto_get_defaults(1, UI_PRESET_FAN, &cfg); cfg.mode=UI_MODE_AUTO;
    ui_set_device_config(1, &cfg);
    sim_post_node_status(true, 1200, true, 85, true, 235, true, 48, true, 0, 0, 0, 0);
    lora_hub_cmd_set_relay_gpios_mask(&lora_hub_cmd, 0);
    lora_node_status.bits.seq_echo=lora_hub_cmd.bits.seq;
    s_fake_node_mode=FAKE_NODE_MODE_FAIL_NEXT;
    advance_ui(12000u);
    printf("recovery setup phase=%d latch=%d\n", ui_get_test_hooks()->device_cmds[1].phase, ui_get_auto_paused_error(1));
    advance_ui(UI_CMD_TIMEOUT_MS+200u);
    if (ui_get_test_hooks()->device_cmds[1].phase == UI_CMD_PHASE_PENDING)
        advance_ui(UI_CMD_TIMEOUT_MS+200u);
    printf("after timeout phase=%d latch=%d\n", ui_get_test_hooks()->device_cmds[1].phase, ui_get_auto_paused_error(1));
    if (!ui_get_auto_paused_error(1)) return 2;
    lora_node_status_set_relay_gpios_mask(&lora_node_status, lora_hub_cmd_get_relay_gpios_mask(&lora_hub_cmd));
    lora_node_status.bits.seq_echo=lora_hub_cmd.bits.seq;
    lora_last_rx_tick_ms=lv_tick_get(); lora_rx_revision++;
    ui_dismiss_toast();
    ui_navigate_to_page(UI_PAGE_HOME); snap("probe_home_latched_recovery");
    printf("after late ACK phase=%d latch=%d\n", ui_get_test_hooks()->device_cmds[1].phase, ui_get_auto_paused_error(1));
    ui_navigate_to_page(UI_PAGE_DEVICES); snap("probe_devices_latched_recovery");
    ui_set_uptime_seconds(3600000u);
    s_fake_node_mode=FAKE_NODE_MODE_STOP_REPORTING;
    advance_ui(6000u); ui_navigate_to_page(UI_PAGE_HOME); snap("probe_uptime_1000h_offline");
    s_fake_node_mode=FAKE_NODE_MODE_NORMAL;
    populate_node_status_baseline(); ui_test_reset_device_commands(); ui_test_reset_device_configs();
    advance_ui(50u);
    ui_trigger_device_toggle(0); advance_ui(20u);
    printf("pending settings setup phase=%d\n", ui_get_test_hooks()->device_cmds[0].phase);
    if (ui_get_test_hooks()->device_cmds[0].phase != UI_CMD_PHASE_PENDING) return 3;
    ui_open_device_settings(0); ui_set_dialog_active_low(1);
    ui_save_device_settings(); snap("probe_settings_pending_rejection");
    ui_close_device_settings();
    ui_test_reset_device_commands(); ui_test_reset_device_configs();
    populate_node_status_baseline(); advance_ui(50u);
    ui_navigate_to_page(UI_PAGE_TRENDS);
    ui_select_trend_metric(UI_METRIC_CO2);
    for (int i=0; i<16; ++i) { vals[i]=420+i*10; valid[i]=true; }
    valid[0]=false; valid[1]=false; valid[14]=false; valid[15]=false;
    ui_feed_history(UI_METRIC_CO2, vals, valid, 16, 600);
    snap("probe_trends_edge_gaps");
    for (int i=0; i<16; ++i) valid[i]=(i==8);
    ui_feed_history(UI_METRIC_CO2, vals, valid, 16, 600);
    snap("probe_trends_middle_singleton");
    for (int i=0; i<16; ++i) valid[i]=true;
    ui_feed_history(UI_METRIC_CO2, vals, valid, 16, 600);
    ui_select_trend_metric(UI_METRIC_HUMIDITY);
    snap("probe_trends_units_inactive");
    ui_select_trend_metric(UI_METRIC_CO2);
    snap("probe_trends_units_active_again");
    return 0;
}
