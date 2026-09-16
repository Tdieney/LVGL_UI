/* Review-only diagnostics. Includes the unchanged implementation to inspect
 * private view state; never compile/export this file as firmware. */
#define main submitted_simulator_main
#include "../../../sim_pc/main.c"
#undef main
#include "../../../ui.c"

static DWORD WINAPI review_watchdog(LPVOID unused)
{
    (void)unused;
    Sleep(5000);
    fprintf(stderr, "REVIEW TIMEOUT: scenario did not finish within 5 seconds\n");
    ExitProcess(124);
    return 0;
}
static unsigned review_deleted;
static void review_delete_cb(lv_event_t *e) { (void)e; review_deleted++; }
static void review_click(int x, int y)
{
    s_mouse_x = (int16_t)x; s_mouse_y = (int16_t)y;
    s_mouse_pressed = true; advance_ui(60);
    s_mouse_pressed = false; advance_ui(60);
}
static lv_obj_t *review_label(lv_obj_t *root, const char *text)
{
    if (lv_obj_check_type(root, &lv_label_class) && strcmp(lv_label_get_text(root), text) == 0) return root;
    for (uint32_t i=0; i<lv_obj_get_child_cnt(root); i++) {
        lv_obj_t *found=review_label(lv_obj_get_child(root,i),text);
        if(found) return found;
    }
    return NULL;
}
int main(int argc, char **argv)
{
    setvbuf(stdout,NULL,_IONBF,0); setvbuf(stderr,NULL,_IONBF,0);
    if(argc<2) return 2;
    CloseHandle(CreateThread(NULL,0,review_watchdog,NULL,0,NULL));
    printf("CASE %s\n",argv[1]);
    if(strcmp(argv[1],"submitted-shots")==0)
        return run_scenario_shots("docs/reviews/implementation-r1");
    lvgl_setup(); ui_init();
    ui_snapshot_t snap; populate_snapshot_baseline(&snap);
    ui_update_snapshot(&snap); advance_ui(50);
    ui_set_relay_command_cb(test_relay_cmd_cb);
    ui_set_settings_command_cb(test_settings_cmd_cb);
    if(strcmp(argv[1],"pending")==0) {
        snap.devices[0].pending=true;
        ui_update_snapshot(&snap); print_heap();
        advance_ui(200); puts("pending rendered");
    } else if(strcmp(argv[1],"ack-race")==0) {
        ui_trigger_device_toggle(0);
        uint32_t req=s_last_cb_relay_req_id;
        bool target=s_last_cb_target_on;
        snap=*ui_get_snapshot(); snap.devices[0].on=target;
        ui_update_snapshot(&snap); ui_handle_command_result(req,true);
        printf("requested=%d reported-before-ACK=%d after-ACK=%d (must stay %d)\n",target,target,ui_get_snapshot()->devices[0].on,target);
    } else if(strcmp(argv[1],"snapshot-pending")==0) {
        ui_trigger_device_toggle(0); uint32_t req=s_last_cb_relay_req_id;
        snap.metrics[0].value++;
        ui_update_snapshot(&snap);
        printf("pending after ordinary telemetry=%d req=%u\n",ui_get_snapshot()->devices[0].pending,req);
        ui_handle_command_result(req,true);
        printf("state after matching ACK=%d target=%d\n",ui_get_snapshot()->devices[0].on,s_last_cb_target_on);
    } else if(strcmp(argv[1],"settings-reject")==0) {
        ui_navigate_to_page(UI_PAGE_DEVICES);
        ui_open_device_settings(1);
        lv_dropdown_set_selected(s_dialog_preset_dd,UI_PRESET_GENERIC);
        s_dialog_edit_mode=UI_MODE_MANUAL;
        ui_save_device_settings();
        printf("before ACK: preset=%u mode=%u message=%s req=%u\n",ui_get_snapshot()->devices[1].preset,ui_get_snapshot()->devices[1].mode,s_toast_buf,s_last_cb_settings_req_id);
        ui_handle_command_result(s_last_cb_settings_req_id,false);
        printf("after rejection: preset=%u mode=%u error=%d message=%s\n",ui_get_snapshot()->devices[1].preset,ui_get_snapshot()->devices[1].mode,ui_get_snapshot()->devices[1].error,s_toast_buf);
    } else if(strcmp(argv[1],"same-snapshot")==0) {
        lv_obj_add_event_cb(s_page_container,review_delete_cb,LV_EVENT_DELETE,NULL);
        s_flushed_pixels=0; ui_update_snapshot(&snap); advance_ui(50);
        printf("identical snapshot: page deletions=%u flushed pixels=%llu\n",review_deleted,(unsigned long long)s_flushed_pixels);
    } else if(strcmp(argv[1],"click")==0) {
        review_click(120,170);
        printf("tap CO2 value: page=%u expected Trends=1\n",s_current_page);
        ui_navigate_to_page(UI_PAGE_HOME); advance_ui(50);
        review_click(240,255);
        printf("tap CO2 background: page=%u expected Trends=1\n",s_current_page);
    } else if(strcmp(argv[1],"negative")==0) {
        snap.metrics[UI_METRIC_TEMP].value=-5; ui_update_snapshot(&snap);
        printf("Home -0.5 C => %s\n",s_temp_val_buf);
        static const int32_t v[]={-5,-15}; static const bool valid[]={true,true};
        ui_feed_history(UI_METRIC_TEMP,v,valid,2,14*60);
        ui_select_trend_metric(UI_METRIC_TEMP);
        printf("Trends current=%s min=%s max=%s\n",s_stat_cur_buf,s_stat_min_buf,s_stat_max_buf);
    } else if(strcmp(argv[1],"history")==0) {
        int32_t v[20]; bool valid[20];
        for(int i=0;i<20;i++){v[i]=100+i; valid[i]=true;}
        ui_feed_history(UI_METRIC_CO2,v,valid,20,857);
        printf("feed 20 samples: kept first=%ld last=%ld count=%u (latest is119)\n",(long)s_history[0][0].value,(long)s_history[0][15].value,s_history_count[0]);
        ui_feed_history(UI_METRIC_CO2,NULL,NULL,2,880);
        printf("null values: first=%ld valid=%d\n",(long)s_history[0][0].value,s_history[0][0].valid);
    } else if(strcmp(argv[1],"cancel-update")==0) {
        ui_open_device_settings(0);
        snap.connection=UI_CONN_DISCONNECTED; ui_update_snapshot(&snap);
        ui_close_device_settings(); advance_ui(50);
        printf("disconnected after cancel: old value 780 visible=%d\n",review_label(s_page_container,"780")!=NULL);
    } else if(strcmp(argv[1],"invalid-comfort")==0) {
        snap.metrics[UI_METRIC_TEMP].valid=false; ui_update_snapshot(&snap);
        lv_obj_t *temp=lv_obj_get_child(s_page_container,2);
        printf("invalid temperature: value=%s Comfortable visible=%d\n",s_temp_val_buf,review_label(temp,"Comfortable")!=NULL);
    } else if(strcmp(argv[1],"trends")==0) {
        feed_standard_history(); ui_navigate_to_page(UI_PAGE_TRENDS); advance_ui(100);
        save_framebuffer("docs/reviews/implementation-r1/review_trends.raw"); print_heap();
    } else if(strcmp(argv[1],"dialog")==0) {
        ui_navigate_to_page(UI_PAGE_DEVICES); advance_ui(100);
        save_framebuffer("docs/reviews/implementation-r1/review_devices.raw");
        ui_open_device_settings(1); advance_ui(100);
        save_framebuffer("docs/reviews/implementation-r1/review_dialog.raw"); print_heap();
    } else return 2;
    return 0;
}
