#define main submitted_simulator_main
#include "../../../sim_pc/main.c"
#undef main
#include "../../../ui.c"

/* Independent host-only observations; nonzero on the documented failed contract. */
#define REQUIRE(x) do { if (!(x)) { fprintf(stderr,"FAIL %d: %s\n",__LINE__,#x); return 1; } } while(0)
static DWORD WINAPI watchdog(LPVOID p) { (void)p; Sleep(10000); ExitProcess(124); return 0; }
static void pulse(uint32_t ms,bool fresh) {
    lv_tick_inc(ms); if(fresh) { lora_last_rx_tick_ms=lv_tick_get(); lora_rx_revision++; }
    ui_tick(); lv_timer_handler(); lv_refr_now(NULL);
}
static void bounds(const char *name,lv_obj_t *obj) {
    lv_area_t a; lv_obj_get_coords(obj,&a);
    printf("%s: %d,%d..%d,%d\n",name,a.x1,a.y1,a.x2,a.y2);
}
int main(int argc,char **argv) {
    if(argc!=2) return 2;
    setvbuf(stdout,NULL,_IONBF,0); CreateThread(NULL,0,watchdog,NULL,0,NULL);
    lvgl_setup(); lv_tick_inc(100);
    for(uint8_t i=0;i<4;i++) {
        ui_hub_device_config_t cfg; ui_auto_get_defaults(i,i?UI_PRESET_LIGHT:UI_PRESET_FAN,&cfg);
        REQUIRE(ui_set_device_config(i,&cfg));
    }
    ui_init(); sim_post_node_status(true,1200,true,85,true,235,true,48,true,0,0,0,0); pulse(40,true);
    if(!strcmp(argv[1],"dropdown")) {
        ui_navigate_to_page(UI_PAGE_DEVICES); pulse(40,true); open_device_dialog(0); pulse(40,true);
        lv_dropdown_open(s_dialog_preset_dd); pulse(40,true);
        lv_obj_t *list=lv_dropdown_get_list(s_dialog_preset_dd);
        lv_obj_t *label=lv_obj_get_child(list,0);
        const lv_font_t *font=lv_obj_get_style_text_font(label,LV_PART_MAIN);
        int spacing=lv_obj_get_style_text_line_space(label,LV_PART_MAIN);
        printf("Dropdown label font18=%u font14=%u line_height=%d line_spacing=%d row_pitch=%d\n",
               font==&ui_font_18,font==&lv_font_montserrat_14,font->line_height,spacing,font->line_height+spacing);
        bounds("list",list); bounds("modal",s_dialog_box);
        save_framebuffer("docs/reviews/2026-09-15-auto-layout-result-review/dropdown.raw");
        REQUIRE(font==&ui_font_18); REQUIRE(font->line_height+spacing>=44); return 0;
    }
    if(!strcmp(argv[1],"timeout")) {
        ui_hub_device_config_t cfg=*ui_get_device_config(0); cfg.mode=UI_MODE_AUTO; REQUIRE(ui_set_device_config(0,&cfg));
        for(unsigned i=0;i<16;i++) pulse(1000,true);
        printf("phase=%u paused_error=%u\n",s_device_cmds[0].phase,s_auto_states[0].paused_error);
        REQUIRE(s_device_cmds[0].phase==UI_CMD_PHASE_ERROR && s_auto_states[0].paused_error);
        ui_dismiss_toast(); pulse(40,true);
        bounds("mode",s_home_dev_modes[0]); bounds("state",s_home_dev_states[0]);
        bounds("tile",lv_obj_get_parent(s_home_dev_modes[0])); bounds("retry",s_home_dev_retries[0]);
        save_framebuffer("docs/reviews/2026-09-15-auto-layout-result-review/timeout.raw");
        return 0;
    }
    return 2;
}
