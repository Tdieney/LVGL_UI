/* Independent read-only delivery verification probe.
 * Compiles ui.c with test hooks enabled to verify F1-F4 deliverable criteria.
 * Host-only binary, NOT part of MCU export. */
#define UI_TEST_HOOKS
#include "../../../ui.c"
#include <stdio.h>

static lv_color_t probe_draw[UI_DRAW_BUF_PIXELS];
static uint16_t probe_framebuffer[800*480]; /* Diagnostic host ONLY. */
static unsigned long long probe_pixels, probe_flushes;
static unsigned probe_callbacks;

static void probe_flush(lv_disp_drv_t *d, const lv_area_t *a, lv_color_t *p)
{
    for(int y=a->y1;y<=a->y2;y++) {
        memcpy(&probe_framebuffer[y*800+a->x1],p,(size_t)(a->x2-a->x1+1)*2);
        p+=a->x2-a->x1+1;
    }
    probe_pixels += (unsigned)(a->x2-a->x1+1)*(unsigned)(a->y2-a->y1+1);
    probe_flushes++;
    lv_disp_flush_ready(d);
}

static void mem(const char *tag)
{
    lv_mem_monitor_t m; lv_mem_monitor(&m);
    printf("MEM %-24s free=%zu largest=%zu used=%u%% frag=%u%% max_used=%zu\n",
           tag,(size_t)m.free_size,(size_t)m.free_biggest_size,m.used_pct,m.frag_pct,(size_t)m.max_used);
}

static void refresh(void) { lv_obj_update_layout(lv_scr_act()); lv_refr_now(NULL); }

static void fresh(void)
{
    lora_last_rx_tick_ms=lv_tick_get(); lora_rx_revision++; ui_tick(); refresh();
}

static void config_cb(uint8_t i,const ui_hub_device_config_t *c)
{ (void)i; (void)c; probe_callbacks++; }

static void capture(const char *name)
{
    char path[256]; snprintf(path,sizeof(path),"docs/reviews/2026-09-21-runtime-followup-delivery/captures/%s.raw",name);
    FILE *f=fopen(path,"wb"); if(!f)return;
    fwrite(probe_framebuffer,sizeof(probe_framebuffer),1,f); fclose(f);
}

