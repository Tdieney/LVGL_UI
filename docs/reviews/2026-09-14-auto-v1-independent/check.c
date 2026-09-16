#define main submitted_simulator_main
#include "../../../sim_pc/main.c"
#undef main
#include "../../../ui.c"

static DWORD WINAPI review_watchdog(LPVOID arg)
{
    (void)arg; Sleep(5000); fputs("REVIEW watchdog timeout\n",stderr); ExitProcess(124);
    return 0;
}

static void review_pulse(uint32_t ms, bool fresh)
{
    lv_tick_inc(ms);
    if (fresh) { lora_last_rx_tick_ms = lv_tick_get(); lora_rx_revision++; }
    ui_tick();
    lv_timer_handler();
    lv_refr_now(NULL);
}
static void review_touch(int x, int y)
{
    s_mouse_x = (int16_t)x; s_mouse_y = (int16_t)y;
    s_mouse_pressed = true; review_pulse(60, false);
    s_mouse_pressed = false; review_pulse(60, false);
}
static void review_init_manual(void)
{
    for (uint8_t i = 0; i < 4; i++) {
        ui_hub_device_config_t cfg;
        ui_auto_get_defaults(i, i == 0 ? UI_PRESET_FAN : UI_PRESET_LIGHT, &cfg);
        cfg.mode = UI_MODE_MANUAL;
        ui_set_device_config(i, &cfg);
    }
    ui_init();
    sim_post_node_status(true, 1200, true, 300, true, 235, true, 30, true, 0, 0, 0, 0);
    review_pulse(40, true);
}
static void review_enable_auto(void)
{
    ui_navigate_to_page(UI_PAGE_DEVICES);review_pulse(40,false);
    open_device_dialog(0);
    s_dialog_edit_mode = UI_MODE_AUTO;
    on_dialog_save_click(NULL);
    review_pulse(40, true);
}
int main(int argc, char **argv)
{
    setvbuf(stdout, NULL, _IONBF, 0);
    if (argc != 2) return 2;
    CreateThread(NULL,0,review_watchdog,NULL,0,NULL);
    lvgl_setup(); lv_tick_inc(100);
    if (!strcmp(argv[1], "boot")) {
        for(uint8_t i=0;i<4;i++) { ui_hub_device_config_t cfg;ui_auto_get_defaults(i,UI_PRESET_FAN,&cfg);cfg.mode=UI_MODE_MANUAL;cfg.active_low=1;ui_set_device_config(i,&cfg); }
        ui_init();
        printf("Pre-init restore preserved: active_low=%u; before report TX=%u\n",s_dev_configs[0].active_low,ui_is_tx_ready());
        unsigned mismatches=0;
        for(unsigned mask=0;mask<16;mask++) {
            review_pulse(6000,false);
            lora_node_status_set_relay_gpios_mask(&lora_node_status,(uint8_t)mask);
            review_pulse(40,true);
            if(lora_hub_cmd_get_relay_gpios_mask(&lora_hub_cmd)!=mask)mismatches++;
        }
        printf("16 reported masks reconciled, mismatches=%u\n",mismatches);
        return 0;
    }
    if (!strcmp(argv[1], "defaults")) {
        ui_init();
        printf("Default modes (0=Auto,1=Manual): %u %u %u %u\n", s_dev_configs[0].mode, s_dev_configs[1].mode, s_dev_configs[2].mode, s_dev_configs[3].mode);
        sim_post_node_status(true,1200,true,300,true,235,true,30,true,0,0,0,0);
        review_pulse(40,true);
        for(int i=0;i<14;i++) review_pulse(1000,true);
        printf("No user action: commanded GPIO mask=%u (initial readback=0)\n",lora_hub_cmd_get_relay_gpios_mask(&lora_hub_cmd));
        ui_hub_device_config_t cfg=*ui_get_device_config(0);
        cfg.auto_on_thresh=0; ui_set_device_config(0,&cfg);
        printf("Invalid stored thresholds fallback mode=%u (expected Manual=1)\n",s_dev_configs[0].mode);
        cfg=*ui_get_device_config(0);cfg.schema_version=255;
        printf("Unknown schema accepted=%u\n",ui_auto_validate_config(&cfg,NULL,0));
        return 0;
    }
    review_init_manual();
    if (!strcmp(argv[1], "chart")) {
        int32_t vals[16];for(int i=0;i<16;i++)vals[i]=400+i;
        ui_feed_history(UI_METRIC_CO2,vals,NULL,16,0);
        ui_navigate_to_page(UI_PAGE_TRENDS);review_pulse(40,false);review_pulse(40,false);
        for(int i=0;i<16;i++)vals[i]=1000+10*i;
        ui_feed_history(UI_METRIC_CO2,vals,NULL,16,0);review_pulse(40,false);
        static uint16_t pixels[800*480];memcpy(pixels,s_framebuffer,sizeof(pixels));
        lv_obj_invalidate(s_screen);lv_refr_now(NULL);
        unsigned diff=0;for(int y=235;y<369;y++)for(int x=35;x<90;x++)if(pixels[y*800+x]!=s_framebuffer[y*800+x])diff++;
        printf("Ytop=%s width=%d, forced redraw Y pixels changed=%u\n",lv_label_get_text(s_trends_y_lbls[0]),lv_obj_get_width(s_trends_y_lbls[0]),diff);
        reset_flush_counters();for(int i=0;i<10;i++){ui_feed_history(UI_METRIC_CO2,vals,NULL,16,0);review_pulse(10,true);}
        printf("10 identical Trends frames+refeeds flushes=%llu pixels=%llu\n",s_flush_calls,s_flushed_pixels);
        return 0;
    }
    if (!strcmp(argv[1], "close")) {
        ui_navigate_to_page(UI_PAGE_DEVICES);review_pulse(40,false);
        open_device_dialog(0);review_pulse(40,false);
        review_touch(578,98);
        printf("X tap on Device view: modal still open=%u\n",s_dialog_overlay!=NULL);
        if(s_dialog_overlay) { on_dialog_open_limits_click(NULL);review_pulse(40,false);review_touch(578,98); }
        printf("X tap on Auto view: modal still open=%u\n",s_dialog_overlay!=NULL);
        return 0;
    }
    if (!strcmp(argv[1], "polarity_draft")) {
        ui_navigate_to_page(UI_PAGE_DEVICES);review_pulse(40,false);
        open_device_dialog(0);review_pulse(40,false);
        review_touch(573,315);
        printf("Physical polarity tap: widget=%u draft=%u\n",lv_obj_has_state(s_dialog_active_low_sw,LV_STATE_CHECKED),s_dialog_edit_active_low);
        review_touch(400,278);
        printf("After Set limits: auto view=%u draft polarity=%u\n",s_dialog_subview_auto!=NULL,s_dialog_edit_active_low);
        review_touch(400,367);
        printf("After Save from Auto view: committed polarity=%u\n",s_dev_configs[0].active_low);
        return 0;
    }
    if (!strcmp(argv[1], "api_hold")) {
        for(int i=0;i<20;i++)review_pulse(1000,true);
        ui_hub_device_config_t cfg=*ui_get_device_config(0);cfg.mode=UI_MODE_AUTO;
        ui_set_device_config(0,&cfg);
        uint32_t enabled=lv_tick_get();
        for(int i=0;i<3;i++)review_pulse(100,true);
        printf("Public API enabled Auto %u ms ago; command phase=%u gpio=%u (hold should be 10000ms)\n",lv_tick_get()-enabled,s_device_cmds[0].phase,lora_hub_cmd_get_relay_gpio(&lora_hub_cmd,0));
        printf("Pending switch checked=%u while reported GPIO=%u\n",lv_obj_has_state(s_home_dev_switches[0],LV_STATE_CHECKED),lora_node_status_get_relay_gpio(&lora_node_status,0));
        cfg.active_low=1;ui_set_device_config(0,&cfg);
        printf("Public API changed polarity during pending: active_low=%u phase=%u\n",s_dev_configs[0].active_low,s_device_cmds[0].phase);
        return 0;
    }
    review_enable_auto();
    if (!strcmp(argv[1], "recovery")) {
        lora_node_status.bits.co2_ppm=900;
        for(int i=0;i<20;i++)review_pulse(1000,true);
        lora_node_status.bits.co2_valid=0;review_pulse(1000,true);
        printf("Sensor invalid: displayed mode=%s\n",s_dev_mode_buf[0]);
        lora_node_status.bits.co2_valid=1;lora_node_status.bits.co2_ppm=1200;
        uint32_t recovered=lv_tick_get();
        for(int i=0;i<3;i++)review_pulse(1000,true);
        printf("Sensor recovered %u ms ago: phase=%u gpio=%u (must restart 10s hold)\n",lv_tick_get()-recovered,s_device_cmds[0].phase,lora_hub_cmd_get_relay_gpio(&lora_hub_cmd,0));
        return 0;
    }
    if (!strcmp(argv[1], "error_reconnect")) {
        for(int i=0;i<14;i++)review_pulse(1000,true);
        for(int i=0;i<4;i++)review_pulse(1000,true);
        printf("Before disconnect: phase=%u paused_error=%u\n",s_device_cmds[0].phase,s_auto_states[0].paused_error);
        review_pulse(6000,false);
        review_pulse(100,true);
        printf("Reconnect without Retry: phase=%u paused_error=%u\n",s_device_cmds[0].phase,s_auto_states[0].paused_error);
        return 0;
    }
    return 2;
}
