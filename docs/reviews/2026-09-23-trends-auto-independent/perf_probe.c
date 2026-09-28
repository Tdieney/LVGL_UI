/* Host-only review probe. Includes production UI without modifying it. */
#include "../../../ui.c"
#include <stdio.h>
#include <time.h>

static lv_color_t s_probe_draw[UI_DRAW_BUF_PIXELS];
static uint64_t s_probe_pixels;
static uint32_t s_probe_calls;
static uint32_t s_probe_max_area;

static void probe_flush(lv_disp_drv_t *drv, const lv_area_t *area, lv_color_t *pixels)
{
    (void)pixels;
    uint32_t p = (uint32_t)(area->x2 - area->x1 + 1) *
                 (uint32_t)(area->y2 - area->y1 + 1);
    s_probe_pixels += p;
    s_probe_calls++;
    if(p > s_probe_max_area) s_probe_max_area = p;
    lv_disp_flush_ready(drv);
}

static void render(void)
{
    lv_obj_update_layout(lv_scr_act());
    lv_refr_now(NULL);
}

static void clear_counts(void)
{
    s_probe_pixels = 0;
    s_probe_calls = 0;
    s_probe_max_area = 0;
}

static void measure_switch(ui_metric_id_t metric)
{
    clear_counts();
    clock_t before = clock();
    ui_select_trend_metric(metric);
    render();
    clock_t after = clock();
    lv_mem_monitor_t m;
    lv_mem_monitor(&m);
    printf("switch=%u pixels=%llu bytes=%llu flush=%u max_area=%u host_cpu_ms=%.2f heap_free=%u biggest=%u\n",
           (unsigned)metric, (unsigned long long)s_probe_pixels,
           (unsigned long long)(s_probe_pixels * 2u), s_probe_calls,
           s_probe_max_area, 1000.0 * (double)(after - before) / CLOCKS_PER_SEC,
           (unsigned)m.free_size, (unsigned)m.free_biggest_size);
}

int main(void)
{
    lv_init();
    static lv_disp_draw_buf_t b;
    lv_disp_draw_buf_init(&b, s_probe_draw, NULL, UI_DRAW_BUF_PIXELS);
    static lv_disp_drv_t d;
    lv_disp_drv_init(&d);
    d.hor_res = 800;
    d.ver_res = 480;
    d.draw_buf = &b;
    d.flush_cb = probe_flush;
    lv_disp_drv_register(&d);
    lv_tick_inc(1000);
    lora_node_status.bits.co2_ppm = 600;
    lora_node_status.bits.voc_index = 120;
    lora_node_status.bits.temp_deci_c = 235;
    lora_node_status.bits.humidity_pct = 46;
    lora_node_status.bits.co2_valid = 1;
    lora_node_status.bits.voc_valid = 1;
    lora_node_status.bits.temp_valid = 1;
    lora_node_status.bits.humid_valid = 1;
    lora_last_rx_tick_ms = lv_tick_get();
    lora_rx_revision = 1;
    ui_init();
    ui_tick();
    render();

    int32_t vals[UI_HISTORY_CAPACITY];
    bool valid[UI_HISTORY_CAPACITY];
    for(uint8_t metric = 0; metric < UI_METRIC_COUNT; metric++) {
        for(uint8_t j = 0; j < UI_HISTORY_CAPACITY; j++) {
            vals[j] = (metric == UI_METRIC_TEMP ? 210 : 70) + metric * 100 + j * 2;
            valid[j] = true;
        }
        ui_feed_history((ui_metric_id_t)metric, vals, valid, UI_HISTORY_CAPACITY, 600);
    }
    ui_navigate_to_page(UI_PAGE_TRENDS);
    lv_timer_handler();
    render();
    measure_switch(UI_METRIC_CO2);
    measure_switch(UI_METRIC_VOC);
    measure_switch(UI_METRIC_TEMP);
    measure_switch(UI_METRIC_HUMIDITY);
    measure_switch(UI_METRIC_CO2);
    measure_switch(UI_METRIC_CO2); /* Same-selection cost. */
    lv_mem_monitor_t before, after;
    lv_mem_monitor(&before);
    clear_counts();
    clock_t start = clock();
    for(int i = 0; i < 100; i++) {
        ui_select_trend_metric((ui_metric_id_t)(i % UI_METRIC_COUNT));
        render();
    }
    clock_t end = clock();
    lv_mem_monitor(&after);
    printf("100_switches pixels=%llu bytes=%llu flush=%u host_cpu_ms=%.2f heap_free_before=%u after=%u\n",
           (unsigned long long)s_probe_pixels,
           (unsigned long long)(s_probe_pixels * 2u), s_probe_calls,
           1000.0 * (double)(end - start) / CLOCKS_PER_SEC,
           (unsigned)before.free_size, (unsigned)after.free_size);

    /* Same-value ownership handoff still changes time semantics. */
    ui_select_trend_metric(UI_METRIC_CO2);
    s_history_externally_fed[UI_METRIC_CO2] = false;
    s_history_count[UI_METRIC_CO2] = 1;
    s_history[UI_METRIC_CO2][0].minute_of_day = 10;
    s_history[UI_METRIC_CO2][0].value = 600;
    s_history[UI_METRIC_CO2][0].valid = true;
    s_history_rev[UI_METRIC_CO2]++;
    update_trends_page_widgets();
    printf("owner_before axis=%s rev=%u\n", lv_label_get_text(s_trends_time_labels[0]),
           (unsigned)s_history_rev[UI_METRIC_CO2]);
    int32_t equal_value = 600;
    bool equal_valid = true;
    ui_feed_history(UI_METRIC_CO2, &equal_value, &equal_valid, 1, 10);
    update_trends_page_widgets();
    printf("owner_after axis=%s rev=%u external=%u\n", lv_label_get_text(s_trends_time_labels[0]),
           (unsigned)s_history_rev[UI_METRIC_CO2],
           (unsigned)s_history_externally_fed[UI_METRIC_CO2]);

    /* Session time wraps at one day although uptime continues. */
    s_history_externally_fed[UI_METRIC_CO2] = false;
    char elapsed_label[16];
    format_trend_time_label(elapsed_label, sizeof(elapsed_label), UI_METRIC_CO2,
                            (uint16_t)(1440u % 1440u));
    printf("elapsed_24h=%s\n", elapsed_label);

    /* Force a report after a long outage; slots should represent the last window. */
    s_history_count[UI_METRIC_CO2] = 1;
    s_history[UI_METRIC_CO2][0].minute_of_day = 0;
    s_history[UI_METRIC_CO2][0].value = 600;
    s_history[UI_METRIC_CO2][0].valid = true;
    s_auto_hist_started[UI_METRIC_CO2] = true;
    s_auto_hist_last_minute[UI_METRIC_CO2] = 0;
    s_uptime_seconds = 100u * 60u;
    lora_rx_revision++;
    lora_last_rx_tick_ms = lv_tick_get();
    ui_tick();
    printf("gap_100m count=%u first=%u second=%u penultimate=%u last=%u\n",
           (unsigned)s_history_count[UI_METRIC_CO2],
           (unsigned)s_history[UI_METRIC_CO2][0].minute_of_day,
           (unsigned)s_history[UI_METRIC_CO2][1].minute_of_day,
           (unsigned)s_history[UI_METRIC_CO2][14].minute_of_day,
           (unsigned)s_history[UI_METRIC_CO2][15].minute_of_day);
    return 0;
}
