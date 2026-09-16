/* =========================================================================
 * smoke_production.c
 *
 * Production smoke test compiling exported UI code without UI_TEST_HOOKS
 * Validates that all fonts, themes, auto rules, and symbols resolve cleanly.
 * ========================================================================= */

#include "ui.h"
#include "ui_types.h"
#include "ui_auto.h"
#include "lora_comm.h"
#include "lora_hub_link.h"
#include "lvgl.h"
#include <stdio.h>
#include <assert.h>

/* Provide extern wire globals required by the UI link contract */
lora_hub_cmd_t lora_hub_cmd;
lora_node_status_t lora_node_status;
volatile uint32_t lora_last_rx_tick_ms = 0;
volatile int8_t lora_last_rssi = 0;
volatile int8_t lora_last_snr = 0;
volatile uint32_t lora_rx_revision = 0;

static void dummy_flush_cb(lv_disp_drv_t *drv, const lv_area_t *area, lv_color_t *color_p)
{
    (void)area;
    (void)color_p;
    lv_disp_flush_ready(drv);
}

#define SMOKE_CHECK(cond) do { \
    if (!(cond)) { \
        fprintf(stderr, "[SMOKE ERROR] Check failed: %s (%s:%d)\n", #cond, __FILE__, __LINE__); \
        return 1; \
    } \
} while (0)

#include <stdlib.h>

int main(void)
{
    if (__argc > 1 && strcmp(__argv[1], "--fail-check") == 0) {
        printf("[SMOKE] Running negative test (deliberate failure detection)...\n");
        SMOKE_CHECK(1 == 2);
    }

    printf("[SMOKE] Starting production UI smoke test (UI_TEST_HOOKS disabled)...\n");

    lv_init();
    lv_tick_inc(100);

    /* Buffer allocation matching MCU baseline (800x10) */
    static lv_color_t buf[800 * 10];
    static lv_disp_draw_buf_t disp_buf;
    lv_disp_draw_buf_init(&disp_buf, buf, NULL, 800 * 10);

    static lv_disp_drv_t disp_drv;
    lv_disp_drv_init(&disp_drv);
    disp_drv.draw_buf = &disp_buf;
    disp_drv.hor_res = 800;
    disp_drv.ver_res = 480;
    disp_drv.flush_cb = dummy_flush_cb;
    lv_disp_drv_register(&disp_drv);

    /* Test pre-init staging */
    ui_hub_device_config_t cfg;
    ui_auto_get_defaults(0, UI_PRESET_FAN, &cfg);
    cfg.auto_on_thresh = 1200;
    cfg.auto_off_thresh = 900;
    cfg.mode = UI_MODE_AUTO;
    SMOKE_CHECK(ui_set_device_config(0, &cfg));

    /* Test UI initialization */
    ui_init();

    /* Outbound TX must be gated before first reconciliation */
    SMOKE_CHECK(!ui_is_tx_ready());

    /* Test history feeding */
    int32_t hist_vals[16] = {500, 520, 550, 600, 700, 800, 900, 1000, 1050, 1100, 1150, 1200, 1250, 1300, 1350, 1400};
    bool hist_valid[16];
    for (int i = 0; i < 16; i++) hist_valid[i] = true;
    ui_feed_history(UI_METRIC_CO2, hist_vals, hist_valid, 16, 720);

    /* Advance LVGL tick to match received packet timestamp */
    lv_tick_inc(900); /* Total tick = 1000 ms */

    /* Simulate fresh status packet arrival */
    lora_node_status.bits.co2_ppm = 1250;
    lora_node_status.bits.co2_valid = 1;
    lora_node_status_set_relay_gpio(&lora_node_status, 0, 0);
    lora_last_rx_tick_ms = lv_tick_get();
    lora_rx_revision = 1;

    /* Execute ticks */
    ui_tick();
    lv_timer_handler();

    /* After reconciliation, TX should be ready */
    SMOKE_CHECK(ui_is_tx_ready());

    /* Verify navigation */
    ui_navigate_to_page(UI_PAGE_TRENDS);
    lv_timer_handler();

    ui_navigate_to_page(UI_PAGE_DEVICES);
    lv_timer_handler();

    ui_navigate_to_page(UI_PAGE_HOME);
    lv_timer_handler();

    printf("[SMOKE] Production UI smoke test PASSED successfully.\n");
    return 0;
}