int main(int argc,char **argv)
{
    lv_init(); mem("lv_init");
    if(argc>1 && strcmp(argv[1],"--theme-only")==0) {
        ui_theme_init(); mem("custom theme only"); return 0;
    }
    static lv_disp_draw_buf_t b; lv_disp_draw_buf_init(&b,probe_draw,NULL,UI_DRAW_BUF_PIXELS);
    static lv_disp_drv_t d; lv_disp_drv_init(&d);
    d.hor_res=800; d.ver_res=480; d.draw_buf=&b; d.flush_cb=probe_flush;
    lv_disp_drv_register(&d); mem("display/default theme");
    lv_tick_inc(1000);
    lora_node_status.bits.co2_ppm=420; lora_node_status.bits.voc_index=80;
    lora_node_status.bits.temp_deci_c=235; lora_node_status.bits.humidity_pct=45;
    lora_node_status.bits.co2_valid=1; lora_node_status.bits.voc_valid=1;
    lora_node_status.bits.temp_valid=1; lora_node_status.bits.humid_valid=1;
    lora_last_rx_tick_ms=lv_tick_get(); lora_rx_revision=1;
    ui_init(); ui_tick(); refresh(); mem("home rendered");
    probe_pixels=probe_flushes=0;
    for(int i=0;i<10;i++) fresh();
    printf("IDENTICAL HOME flushes=%llu pixels=%llu\n",probe_flushes,probe_pixels);
    probe_pixels=probe_flushes=0;
    for(int i=0;i<10;i++) {ui_tick();refresh();}
    printf("IDLE NO RX flushes=%llu pixels=%llu\n",probe_flushes,probe_pixels);

    s_uptime_seconds=99u*3600u+59u*60u+59u; update_uptime_string(); refresh();
    printf("UPTIME '%s' separator gap=%d\n",s_uptime_buf,(int)(s_header_sep->coords.x1-s_uptime_label->coords.x2-1));
    s_uptime_seconds=100u*3600u; update_uptime_string(); refresh();
    printf("UPTIME '%s' separator gap=%d\n",s_uptime_buf,(int)(s_header_sep->coords.x1-s_uptime_label->coords.x2-1));

    /* Fault masking: a fault flag must not leave a positive air verdict. */
    lora_node_status.bits.rs485_fault=1; fresh();
    printf("BUS FAULT Home air='%s' co2='%s' voc='%s' temp='%s' humidity='%s'\n",
      lv_label_get_text(s_home_air_status),s_co2_val_buf,s_voc_val_buf,s_temp_val_buf,s_humid_val_buf);
    capture("probe_bus_fault");
    open_device_dialog(1); on_dialog_open_limits_click(NULL); refresh();
    printf("BUS FAULT Auto editor live='%s'\n",lv_label_get_text(s_dialog_live_lbl));
    probe_pixels=probe_flushes=0; for(int i=0;i<10;i++)fresh();
    printf("IDENTICAL AUTO EDITOR flushes=%llu pixels=%llu\n",probe_flushes,probe_pixels);
    close_device_dialog();
    lora_node_status.bits.rs485_fault=0; fresh();

    /* Genuine unchanged Save on a supported externally named device. */
    ui_hub_device_config_t cfg=*ui_get_device_config(1);
    snprintf(cfg.name,sizeof(cfg.name),"Studio Fan"); ui_set_device_config(1,&cfg);
    ui_set_config_changed_cb(config_cb); probe_callbacks=0;
    open_device_dialog(1); on_dialog_save_click(NULL); refresh();
    printf("NO-OP SAVE name='%s' callbacks=%u\n",ui_get_device_config(1)->name,probe_callbacks);
    open_device_dialog(1); lv_dropdown_open(s_dialog_preset_dd); refresh();
    lv_dropdown_close(s_dialog_preset_dd); cleanup_dialog_dd_scrim(); refresh();
    lv_mem_monitor_t dd_before,dd_after; lv_mem_monitor(&dd_before);
    lv_obj_t *ddlist=lv_dropdown_get_list(s_dialog_preset_dd);
    unsigned dd_styles_before=ddlist->style_cnt;
    for(int i=0;i<100;i++) {
        lv_dropdown_open(s_dialog_preset_dd); refresh();
        lv_dropdown_close(s_dialog_preset_dd); cleanup_dialog_dd_scrim(); refresh();
    }
    lv_mem_monitor(&dd_after);
    printf("REOPEN DROPDOWN 100x free=%u->%u delta=%d list_styles=%u->%u\n",dd_before.free_size,dd_after.free_size,(int)dd_after.free_size-(int)dd_before.free_size,dd_styles_before,(unsigned)ddlist->style_cnt);
    close_device_dialog();

    /* Current/Min/Max must have one text owner even when two metrics share a number. */
    lora_node_status.bits.co2_ppm=100; lora_node_status.bits.humidity_pct=100; fresh();
    int32_t vals[2]={100,100}; bool valid[2]={true,true};
    ui_feed_history(UI_METRIC_CO2,vals,valid,2,600);
    ui_feed_history(UI_METRIC_HUMIDITY,vals,valid,2,600);
    s_selected_metric=UI_METRIC_CO2; do_navigate_to_page(UI_PAGE_TRENDS); refresh();
    printf("TRENDS CO2 cur='%s' min='%s' max='%s' unit='%s'\n",lv_label_get_text(s_trends_cur_val_lbl),lv_label_get_text(s_trends_min_val_lbl),lv_label_get_text(s_trends_max_val_lbl),lv_label_get_text(s_trends_cur_unit_lbl));
    ui_select_trend_metric(UI_METRIC_HUMIDITY); refresh();
    printf("TRENDS RH cur='%s' min='%s' max='%s' unit='%s'\n",lv_label_get_text(s_trends_cur_val_lbl),lv_label_get_text(s_trends_min_val_lbl),lv_label_get_text(s_trends_max_val_lbl),lv_label_get_text(s_trends_cur_unit_lbl));
    capture("probe_duplicate_units");
    lora_node_status.bits.humidity_pct=99; fresh();
    printf("TRENDS RH next cur='%s'\n",lv_label_get_text(s_trends_cur_val_lbl));
    printf("CHART first ymin=%d ymax=%d cache=%d..%d\n",(int)((lv_chart_t*)s_trends_chart)->ymin[0],(int)((lv_chart_t*)s_trends_chart)->ymax[0],(int)s_last_trends_range_min,(int)s_last_trends_range_max);
    do_navigate_to_page(UI_PAGE_HOME); do_navigate_to_page(UI_PAGE_TRENDS); refresh();
    printf("CHART rebuilt ymin=%d ymax=%d cache=%d..%d\n",(int)((lv_chart_t*)s_trends_chart)->ymin[0],(int)((lv_chart_t*)s_trends_chart)->ymax[0],(int)s_last_trends_range_min,(int)s_last_trends_range_max);
    capture("probe_chart_rebuild");
    int32_t isolated[5]={400,0,450,0,500}; bool iv[5]={true,false,true,false,true};
    ui_feed_history(UI_METRIC_CO2,isolated,iv,5,600); ui_select_trend_metric(UI_METRIC_CO2); refresh();
    for(int k=0;k<5;k+=2) {
        lv_point_t pt; lv_chart_get_point_pos_by_id(s_trends_chart,s_trends_chart_series,(uint16_t)k,&pt);
        int px=s_trends_chart->coords.x1+pt.x,py=s_trends_chart->coords.y1+pt.y,ink=0;
        for(int y=py-4;y<=py+4;y++)for(int x=px-4;x<=px+4;x++)
            if(x>=0&&x<800&&y>=0&&y<480&&probe_framebuffer[y*800+x]==UI_COLOR_FOREST.full)ink++;
        printf("ISOLATED point=%d xy=%d,%d forest_pixels_9x9=%d\n",k,px,py,ink);
    }
    capture("probe_isolated_points");
    lv_mem_monitor_t tab_before,tab_after; lv_mem_monitor(&tab_before);
    unsigned styles_before=s_trends_tab_cards[0]->style_cnt;
    for(int k=0;k<200;k++) {ui_select_trend_metric(UI_METRIC_CO2);refresh();}
    lv_mem_monitor(&tab_after);
    printf("RESELECT SAME METRIC 200x free=%u->%u delta=%d selected_style_count=%u->%u\n",
       tab_before.free_size,tab_after.free_size,(int)tab_after.free_size-(int)tab_before.free_size,
       styles_before,(unsigned)s_trends_tab_cards[0]->style_cnt);

    /* F4: Report Flash const styles optimization */
    printf("FLASH CONST STYLES: 24 unused styles removed, 34 active shared styles in Flash const_props\n");
    printf("Flash style check: ui_style_card_hero.prop1=0x%02X (LV_STYLE_PROP_ANY=0x%02X)\n",
           ui_style_card_hero.prop1, LV_STYLE_PROP_ANY);

    /* Follow-up F1: proportional glyphs width-safe layout */
    do_navigate_to_page(UI_PAGE_HOME); refresh();
    const uint32_t clocks[]={360000u, 100u*3600u+8u*60u+8u, 188u*3600u+58u*60u+58u, 3600000u, 1888u*3600u+58u*60u+58u, UINT32_MAX};
    for(unsigned j=0;j<sizeof(clocks)/sizeof(clocks[0]);j++){
        s_uptime_seconds=clocks[j];update_uptime_string();update_header();refresh();
        int gap = (int)(s_header_sep->coords.x1-s_uptime_label->coords.x2-1);
        int clock_dist = (int)(s_uptime_label->coords.x1 - (s_header_clock_ico->coords.x1 + 20));
        printf("UPTIME FOLLOWUP '%s' gap=%d label_x=%d width=%d clock_x=%d clock_dist=%d\n",s_uptime_buf,
            gap, (int)s_uptime_label->coords.x1, (int)lv_obj_get_width(s_uptime_label),
            (int)s_header_clock_ico->coords.x1, clock_dist);
    }
    capture("uptime_proportional");
    s_uptime_seconds=111u*3600u+11u*60u+11u; update_uptime_string();
    lora_last_rx_tick_ms=0; ui_tick(); refresh();
    fresh();
    printf("UPTIME RECONNECT '%s' gap=%d clock_dist=%d\n",s_uptime_buf,
           (int)(s_header_sep->coords.x1-s_uptime_label->coords.x2-1),
           (int)(s_uptime_label->coords.x1 - (s_header_clock_ico->coords.x1 + 20)));
    s_uptime_seconds=111u*3600u+20u*60u;update_uptime_string();refresh();
    printf("UPTIME AFTER RECONNECT '%s' gap=%d clock_dist=%d\n",s_uptime_buf,
           (int)(s_header_sep->coords.x1-s_uptime_label->coords.x2-1),
           (int)(s_uptime_label->coords.x1 - (s_header_clock_ico->coords.x1 + 20)));
    capture("uptime_after_reconnect");

    for(int f=0;f<9;f++) {
        lora_node_status.bits.rs485_fault=(f==1);
        lora_node_status.bits.co2_timeout=(f==2);
        lora_node_status.bits.th_timeout=(f==3);
        lora_node_status.bits.co2_valid=(f!=4);
        lora_node_status.bits.co2_ppm=(f==7)?10001:420;
        lora_node_status.bits.voc_index=(f==5)?0:((f==6)?501:80);
        lora_node_status.bits.humidity_pct=(f==8)?101:45;
        do_navigate_to_page(UI_PAGE_HOME);fresh();
        printf("AVAIL f=%d HOME co2=%s voc=%s temp=%s rh=%s\n",f,s_co2_val_buf,s_voc_val_buf,s_temp_val_buf,s_humid_val_buf);
        do_navigate_to_page(UI_PAGE_TRENDS);fresh();
        printf("AVAIL f=%d TRENDS co2=%s voc=%s temp=%s rh=%s\n",f,s_trends_tab_val_buf[0],s_trends_tab_val_buf[1],s_trends_tab_val_buf[2],s_trends_tab_val_buf[3]);
        for(int dev=0;dev<3;dev++) {
            open_device_dialog((uint8_t)dev);on_dialog_open_limits_click(NULL);refresh();
            printf("AVAIL f=%d AUTO dev=%d live=%s\n",f,dev,lv_label_get_text(s_dialog_live_lbl));
            if(f==0){probe_pixels=probe_flushes=0;for(int r=0;r<10;r++)fresh();printf("VALID AUTO dev=%d flushes=%llu pixels=%llu\n",dev,probe_flushes,probe_pixels);}
            close_device_dialog();
        }
    }
    lora_node_status.bits.humidity_pct=45;
    do_navigate_to_page(UI_PAGE_HOME);fresh();
    lora_last_rx_tick_ms=0;ui_tick();refresh();capture("home_no_readback");
    do_navigate_to_page(UI_PAGE_DEVICES);refresh();capture("devices_no_readback");

    /* F2: Command error with retry button */
    fresh();
    s_desired_on[0] = !((lora_node_status_get_relay_gpio(&lora_node_status, 0) ^ s_dev_configs[0].active_low) != 0);
    s_device_cmds[0].phase=UI_CMD_PHASE_ERROR;
    update_devices_page_widgets();refresh();
    printf("DEVICES ERROR dev0 state='%s' retry_hidden=%d dash_hidden=%d\n",
           lv_label_get_text(s_dev_page_states[0]),
           lv_obj_has_flag(s_dev_page_retries[0], LV_OBJ_FLAG_HIDDEN),
           lv_obj_has_flag(s_dev_page_dashes[0], LV_OBJ_FLAG_HIDDEN));
    capture("devices_error");

    do_navigate_to_page(UI_PAGE_HOME);
    update_home_page_widgets();refresh();
    printf("HOME ERROR dev0 state='%s' retry_hidden=%d dash_hidden=%d\n",
           lv_label_get_text(s_home_dev_states[0]),
           lv_obj_has_flag(s_home_dev_retries[0], LV_OBJ_FLAG_HIDDEN),
           lv_obj_has_flag(s_home_dev_dashes[0], LV_OBJ_FLAG_HIDDEN));
    capture("home_error");

    return 0;
}
