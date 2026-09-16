#define main submitted_simulator_main
#include "../../../sim_pc/main.c"
#undef main
#include "../../../ui.c"

/* Review-only host diagnostics. Never exported to the MCU. */
#define CHECK(x) do { if (!(x)) { fprintf(stderr,"FAIL line %d: %s\n",__LINE__,#x); return 1; } } while (0)
static DWORD WINAPI watchdog(LPVOID arg) { (void)arg; Sleep(10000); ExitProcess(124); return 0; }
static void pulse(uint32_t ms, bool fresh) {
    lv_tick_inc(ms);
    if (fresh) { lora_last_rx_tick_ms=lv_tick_get(); lora_rx_revision++; }
    ui_tick(); lv_timer_handler(); lv_refr_now(NULL);
}
static void touch(int x,int y) {
    s_mouse_x=(int16_t)x; s_mouse_y=(int16_t)y;
    s_mouse_pressed=true; pulse(60,false);
    s_mouse_pressed=false; pulse(60,false);
}
static unsigned callbacks;
static void config_cb(uint8_t idx,const ui_hub_device_config_t *cfg) { (void)idx; (void)cfg; callbacks++; }
int main(int argc,char **argv) {
    setvbuf(stdout,NULL,_IONBF,0);
    if(argc!=2) return 2;
    CreateThread(NULL,0,watchdog,NULL,0,NULL);
    lvgl_setup(); lv_tick_inc(100);
    for(uint8_t i=0;i<4;i++) {
        ui_hub_device_config_t cfg;
        ui_auto_get_defaults(i,i==0?UI_PRESET_FAN:UI_PRESET_LIGHT,&cfg);
        CHECK(ui_set_device_config(i,&cfg));
    }
    ui_init(); ui_set_config_changed_cb(config_cb);
    sim_post_node_status(true,1200,true,300,true,235,true,30,true,0,0,0,0);
    pulse(40,true);
    ui_navigate_to_page(UI_PAGE_DEVICES); pulse(40,true);
    if(!strcmp(argv[1],"cancel")) {
        open_device_dialog(0); pulse(40,false); touch(578,98);
        CHECK(s_dialog_overlay==NULL);
        open_device_dialog(0); pulse(40,false); touch(400,278);
        CHECK(s_dialog_subview_auto!=NULL); touch(578,98);
        CHECK(s_dialog_overlay==NULL); CHECK(callbacks==0);
        puts("PASS: independent pointer X on both separately opened subviews"); return 0;
    }
    if(!strcmp(argv[1],"pending")) {
        ui_hub_device_config_t cfg=*ui_get_device_config(0);
        cfg.active_low=1;
        CHECK(ui_set_device_config(0,&cfg));
        CHECK(s_device_cmds[0].phase==UI_CMD_PHASE_PENDING);
        CHECK(callbacks==1);
        const lora_hub_cmd_t before=lora_hub_cmd;
        cfg.active_low=0;
        CHECK(!ui_set_device_config(0,&cfg));
        CHECK(s_dev_configs[0].active_low==1);
        CHECK(memcmp(&before,&lora_hub_cmd,sizeof(before))==0); CHECK(callbacks==1);
        puts("PASS: real live polarity command creates pending; unsafe second edit rejected atomically"); return 0;
    }
    if(!strcmp(argv[1],"layout")) {
        ui_hub_device_config_t cfg=*ui_get_device_config(0); cfg.mode=UI_MODE_AUTO;
        CHECK(ui_set_device_config(0,&cfg));
        ui_navigate_to_page(UI_PAGE_HOME); pulse(40,true);
        lora_node_status.bits.co2_valid=0; pulse(40,true);
        lv_area_t mode,sw; lv_obj_get_coords(s_home_dev_modes[0],&mode); lv_obj_get_coords(s_home_dev_switches[0],&sw);
        printf("Mode '%s' bounds %d,%d..%d,%d; switch %d,%d..%d,%d\n",s_dev_mode_buf[0],mode.x1,mode.y1,mode.x2,mode.y2,sw.x1,sw.y1,sw.x2,sw.y2);
        save_framebuffer("docs/reviews/2026-09-15-auto-v1-layout-review/home-paused.raw");
        CHECK(mode.x2<sw.x1 || mode.y2<sw.y1 || mode.y1>sw.y2);
        return 0;
    }
    if(!strcmp(argv[1],"switches")) {
        for(unsigned polarity=0;polarity<2;polarity++) {
            ui_hub_device_config_t cfg=*ui_get_device_config(0); cfg.active_low=(uint8_t)polarity;
            CHECK(ui_set_device_config(0,&cfg));
            lora_node_status_set_relay_gpios_mask(&lora_node_status,lora_hub_cmd_get_relay_gpios_mask(&lora_hub_cmd));
            lora_node_status.bits.seq_echo=lora_hub_cmd.bits.seq; pulse(40,true);
            for(unsigned page=0;page<2;page++) {
                ui_navigate_to_page(page?UI_PAGE_DEVICES:UI_PAGE_HOME); pulse(40,true);
                lv_obj_t *sw=page?s_dev_page_switches[0]:s_home_dev_switches[0];
                bool before=lv_obj_has_state(sw,LV_STATE_CHECKED);
                lv_area_t box; lv_obj_get_coords(sw,&box);
                touch((box.x1+box.x2)/2,(box.y1+box.y2)/2);
                CHECK(s_device_cmds[0].phase==UI_CMD_PHASE_PENDING);
                CHECK(lv_obj_has_state(sw,LV_STATE_CHECKED)==before);
                CHECK(s_desired_on[0]!=before);
                lora_node_status_set_relay_gpios_mask(&lora_node_status,lora_hub_cmd_get_relay_gpios_mask(&lora_hub_cmd));
                lora_node_status.bits.seq_echo=lora_hub_cmd.bits.seq; pulse(40,true);
                CHECK(lv_obj_has_state(sw,LV_STATE_CHECKED)!=before);
            }
        }
        puts("PASS: pointer switches show old readback until ACK, Home/Devices and both polarities"); return 0;
    }
    if(!strcmp(argv[1],"link_pause")) {
        ui_hub_device_config_t cfg=*ui_get_device_config(0); cfg.mode=UI_MODE_AUTO;
        CHECK(ui_set_device_config(0,&cfg)); pulse(40,true); pulse(6000,false);
        printf("Offline Auto caption: '%s' (expected Auto paused)\n",s_dev_mode_buf[0]);
        CHECK(!strcmp(s_dev_mode_buf[0],"Auto paused")); return 0;
    }
    if(!strcmp(argv[1],"stress")) {
        lv_mem_monitor_t mon; unsigned min_free=UINT_MAX,min_big=UINT_MAX;
        for(unsigned n=0;n<50;n++) {
            open_device_dialog(0); pulse(40,true);
            lv_dropdown_open(s_dialog_preset_dd); pulse(40,false);
            lv_mem_monitor(&mon); if(mon.free_size<min_free)min_free=mon.free_size; if(mon.free_biggest_size<min_big)min_big=mon.free_biggest_size;
            CHECK(mon.free_size>4096);
            lv_dropdown_close(s_dialog_preset_dd); pulse(40,false);
            touch(400,278); CHECK(s_dialog_subview_auto!=NULL);
            touch(578,98); CHECK(s_dialog_overlay==NULL);
        }
        printf("PASS: 50 dropdown/modal/subview cycles min_free=%u min_big=%u\n",min_free,min_big); return 0;
    }
    return 2;
}
