#define main submitted_simulator_main
#include "../../../sim_pc/main.c"
#undef main
#include "../../../ui.c"

/* Host-only independent diagnostics. Do not export. */
#define REQUIRE(x) do { if (!(x)) { fprintf(stderr,"FAIL %d: %s\n",__LINE__,#x); return 1; } } while(0)
static DWORD WINAPI watchdog(LPVOID p) { (void)p; Sleep(15000); ExitProcess(124); return 0; }
static void pulse(uint32_t ms,bool fresh) {
    lv_tick_inc(ms); if(fresh) { lora_last_rx_tick_ms=lv_tick_get(); lora_rx_revision++; }
    ui_tick(); lv_timer_handler(); lv_refr_now(NULL);
}
static unsigned heap(const char *name) {
    lv_mem_monitor_t m; lv_mem_monitor(&m);
    printf("HEAP %s: used=%u free=%u biggest=%u frag=%u\n",name,m.used_pct,(unsigned)m.free_size,(unsigned)m.free_biggest_size,m.frag_pct);
    return (unsigned)m.free_size;
}
int main(int argc,char **argv) {
    if(argc!=2) return 2;
    setvbuf(stdout,NULL,_IONBF,0); CreateThread(NULL,0,watchdog,NULL,0,NULL);
    if(!strcmp(argv[1],"heap_after_suite")) {
        REQUIRE(run_regression()==0);
        ui_navigate_to_page(UI_PAGE_DEVICES); pulse(40,true); heap("Devices");
        open_device_dialog(0); pulse(40,true); unsigned minimum=heap("Device settings");
        pulse(40,true); unsigned settled=heap("Device settings settled"); if(settled<minimum)minimum=settled;
        lv_dropdown_open(s_dialog_preset_dd); pulse(40,true);
        unsigned free_open=heap("Dropdown open");
        if(free_open<minimum)minimum=free_open;
        lv_dropdown_close(s_dialog_preset_dd); pulse(40,true);
        on_dialog_open_limits_click(NULL); pulse(40,true); unsigned limits_free=heap("Auto limits");
        if(limits_free<minimum)minimum=limits_free;
        close_device_dialog(); pulse(40,true);
        REQUIRE(minimum>4096); return 0;
    }
    lvgl_setup(); lv_tick_inc(100);
    for(uint8_t i=0;i<4;i++) {
        ui_hub_device_config_t cfg; ui_auto_get_defaults(i,i?UI_PRESET_LIGHT:UI_PRESET_FAN,&cfg);
        cfg.mode=i?UI_MODE_MANUAL:UI_MODE_AUTO; REQUIRE(ui_set_device_config(i,&cfg));
    }
    ui_init(); sim_post_node_status(true,1200,true,85,true,235,true,48,true,0,0,0,0); pulse(40,true);
    for(unsigned n=0;n<16;n++) pulse(1000,true);
    REQUIRE(s_device_cmds[0].phase==UI_CMD_PHASE_ERROR && s_auto_states[0].paused_error);
    REQUIRE(!lv_obj_has_flag(s_home_dev_retries[0],LV_OBJ_FLAG_HIDDEN));
    if(!strcmp(argv[1],"late_ack")) {
        lora_node_status_set_relay_gpios_mask(&lora_node_status,lora_hub_cmd_get_relay_gpios_mask(&lora_hub_cmd));
        lora_node_status.bits.seq_echo=lora_hub_cmd.bits.seq; pulse(40,true);
    } else if(!strcmp(argv[1],"reconnect")) {
        pulse(6000,false); pulse(40,true);
    } else return 2;
    ui_dismiss_toast(); pulse(40,true);
    bool home_visible=!lv_obj_has_flag(s_home_dev_retries[0],LV_OBJ_FLAG_HIDDEN);
    printf("RECOVERY %s phase=%u latch=%u Home Retry visible=%u\n",argv[1],s_device_cmds[0].phase,s_auto_states[0].paused_error,home_visible);
    save_framebuffer(!strcmp(argv[1],"late_ack")?
        "docs/reviews/2026-09-15-auto-v1-v4-independent/late-ack.raw":
        "docs/reviews/2026-09-15-auto-v1-v4-independent/reconnect.raw");
    ui_navigate_to_page(UI_PAGE_DEVICES); pulse(40,true);
    bool devices_visible=!lv_obj_has_flag(s_dev_page_retries[0],LV_OBJ_FLAG_HIDDEN);
    printf("RECOVERY Devices Retry visible=%u\n",devices_visible);
    REQUIRE(s_auto_states[0].paused_error && home_visible && devices_visible); return 0;
}
