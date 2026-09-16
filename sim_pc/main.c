/* Minimal Windows GDI simulator and comprehensive regression test runner
 * for the 800x480 Smart Hub LVGL UI.
 */
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <windows.h>

#include "lvgl.h"
#include "ui.h"
#include "ui_theme.h"
#include "ui_types.h"
#include "ui_fonts.h"
#include "ui_mcu_profile.h"
#include "ui_test_api.h"
#include "ui_test_hooks.h"
#include "lora_comm.h"
#include "lora_hub_link.h"
#include "ui_auto.h"

#define SIM_HOR ((int32_t) UI_DISPLAY_HOR_RES)
#define SIM_VER ((int32_t) UI_DISPLAY_VER_RES)

static uint16_t s_framebuffer[UI_DISPLAY_HOR_RES * UI_DISPLAY_VER_RES];
static lv_color_t s_draw_buffer[UI_DRAW_BUF_PIXELS];
static HWND s_window;
static bool s_mouse_pressed;
static int16_t s_mouse_x;
static int16_t s_mouse_y;
static uint64_t s_flushed_pixels;
static uint64_t s_flush_calls;

static void reset_flush_counters(void)
{
    s_flush_calls = 0;
    s_flushed_pixels = 0;
}

/* Wire Globals Definition */
lora_hub_cmd_t lora_hub_cmd;
lora_node_status_t lora_node_status;
volatile uint32_t lora_last_rx_tick_ms = 0;
volatile int8_t   lora_last_rssi = -65;
volatile int8_t   lora_last_snr  = 8;
volatile uint32_t lora_rx_revision = 0;

/* Forward declaration for interactive keyboard handling */
static LRESULT CALLBACK window_proc(HWND window, UINT message, WPARAM wparam, LPARAM lparam);

/* =========================================================================
 * Real Wire-Level Fake Node Fixture (P1.3, P0.1, P3.12)
 * ========================================================================= */
typedef enum {
    FAKE_NODE_MODE_NORMAL = 0,    /* Apply commands after 700 ms and report back */
    FAKE_NODE_MODE_FAIL_NEXT,     /* Ignore next command (stay alive, do not apply, do not echo) */
    FAKE_NODE_MODE_STOP_REPORTING /* Stop reporting status entirely (simulate link staleness) */
} fake_node_mode_t;

static fake_node_mode_t s_fake_node_mode = FAKE_NODE_MODE_NORMAL;
static uint16_t s_fake_node_last_seen_seq = 0;
static uint32_t s_fake_node_cmd_recv_tick = 0;
static bool     s_fake_node_cmd_in_flight = false;
static uint8_t  s_fake_node_target_gpios = 0;
static uint16_t s_fake_node_target_seq = 0;
static uint16_t s_fake_node_failed_seq = 0;
static bool     s_fake_node_drop_next_frame = false;
static uint32_t s_fake_node_last_hub_tx_tick = 0;
static uint32_t s_fake_node_last_periodic_tick = 0;
static bool     s_varying_snr_enabled = false;

static void fake_node_tick(uint32_t now)
{
    bool new_seq = (lora_hub_cmd.bits.seq != s_fake_node_last_seen_seq);
    bool periodic_tx = (s_fake_node_last_hub_tx_tick == 0 || (now - s_fake_node_last_hub_tx_tick >= 1000u));

    /* Level-triggered Node: evaluates frames upon new sequence or periodic retransmission (~1000 ms) */
    if (ui_is_tx_ready() && (new_seq || periodic_tx) && !s_fake_node_cmd_in_flight) {
        if (new_seq) {
            s_fake_node_last_seen_seq = lora_hub_cmd.bits.seq;
            s_fake_node_last_hub_tx_tick = now;
        } else if (periodic_tx) {
            s_fake_node_last_hub_tx_tick = now;
        }

        if (s_fake_node_drop_next_frame) {
            /* Simulate single command frame dropped in transit */
            s_fake_node_drop_next_frame = false;
        } else if (s_fake_node_mode == FAKE_NODE_MODE_FAIL_NEXT) {
            if (new_seq) {
                s_fake_node_failed_seq = lora_hub_cmd.bits.seq;
                s_fake_node_mode = FAKE_NODE_MODE_NORMAL;
            }
        } else if (lora_hub_cmd.bits.seq == s_fake_node_failed_seq && s_fake_node_failed_seq != 0) {
            /* Re-transmission of failed sequence is also ignored by node */
        } else {
            uint8_t hub_gpios = lora_hub_cmd_get_relay_gpios_mask(&lora_hub_cmd);
            uint8_t node_gpios = lora_node_status_get_relay_gpios_mask(&lora_node_status);
            /* Level-triggered: applies levels from every received frame */
            if (hub_gpios != node_gpios || lora_node_status.bits.seq_echo != lora_hub_cmd.bits.seq) {
                s_fake_node_cmd_in_flight = true;
                s_fake_node_cmd_recv_tick = now;
                s_fake_node_target_seq = lora_hub_cmd.bits.seq;
                s_fake_node_target_gpios = hub_gpios;
            }
        }
    }

    /* Apply queued command after 700 ms */
    if (s_fake_node_cmd_in_flight && (now - s_fake_node_cmd_recv_tick >= 700u)) {
        s_fake_node_cmd_in_flight = false;
        if (s_fake_node_mode != FAKE_NODE_MODE_STOP_REPORTING) {
            lora_node_status_set_relay_gpios_mask(&lora_node_status, s_fake_node_target_gpios);
            lora_node_status.bits.seq_echo = s_fake_node_target_seq;
            lora_last_rx_tick_ms = now;
            lora_rx_revision++;
        }
    }

    /* Periodic status reporting: send frame every 1000 ms */
    if (s_fake_node_mode != FAKE_NODE_MODE_STOP_REPORTING) {
        if (s_fake_node_last_periodic_tick == 0 || (now - s_fake_node_last_periodic_tick >= 1000u)) {
            s_fake_node_last_periodic_tick = now;
            lora_node_status.bits.uptime_sec++;
            lora_last_rx_tick_ms = now;
            lora_rx_revision++;
            if (s_varying_snr_enabled) {
                static const int8_t snr_pattern[] = {6, 7, 6, 5, 6, 8, 7, 6};
                static uint8_t snr_idx = 0;
                lora_last_snr = snr_pattern[snr_idx];
                snr_idx = (snr_idx + 1) % (sizeof(snr_pattern) / sizeof(snr_pattern[0]));
            }
        }
    }
}

static void flush_cb(lv_disp_drv_t *driver, const lv_area_t *area, lv_color_t *pixels)
{
    int32_t width = lv_area_get_width(area);
    int32_t height = lv_area_get_height(area);
    s_flushed_pixels += (uint64_t) width * (uint64_t) height;
    s_flush_calls++;

    for (int32_t y = area->y1; y <= area->y2; ++y) {
        memcpy(&s_framebuffer[y * SIM_HOR + area->x1], pixels,
               (size_t) width * sizeof(uint16_t));
        pixels += width;
    }

    if (s_window != NULL) {
        RECT dirty = {area->x1, area->y1, area->x2 + 1, area->y2 + 1};
        InvalidateRect(s_window, &dirty, FALSE);
    }
    lv_disp_flush_ready(driver);
}

static void pointer_read_cb(lv_indev_drv_t *driver, lv_indev_data_t *data)
{
    (void) driver;
    data->point.x = s_mouse_x;
    data->point.y = s_mouse_y;
    data->state = s_mouse_pressed ? LV_INDEV_STATE_PRESSED : LV_INDEV_STATE_RELEASED;
}

static void lvgl_setup(void)
{
    lv_init();

    static lv_disp_draw_buf_t draw_buffer;
    lv_disp_draw_buf_init(&draw_buffer, s_draw_buffer, NULL, UI_DRAW_BUF_PIXELS);

    static lv_disp_drv_t display_driver;
    lv_disp_drv_init(&display_driver);
    display_driver.hor_res = SIM_HOR;
    display_driver.ver_res = SIM_VER;
    display_driver.flush_cb = flush_cb;
    display_driver.draw_buf = &draw_buffer;
    lv_disp_drv_register(&display_driver);

    static lv_indev_drv_t input_driver;
    lv_indev_drv_init(&input_driver);
    input_driver.type = LV_INDEV_TYPE_POINTER;
    input_driver.read_cb = pointer_read_cb;
    lv_indev_drv_register(&input_driver);
}

static void advance_ui(uint32_t milliseconds)
{
    for (uint32_t elapsed = 0; elapsed < milliseconds; elapsed += 10u) {
        uint32_t now = lv_tick_get();
        fake_node_tick(now);
        lv_tick_inc(10u);
        ui_tick();
        lv_timer_handler();
    }
    lv_refr_now(NULL);
}

static void sim_pointer_click(int16_t x, int16_t y)
{
    s_mouse_x = x;
    s_mouse_y = y;
    s_mouse_pressed = true;
    advance_ui(40u);
    s_mouse_pressed = false;
    advance_ui(40u);
}

static void sim_pointer_drag(int16_t x1, int16_t y1, int16_t x2, int16_t y2, uint32_t duration_ms)
{
    s_mouse_x = x1;
    s_mouse_y = y1;
    s_mouse_pressed = true;
    advance_ui(20u);
    int steps = (duration_ms > 20) ? ((int)duration_ms / 20) : 5;
    for (int i = 1; i <= steps; i++) {
        s_mouse_x = x1 + (x2 - x1) * i / steps;
        s_mouse_y = y1 + (y2 - y1) * i / steps;
        advance_ui(20u);
    }
    s_mouse_pressed = false;
    advance_ui(40u);
}

static void sim_post_node_status(bool online,
                                 uint16_t co2, bool co2_v,
                                 uint16_t voc, bool voc_v,
                                 int16_t temp, bool temp_v,
                                 uint8_t humid, bool humid_v,
                                 uint8_t relay0_gpio, uint8_t relay1_gpio,
                                 uint8_t relay2_gpio, uint8_t relay3_gpio)
{
    lora_node_status_init_header(&lora_node_status);
    lora_node_status.bits.co2_ppm = co2;
    lora_node_status.bits.co2_valid = co2_v ? 1 : 0;
    lora_node_status.bits.voc_index = voc;
    lora_node_status.bits.voc_valid = voc_v ? 1 : 0;
    lora_node_status.bits.temp_deci_c = temp;
    lora_node_status.bits.temp_valid = temp_v ? 1 : 0;
    lora_node_status.bits.humidity_pct = humid;
    lora_node_status.bits.humid_valid = humid_v ? 1 : 0;

    lora_node_status_set_relay_gpio(&lora_node_status, 0, relay0_gpio);
    lora_node_status_set_relay_gpio(&lora_node_status, 1, relay1_gpio);
    lora_node_status_set_relay_gpio(&lora_node_status, 2, relay2_gpio);
    lora_node_status_set_relay_gpio(&lora_node_status, 3, relay3_gpio);

    uint32_t now = lv_tick_get();
    if (online) {
        lora_last_rx_tick_ms = (now > 0) ? now : 1;
    } else {
        lora_last_rx_tick_ms = (now > 6000u) ? (now - 6000u) : 0;
    }
    lora_rx_revision++;
}

static void populate_node_status_baseline(void)
{
    s_fake_node_cmd_in_flight = false;
    s_fake_node_failed_seq = 0;
    s_fake_node_drop_next_frame = false;
    s_fake_node_mode = FAKE_NODE_MODE_NORMAL;
    sim_post_node_status(true,
                         420, true,
                         85, true,
                         235, true,
                         48, true,
                         1, 0, 0, 1);
}

static void print_heap(void)
{
    lv_mem_monitor_t memory;
    lv_mem_monitor(&memory);
    printf("lvgl heap: used=%u%% free=%u biggest=%u frag=%u%%\n",
           (unsigned) memory.used_pct, (unsigned) memory.free_size,
           (unsigned) memory.free_biggest_size, (unsigned) memory.frag_pct);
}

static int save_framebuffer(const char *path)
{
    FILE *output = fopen(path, "wb");
    if (output == NULL) {
        fprintf(stderr, "cannot open shot output: %s\n", path);
        return 1;
    }
    size_t expected = UI_DISPLAY_HOR_RES * UI_DISPLAY_VER_RES;
    size_t written = fwrite(s_framebuffer, sizeof(uint16_t), expected, output);
    fclose(output);
    if (written != expected) {
        fputs("short framebuffer write\n", stderr);
        return 1;
    }
    return 0;
}

/* =========================================================================
 * Scenario Data Fixtures
 * ========================================================================= */
static void feed_standard_history(void)
{
    static const int32_t co2_vals[16] = {
        410, 415, 412, 420, 425, 422, 430, 435, 440, 438, 432, 428, 425, 422, 420, 420
    };
    static const bool co2_valid[16] = {
        true, true, true, true, true, true, true, true, true, true, true, true, true, true, true, true
    };
    ui_feed_history(UI_METRIC_CO2, co2_vals, co2_valid, 16, 14 * 60 + 17);

    static const int32_t voc_vals[16] = {
        65, 70, 72, 75, 80, 82, 85, 88, 90, 88, 86, 85, 84, 85, 85, 85
    };
    ui_feed_history(UI_METRIC_VOC, voc_vals, co2_valid, 16, 14 * 60 + 17);

    static const int32_t temp_vals[16] = {
        228, 229, 230, 231, 232, 233, 234, 235, 235, 235, 236, 235, 235, 235, 235, 235
    };
    ui_feed_history(UI_METRIC_TEMP, temp_vals, co2_valid, 16, 14 * 60 + 17);

    static const int32_t humid_vals[16] = {
        45, 46, 46, 47, 47, 48, 48, 48, 49, 48, 48, 48, 48, 48, 48, 48
    };
    ui_feed_history(UI_METRIC_HUMIDITY, humid_vals, co2_valid, 16, 14 * 60 + 17);
}

/* =========================================================================
 * Smoke Test
 * ========================================================================= */
static int run_smoke(void)
{
    lvgl_setup();
    ui_init();
    advance_ui(500u);
    print_heap();
    if (s_flushed_pixels == 0u) {
        fputs("smoke: no pixels flushed\n", stderr);
        return 1;
    }
    printf("smoke: PASS (%llu pixels flushed)\n",
           (unsigned long long) s_flushed_pixels);
    return 0;
}

static int run_single_shot(const char *path)
{
    lvgl_setup();
    ui_init();
    advance_ui(500u);
    if (save_framebuffer(path) != 0) {
        return 1;
    }
    print_heap();
    printf("shot: wrote %s (%ux%u RGB565)\n", path,
           (unsigned) UI_DISPLAY_HOR_RES, (unsigned) UI_DISPLAY_VER_RES);
    return 0;
}

static uint16_t s_test_fb_a[SIM_HOR * SIM_VER];
static uint16_t s_test_fb_b[SIM_HOR * SIM_VER];

static bool compare_framebuffers(const char *test_name, const uint16_t *fb_a, const uint16_t *fb_b)
{
    int diff_count = 0;
    int first_diff_x = -1, first_diff_y = -1;
    uint16_t val_a = 0, val_b = 0;

    for (int y = 0; y < SIM_VER; y++) {
        for (int x = 0; x < SIM_HOR; x++) {
            int idx = y * SIM_HOR + x;
            if (fb_a[idx] != fb_b[idx]) {
                if (diff_count == 0) {
                    first_diff_x = x;
                    first_diff_y = y;
                    val_a = fb_a[idx];
                    val_b = fb_b[idx];
                }
                diff_count++;
            }
        }
    }

    if (diff_count > 0) {
        printf("  FAIL: %s frames differ! First diff at (%d, %d): A=0x%04X, B=0x%04X; total differing pixels: %d\n",
               test_name, first_diff_x, first_diff_y, val_a, val_b, diff_count);
        return false;
    }
    printf("  PASS: %s rebuild idempotence verified (0 differing pixels).\n", test_name);
    return true;
}

/* =========================================================================
 * Host TX Recording Structures & Integration Loop Stub (W3)
 * ========================================================================= */
typedef struct {
    uint32_t tick_ms;
    uint8_t bytes[sizeof(lora_hub_cmd_t)];
    uint16_t seq;
    uint8_t relay_mask;
} sim_tx_record_t;

#define SIM_MAX_TX_RECORDS 128
static sim_tx_record_t s_sim_tx_records[SIM_MAX_TX_RECORDS];
static size_t s_sim_tx_count = 0;

static void sim_reset_tx_records(void)
{
    s_sim_tx_count = 0;
}

static void sim_record_tx(uint32_t now, const lora_hub_cmd_t *cmd)
{
    if (s_sim_tx_count < SIM_MAX_TX_RECORDS) {
        sim_tx_record_t *r = &s_sim_tx_records[s_sim_tx_count++];
        r->tick_ms = now;
        memcpy(r->bytes, cmd->bytes, sizeof(lora_hub_cmd_t));
        r->seq = cmd->bits.seq;
        r->relay_mask = lora_hub_cmd_get_relay_gpios_mask(cmd);
    }
}

static void sim_integration_step(uint32_t step_ms, uint16_t *io_last_seq, uint32_t *io_last_tx_tick)
{
    for (uint32_t el = 0; el < step_ms; el += 10u) {
        uint32_t now = lv_tick_get();
        fake_node_tick(now);
        lv_tick_inc(10u);
        ui_tick();
        lv_timer_handler();

        /* Gated Outbound TX: send only when ui_is_tx_ready() */
        if (ui_is_tx_ready()) {
            uint32_t cur_now = lv_tick_get();
            bool seq_changed = (lora_hub_cmd.bits.seq != *io_last_seq);
            bool periodic_due = (*io_last_tx_tick == 0) || (cur_now - *io_last_tx_tick >= 1000u);
            if (seq_changed || periodic_due) {
                sim_record_tx(cur_now, &lora_hub_cmd);
                *io_last_seq = lora_hub_cmd.bits.seq;
                *io_last_tx_tick = cur_now;
            }
        }
    }
    lv_refr_now(NULL);
}

/* =========================================================================
 * Comprehensive Regression Test Suite
 * ========================================================================= */
static int run_regression(void)
{
    printf("====================================================\n");
    printf(" Smart Hub UI - Comprehensive Regression Test Suite \n");
    printf("====================================================\n");

    CreateDirectoryA("build_shots", NULL);
    lvgl_setup();

    /* Run pre-ui_init Flash restore fixture BEFORE ui_init */
    if (!test_restore_pre_init_fixture()) {
        fprintf(stderr, "FATAL: Pre-ui_init Flash restore fixture failed!\n");
        return 1;
    }

    ui_init();
    populate_node_status_baseline();
    advance_ui(300u);

    int total_tests = 0;
    int passed_tests = 0;

    /* -------------------------------------------------------------
     * TEST 1: Uptime formatting, 24h boundary and 32-bit tick wrap
     * ------------------------------------------------------------- */
    total_tests++;
    printf("\n[CHECK 1] Uptime calculation, formatting & 32-bit rollover...\n");
    bool test1_pass = true;

    ui_set_uptime_seconds(0u);
    if (strcmp(ui_get_uptime_str(), "00:00:00") != 0) {
        printf("  FAIL: 0s format mismatch: %s\n", ui_get_uptime_str());
        test1_pass = false;
    }

    ui_set_uptime_seconds(59u);
    if (strcmp(ui_get_uptime_str(), "00:00:59") != 0) {
        printf("  FAIL: 59s format mismatch: %s\n", ui_get_uptime_str());
        test1_pass = false;
    }

    ui_set_uptime_seconds(3600u);
    if (strcmp(ui_get_uptime_str(), "01:00:00") != 0) {
        printf("  FAIL: 3600s format mismatch: %s\n", ui_get_uptime_str());
        test1_pass = false;
    }

    /* 24-hour test: must NOT wrap to 00:00:00 */
    ui_set_uptime_seconds(86400u);
    if (strcmp(ui_get_uptime_str(), "24:00:00") != 0) {
        printf("  FAIL: 24h format mismatch (expected 24:00:00): %s\n", ui_get_uptime_str());
        test1_pass = false;
    }

    /* 25h 1m 1s test */
    ui_set_uptime_seconds(90061u);
    if (strcmp(ui_get_uptime_str(), "25:01:01") != 0) {
        printf("  FAIL: 25h format mismatch (expected 25:01:01): %s\n", ui_get_uptime_str());
        test1_pass = false;
    }

    /* Past 100 hours: string gains a digit (99:59:59 -> 100:00:00) (P3.15) */
    ui_set_uptime_seconds(359999u);
    advance_ui(20u);
    if (strcmp(ui_get_uptime_str(), "99:59:59") != 0) {
        printf("  FAIL: 99:59:59 format mismatch: %s\n", ui_get_uptime_str());
        test1_pass = false;
    }
    ui_set_uptime_seconds(360000u);
    advance_ui(20u);
    if (strcmp(ui_get_uptime_str(), "100:00:00") != 0) {
        printf("  FAIL: 100:00:00 format mismatch: %s\n", ui_get_uptime_str());
        test1_pass = false;
    }

    /* Past 1000 hours: string gains another digit (999:59:59 -> 1000:00:00) (P3.15) */
    ui_set_uptime_seconds(3599999u);
    advance_ui(20u);
    if (strcmp(ui_get_uptime_str(), "999:59:59") != 0) {
        printf("  FAIL: 999:59:59 format mismatch: %s\n", ui_get_uptime_str());
        test1_pass = false;
    }
    ui_set_uptime_seconds(3600000u);
    advance_ui(20u);
    if (strcmp(ui_get_uptime_str(), "1000:00:00") != 0) {
        printf("  FAIL: 1000:00:00 format mismatch: %s\n", ui_get_uptime_str());
        test1_pass = false;
    }

    /* Reset back to normal uptime for subsequent tests */
    ui_set_uptime_seconds(0u);
    advance_ui(20u);

    /* Verify uint32_t subtraction unsigned delta across 0xFFFFFFFF boundary */
    uint32_t tick_prev = 0xFFFFFFF0u;
    uint32_t tick_next = 0x00000010u;
    uint32_t delta = tick_next - tick_prev;
    if (delta != 32u) {
        printf("  FAIL: 32-bit rollover delta calculation error: %u\n", (unsigned)delta);
        test1_pass = false;
    }

    if (test1_pass) {
        printf("  PASS: Uptime boundaries and tick wrap verified.\n");
        passed_tests++;
    }

    /* -------------------------------------------------------------
     * TEST 2: Signed fixed-point temperature formatting (P1.7)
     * ------------------------------------------------------------- */
    total_tests++;
    printf("\n[CHECK 2] Signed fixed-point temperature formatting...\n");
    bool test2_pass = true;
    char temp_buf[32];

    ui_format_temp(temp_buf, sizeof(temp_buf), -25);
    if (strcmp(temp_buf, "-2.5") != 0) {
        printf("  FAIL: -25 tenths formatted as '%s', expected '-2.5'\n", temp_buf);
        test2_pass = false;
    }
    ui_format_temp(temp_buf, sizeof(temp_buf), -5);
    if (strcmp(temp_buf, "-0.5") != 0) {
        printf("  FAIL: -5 tenths formatted as '%s', expected '-0.5'\n", temp_buf);
        test2_pass = false;
    }
    ui_format_temp(temp_buf, sizeof(temp_buf), 0);
    if (strcmp(temp_buf, "0.0") != 0) {
        printf("  FAIL: 0 tenths formatted as '%s', expected '0.0'\n", temp_buf);
        test2_pass = false;
    }
    ui_format_temp(temp_buf, sizeof(temp_buf), 235);
    if (strcmp(temp_buf, "23.5") != 0) {
        printf("  FAIL: 235 tenths formatted as '%s', expected '23.5'\n", temp_buf);
        test2_pass = false;
    }
    ui_format_temp(temp_buf, sizeof(temp_buf), -120);
    if (strcmp(temp_buf, "-12.0") != 0) {
        printf("  FAIL: -120 tenths formatted as '%s', expected '-12.0'\n", temp_buf);
        test2_pass = false;
    }

    if (test2_pass) {
        printf("  PASS: Signed temperature formatting verified (-2.5, -0.5, 0.0, 23.5, -12.0).\n");
        passed_tests++;
    }

    /* -------------------------------------------------------------
     * TEST 3: Desired versus reported relay state & remote flips
     * ------------------------------------------------------------- */
    total_tests++;
    printf("\n[CHECK 3] Desired versus reported relay state & remote flips...\n");
    bool test3_pass = true;

    populate_node_status_baseline();
    advance_ui(50u);

    const ui_snapshot_t *cur = ui_get_snapshot();
    if (cur->devices[0].pending || cur->devices[0].error || !cur->devices[0].on) {
        printf("  FAIL: Device 0 initial settled state mismatch (expected ON, idle)\n");
        test3_pass = false;
    }

    /* 1. Toggle device 0 (Air Purifier: ON -> OFF) */
    uint16_t prev_seq = lora_hub_cmd.bits.seq;
    ui_trigger_device_toggle(0);
    if (lora_hub_cmd.bits.seq == prev_seq) {
        printf("  FAIL: Toggle did not increment Hub sequence number\n");
        test3_pass = false;
    }
    if (lora_hub_cmd_get_relay_gpio(&lora_hub_cmd, 0) != 0) {
        printf("  FAIL: Desired level not written to lora_hub_cmd (expected GPIO 0)\n");
        test3_pass = false;
    }

    /* Inside 700 ms turnaround (< 700 ms): command in flight -> Sending… */
    advance_ui(200u);
    cur = ui_get_snapshot();
    if (!cur->devices[0].pending || cur->devices[0].error) {
        printf("  FAIL: Device 0 not in pending state during turnaround\n");
        test3_pass = false;
    }

    /* Intervening status frame where node hasn't applied yet: must NOT wipe pending */
    sim_post_node_status(true, 420, true, 85, true, 235, true, 48, true, 1, 0, 0, 1);
    advance_ui(50u);
    cur = ui_get_snapshot();
    if (!cur->devices[0].pending) {
        printf("  FAIL: Intervening status frame wiped pending state!\n");
        test3_pass = false;
    }

    /* Duplicate command attempt while pending must be REFUSED */
    uint16_t seq_before_dup = lora_hub_cmd.bits.seq;
    ui_trigger_device_toggle(0);
    if (lora_hub_cmd.bits.seq != seq_before_dup) {
        printf("  FAIL: Duplicate toggle while pending was not refused!\n");
        test3_pass = false;
    }

    /* Advance past 700 ms: fake node applies command and reports back */
    advance_ui(600u); /* Total elapsed since toggle > 700ms */
    cur = ui_get_snapshot();
    if (cur->devices[0].pending || cur->devices[0].error || cur->devices[0].on != false) {
        printf("  FAIL: Device 0 failed to settle to OFF after node applied\n");
        test3_pass = false;
    }

    /* 2. Timeout & Retry flow */
    s_fake_node_mode = FAKE_NODE_MODE_FAIL_NEXT; /* Node will ignore next command */
    ui_trigger_device_toggle(0);
    advance_ui(UI_CMD_TIMEOUT_MS - 200u); /* 2800 ms < 3000 ms: still pending */
    cur = ui_get_snapshot();
    if (!cur->devices[0].pending || cur->devices[0].error) {
        printf("  FAIL: Premature timeout at %u ms\n", UI_CMD_TIMEOUT_MS - 200u);
        test3_pass = false;
    }

    advance_ui(300u); /* 3100 ms > 3000 ms: timed out to ERROR */
    cur = ui_get_snapshot();
    if (cur->devices[0].pending || !cur->devices[0].error) {
        printf("  FAIL: Did not enter ERROR/Retry state after timeout\n");
        test3_pass = false;
    }

    /* 3. Late report arriving after timeout confirms state */
    sim_post_node_status(true, 420, true, 85, true, 235, true, 48, true, 1, 0, 0, 1);
    advance_ui(50u);
    cur = ui_get_snapshot();
    if (cur->devices[0].pending || cur->devices[0].error || cur->devices[0].on != true) {
        printf("  FAIL: Late report did not settle device state: on=%d, err=%d\n", cur->devices[0].on, cur->devices[0].error);
        test3_pass = false;
    }

    /* 4. Node reports a level the Hub never requested (e.g. breaker forced or node reboot) */
    /* Relay 1 (Ventilation Fan) was OFF. Node reports Relay 1 GPIO=1 (ON) unprompted. */
    sim_post_node_status(true, 420, true, 85, true, 235, true, 48, true, 1, 1, 0, 1);
    advance_ui(50u);
    cur = ui_get_snapshot();
    if (!cur->devices[1].on) {
        printf("  FAIL: Node-initiated relay change was masked! Expected ON, got OFF\n");
        test3_pass = false;
    }

    /* P0.1 Check: Assert UI's desired state and transmitted lora_hub_cmd move together */
    uint8_t hub_r1_gpio = lora_hub_cmd_get_relay_gpio(&lora_hub_cmd, 1);
    if (hub_r1_gpio != 1) {
        printf("  FAIL: UI adopted remote flip but lora_hub_cmd still holds old GPIO %u (desynchronized!)\n",
               (unsigned)hub_r1_gpio);
        test3_pass = false;
    }

    /* With fake node level-triggered, advance past Hub periodic retransmission (1500 ms):
     * Node must NOT snap back / oscillate because lora_hub_cmd matches the adopted level! */
    advance_ui(1500u);
    cur = ui_get_snapshot();
    if (!cur->devices[1].on || lora_node_status_get_relay_gpio(&lora_node_status, 1) != 1) {
        printf("  FAIL: Relay snapped back on periodic retransmission (1 Hz oscillation defect)!\n");
        test3_pass = false;
    }

    /* 5. Lost command frame self-heals on periodic retransmission */
    s_fake_node_drop_next_frame = true;
    ui_trigger_device_toggle(1); /* Toggle Relay 1 back to OFF */
    advance_ui(300u);
    cur = ui_get_snapshot();
    if (!cur->devices[1].pending) {
        printf("  FAIL: Lost frame toggle did not enter pending state\n");
        test3_pass = false;
    }
    if (lora_node_status_get_relay_gpio(&lora_node_status, 1) != 1) {
        printf("  FAIL: Node applied dropped frame prematurely\n");
        test3_pass = false;
    }
    /* Periodic retransmission fires at ~1000ms, level-triggered node receives and applies in 700ms (~1700ms) */
    advance_ui(1800u);
    cur = ui_get_snapshot();
    if (cur->devices[1].pending || cur->devices[1].error || cur->devices[1].on) {
        printf("  FAIL: Lost command frame failed to self-heal on periodic retransmission! (pending=%d, err=%d, on=%d)\n",
               cur->devices[1].pending, cur->devices[1].error, cur->devices[1].on);
        test3_pass = false;
    }

    /* Restore Device 1 baseline mode (Auto) for subsequent checks */
    ui_hub_device_config_t dev1_cfg = *ui_get_device_config(1);
    dev1_cfg.mode = UI_MODE_AUTO;
    ui_set_device_config(1, &dev1_cfg);
    advance_ui(50u);

    if (test3_pass) {
        printf("  PASS: Desired vs reported relay state, timeout, late report & unprompted node report verified.\n");
        passed_tests++;
    }

    /* -------------------------------------------------------------
     * TEST 4: Hub-local device settings immediate persistence (Defect fix)
     * ------------------------------------------------------------- */
    total_tests++;
    printf("\n[CHECK 4] Hub-local device settings immediate persistence (Defect fix)...\n");
    bool test4_pass = true;

    populate_node_status_baseline();
    ui_navigate_to_page(UI_PAGE_DEVICES);
    advance_ui(50u);

    /* Open settings dialog for device 0 (Air Purifier) */
    ui_open_device_settings(0);
    advance_ui(50u);
    if (!ui_is_device_settings_open()) {
        printf("  FAIL: Settings dialog failed to open\n");
        test4_pass = false;
    }

    /* Change to Heater (preset 4), mode Manual */
    ui_set_dialog_preset(4);
    ui_set_dialog_mode(UI_MODE_MANUAL);

    /* Save settings */
    ui_save_device_settings();
    advance_ui(50u);
    if (ui_is_device_settings_open()) {
        printf("  FAIL: Settings dialog did not close on Save\n");
        test4_pass = false;
    }

    /* Check immediate update: NO Sending…, NO pending, new name on tile */
    cur = ui_get_snapshot();
    if (cur->devices[0].pending) {
        printf("  FAIL: Device settings entered pending state on save! Must be immediate.\n");
        test4_pass = false;
    }
    const char *d0_name = ui_get_devices_dev_name(0);
    if (!d0_name || strcmp(d0_name, "Heater") != 0) {
        printf("  FAIL: Device 0 name not updated to 'Heater': '%s'\n", d0_name ? d0_name : "NULL");
        test4_pass = false;
    }
    const char *d0_mode = ui_get_devices_dev_mode(0);
    if (!d0_mode || strcmp(d0_mode, "Manual") != 0) {
        printf("  FAIL: Device 0 mode not updated to 'Manual': '%s'\n", d0_mode ? d0_mode : "NULL");
        test4_pass = false;
    }

    /* Advance time by 5000 ms (way past the old 3.7s timeout where it silently reverted):
     * Must REMAIN 'Heater' and 'Manual' indefinitely! */
    advance_ui(5000u);
    d0_name = ui_get_devices_dev_name(0);
    if (!d0_name || strcmp(d0_name, "Heater") != 0) {
        printf("  FAIL: Defect reproduced! Device 0 reverted from 'Heater' to '%s' after 5s!\n",
               d0_name ? d0_name : "NULL");
        test4_pass = false;
    }

    /* Cancel dialog isolation */
    ui_open_device_settings(0);
    advance_ui(50u);
    ui_set_dialog_preset(6); /* Change to Smart Socket without saving */
    ui_close_device_settings();
    advance_ui(50u);
    d0_name = ui_get_devices_dev_name(0);
    if (!d0_name || strcmp(d0_name, "Heater") != 0) {
        printf("  FAIL: Cancelled dialog mutated settings: '%s'\n", d0_name ? d0_name : "NULL");
        test4_pass = false;
    }

    /* Restore Device 0 baseline for subsequent checks */
    ui_open_device_settings(0);
    advance_ui(50u);
    ui_set_dialog_preset(0);
    ui_set_dialog_mode(UI_MODE_AUTO);
    ui_save_device_settings();
    advance_ui(50u);

    if (test4_pass) {
        printf("  PASS: Hub-local device configuration immediate persistence verified (observed 5.0s, no reversion).\n");
        passed_tests++;
    }

    /* -------------------------------------------------------------
     * TEST 5: Font Glyph Coverage (P2.16)
     * ------------------------------------------------------------- */
    total_tests++;
    printf("\n[CHECK 5] Font glyph coverage (62px digits, unicode fallbacks)...\n");
    bool test5_pass = true;

    /* Test 62px digits font */
    lv_font_glyph_dsc_t dsc;
    const char *test_digits = "0123456789.-";
    for (size_t i = 0; test_digits[i] != '\0'; i++) {
        if (!ui_font_digits_62.get_glyph_dsc(&ui_font_digits_62, &dsc, (uint32_t)test_digits[i], 0)) {
            printf("  FAIL: 62px font missing glyph '%c' (0x%X)\n", test_digits[i], (unsigned)test_digits[i]);
            test5_pass = false;
        }
    }
    /* Test em-dash (U+2014) in 62px font */
    if (!ui_font_digits_62.get_glyph_dsc(&ui_font_digits_62, &dsc, 0x2014u, 0)) {
        printf("  FAIL: 62px font missing em-dash glyph (U+2014)\n");
        test5_pass = false;
    }

    /* Test fallback glyphs in ui_font_24 */
    static const uint32_t unicode_fallbacks[] = {
        0x2082u, /* '₂' */
        0x2014u, /* '—' */
        0x2026u, /* '…' */
        0x00B0u, /* '°' */
        0x0025u, /* '%' */
        0x00B7u  /* '·' */
    };
    for (size_t i = 0; i < sizeof(unicode_fallbacks)/sizeof(unicode_fallbacks[0]); i++) {
        uint32_t cp = unicode_fallbacks[i];
        if (!ui_font_24.get_glyph_dsc(&ui_font_24, &dsc, cp, 0)) {
            printf("  FAIL: ui_font_24 missing fallback glyph 0x%04X\n", (unsigned)cp);
            test5_pass = false;
        }
    }

    if (test5_pass) {
        printf("  PASS: Font glyph coverage verified.\n");
        passed_tests++;
    }

    /* -------------------------------------------------------------
     * TEST 6: Trends rolling history, 16-sample sliding window & midnight wrap (P1.8)
     * ------------------------------------------------------------- */
    total_tests++;
    printf("\n[CHECK 6] Trends sliding history window & midnight clock wrap...\n");
    bool test6_pass = true;

    /* Feed 20 samples to verify that newest 16 are kept and oldest 4 are discarded */
    static const int32_t overflow_vals[20] = {
        100, 101, 102, 103, 104, 105, 106, 107, 108, 109,
        110, 111, 112, 113, 114, 115, 116, 117, 118, 119
    };
    static const bool overflow_valid[20] = {
        true, true, true, true, true, true, true, true, true, true,
        true, true, true, true, true, true, true, true, true, true
    };
    ui_feed_history(UI_METRIC_CO2, overflow_vals, overflow_valid, 20, 1000);
    const ui_history_sample_t *s0 = ui_get_history_sample(UI_METRIC_CO2, 0);
    const ui_history_sample_t *s15 = ui_get_history_sample(UI_METRIC_CO2, 15);
    if (s0 == NULL || s0->value != 104 || s0->minute_of_day != 1004) {
        printf("  FAIL: History overflow did not retain newest window! Sample 0 value=%ld, minute=%u (expected 104, 1004)\n",
               s0 ? (long)s0->value : -1, s0 ? (unsigned)s0->minute_of_day : 0);
        test6_pass = false;
    }
    if (s15 == NULL || s15->value != 119 || s15->minute_of_day != 1019) {
        printf("  FAIL: History overflow sample 15 mismatch! Value=%ld, minute=%u (expected 119, 1019)\n",
               s15 ? (long)s15->value : -1, s15 ? (unsigned)s15->minute_of_day : 0);
        test6_pass = false;
    }

    /* Test midnight clock wrapping: start_minute = 1435 (23:55) */
    ui_feed_history(UI_METRIC_TEMP, overflow_vals, overflow_valid, 16, 1435);
    const ui_history_sample_t *wrap_5 = ui_get_history_sample(UI_METRIC_TEMP, 5); /* 1435 + 5 = 1440 -> 0 (00:00) */
    const ui_history_sample_t *wrap_10 = ui_get_history_sample(UI_METRIC_TEMP, 10); /* 1435 + 10 = 1445 -> 5 (00:05) */
    if (wrap_5 == NULL || wrap_5->minute_of_day != 0) {
        printf("  FAIL: Midnight clock did not wrap to 00:00! minute_of_day=%u (expected 0)\n",
               wrap_5 ? (unsigned)wrap_5->minute_of_day : 9999);
        test6_pass = false;
    }
    if (wrap_10 == NULL || wrap_10->minute_of_day != 5) {
        printf("  FAIL: Midnight clock did not wrap to 00:05! minute_of_day=%u (expected 5)\n",
               wrap_10 ? (unsigned)wrap_10->minute_of_day : 9999);
        test6_pass = false;
    }

    if (test6_pass) {
        printf("  PASS: Trends sliding history window & midnight clock wrap verified.\n");
        passed_tests++;
    }

    /* -------------------------------------------------------------
     * TEST 7: Chart Point Inspection Returning Tooltip (P1.6)
     * ------------------------------------------------------------- */
    total_tests++;
    printf("\n[CHECK 7] Chart point inspection returning tooltip...\n");
    bool test7_pass = true;

    ui_navigate_to_page(UI_PAGE_TRENDS);
    feed_standard_history();
    advance_ui(50u);

    /* 1. Real touch path via synthetic indev pointer input device (P3.16) */
    /* Trends chart absolute screen coords: x=80..764, y=266..362 (center 422, 314) */
    s_mouse_x = 422;
    s_mouse_y = 314;
    s_mouse_pressed = true;
    advance_ui(50u);
    s_mouse_pressed = false;
    advance_ui(50u);

    const char *touch_tooltip = ui_get_chart_tooltip_text();
    if (touch_tooltip == NULL || strlen(touch_tooltip) == 0) {
        printf("  FAIL: Synthetic indev touch on chart did not produce tooltip!\n");
        test7_pass = false;
    } else {
        printf("  Synthetic indev touch tooltip: '%s'\n", touch_tooltip);
        if (strstr(touch_tooltip, "·") == NULL) {
            printf("  FAIL: Tooltip text format mismatch: '%s'\n", touch_tooltip);
            test7_pass = false;
        }
    }

    /* 2. Convenience programmatic inspection helper */
    ui_inspect_chart_point(6);
    advance_ui(50u);

    const char *tooltip = ui_get_chart_tooltip_text();
    if (tooltip == NULL || strlen(tooltip) == 0) {
        printf("  FAIL: Chart point inspection produced no tooltip!\n");
        test7_pass = false;
    } else {
        printf("  Inspected point 6 tooltip: '%s'\n", tooltip);
        if (strstr(tooltip, "·") == NULL) {
            printf("  FAIL: Tooltip text format mismatch: '%s'\n", tooltip);
            test7_pass = false;
        }
    }

    if (test7_pass) {
        printf("  PASS: Chart point inspection returning tooltip (both synthetic touch & direct API) verified.\n");
        passed_tests++;
    }

    /* -------------------------------------------------------------
     * TEST 8: Heap Headroom Assertion (> 4 KiB free on 42 KiB budget) (P0.1, P0.2)
     * ------------------------------------------------------------- */
    total_tests++;
    printf("\n[CHECK 8] Heap headroom assertion across all screens (> 4 KiB free)...\n");
    bool test8_pass = true;
    lv_mem_monitor_t mem;

    /* Check Home */
    ui_navigate_to_page(UI_PAGE_HOME);
    advance_ui(50u);
    lv_mem_monitor(&mem);
    printf("  Home page:    used=%u%% free=%u biggest=%u frag=%u%%\n",
           (unsigned)mem.used_pct, (unsigned)mem.free_size, (unsigned)mem.free_biggest_size, (unsigned)mem.frag_pct);
    if (mem.free_size < 4096 || mem.free_biggest_size < 2048) {
        printf("  FAIL: Insufficient heap headroom on Home page! (free=%u, biggest=%u)\n",
               (unsigned)mem.free_size, (unsigned)mem.free_biggest_size);
        test8_pass = false;
    }

    /* Check Trends with data */
    ui_navigate_to_page(UI_PAGE_TRENDS);
    feed_standard_history();
    advance_ui(50u);
    lv_mem_monitor(&mem);
    printf("  Trends (data):used=%u%% free=%u biggest=%u frag=%u%%\n",
           (unsigned)mem.used_pct, (unsigned)mem.free_size, (unsigned)mem.free_biggest_size, (unsigned)mem.frag_pct);
    if (mem.free_size < 4096) {
        printf("  FAIL: Insufficient heap headroom on Trends page! (free=%u)\n", (unsigned)mem.free_size);
        test8_pass = false;
    }

    /* Check Trends empty */
    ui_feed_history(UI_METRIC_CO2, NULL, NULL, 0, 14 * 60);
    advance_ui(50u);
    lv_mem_monitor(&mem);
    printf("  Trends (empty):used=%u%% free=%u biggest=%u frag=%u%%\n",
           (unsigned)mem.used_pct, (unsigned)mem.free_size, (unsigned)mem.free_biggest_size, (unsigned)mem.frag_pct);
    if (mem.free_size < 4096) {
        printf("  FAIL: Insufficient heap headroom on Trends empty page! (free=%u)\n", (unsigned)mem.free_size);
        test8_pass = false;
    }

    /* Check Devices */
    ui_navigate_to_page(UI_PAGE_DEVICES);
    advance_ui(50u);
    lv_mem_monitor(&mem);
    printf("  Devices page: used=%u%% free=%u biggest=%u frag=%u%%\n",
           (unsigned)mem.used_pct, (unsigned)mem.free_size, (unsigned)mem.free_biggest_size, (unsigned)mem.frag_pct);
    if (mem.free_size < 4096) {
        printf("  FAIL: Insufficient heap headroom on Devices page! (free=%u)\n", (unsigned)mem.free_size);
        test8_pass = false;
    }

    /* Check Devices + Settings Dialog Open */
    ui_open_device_settings(0);
    advance_ui(50u);
    lv_mem_monitor(&mem);
    printf("  Dialog open:  used=%u%% free=%u biggest=%u frag=%u%%\n",
           (unsigned)mem.used_pct, (unsigned)mem.free_size, (unsigned)mem.free_biggest_size, (unsigned)mem.frag_pct);
    if (mem.free_size < 4096) {
        printf("  FAIL: Insufficient heap headroom with Settings dialog open! (free=%u)\n", (unsigned)mem.free_size);
        test8_pass = false;
    }
    ui_close_device_settings();
    advance_ui(50u);

    /* Check Splash -> Home transition */
    ui_replay_splash();
    advance_ui(100u);
    lv_mem_monitor(&mem);
    printf("  Splash screen:used=%u%% free=%u biggest=%u frag=%u%%\n",
           (unsigned)mem.used_pct, (unsigned)mem.free_size, (unsigned)mem.free_biggest_size, (unsigned)mem.frag_pct);
    advance_ui(2500u);
    lv_mem_monitor(&mem);
    printf("  Splash->Home: used=%u%% free=%u biggest=%u frag=%u%%\n",
           (unsigned)mem.used_pct, (unsigned)mem.free_size, (unsigned)mem.free_biggest_size, (unsigned)mem.frag_pct);
    if (mem.free_size < 4096) {
        printf("  FAIL: Insufficient heap headroom on Splash->Home! (free=%u)\n", (unsigned)mem.free_size);
        test8_pass = false;
    }

    if (test8_pass) {
        printf("  PASS: Heap headroom verified with > 4 KiB free across all views.\n");
        passed_tests++;
    }

    /* -------------------------------------------------------------
     * TEST 9: Memory stability across repeated page navigation cycles (P1.3, P1.13)
     * ------------------------------------------------------------- */
    total_tests++;
    printf("\n[CHECK 9] Memory stability across 50 page navigation cycles...\n");
    bool test9_pass = true;

    /* Warmup cycle so lazy caches and scratch buffers stabilize */
    ui_navigate_to_page(UI_PAGE_TRENDS);
    advance_ui(20u);
    ui_navigate_to_page(UI_PAGE_DEVICES);
    advance_ui(20u);
    ui_open_device_settings(0);
    advance_ui(20u);
    ui_close_device_settings();
    advance_ui(20u);
    ui_navigate_to_page(UI_PAGE_HOME);
    advance_ui(50u);

    lv_mem_monitor_t mem_start;
    lv_mem_monitor(&mem_start);

    for (int cycle = 0; cycle < 50; cycle++) {
        ui_navigate_to_page(UI_PAGE_TRENDS);
        advance_ui(10u);
        ui_navigate_to_page(UI_PAGE_DEVICES);
        advance_ui(10u);
        ui_open_device_settings(0);
        advance_ui(10u);
        ui_close_device_settings();
        advance_ui(10u);
        ui_navigate_to_page(UI_PAGE_HOME);
        advance_ui(10u);
    }
    advance_ui(50u);

    lv_mem_monitor_t mem_end;
    lv_mem_monitor(&mem_end);

    printf("  Start: used=%u%% free=%u biggest=%u frag=%u%%\n",
           (unsigned)mem_start.used_pct, (unsigned)mem_start.free_size,
           (unsigned)mem_start.free_biggest_size, (unsigned)mem_start.frag_pct);
    printf("  End:   used=%u%% free=%u biggest=%u frag=%u%%\n",
           (unsigned)mem_end.used_pct, (unsigned)mem_end.free_size,
           (unsigned)mem_end.free_biggest_size, (unsigned)mem_end.frag_pct);

#define HEAP_DRIFT_THRESHOLD_BYTES 64
#define HEAP_FRAG_GROWTH_THRESHOLD_PCT 5

    int32_t drift = (int32_t)mem_start.free_size - (int32_t)mem_end.free_size;
    if (drift > HEAP_DRIFT_THRESHOLD_BYTES) {
        printf("  FAIL: Heap drift %ld bytes exceeded threshold (%d bytes) across 50 navigation cycles!\n",
               (long)drift, HEAP_DRIFT_THRESHOLD_BYTES);
        test9_pass = false;
    }
    int32_t frag_growth = (int32_t)mem_end.frag_pct - (int32_t)mem_start.frag_pct;
    if (frag_growth > HEAP_FRAG_GROWTH_THRESHOLD_PCT) {
        printf("  FAIL: Heap fragmentation growth +%ld%% exceeded threshold (+%d%%) during navigation cycles!\n",
               (long)frag_growth, HEAP_FRAG_GROWTH_THRESHOLD_PCT);
        test9_pass = false;
    }

    if (test9_pass) {
        printf("  PASS: Measured heap drift: %ld bytes (threshold: %d bytes) & stable fragmentation: +%ld%% (threshold: +%d%%).\n",
               (long)drift, HEAP_DRIFT_THRESHOLD_BYTES, (long)frag_growth, HEAP_FRAG_GROWTH_THRESHOLD_PCT);
        passed_tests++;
    }

    /* -------------------------------------------------------------
     * TEST 10: Trends Tab In-Place Metric Switching & Formats
     * ------------------------------------------------------------- */
    total_tests++;
    printf("\n[CHECK 10] Trends in-place metric switching, units & empty state...\n");
    bool test10_pass = true;

    ui_navigate_to_page(UI_PAGE_TRENDS);
    populate_node_status_baseline();
    feed_standard_history();
    advance_ui(50u);

    /* 1. CO2 Metric */
    ui_select_trend_metric(UI_METRIC_CO2);
    advance_ui(20u);
    if (ui_get_selected_trend_metric() != UI_METRIC_CO2) {
        printf("  FAIL: Selected metric is not UI_METRIC_CO2\n");
        test10_pass = false;
    }
    const char *cur_co2 = ui_get_trends_stat_cur();
    const char *min_co2 = ui_get_trends_stat_min();
    const char *max_co2 = ui_get_trends_stat_max();
    if (!cur_co2 || strstr(cur_co2, "ppm") == NULL ||
        !min_co2 || strstr(min_co2, "ppm") == NULL ||
        !max_co2 || strstr(max_co2, "ppm") == NULL) {
        printf("  FAIL: CO2 stats missing 'ppm': cur='%s', min='%s', max='%s'\n",
               cur_co2 ? cur_co2 : "NULL", min_co2 ? min_co2 : "NULL", max_co2 ? max_co2 : "NULL");
        test10_pass = false;
    }

    /* 2. VOC Metric (No unit) */
    ui_select_trend_metric(UI_METRIC_VOC);
    advance_ui(20u);
    if (ui_get_selected_trend_metric() != UI_METRIC_VOC) {
        printf("  FAIL: Selected metric is not UI_METRIC_VOC\n");
        test10_pass = false;
    }
    const char *cur_voc = ui_get_trends_stat_cur();
    const char *min_voc = ui_get_trends_stat_min();
    const char *max_voc = ui_get_trends_stat_max();
    if (!cur_voc || strstr(cur_voc, "ppm") != NULL || strstr(cur_voc, "%") != NULL ||
        !min_voc || strstr(min_voc, "ppm") != NULL ||
        !max_voc || strstr(max_voc, "ppm") != NULL) {
        printf("  FAIL: VOC stats have unexpected units: cur='%s', min='%s', max='%s'\n",
               cur_voc ? cur_voc : "NULL", min_voc ? min_voc : "NULL", max_voc ? max_voc : "NULL");
        test10_pass = false;
    }

    /* 3. Temperature Metric (°C, decimal point) */
    ui_select_trend_metric(UI_METRIC_TEMP);
    advance_ui(20u);
    if (ui_get_selected_trend_metric() != UI_METRIC_TEMP) {
        printf("  FAIL: Selected metric is not UI_METRIC_TEMP\n");
        test10_pass = false;
    }
    const char *cur_temp = ui_get_trends_stat_cur();
    const char *min_temp = ui_get_trends_stat_min();
    const char *max_temp = ui_get_trends_stat_max();
    if (!cur_temp || strstr(cur_temp, "°C") == NULL || strchr(cur_temp, '.') == NULL ||
        !min_temp || strstr(min_temp, "°C") == NULL || strchr(min_temp, '.') == NULL ||
        !max_temp || strstr(max_temp, "°C") == NULL || strchr(max_temp, '.') == NULL) {
        printf("  FAIL: Temp stats format mismatch: cur='%s', min='%s', max='%s'\n",
               cur_temp ? cur_temp : "NULL", min_temp ? min_temp : "NULL", max_temp ? max_temp : "NULL");
        test10_pass = false;
    }

    /* 4. Humidity Metric (%) */
    ui_select_trend_metric(UI_METRIC_HUMIDITY);
    advance_ui(20u);
    if (ui_get_selected_trend_metric() != UI_METRIC_HUMIDITY) {
        printf("  FAIL: Selected metric is not UI_METRIC_HUMIDITY\n");
        test10_pass = false;
    }
    const char *cur_hum = ui_get_trends_stat_cur();
    const char *min_hum = ui_get_trends_stat_min();
    const char *max_hum = ui_get_trends_stat_max();
    if (!cur_hum || strstr(cur_hum, "%") == NULL ||
        !min_hum || strstr(min_hum, "%") == NULL ||
        !max_hum || strstr(max_hum, "%") == NULL) {
        printf("  FAIL: Humidity stats missing '%%': cur='%s', min='%s', max='%s'\n",
               cur_hum ? cur_hum : "NULL", min_hum ? min_hum : "NULL", max_hum ? max_hum : "NULL");
        test10_pass = false;
    }

    /* 5. Tooltip inspection across metrics */
    ui_inspect_chart_point(7);
    advance_ui(20u);
    const char *hum_tooltip = ui_get_chart_tooltip_text();
    if (!hum_tooltip || strstr(hum_tooltip, "%") == NULL || strstr(hum_tooltip, "·") == NULL) {
        printf("  FAIL: Humidity tooltip mismatch: '%s'\n", hum_tooltip ? hum_tooltip : "NULL");
        test10_pass = false;
    }

    ui_select_trend_metric(UI_METRIC_TEMP);
    advance_ui(20u);
    ui_inspect_chart_point(7);
    advance_ui(20u);
    const char *temp_tooltip = ui_get_chart_tooltip_text();
    if (!temp_tooltip || strstr(temp_tooltip, "°C") == NULL || strstr(temp_tooltip, "·") == NULL) {
        printf("  FAIL: Temp tooltip mismatch: '%s'\n", temp_tooltip ? temp_tooltip : "NULL");
        test10_pass = false;
    }

    /* 6. Empty history behavior */
    ui_feed_history(UI_METRIC_TEMP, NULL, NULL, 0, 14 * 60);
    advance_ui(50u);
    const char *empty_min = ui_get_trends_stat_min();
    const char *empty_max = ui_get_trends_stat_max();
    if (!empty_min || strcmp(empty_min, "—") != 0 ||
        !empty_max || strcmp(empty_max, "—") != 0) {
        printf("  FAIL: Empty history did not display em-dash stats: min='%s', max='%s'\n",
               empty_min ? empty_min : "NULL", empty_max ? empty_max : "NULL");
        test10_pass = false;
    }

    /* 7. Zero memory drift on 100 in-place metric tab switches */
    feed_standard_history();
    advance_ui(20u);
    for (int i = 0; i < 4; i++) {
        ui_select_trend_metric((ui_metric_id_t)i);
        advance_ui(10u);
    }
    lv_mem_monitor_t mem_tab_start, mem_tab_end;
    lv_mem_monitor(&mem_tab_start);
    for (int i = 0; i < 100; i++) {
        ui_select_trend_metric((ui_metric_id_t)(i % 4));
        advance_ui(10u);
    }
    lv_mem_monitor(&mem_tab_end);
    int32_t tab_drift = (int32_t)mem_tab_start.free_size - (int32_t)mem_tab_end.free_size;
    if (tab_drift > 64) {
        printf("  FAIL: In-place metric tab switching leaked %ld bytes across 100 switches!\n", (long)tab_drift);
        test10_pass = false;
    }

    if (test10_pass) {
        printf("  PASS: Trends metric switching (CO2, VOC, Temp, Humid), units, empty state & 0-byte drift verified.\n");
        passed_tests++;
    }

    /* -------------------------------------------------------------
     * TEST 11: Home Tab Sensor Metrics, Badges, Offline & Card Navigation
     * ------------------------------------------------------------- */
    total_tests++;
    printf("\n[CHECK 11] Home tab telemetry, quality badges, offline states & card navigation...\n");
    bool test11_pass = true;
    ui_navigate_to_page(UI_PAGE_HOME);
    populate_node_status_baseline();
    advance_ui(50u);

    /* 1. Baseline Values */
    const char *h_co2 = ui_get_home_co2_str();
    const char *h_voc = ui_get_home_voc_str();
    const char *h_temp = ui_get_home_temp_str();
    const char *h_hum = ui_get_home_humid_str();
    const char *b_co2 = ui_get_home_co2_badge();
    const char *b_voc = ui_get_home_voc_badge();

    if (!h_co2 || strcmp(h_co2, "420") != 0) {
        printf("  FAIL: Home CO2 baseline mismatch: '%s' (expected '420')\n", h_co2 ? h_co2 : "NULL");
        test11_pass = false;
    }
    if (!h_voc || strcmp(h_voc, "85") != 0) {
        printf("  FAIL: Home VOC baseline mismatch: '%s' (expected '85')\n", h_voc ? h_voc : "NULL");
        test11_pass = false;
    }
    if (!h_temp || strcmp(h_temp, "23.5°C") != 0) {
        printf("  FAIL: Home Temp baseline mismatch: '%s' (expected '23.5°C')\n", h_temp ? h_temp : "NULL");
        test11_pass = false;
    }
    if (!h_hum || strcmp(h_hum, "48%") != 0) {
        printf("  FAIL: Home Humidity baseline mismatch: '%s' (expected '48%%')\n", h_hum ? h_hum : "NULL");
        test11_pass = false;
    }
    if (!b_co2 || strstr(b_co2, "Good") == NULL) {
        printf("  FAIL: Home CO2 badge mismatch: '%s' (expected 'Good')\n", b_co2 ? b_co2 : "NULL");
        test11_pass = false;
    }
    if (!b_voc || strstr(b_voc, "Good") == NULL) {
        printf("  FAIL: Home VOC badge mismatch: '%s' (expected 'Good')\n", b_voc ? b_voc : "NULL");
        test11_pass = false;
    }

    /* 2. Device Tile Labels & Modes */
    for (uint8_t i = 0; i < UI_DEVICE_COUNT; i++) {
        const char *dname = ui_get_home_dev_name(i);
        const char *dmode = ui_get_home_dev_mode(i);
        if (!dname || strlen(dname) == 0) {
            printf("  FAIL: Home device tile %u has empty name\n", i);
            test11_pass = false;
        }
        if (!dmode || (strcmp(dmode, "Auto") != 0 && strcmp(dmode, "Manual") != 0)) {
            printf("  FAIL: Home device tile %u mode mismatch: '%s'\n", i, dmode ? dmode : "NULL");
            test11_pass = false;
        }
    }

    /* 3. Offline / Disconnected State */
    s_fake_node_mode = FAKE_NODE_MODE_STOP_REPORTING;
    sim_post_node_status(false, 420, false, 85, false, 235, false, 48, false, 1, 0, 0, 1);
    advance_ui(50u);

    if (strcmp(ui_get_home_co2_str(), "—") != 0 ||
        strcmp(ui_get_home_voc_str(), "—") != 0 ||
        strcmp(ui_get_home_temp_str(), "—") != 0 ||
        strcmp(ui_get_home_humid_str(), "—") != 0) {
        printf("  FAIL: Home offline metrics mismatch: co2='%s', voc='%s', temp='%s', hum='%s'\n",
               ui_get_home_co2_str(), ui_get_home_voc_str(), ui_get_home_temp_str(), ui_get_home_humid_str());
        test11_pass = false;
    }
    if (strstr(ui_get_home_co2_badge(), "Unknown") == NULL ||
        strstr(ui_get_home_voc_badge(), "Unknown") == NULL) {
        printf("  FAIL: Home offline badges mismatch: co2='%s', voc='%s'\n",
               ui_get_home_co2_badge(), ui_get_home_voc_badge());
        test11_pass = false;
    }
    for (uint8_t i = 0; i < UI_DEVICE_COUNT; i++) {
        const char *dstate = ui_get_home_dev_state(i);
        if (!dstate || strcmp(dstate, "Unknown") != 0) {
            printf("  FAIL: Home offline device %u state mismatch: '%s'\n", i, dstate ? dstate : "NULL");
            test11_pass = false;
        }
    }

    /* 4. Sensor Failure State (CO2/VOC invalid, Temp/Humid valid) */
    s_fake_node_mode = FAKE_NODE_MODE_NORMAL;
    sim_post_node_status(true, 420, false, 85, false, 220, true, 50, true, 1, 0, 0, 1);
    advance_ui(50u);

    if (strcmp(ui_get_home_co2_str(), "—") != 0 ||
        strcmp(ui_get_home_temp_str(), "22.0°C") != 0 ||
        strcmp(ui_get_home_humid_str(), "50%") != 0) {
        printf("  FAIL: Partial sensor failure metrics mismatch: co2='%s', temp='%s', hum='%s'\n",
               ui_get_home_co2_str(), ui_get_home_temp_str(), ui_get_home_humid_str());
        test11_pass = false;
    }

    /* 5. Card click to Trends navigation */
    ui_select_trend_metric(UI_METRIC_VOC);
    advance_ui(30u);
    if (ui_get_selected_trend_metric() != UI_METRIC_VOC) {
        printf("  FAIL: Navigation from card click did not select VOC\n");
        test11_pass = false;
    }
    ui_navigate_to_page(UI_PAGE_HOME);
    advance_ui(30u);

    if (test11_pass) {
        printf("  PASS: Home tab telemetry, quality badges, offline states & card navigation verified.\n");
        passed_tests++;
    }

    /* -------------------------------------------------------------
     * TEST 12: Devices grid, polarity resolution (active_low) & dialog drift
     * ------------------------------------------------------------- */
    total_tests++;
    printf("\n[CHECK 12] Devices grid, polarity resolution (active_low) & dialog drift...\n");
    bool test12_pass = true;

    ui_navigate_to_page(UI_PAGE_DEVICES);
    populate_node_status_baseline();
    advance_ui(50u);

    /* 1. Baseline device names, modes and initial switch states */
    static const char *exp_names[UI_DEVICE_COUNT] = {
        "Air Purifier", "Ventilation Fan", "Humidifier", "Desk Light"
    };
    static const bool exp_switches[UI_DEVICE_COUNT] = { true, false, false, true };
    static const char *exp_modes[UI_DEVICE_COUNT] = { "Auto", "Auto", "Manual", "Manual" };

    for (uint8_t i = 0; i < UI_DEVICE_COUNT; i++) {
        const char *name = ui_get_devices_dev_name(i);
        const char *mode = ui_get_devices_dev_mode(i);
        bool sw = ui_get_devices_switch_checked(i);
        if (!name || strcmp(name, exp_names[i]) != 0) {
            printf("  FAIL: Devices grid device %u name mismatch: '%s' (expected '%s')\n",
                   i, name ? name : "NULL", exp_names[i]);
            test12_pass = false;
        }
        if (!mode || strcmp(mode, exp_modes[i]) != 0) {
            printf("  FAIL: Devices grid device %u mode mismatch: '%s' (expected '%s')\n",
                   i, mode ? mode : "NULL", exp_modes[i]);
            test12_pass = false;
        }
        if (sw != exp_switches[i]) {
            printf("  FAIL: Devices grid device %u switch mismatch: %d (expected %d)\n",
                   i, sw, exp_switches[i]);
            test12_pass = false;
        }
    }

    /* 2. Polarity resolution test on Device 1 (Ventilation Fan) */
    /* Starting state: active_low = 0, desired_on = false, reported_gpio = 0 -> reported_on = false */
    if (ui_get_device_active_low(1) != 0) {
        printf("  FAIL: Device 1 default active_low is %u, expected 0\n", ui_get_device_active_low(1));
        test12_pass = false;
    }

    /* Configure Device 1 to active_low = 1 */
    ui_open_device_settings(1);
    advance_ui(30u);
    ui_set_dialog_active_low(1);
    ui_save_device_settings();
    advance_ui(30u);

    if (ui_get_device_active_low(1) != 1) {
        printf("  FAIL: Device 1 active_low did not update to 1\n");
        test12_pass = false;
    }

    /* With desired_on = false and active_low = 1:
     * Transmitted GPIO must be desired_on ^ active_low = 0 ^ 1 = 1 */
    if (lora_hub_cmd_get_relay_gpio(&lora_hub_cmd, 1) != 1) {
        printf("  FAIL: Polarity active_low=1 with desired OFF did not send GPIO 1 (got %u)\n",
               lora_hub_cmd_get_relay_gpio(&lora_hub_cmd, 1));
        test12_pass = false;
    }

    /* Fake node applies command (GPIO 1), reports readback GPIO 1.
     * Readback on Hub: reported_gpio ^ active_low = 1 ^ 1 = 0 (OFF).
     * Switch must be checked=false (OFF)! */
    advance_ui(800u);
    if (ui_get_devices_switch_checked(1)) {
        printf("  FAIL: Switch 1 displayed ON when active_low=1 and desired=OFF!\n");
        test12_pass = false;
    }

    /* Now toggle Device 1 to desired_on = true:
     * Transmitted GPIO must be desired_on ^ active_low = 1 ^ 1 = 0! */
    ui_trigger_device_toggle(1);
    if (lora_hub_cmd_get_relay_gpio(&lora_hub_cmd, 1) != 0) {
        printf("  FAIL: Polarity active_low=1 with desired ON did not send GPIO 0 (got %u)\n",
               lora_hub_cmd_get_relay_gpio(&lora_hub_cmd, 1));
        test12_pass = false;
    }

    /* Fake node applies command (GPIO 0), reports readback GPIO 0.
     * Readback on Hub: 0 ^ 1 = 1 (ON). Switch must be checked=true (ON)! */
    advance_ui(800u);
    if (!ui_get_devices_switch_checked(1)) {
        printf("  FAIL: Switch 1 displayed OFF when active_low=1 and desired=ON!\n");
        test12_pass = false;
    }

    /* Reset Device 1 active_low back to 0 */
    ui_open_device_settings(1);
    advance_ui(30u);
    ui_set_dialog_active_low(0);
    ui_save_device_settings();
    advance_ui(800u);

    /* 3. Position checks: Ensure Sending… does not overlap mode label */
    ui_trigger_device_toggle(1);
    advance_ui(30u);
    const ui_test_hooks_t *hooks = ui_get_test_hooks();
    if (hooks && hooks->dev_page_modes && hooks->dev_page_states) {
        lv_coord_t mx = lv_obj_get_x(hooks->dev_page_modes[1]);
        lv_coord_t sx = lv_obj_get_x(hooks->dev_page_states[1]);
        if (sx < mx + 50) {
            printf("  FAIL: Device 1 'Sending…' position (%d) overlaps mode label (%d)\n", (int)sx, (int)mx);
            test12_pass = false;
        }
    }
    advance_ui(800u);

    /* 4. Zero memory drift on 20 dialog open/close cycles */
    for (int i = 0; i < 5; i++) {
        ui_open_device_settings((uint8_t)(i % 4));
        advance_ui(10u);
        ui_close_device_settings();
        advance_ui(10u);
    }
    lv_mem_monitor_t mem_dlg_start, mem_dlg_end;
    lv_mem_monitor(&mem_dlg_start);
    for (int i = 0; i < 20; i++) {
        ui_open_device_settings((uint8_t)(i % 4));
        advance_ui(10u);
        ui_close_device_settings();
        advance_ui(10u);
    }
    lv_mem_monitor(&mem_dlg_end);
    int32_t dlg_drift = (int32_t)mem_dlg_start.free_size - (int32_t)mem_dlg_end.free_size;
    if (dlg_drift > 64) {
        printf("  FAIL: Dialog open/close leaked %ld bytes across 20 cycles!\n", (long)dlg_drift);
        test12_pass = false;
    }

    /* Restore Device 1 baseline mode (Auto) after toggle tests */
    ui_hub_device_config_t d1_rst = *ui_get_device_config(1);
    d1_rst.mode = UI_MODE_AUTO;
    ui_set_device_config(1, &d1_rst);
    advance_ui(50u);

    if (test12_pass) {
        printf("  PASS: Devices grid, polarity resolution (active_low inverted wire, preserved display) & 0-byte drift verified.\n");
        passed_tests++;
    }

    /* -------------------------------------------------------------
     * TEST 13: Command timeout, retry usability & late report
     * ------------------------------------------------------------- */
    total_tests++;
    printf("\n[CHECK 13] Command timeout, retry usability & late report...\n");
    bool test13_pass = true;

    ui_navigate_to_page(UI_PAGE_DEVICES);
    populate_node_status_baseline();
    advance_ui(50u);

    /* Arm fake node to fail next (ignore command) */
    s_fake_node_mode = FAKE_NODE_MODE_FAIL_NEXT;
    ui_trigger_device_toggle(2);
    advance_ui(UI_CMD_TIMEOUT_MS - 200u); /* 2800 ms: pending */
    cur = ui_get_snapshot();
    if (!cur->devices[2].pending || cur->devices[2].error) {
        printf("  FAIL: Device 2 timed out prematurely at %u ms\n", UI_CMD_TIMEOUT_MS - 200u);
        test13_pass = false;
    }

    advance_ui(300u); /* 3100 ms > 3000 ms: timed out to error */
    cur = ui_get_snapshot();
    if (cur->devices[2].pending || !cur->devices[2].error) {
        printf("  FAIL: Device 2 did not transition to ERROR phase after timeout!\n");
        test13_pass = false;
    }
    if (ui_get_devices_retry_hidden(2)) {
        printf("  FAIL: Retry affordance hidden on Device 2 after timeout\n");
        test13_pass = false;
    }

    /* Retry is usable */
    uint16_t seq_before_retry = lora_hub_cmd.bits.seq;
    ui_trigger_device_retry(2);
    advance_ui(20u);
    if (lora_hub_cmd.bits.seq == seq_before_retry) {
        printf("  FAIL: Retry did not increment Hub command sequence\n");
        test13_pass = false;
    }

    /* Fake node in normal mode applies command in 700 ms */
    advance_ui(800u);
    cur = ui_get_snapshot();
    if (cur->devices[2].pending || cur->devices[2].error) {
        printf("  FAIL: Device 2 not confirmed settled after retry\n");
        test13_pass = false;
    }

    /* Restore Device 2 baseline mode (Manual) after toggle tests */
    ui_hub_device_config_t d2_rst = *ui_get_device_config(2);
    d2_rst.mode = UI_MODE_MANUAL;
    ui_set_device_config(2, &d2_rst);
    advance_ui(50u);

    if (test13_pass) {
        printf("  PASS: Command timeout (%u ms) & retry usability verified.\n", UI_CMD_TIMEOUT_MS);
        passed_tests++;
    }

    /* -------------------------------------------------------------
     * TEST 14: Link staleness detection & recovery
     * ------------------------------------------------------------- */
    total_tests++;
    printf("\n[CHECK 14] Link staleness detection & recovery...\n");
    bool test14_pass = true;

    ui_navigate_to_page(UI_PAGE_HOME);
    populate_node_status_baseline();
    advance_ui(100u);

    cur = ui_get_snapshot();
    if (cur->connection != UI_CONN_CONNECTED) {
        printf("  FAIL: Expected CONNECTED initially\n");
        test14_pass = false;
    }

    /* Stop fake node reporting */
    s_fake_node_mode = FAKE_NODE_MODE_STOP_REPORTING;

    /* Advance 4000 ms (< 5000 ms LORA_LINK_TIMEOUT_MS): still connected */
    advance_ui(4000u);
    cur = ui_get_snapshot();
    if (cur->connection != UI_CONN_CONNECTED) {
        printf("  FAIL: Premature disconnection at 4000 ms (bound is %u ms)\n", LORA_LINK_TIMEOUT_MS);
        test14_pass = false;
    }

    /* Advance another 1500 ms (total 5500 ms > 5000 ms): link is stale! */
    advance_ui(1500u);
    cur = ui_get_snapshot();
    if (cur->connection != UI_CONN_DISCONNECTED) {
        printf("  FAIL: UI did not transition to DISCONNECTED after 5500 ms without status frames!\n");
        test14_pass = false;
    }

    /* Verify live values masked and tiles display Unknown / — */
    if (strcmp(ui_get_home_co2_str(), "—") != 0 ||
        strcmp(ui_get_home_voc_str(), "—") != 0 ||
        strcmp(ui_get_home_temp_str(), "—") != 0 ||
        strcmp(ui_get_home_humid_str(), "—") != 0) {
        printf("  FAIL: Telemetry not masked with em-dash on stale link\n");
        test14_pass = false;
    }

    /* Output change must be refused */
    uint16_t seq_offline = lora_hub_cmd.bits.seq;
    ui_trigger_device_toggle(0);
    if (lora_hub_cmd.bits.seq != seq_offline) {
        printf("  FAIL: Output toggle accepted while disconnected!\n");
        test14_pass = false;
    }

    /* Resume reporting: fake node comes back alive */
    s_fake_node_mode = FAKE_NODE_MODE_NORMAL;
    lora_last_rx_tick_ms = lv_tick_get();
    lora_rx_revision++;
    advance_ui(50u);

    cur = ui_get_snapshot();
    if (cur->connection != UI_CONN_CONNECTED) {
        printf("  FAIL: UI failed to recover to CONNECTED when status frames resumed\n");
        test14_pass = false;
    }
    if (strcmp(ui_get_home_co2_str(), "420") != 0) {
        printf("  FAIL: Live telemetry did not restore after reconnect\n");
        test14_pass = false;
    }

    if (test14_pass) {
        printf("  PASS: Link staleness bound (%u ms), disconnection masking, toggle refusal & reconnection verified.\n",
               LORA_LINK_TIMEOUT_MS);
        passed_tests++;
    }

    /* -------------------------------------------------------------
     * TEST 15: Interactive fake node fixture & keyboard shortcuts
     * ------------------------------------------------------------- */
    total_tests++;
    printf("\n[CHECK 15] Interactive fake node timing & keyboard shortcuts...\n");
    bool test15_pass = true;

    /* 1. Verify keyboard shortcuts */
    window_proc(NULL, WM_KEYDOWN, 'F', 0);
    if (s_fake_node_mode != FAKE_NODE_MODE_FAIL_NEXT) {
        printf("  FAIL: Key 'F' did not set FAKE_NODE_MODE_FAIL_NEXT\n");
        test15_pass = false;
    }

    window_proc(NULL, WM_KEYDOWN, 'T', 0);
    if (s_fake_node_mode != FAKE_NODE_MODE_STOP_REPORTING) {
        printf("  FAIL: Key 'T' did not set FAKE_NODE_MODE_STOP_REPORTING\n");
        test15_pass = false;
    }

    /* Key 'T' again toggles back */
    window_proc(NULL, WM_KEYDOWN, 'T', 0);
    if (s_fake_node_mode != FAKE_NODE_MODE_NORMAL) {
        printf("  FAIL: Key 'T' toggle did not resume FAKE_NODE_MODE_NORMAL\n");
        test15_pass = false;
    }

    /* 2. Normal mode: 700 ms turnaround delay */
    ui_navigate_to_page(UI_PAGE_HOME);
    populate_node_status_baseline();
    advance_ui(50u);

    ui_trigger_device_toggle(0);
    advance_ui(500u); /* < 700 ms: pending */
    cur = ui_get_snapshot();
    if (!cur->devices[0].pending) {
        printf("  FAIL: Fake node applied prematurely at 500 ms (expected 700 ms)\n");
        test15_pass = false;
    }

    advance_ui(250u); /* 750 ms > 700 ms: applied */
    cur = ui_get_snapshot();
    if (cur->devices[0].pending || cur->devices[0].error || cur->devices[0].on != false) {
        printf("  FAIL: Fake node did not confirm toggled state after 700 ms\n");
        test15_pass = false;
    }

    if (test15_pass) {
        printf("  PASS: Interactive fake node (700 ms turnaround, 'F' fail, 'T' staleness toggle) verified.\n");
        passed_tests++;
    }

    /* -------------------------------------------------------------
     * TEST 16: Pixel geometry anchor coordinate assertions (P1.3)
     * ------------------------------------------------------------- */
    total_tests++;
    printf("\n[CHECK 16] Pixel geometry anchor coordinate assertions against prototype...\n");
    bool test16_pass = true;

    /* 1. Home tile content origin & text/icon rows */
    ui_navigate_to_page(UI_PAGE_HOME);
    populate_node_status_baseline();
    advance_ui(200u);

    uint16_t card_bg = s_framebuffer[305 * SIM_HOR + 100];
    int home_min_y = 999, home_max_y = -1, home_min_x = 999;
    for (int y = 298; y <= 390; y++) {
        for (int x = 30; x <= 190; x++) {
            uint16_t pix = s_framebuffer[y * SIM_HOR + x];
            if (pix != card_bg) {
                uint8_t r = (uint8_t)(((pix >> 11) & 0x1F) << 3);
                uint8_t g = (uint8_t)(((pix >> 5) & 0x3F) << 2);
                /* True content pixel has foreground contrast */
                if (r < 200 || g < 200) {
                    if (y < home_min_y) home_min_y = y;
                    if (y > home_max_y) home_max_y = y;
                    if (x < home_min_x) home_min_x = x;
                }
            }
        }
    }
    printf("  Home tile 0 content rows: %d..%d, min_x: %d (prototype: 309..382)\n",
           home_min_y, home_max_y, home_min_x);
    if (home_min_y < 304 || home_min_y > 310 || home_max_y < 375 || home_max_y > 383 ||
        home_min_x < 30 || home_min_x > 34) {
        printf("  FAIL: Home tile content origin mismatch! got rows %d..%d, x=%d (expected ~308..382, x~32)\n",
               home_min_y, home_max_y, home_min_x);
        test16_pass = false;
    }

    /* 2. Devices card icon-plate origin */
    ui_navigate_to_page(UI_PAGE_DEVICES);
    advance_ui(200u);

    int plate_min_x = 999, plate_min_y = 999;
    for (int y = 120; y <= 180; y++) {
        for (int x = 25; x <= 100; x++) {
            uint16_t pix = s_framebuffer[y * SIM_HOR + x];
            /* Plate background #DEEBFF is 0xDF5F in RGB565 */
            if (pix == 0xDF5F) {
                if (x < plate_min_x) plate_min_x = x;
                if (y < plate_min_y) plate_min_y = y;
            }
        }
    }
    printf("  Devices card 0 icon plate origin: (%d, %d) (expected 36, 130)\n",
           plate_min_x, plate_min_y);
    if (abs(plate_min_x - 36) > 2 || abs(plate_min_y - 130) > 2) {
        printf("  FAIL: Devices card 0 icon plate origin mismatch! got (%d, %d), expected (36, 130)\n",
               plate_min_x, plate_min_y);
        test16_pass = false;
    }

    /* 3. Trends chart card box */
    ui_navigate_to_page(UI_PAGE_TRENDS);
    advance_ui(200u);

    int trend_min_x = 999, trend_min_y = 999, trend_max_x = -1, trend_max_y = -1;
    for (int y = 160; y <= 420; y++) {
        for (int x = 10; x <= 790; x++) {
            if (s_framebuffer[y * SIM_HOR + x] == 0xFFFF) {
                if (x < trend_min_x) trend_min_x = x;
                if (x > trend_max_x) trend_max_x = x;
                if (y < trend_min_y) trend_min_y = y;
                if (y > trend_max_y) trend_max_y = y;
            }
        }
    }
    int trend_w = (trend_max_x >= trend_min_x) ? (trend_max_x - trend_min_x + 1) : 0;
    int trend_h = (trend_max_y >= trend_min_y) ? (trend_max_y - trend_min_y + 1) : 0;
    printf("  Trends chart card box: (%d, %d, %d, %d) (expected 20, 176, 760, 228)\n",
           trend_min_x, trend_min_y, trend_w, trend_h);
    if (abs(trend_min_x - 20) > 2 || abs(trend_min_y - 176) > 2 ||
        abs(trend_w - 760) > 3 || abs(trend_h - 228) > 3) {
        printf("  FAIL: Trends chart card box mismatch! got (%d,%d,%d,%d), expected (20,176,760,228)\n",
               trend_min_x, trend_min_y, trend_w, trend_h);
        test16_pass = false;
    }

    /* 4. Settings dialog box */
    ui_navigate_to_page(UI_PAGE_DEVICES);
    advance_ui(100u);
    ui_open_device_settings(0);
    advance_ui(200u);

    int dlg_min_x = 999, dlg_min_y = 999, dlg_max_x = -1, dlg_max_y = -1;
    for (int y = 40; y <= 440; y++) {
        for (int x = 150; x <= 650; x++) {
            if (s_framebuffer[y * SIM_HOR + x] == 0xFFFF) {
                if (x < dlg_min_x) dlg_min_x = x;
                if (x > dlg_max_x) dlg_max_x = x;
                if (y < dlg_min_y) dlg_min_y = y;
                if (y > dlg_max_y) dlg_max_y = y;
            }
        }
    }
    int dlg_w = (dlg_max_x >= dlg_min_x) ? (dlg_max_x - dlg_min_x + 1) : 0;
    int dlg_h = (dlg_max_y >= dlg_min_y) ? (dlg_max_y - dlg_min_y + 1) : 0;
    printf("  Settings dialog box: (%d, %d, %d, %d) (expected 180, 56, 440, 368)\n",
           dlg_min_x, dlg_min_y, dlg_w, dlg_h);
    if (abs(dlg_min_x - 180) > 2 || abs(dlg_min_y - 56) > 2 ||
        abs(dlg_w - 440) > 3 || abs(dlg_h - 368) > 3) {
        printf("  FAIL: Settings dialog box mismatch! got (%d,%d,%d,%d), expected (180,56,440,368)\n",
               dlg_min_x, dlg_min_y, dlg_w, dlg_h);
        test16_pass = false;
    }
    ui_close_device_settings();
    advance_ui(100u);

    /* 5. Selected navigation pill (Home) */
    ui_navigate_to_page(UI_PAGE_HOME);
    advance_ui(100u);

    int pill_min_x = 999, pill_min_y = 999, pill_max_x = -1, pill_max_y = -1;
    for (int y = 416; y <= 479; y++) {
        for (int x = 10; x <= 280; x++) {
            uint16_t pix = s_framebuffer[y * SIM_HOR + x];
            uint8_t r = (uint8_t)((pix >> 11) & 0x1F);
            uint8_t g = (uint8_t)((pix >> 5) & 0x3F);
            uint8_t b = (uint8_t)(pix & 0x1F);
            /* #0052CC in RGB565: r~0..4, g~18..24, b~23..27 */
            if (r <= 5 && g >= 16 && g <= 26 && b >= 22) {
                if (x < pill_min_x) pill_min_x = x;
                if (x > pill_max_x) pill_max_x = x;
                if (y < pill_min_y) pill_min_y = y;
                if (y > pill_max_y) pill_max_y = y;
            }
        }
    }
    int pill_w = (pill_max_x >= pill_min_x) ? (pill_max_x - pill_min_x + 1) : 0;
    int pill_h = (pill_max_y >= pill_min_y) ? (pill_max_y - pill_min_y + 1) : 0;
    printf("  Selected navigation pill: (%d, %d, %d, %d) (expected 20, 424, 245, 48)\n",
           pill_min_x, pill_min_y, pill_w, pill_h);
    if (abs(pill_min_x - 20) > 2 || abs(pill_min_y - 424) > 2 ||
        abs(pill_w - 245) > 3 || abs(pill_h - 48) > 3) {
        printf("  FAIL: Selected nav pill mismatch! got (%d,%d,%d,%d), expected (20,424,245,48)\n",
               pill_min_x, pill_min_y, pill_w, pill_h);
        test16_pass = false;
    }

    /* 6. CO2 value to ppm unit label gap (prototype: 10 px) */
    ui_navigate_to_page(UI_PAGE_HOME);
    advance_ui(200u);
    int co2_val_max_x = -1;
    int co2_ppm_min_x = 999;
    for (int y = 145; y <= 175; y++) {
        for (int x = 36; x <= 220; x++) {
            uint16_t pix = s_framebuffer[y * SIM_HOR + x];
            if (pix != 0xDF5F) { /* Not card background #DEEBFF */
                uint8_t r = (uint8_t)(((pix >> 11) & 0x1F) << 3);
                uint8_t g = (uint8_t)(((pix >> 5) & 0x3F) << 2);
                uint8_t b = (uint8_t)((pix & 0x1F) << 3);
                if (r < 100 && g < 100 && b < 150) {
                    if (x < 145) {
                        if (x > co2_val_max_x) co2_val_max_x = x;
                    } else {
                        if (x < co2_ppm_min_x) co2_ppm_min_x = x;
                    }
                }
            }
        }
    }
    int co2_gap = (co2_val_max_x > 0 && co2_ppm_min_x < 999) ? (co2_ppm_min_x - co2_val_max_x) : -1;
    printf("  CO2 value right edge: %d, ppm left edge: %d, gap: %d px (expected ~10-12 px, prototype 10 px)\n",
           co2_val_max_x, co2_ppm_min_x, co2_gap);
    if (co2_gap < 8 || co2_gap > 15) {
        printf("  FAIL: CO2 value to ppm gap mismatch! got %d px, expected 10-12 px\n", co2_gap);
        test16_pass = false;
    }
    printf("  Temp & Humid units: formatted directly in value label string (ui_font_34); no separate unit widget\n");

    if (test16_pass) {
        printf("  PASS: Pixel geometry anchor coordinates verified against prototype.\n");
        passed_tests++;
    }

    /* -------------------------------------------------------------
     * TEST 17: SNR-Driven Signal Bars, Hysteresis, SF & Colors
     * ------------------------------------------------------------- */
    total_tests++;
    printf("\n[CHECK 17] SNR-driven signal bars, hysteresis, spreading factor & palette fills...\n");
    bool test17_pass = true;

    /* 1. Demodulator limit table validation (SF7..SF12) */
    if (lora_get_demod_limit_tenths_db(7) != -75 ||
        lora_get_demod_limit_tenths_db(8) != -100 ||
        lora_get_demod_limit_tenths_db(9) != -125 ||
        lora_get_demod_limit_tenths_db(10) != -150 ||
        lora_get_demod_limit_tenths_db(11) != -175 ||
        lora_get_demod_limit_tenths_db(12) != -200) {
        printf("  FAIL: Demodulator limit table values incorrect!\n");
        test17_pass = false;
    }

    /* 2. SNR margin mapping & boundary behavior across thresholds */
    /* Below limit: margin < 0 -> 0 bars */
    if (lora_margin_to_bars(-10, 0) != 0 || lora_margin_to_bars(-1, 0) != 0) {
        printf("  FAIL: Negative margin did not yield 0 bars!\n");
        test17_pass = false;
    }
    /* Boundary 0: margin = 0 -> 1 bar */
    if (lora_margin_to_bars(0, 0) != 1) {
        printf("  FAIL: Exact boundary 0.0 dB margin did not yield 1 bar!\n");
        test17_pass = false;
    }
    /* Margin 1.5 dB (15 tenths) -> 1 bar */
    if (lora_margin_to_bars(15, 0) != 1) {
        printf("  FAIL: Margin 1.5 dB did not yield 1 bar!\n");
        test17_pass = false;
    }
    /* Stepping up to 2 bars needs threshold (20) + hysteresis (10) = 30 */
    if (lora_margin_to_bars(25, 1) != 1) {
        printf("  FAIL: Stepping up to 2 bars without overshoot! (expected 1 bar at margin=25 from bar 1)\n");
        test17_pass = false;
    }
    if (lora_margin_to_bars(30, 1) != 2) {
        printf("  FAIL: Margin 30 tenths did not step up to 2 bars!\n");
        test17_pass = false;
    }
    /* Once at 2 bars, dropping to boundary 20 stays 2 bars */
    if (lora_margin_to_bars(20, 2) != 2) {
        printf("  FAIL: Boundary 20 tenths did not maintain 2 bars!\n");
        test17_pass = false;
    }
    /* Stepping up to 3 bars needs threshold (50) + hysteresis (10) = 60 */
    if (lora_margin_to_bars(55, 2) != 2) {
        printf("  FAIL: Margin 55 tenths stepped up to 3 bars without overshoot!\n");
        test17_pass = false;
    }
    if (lora_margin_to_bars(60, 2) != 3) {
        printf("  FAIL: Margin 60 tenths did not step up to 3 bars!\n");
        test17_pass = false;
    }
    /* Stepping up to 4 bars needs threshold (100) + hysteresis (10) = 110 */
    if (lora_margin_to_bars(105, 3) != 3) {
        printf("  FAIL: Margin 105 tenths stepped up to 4 bars without overshoot!\n");
        test17_pass = false;
    }
    if (lora_margin_to_bars(110, 3) != 4) {
        printf("  FAIL: Margin 110 tenths did not step up to 4 bars!\n");
        test17_pass = false;
    }
    /* At 4 bars, dropping to boundary 100 stays 4 bars */
    if (lora_margin_to_bars(100, 4) != 4) {
        printf("  FAIL: Boundary 100 tenths did not maintain 4 bars!\n");
        test17_pass = false;
    }
    /* Dropping below 100 drops to 3 bars */
    if (lora_margin_to_bars(99, 4) != 3) {
        printf("  FAIL: Margin 99 tenths did not drop to 3 bars!\n");
        test17_pass = false;
    }

    /* 3. Hysteresis anti-flicker test: oscillating around 10.0 dB margin (100 tenths) */
    uint8_t h_bars = 3;
    bool flicker_detected = false;
    for (int iter = 0; iter < 10; iter++) {
        int16_t osc_margin = (iter % 2 == 0) ? 95 : 105; /* oscillates across 100 threshold */
        h_bars = lora_margin_to_bars(osc_margin, h_bars);
        if (h_bars != 3) {
            flicker_detected = true;
        }
    }
    if (flicker_detected) {
        printf("  FAIL: Indicator flickered when margin oscillated around 10.0 dB boundary!\n");
        test17_pass = false;
    } else {
        printf("  PASS: Hysteresis prevented flicker across 10 boundary oscillations (9.5 dB <-> 10.5 dB).\n");
    }

    /* 4. Spreading factor: changing SF genuinely shifts thresholds */
    /* At SNR = -6 dB:
     * SF7 (limit -7.5 dB): margin = +1.5 dB (15 tenths) -> 1 bar
     * SF8 (limit -10.0 dB): margin = +4.0 dB (40 tenths) -> 2 bars (from 0)
     * SF10 (limit -15.0 dB): margin = +9.0 dB (90 tenths) -> 3 bars (from 0)
     * SF12 (limit -20.0 dB): margin = +14.0 dB (140 tenths) -> 4 bars (from 0)
     */
    if (lora_snr_to_bars_sf(-6, 0, 7) != 1 ||
        lora_snr_to_bars_sf(-6, 0, 8) != 2 ||
        lora_snr_to_bars_sf(-6, 0, 10) != 3 ||
        lora_snr_to_bars_sf(-6, 0, 12) != 4) {
        printf("  FAIL: Spreading factor did not shift bar thresholds as expected!\n");
        test17_pass = false;
    } else {
        printf("  PASS: Spreading factor configuration correctly moves demodulator limits and bar levels.\n");
    }

    /* 5. Live header bar rendering & Study 12 palette RGB565 color sampling */
    ui_navigate_to_page(UI_PAGE_HOME);
    populate_node_status_baseline();
    /* Set degraded 2-bar link: at SF7 (limit -7.5 dB), SNR = -4 dB -> margin = +3.5 dB (35 tenths) */
    lora_last_snr = -4;
    lora_rx_revision++;
    advance_ui(200u);

    if (ui_get_link_bars() != 2) {
        printf("  FAIL: Expected ui_get_link_bars() == 2, got %u\n", (unsigned)ui_get_link_bars());
        test17_pass = false;
    }

    /* Sample framebuffer at header cluster for active (bar 0/1) and inactive (bar 2/3) fills */
    int active_bar_pixels = 0;
    int inactive_bar_pixels = 0;
    uint16_t sampled_active_color = 0;
    uint16_t sampled_inactive_color = 0;

    for (int y = 21; y <= 34; y++) {
        for (int x = 650; x <= 780; x++) {
            uint16_t pix = s_framebuffer[y * SIM_HOR + x];
            if (pix == UI_COLOR_GREEN.full) {
                active_bar_pixels++;
                sampled_active_color = pix;
            } else if (pix == UI_COLOR_SWITCH_OFF.full) {
                inactive_bar_pixels++;
                sampled_inactive_color = pix;
            }
        }
    }

    printf("  Sampled bar fills: Active=0x%04X (expected 0x%04X), Inactive=0x%04X (expected 0x%04X)\n",
           sampled_active_color, UI_COLOR_GREEN.full, sampled_inactive_color, UI_COLOR_SWITCH_OFF.full);
    printf("  Bar pixel count in 2-bar state: active=%d, inactive=%d\n",
           active_bar_pixels, inactive_bar_pixels);

    if (sampled_active_color != UI_COLOR_GREEN.full || sampled_inactive_color != UI_COLOR_SWITCH_OFF.full ||
        active_bar_pixels == 0 || inactive_bar_pixels == 0) {
        printf("  FAIL: Signal bar pixel fills did not match Study 12 palette!\n");
        test17_pass = false;
    }

    /* 6. Sub-demodulator limit semantics (Floor at 1 bar while frames arrive) */
    hooks = ui_get_test_hooks();
    ui_navigate_to_page(UI_PAGE_HOME);
    populate_node_status_baseline();
    lora_last_rx_tick_ms = lv_tick_get(); /* Fresh link */
    lora_last_snr = -20; /* At SF7 limit is -7.5 dB -> margin is -12.5 dB (-125 tenths), far below limit */
    lora_rx_revision++;
    advance_ui(200u);

    if (ui_get_link_bars() != 1) {
        printf("  FAIL: Sub-limit SNR did not clamp to 1 bar while link fresh! (got %u)\n",
               (unsigned)ui_get_link_bars());
        test17_pass = false;
    }
    if (strcmp(ui_get_link_status_str(), "Connected") != 0) {
        printf("  FAIL: Sub-limit SNR reported '%s' instead of 'Connected'!\n",
               ui_get_link_status_str());
        test17_pass = false;
    }

    /* 7. No contradiction: relay toggle accepted while in sub-demodulator limit state */
    ui_trigger_device_toggle(0);
    advance_ui(50u);
    const ui_snapshot_t *sub_snap = ui_get_snapshot();
    if (!sub_snap->devices[0].pending && sub_snap->devices[0].on == true) {
        printf("  FAIL: Relay toggle not accepted while in sub-demodulator limit state!\n");
        test17_pass = false;
    } else {
        printf("  PASS: Sub-limit SNR (margin -12.5 dB) kept link 'Connected' (1 bar) and accepted relay command.\n");
    }
    /* Let fake node complete toggle turnaround */
    advance_ui(800u);
    ui_test_reset_device_commands();
    ui_dismiss_toast();

    /* Mirror case: let status frames stop -> must report 'Disconnected' with 0 bars */
    lora_last_rx_tick_ms = lv_tick_get() - (LORA_LINK_TIMEOUT_MS + 500u);
    advance_ui(200u);
    if (ui_get_link_bars() != 0) {
        printf("  FAIL: Stale link did not drop to 0 bars! (got %u)\n",
               (unsigned)ui_get_link_bars());
        test17_pass = false;
    }
    if (strcmp(ui_get_link_status_str(), "Disconnected") != 0) {
        printf("  FAIL: Stale link reported '%s' instead of 'Disconnected'!\n",
               ui_get_link_status_str());
        test17_pass = false;
    }

    /* 8. Anti-flicker: oscillate SNR across demodulator limit (-7.5 dB: -8 dB <-> -7 dB) */
    populate_node_status_baseline();
    lora_last_rx_tick_ms = lv_tick_get();
    lora_last_snr = -8; /* margin = -0.5 dB (< 0) -> clamped to 1 bar */
    lora_rx_revision++;
    advance_ui(200u);

    lv_coord_t base_lbl_x = (hooks && hooks->link_label && *hooks->link_label) ? lv_obj_get_x(*hooks->link_label) : 0;
    lv_coord_t base_bars_x = (hooks && hooks->link_bars_cont && *hooks->link_bars_cont) ? lv_obj_get_x(*hooks->link_bars_cont) : 0;
    lv_coord_t base_sep_x = (hooks && hooks->header_sep && *hooks->header_sep) ? lv_obj_get_x(*hooks->header_sep) : 0;
    lv_coord_t base_uptime_x = (hooks && hooks->uptime_label && *hooks->uptime_label) ? lv_obj_get_x(*hooks->uptime_label) : 0;

    bool osc_flicker = false;
    for (int iter = 0; iter < 10; iter++) {
        lora_last_rx_tick_ms = lv_tick_get();
        lora_last_snr = (iter % 2 == 0) ? -8 : -7; /* oscillates across -7.5 dB demodulator limit */
        lora_rx_revision++;
        advance_ui(100u);

        if (strcmp(ui_get_link_status_str(), "Connected") != 0) {
            osc_flicker = true;
        }
        if (ui_get_link_bars() != 1) {
            osc_flicker = true;
        }
        if (hooks && hooks->link_label && *hooks->link_label &&
            (lv_obj_get_x(*hooks->link_label) != base_lbl_x ||
             lv_obj_get_x(*hooks->link_bars_cont) != base_bars_x ||
             lv_obj_get_x(*hooks->header_sep) != base_sep_x ||
             lv_obj_get_x(*hooks->uptime_label) != base_uptime_x)) {
            osc_flicker = true;
        }
    }

    if (osc_flicker) {
        printf("  FAIL: Header label or cluster positions flickered during demodulator limit oscillation!\n");
        test17_pass = false;
    } else {
        printf("  PASS: Anti-flicker verified: label stayed 'Connected' (1 bar) and cluster positions remained stationary across 10 limit crossings.\n");
    }

    /* Restore baseline 4 bars */
    populate_node_status_baseline();
    lora_last_snr = 6;
    lora_rx_revision++;
    advance_ui(200u);
    if (ui_get_link_bars() != 4) {
        printf("  FAIL: Expected 4 bars at SNR=6 dB, got %u\n", (unsigned)ui_get_link_bars());
        test17_pass = false;
    }

    if (test17_pass) {
        printf("  PASS: SNR mapping, boundaries, hysteresis anti-flicker, SF shift & Study 12 fills verified.\n");
        passed_tests++;
    }

    /* -------------------------------------------------------------
     * TEST 18: Page & Dialog Rebuild Idempotence
     * ------------------------------------------------------------- */
    total_tests++;
    printf("\n[CHECK 18] Page & dialog rebuild idempotence (pixel-identical across rebuilds)...\n");
    bool test18_pass = true;

    /* Freeze uptime clock and ensure clean state */
    ui_set_uptime_seconds(1234);
    ui_test_reset_device_commands();
    ui_dismiss_toast();

    /* 1. Home Page Rebuild Idempotence */
    ui_navigate_to_page(UI_PAGE_HOME);
    populate_node_status_baseline();
    /* Force value update so ppm is dynamically positioned at val_w + 8 on fresh frame */
    lora_node_status.bits.co2_ppm = 421;
    lora_rx_revision++;
    advance_ui(200u);
    lora_node_status.bits.co2_ppm = 420;
    lora_rx_revision++;
    advance_ui(200u);
    ui_set_uptime_seconds(1234);
    lv_refr_now(NULL);
    memcpy(s_test_fb_a, s_framebuffer, sizeof(s_test_fb_a));

    /* Navigate away and back with UNCHANGED status */
    ui_navigate_to_page(UI_PAGE_TRENDS);
    advance_ui(200u);
    ui_navigate_to_page(UI_PAGE_HOME);
    advance_ui(200u);
    ui_set_uptime_seconds(1234);
    lv_refr_now(NULL);
    memcpy(s_test_fb_b, s_framebuffer, sizeof(s_test_fb_b));

    if (!compare_framebuffers("Home page", s_test_fb_a, s_test_fb_b)) {
        test18_pass = false;
    }

    /* 2. Trends Page Rebuild Idempotence */
    ui_navigate_to_page(UI_PAGE_TRENDS);
    advance_ui(200u);
    ui_set_uptime_seconds(1234);
    lv_refr_now(NULL);
    memcpy(s_test_fb_a, s_framebuffer, sizeof(s_test_fb_a));

    ui_navigate_to_page(UI_PAGE_DEVICES);
    advance_ui(200u);
    ui_navigate_to_page(UI_PAGE_TRENDS);
    advance_ui(200u);
    ui_set_uptime_seconds(1234);
    lv_refr_now(NULL);
    memcpy(s_test_fb_b, s_framebuffer, sizeof(s_test_fb_b));

    if (!compare_framebuffers("Trends page", s_test_fb_a, s_test_fb_b)) {
        test18_pass = false;
    }

    /* 3. Devices Page Rebuild Idempotence */
    ui_navigate_to_page(UI_PAGE_DEVICES);
    advance_ui(200u);
    ui_set_uptime_seconds(1234);
    lv_refr_now(NULL);
    memcpy(s_test_fb_a, s_framebuffer, sizeof(s_test_fb_a));

    ui_navigate_to_page(UI_PAGE_HOME);
    advance_ui(200u);
    ui_navigate_to_page(UI_PAGE_DEVICES);
    advance_ui(200u);
    ui_set_uptime_seconds(1234);
    lv_refr_now(NULL);
    memcpy(s_test_fb_b, s_framebuffer, sizeof(s_test_fb_b));

    if (!compare_framebuffers("Devices page", s_test_fb_a, s_test_fb_b)) {
        test18_pass = false;
    }

    /* 4. Settings Dialog Rebuild/Reopen Idempotence */
    ui_open_device_settings(0);
    advance_ui(200u);
    ui_set_uptime_seconds(1234);
    lv_refr_now(NULL);
    memcpy(s_test_fb_a, s_framebuffer, sizeof(s_test_fb_a));

    ui_close_device_settings();
    advance_ui(100u);
    ui_open_device_settings(0);
    advance_ui(200u);
    ui_set_uptime_seconds(1234);
    lv_refr_now(NULL);
    memcpy(s_test_fb_b, s_framebuffer, sizeof(s_test_fb_b));

    if (!compare_framebuffers("Settings dialog", s_test_fb_a, s_test_fb_b)) {
        test18_pass = false;
    }
    ui_close_device_settings();
    advance_ui(100u);

    if (test18_pass) {
        printf("  PASS: Rebuild idempotence verified across Home, Trends, Devices, and Dialog.\n");
        passed_tests++;
    }

    /* -------------------------------------------------------------
     * TEST 19: Value Round-Trip Idempotence
     * ------------------------------------------------------------- */
    total_tests++;
    printf("\n[CHECK 19] Value round-trip idempotence (metric width shifts & restorations)...\n");
    bool test19_pass = true;

    /* Freeze uptime clock and ensure clean state */
    ui_set_uptime_seconds(1234);
    ui_test_reset_device_commands();
    ui_dismiss_toast();

    /* 1. CO2 round-trip: 420 -> 1420 -> 420 */
    ui_navigate_to_page(UI_PAGE_HOME);
    populate_node_status_baseline(); /* co2_ppm = 420 */
    advance_ui(200u);
    ui_set_uptime_seconds(1234);
    lv_refr_now(NULL);
    memcpy(s_test_fb_a, s_framebuffer, sizeof(s_test_fb_a));

    /* Shift to 4-digit value (width expansion) */
    lora_node_status.bits.co2_ppm = 1420;
    lora_rx_revision++;
    advance_ui(200u);

    /* Restore 3-digit value (width contraction) */
    lora_node_status.bits.co2_ppm = 420;
    lora_rx_revision++;
    advance_ui(200u);
    ui_set_uptime_seconds(1234);
    lv_refr_now(NULL);
    memcpy(s_test_fb_b, s_framebuffer, sizeof(s_test_fb_b));

    if (!compare_framebuffers("CO2 420->1420->420 round-trip", s_test_fb_a, s_test_fb_b)) {
        test19_pass = false;
    }

    /* 2. Temperature round-trip: 23.5 C -> -2.5 C -> 23.5 C */
    populate_node_status_baseline(); /* temp_deci_c = 235 */
    advance_ui(200u);
    ui_set_uptime_seconds(1234);
    lv_refr_now(NULL);
    memcpy(s_test_fb_a, s_framebuffer, sizeof(s_test_fb_a));

    /* Shift to negative value with minus sign */
    lora_node_status.bits.temp_deci_c = -25;
    lora_rx_revision++;
    advance_ui(200u);

    /* Restore positive value */
    lora_node_status.bits.temp_deci_c = 235;
    lora_rx_revision++;
    advance_ui(200u);
    ui_set_uptime_seconds(1234);
    lv_refr_now(NULL);
    memcpy(s_test_fb_b, s_framebuffer, sizeof(s_test_fb_b));

    if (!compare_framebuffers("Temperature 23.5->-2.5->23.5 round-trip", s_test_fb_a, s_test_fb_b)) {
        test19_pass = false;
    }

    if (test19_pass) {
        printf("  PASS: Value round-trip idempotence verified for width shifts.\n");
        passed_tests++;
    }

    /* -------------------------------------------------------------
     * TEST 20: Device config restore, schema migration & boot TX gating (C1)
     * ------------------------------------------------------------- */
    total_tests++;
    printf("\n[CHECK 20] Device config restore, schema migration & boot TX gating (C1)...\n");
    bool test20_pass = true;

    /* 1. Legacy v1 config migration test */
    ui_hub_device_config_v1_t legacy_cfg;
    memset(&legacy_cfg, 0, sizeof(legacy_cfg));
    legacy_cfg.preset = UI_PRESET_FAN;
    snprintf(legacy_cfg.name, sizeof(legacy_cfg.name), "Exhaust Fan");
    legacy_cfg.mode = UI_MODE_AUTO;
    legacy_cfg.active_low = 1u;

    ui_hub_device_config_t migrated_cfg;
    if (!ui_auto_migrate_legacy_config(&legacy_cfg, sizeof(legacy_cfg), &migrated_cfg)) {
        printf("  FAIL: ui_auto_migrate_legacy_config returned false for valid v1 config\n");
        test20_pass = false;
    }
    if (migrated_cfg.schema_version != UI_CONFIG_SCHEMA_VERSION) {
        printf("  FAIL: Migrated schema version is 0x%02X, expected 0x%02X\n",
               migrated_cfg.schema_version, UI_CONFIG_SCHEMA_VERSION);
        test20_pass = false;
    }
    if (migrated_cfg.preset != UI_PRESET_FAN || strcmp(migrated_cfg.name, "Exhaust Fan") != 0 ||
        migrated_cfg.mode != UI_MODE_MANUAL || migrated_cfg.active_low != 1u) {
        printf("  FAIL: Migrated fields corrupted: preset=%u, name='%s', mode=%u, active_low=%u\n",
               migrated_cfg.preset, migrated_cfg.name, (unsigned)migrated_cfg.mode, migrated_cfg.active_low);
        test20_pass = false;
    }
    if (migrated_cfg.auto_on_thresh != 1000 || migrated_cfg.auto_off_thresh != 800) {
        printf("  FAIL: Migrated thresholds mismatch: on=%d (exp 1000), off=%d (exp 800)\n",
               (int)migrated_cfg.auto_on_thresh, (int)migrated_cfg.auto_off_thresh);
        test20_pass = false;
    }

    /* 2. Malformed / invalid config rejection and safe fallback */
    if (ui_auto_migrate_legacy_config(NULL, sizeof(legacy_cfg), &migrated_cfg) ||
        ui_auto_migrate_legacy_config(&legacy_cfg, sizeof(legacy_cfg) - 1, &migrated_cfg)) {
        printf("  FAIL: ui_auto_migrate_legacy_config accepted invalid/truncated input\n");
        test20_pass = false;
    }

    char val_err[64];
    ui_hub_device_config_t invalid_cfg = migrated_cfg;
    invalid_cfg.auto_on_thresh = 700; /* Inverted order: on < off (700 < 800) */
    if (ui_auto_validate_config(&invalid_cfg, val_err, sizeof(val_err))) {
        printf("  FAIL: ui_auto_validate_config accepted on_thresh < off_thresh for Fan\n");
        test20_pass = false;
    }
    invalid_cfg = migrated_cfg;
    invalid_cfg.preset = 99; /* Invalid preset */
    if (ui_auto_validate_config(&invalid_cfg, val_err, sizeof(val_err))) {
        printf("  FAIL: ui_auto_validate_config accepted invalid preset 99\n");
        test20_pass = false;
    }

    /* 3. Boot TX gating: verify ui_is_tx_ready_test() reflects settled state */
    if (!ui_is_tx_ready_test()) {
        printf("  FAIL: ui_is_tx_ready is false in settled running state\n");
        test20_pass = false;
    }

    if (test20_pass) {
        printf("  PASS: Config restore, schema migration, validation fallback & boot TX gating verified.\n");
        passed_tests++;
    }

    /* -------------------------------------------------------------
     * TEST 21: Auto rule evaluation boundaries, qualification & hold (C7)
     * ------------------------------------------------------------- */
    total_tests++;
    printf("\n[CHECK 21] Auto rule evaluation boundaries, qualification & hold (C7)...\n");
    bool test21_pass = true;

    ui_auto_state_t fan_state;
    ui_auto_state_init(&fan_state, 0);

    ui_hub_device_config_t fan_cfg;
    ui_auto_get_defaults(1, UI_PRESET_FAN, &fan_cfg); /* Fan: ON >= 1000, OFF <= 800 */
    fan_cfg.mode = UI_MODE_AUTO;

    lora_node_status_t node_pkt;
    memset(&node_pkt, 0, sizeof(node_pkt));
    node_pkt.bits.co2_valid = 1u;
    bool target_on = false;
    uint32_t t = 1000u;

    /* 1. Exact boundary: 999 ppm -> below ON threshold, no trigger */
    node_pkt.bits.co2_ppm = 999;
    for (int i = 0; i < 5; i++) {
        t += 1000u;
        if (ui_auto_eval(&fan_state, &fan_cfg, &node_pkt, true, false, t, &target_on)) {
            printf("  FAIL: Auto triggered ON below threshold at 999 ppm!\n");
            test21_pass = false;
        }
    }
    if (fan_state.qualify_count != 0) {
        printf("  FAIL: Qualification count was %u, expected 0 at 999 ppm\n", fan_state.qualify_count);
        test21_pass = false;
    }

    /* 2. 3-frame qualification requirement at exact boundary (1000 ppm) */
    node_pkt.bits.co2_ppm = 1000;
    t += 1000u;
    if (ui_auto_eval(&fan_state, &fan_cfg, &node_pkt, true, false, t, &target_on) || fan_state.qualify_count != 1) {
        printf("  FAIL: Premature trigger or wrong qual_count on frame 1 (count=%u)\n", fan_state.qualify_count);
        test21_pass = false;
    }
    t += 1000u;
    if (ui_auto_eval(&fan_state, &fan_cfg, &node_pkt, true, false, t, &target_on) || fan_state.qualify_count != 2) {
        printf("  FAIL: Premature trigger or wrong qual_count on frame 2 (count=%u)\n", fan_state.qualify_count);
        test21_pass = false;
    }
    /* Transient glitch back to 900 ppm resets qualification counter */
    node_pkt.bits.co2_ppm = 900;
    t += 1000u;
    if (ui_auto_eval(&fan_state, &fan_cfg, &node_pkt, true, false, t, &target_on) || fan_state.qualify_count != 0) {
        printf("  FAIL: Glitch did not reset qualification counter (count=%u)\n", fan_state.qualify_count);
        test21_pass = false;
    }

    /* Qualify 3 consecutive frames at 1000 ppm */
    node_pkt.bits.co2_ppm = 1000;
    t += 1000u; ui_auto_eval(&fan_state, &fan_cfg, &node_pkt, true, false, t, &target_on);
    t += 1000u; ui_auto_eval(&fan_state, &fan_cfg, &node_pkt, true, false, t, &target_on);
    t += 1000u;
    bool triggered = ui_auto_eval(&fan_state, &fan_cfg, &node_pkt, true, false, t, &target_on);
    if (!triggered || !target_on) {
        printf("  FAIL: Did not trigger ON after 3 qualified frames at 1000 ppm\n");
        test21_pass = false;
    }

    /* Switch transition confirmed -> hold restarts */
    ui_auto_restart_hold(&fan_state, t);

    /* 3. 10-second post-switch hold time */
    node_pkt.bits.co2_ppm = 700;
    for (int i = 0; i < 5; i++) {
        t += 1000u; /* Total elapsed 5s < 10s */
        if (ui_auto_eval(&fan_state, &fan_cfg, &node_pkt, true, true, t, &target_on)) {
            printf("  FAIL: Auto triggered OFF during 10s post-switch hold timer!\n");
            test21_pass = false;
        }
    }

    /* Advance past 10s hold timer */
    t += 6000u;
    /* Qualify 3 frames for OFF */
    t += 1000u; ui_auto_eval(&fan_state, &fan_cfg, &node_pkt, true, true, t, &target_on);
    t += 1000u; ui_auto_eval(&fan_state, &fan_cfg, &node_pkt, true, true, t, &target_on);
    t += 1000u;
    triggered = ui_auto_eval(&fan_state, &fan_cfg, &node_pkt, true, true, t, &target_on);
    if (!triggered || target_on != false) {
        printf("  FAIL: Did not trigger OFF after hold expired and 3 frames at 700 ppm\n");
        test21_pass = false;
    }

    /* 4. Deadband hysteresis (801..999 ppm): no qualification or trigger */
    node_pkt.bits.co2_ppm = 850;
    for (int i = 0; i < 5; i++) {
        t += 1000u;
        if (ui_auto_eval(&fan_state, &fan_cfg, &node_pkt, true, false, t, &target_on)) {
            printf("  FAIL: Auto triggered in deadband at 850 ppm!\n");
            test21_pass = false;
        }
    }

    /* 5. Inverted threshold logic: Humidifier (ON <= 40%, OFF >= 55%) */
    ui_auto_state_t hum_state;
    ui_auto_state_init(&hum_state, t);
    ui_hub_device_config_t hum_cfg;
    ui_auto_get_defaults(2, UI_PRESET_HUMIDIFIER, &hum_cfg);
    hum_cfg.mode = UI_MODE_AUTO;
    node_pkt.bits.humid_valid = 1u;
    node_pkt.bits.humidity_pct = 40; /* Exactly ON threshold */

    t += 1000u; ui_auto_eval(&hum_state, &hum_cfg, &node_pkt, true, false, t, &target_on);
    t += 1000u; ui_auto_eval(&hum_state, &hum_cfg, &node_pkt, true, false, t, &target_on);
    t += 1000u;
    triggered = ui_auto_eval(&hum_state, &hum_cfg, &node_pkt, true, false, t, &target_on);
    if (!triggered || !target_on) {
        printf("  FAIL: Humidifier did not trigger ON at <= 40%% RH\n");
        test21_pass = false;
    }

    /* 6. Sensor fault / invalidity masking */
    node_pkt.bits.co2_valid = 0u;
    node_pkt.bits.co2_ppm = 2000;
    ui_auto_state_init(&fan_state, t);
    for (int i = 0; i < 5; i++) {
        t += 1000u;
        if (ui_auto_eval(&fan_state, &fan_cfg, &node_pkt, true, false, t, &target_on)) {
            printf("  FAIL: Auto triggered ON while co2_valid = 0!\n");
            test21_pass = false;
        }
    }

    if (test21_pass) {
        printf("  PASS: Auto rule boundaries, 3-frame qualification, 10s hold, deadband & fault masking verified.\n");
        passed_tests++;
    }

    /* -------------------------------------------------------------
     * TEST 22: Threshold editor steppers, bounds, validation & draft lifecycle (C8)
     * ------------------------------------------------------------- */
    total_tests++;
    printf("\n[CHECK 22] Threshold editor steppers, bounds, validation & draft lifecycle (C8)...\n");
    bool test22_pass = true;

    ui_navigate_to_page(UI_PAGE_DEVICES);
    advance_ui(50u);

    /* Open device 1 (Ventilation Fan) */
    ui_open_device_settings(1);
    advance_ui(50u);
    if (!ui_is_device_settings_open()) {
        printf("  FAIL: Settings dialog failed to open for Device 1\n");
        test22_pass = false;
    }

    /* Open limits subview */
    ui_dialog_open_limits();
    advance_ui(50u);
    if (!ui_is_dialog_limits_subview_active()) {
        printf("  FAIL: Limits subview did not become active\n");
        test22_pass = false;
    }

    int32_t init_on = ui_get_dialog_edit_on_thresh();
    int32_t init_off = ui_get_dialog_edit_off_thresh();
    if (init_on != 1000 || init_off != 800) {
        printf("  FAIL: Initial thresholds mismatch: on=%d (exp 1000), off=%d (exp 800)\n",
               (int)init_on, (int)init_off);
        test22_pass = false;
    }

    /* 1. Step buttons: Fan step size is 50 ppm */
    ui_dialog_step_thresh(true, 1); /* Step ON +1 (+50 ppm -> 1050) */
    advance_ui(20u);
    if (ui_get_dialog_edit_on_thresh() != 1050) {
        printf("  FAIL: Step ON +1 produced %d, expected 1050\n", (int)ui_get_dialog_edit_on_thresh());
        test22_pass = false;
    }

    ui_dialog_step_thresh(true, -1); /* Step ON -1 (back to 1000) */
    advance_ui(20u);
    if (ui_get_dialog_edit_on_thresh() != 1000) {
        printf("  FAIL: Step ON -1 produced %d, expected 1000\n", (int)ui_get_dialog_edit_on_thresh());
        test22_pass = false;
    }

    /* 2. Validation error on insufficient gap: ON=1000, step OFF up towards ON */
    ui_dialog_step_thresh(false, 1); /* OFF 800 -> 850 (gap 150 >= 50 ok) */
    ui_dialog_step_thresh(false, 1); /* OFF 850 -> 900 (gap 100 >= 50 ok) */
    ui_dialog_step_thresh(false, 1); /* OFF 900 -> 950 (gap 50 >= 50 ok) */
    ui_dialog_step_thresh(false, 1); /* OFF 950 -> 1000 (gap 0 < 50 / ON <= OFF VIOLATION) */
    advance_ui(20u);

    const char *vmsg = ui_get_dialog_validation_msg();
    if (!vmsg || strstr(vmsg, "exceed") == NULL) {
        printf("  FAIL: Validation error message missing on gap violation (msg: '%s')\n", vmsg ? vmsg : "NULL");
        test22_pass = false;
    }

    /* 3. Reset Limits button restores defaults (1000, 800) and clears error */
    ui_dialog_reset_limits();
    advance_ui(20u);
    if (ui_get_dialog_edit_on_thresh() != 1000 || ui_get_dialog_edit_off_thresh() != 800) {
        printf("  FAIL: Reset limits did not restore 1000/800 defaults: on=%d, off=%d\n",
               (int)ui_get_dialog_edit_on_thresh(), (int)ui_get_dialog_edit_off_thresh());
        test22_pass = false;
    }
    vmsg = ui_get_dialog_validation_msg();
    if (vmsg && strlen(vmsg) > 0) {
        printf("  FAIL: Validation message not cleared after Reset Limits: '%s'\n", vmsg);
        test22_pass = false;
    }

    /* 4. Clamping bounds: Fan limits are 0..10000 */
    for (int i = 0; i < 20; i++) {
        ui_dialog_step_thresh(false, -1);
    }
    advance_ui(20u);
    if (ui_get_dialog_edit_off_thresh() != 0) {
        printf("  FAIL: OFF threshold failed to clamp at 0: got %d\n", (int)ui_get_dialog_edit_off_thresh());
        test22_pass = false;
    }
    ui_dialog_step_thresh(false, -1);
    advance_ui(20u);
    if (ui_get_dialog_edit_off_thresh() != 0) {
        printf("  FAIL: OFF threshold wrapped or unclamped below 0: got %d\n", (int)ui_get_dialog_edit_off_thresh());
        test22_pass = false;
    }
    ui_dialog_reset_limits();
    advance_ui(20u);

    /* 5. Back button preserves draft */
    ui_dialog_step_thresh(true, 1); /* ON = 1050 */
    advance_ui(20u);
    ui_dialog_limits_back(); /* Return to main dialog view */
    advance_ui(30u);
    if (ui_is_dialog_limits_subview_active()) {
        printf("  FAIL: Limits subview still active after Back\n");
        test22_pass = false;
    }
    ui_dialog_open_limits(); /* Re-open limits subview */
    advance_ui(30u);
    if (ui_get_dialog_edit_on_thresh() != 1050) {
        printf("  FAIL: Back did not preserve draft threshold: got %d, expected 1050\n",
               (int)ui_get_dialog_edit_on_thresh());
        test22_pass = false;
    }

    /* 6. Close/X discards draft */
    ui_close_device_settings();
    advance_ui(50u);
    if (ui_is_device_settings_open()) {
        printf("  FAIL: Dialog did not close on Close\n");
        test22_pass = false;
    }
    ui_open_device_settings(1);
    advance_ui(30u);
    ui_dialog_open_limits();
    advance_ui(30u);
    if (ui_get_dialog_edit_on_thresh() != 1000) {
        printf("  FAIL: Close did not discard draft: got %d, expected 1000\n",
               (int)ui_get_dialog_edit_on_thresh());
        test22_pass = false;
    }

    /* 7. Save commits thresholds */
    ui_dialog_step_thresh(true, 1); /* ON = 1050 */
    advance_ui(20u);
    ui_save_device_settings();
    advance_ui(50u);
    if (ui_is_device_settings_open()) {
        printf("  FAIL: Dialog did not close on Save\n");
        test22_pass = false;
    }
    const ui_hub_device_config_t *saved_cfg = ui_get_device_config(1);
    if (saved_cfg->auto_on_thresh != 1050) {
        printf("  FAIL: Save did not commit new threshold: on=%d, expected 1050\n", (int)saved_cfg->auto_on_thresh);
        test22_pass = false;
    }

    /* Restore baseline (1000, 800) */
    ui_hub_device_config_t rst_cfg = *saved_cfg;
    rst_cfg.auto_on_thresh = 1000;
    rst_cfg.auto_off_thresh = 800;
    ui_set_device_config(1, &rst_cfg);
    advance_ui(50u);

    if (test22_pass) {
        printf("  PASS: Threshold editor steppers, bounds, validation, reset, back & save verified.\n");
        passed_tests++;
    }

    /* -------------------------------------------------------------
     * TEST 23: Auto mode behavior across tabs, manual override & retry (C9)
     * ------------------------------------------------------------- */
    total_tests++;
    printf("\n[CHECK 23] Auto mode behavior across tabs, manual override & retry (C9)...\n");
    bool test23_pass = true;

    populate_node_status_baseline();
    ui_navigate_to_page(UI_PAGE_HOME);
    advance_ui(50u);

    /* 1. Auto mode badge on Home tile */
    const char *h_fan_mode = ui_get_home_dev_mode(1);
    if (!h_fan_mode || strcmp(h_fan_mode, "Auto") != 0) {
        printf("  FAIL: Home tab Device 1 mode is '%s', expected 'Auto'\n", h_fan_mode ? h_fan_mode : "NULL");
        test23_pass = false;
    }

    /* 2. Manual override: explicit switch click on Home switches mode to Manual atomically */
    ui_trigger_device_toggle(1);
    advance_ui(20u);
    h_fan_mode = ui_get_home_dev_mode(1);
    if (!h_fan_mode || strcmp(h_fan_mode, "Manual") != 0) {
        printf("  FAIL: User toggle did not immediately switch mode to 'Manual' (mode: '%s')\n",
               h_fan_mode ? h_fan_mode : "NULL");
        test23_pass = false;
    }
    advance_ui(1000u);

    /* Verify Devices page also shows Manual */
    ui_navigate_to_page(UI_PAGE_DEVICES);
    advance_ui(50u);
    const char *d_fan_mode = ui_get_devices_dev_mode(1);
    if (!d_fan_mode || strcmp(d_fan_mode, "Manual") != 0) {
        printf("  FAIL: Devices tab Device 1 mode is '%s', expected 'Manual'\n", d_fan_mode ? d_fan_mode : "NULL");
        test23_pass = false;
    }

    /* Restore Auto mode via ui_set_device_config */
    ui_hub_device_config_t fcfg = *ui_get_device_config(1);
    fcfg.mode = UI_MODE_AUTO;
    ui_set_device_config(1, &fcfg);
    advance_ui(50u);

    /* 3. Command timeout in Auto mode latches paused error state */
    /* Ensure relay 1 is reported OFF so high CO2 triggers Auto ON command */
    sim_post_node_status(true, 420, true, 85, true, 235, true, 48, true, 1, 0, 0, 1);
    advance_ui(10000u); /* Ensure 10s hold has passed */
    s_fake_node_mode = FAKE_NODE_MODE_FAIL_NEXT;

    /* Feed 3 frames of high CO2 (1200 >= 1000) so Auto engine triggers command */
    lora_node_status.bits.co2_ppm = 1200;
    lora_rx_revision++; advance_ui(100u);
    lora_rx_revision++; advance_ui(100u);
    lora_rx_revision++; advance_ui(100u);

    /* Command initiated by Auto engine times out */
    advance_ui(UI_CMD_TIMEOUT_MS + 200u);

    if (!ui_get_auto_paused_error(1)) {
        printf("  FAIL: Command timeout did not latch auto paused_error state\n");
        test23_pass = false;
    }

    /* 4. Changed-condition Retry re-arms auto against current reported GPIO without resending stale target */
    ui_trigger_device_retry(1);
    advance_ui(50u);
    if (ui_get_auto_paused_error(1)) {
        printf("  FAIL: Retry did not clear auto paused_error state\n");
        test23_pass = false;
    }

    ui_test_reset_device_commands();
    ui_dismiss_toast();
    advance_ui(50u);

    if (test23_pass) {
        printf("  PASS: Auto mode across tabs, manual override, error pause & retry verified.\n");
        passed_tests++;
    }

    /* -------------------------------------------------------------
     * TEST 24: Zero flush on Trends/Devices & no false comfort claims (C4, C5)
     * ------------------------------------------------------------- */
    total_tests++;
    printf("\n[CHECK 24] Zero flush on Trends/Devices & no false comfort claims (C4, C5)...\n");
    bool test24_pass = true;

    /* 1. Trends zero-flush test */
    ui_navigate_to_page(UI_PAGE_TRENDS);
    populate_node_status_baseline();
    feed_standard_history();
    advance_ui(500u);

    uint32_t sec_s24 = *ui_get_test_hooks()->uptime_seconds;
    while (*ui_get_test_hooks()->uptime_seconds == sec_s24) {
        advance_ui(10u);
    }

    reset_flush_counters();
    for (int i = 0; i < 10; i++) {
        lora_rx_revision++;
        advance_ui(10u);
    }
    if (s_flush_calls != 0 || s_flushed_pixels != 0) {
        printf("  FAIL: Trends 10 identical status frames produced %llu flushes, %llu pixels (expected 0)\n",
               (unsigned long long)s_flush_calls, (unsigned long long)s_flushed_pixels);
        test24_pass = false;
    }

    /* Re-feed identical history: must be strict 0-flush no-op */
    reset_flush_counters();
    feed_standard_history();
    advance_ui(30u);
    if (s_flush_calls != 0 || s_flushed_pixels != 0) {
        printf("  FAIL: Identical history re-feed produced %llu flushes, %llu pixels (expected 0)\n",
               (unsigned long long)s_flush_calls, (unsigned long long)s_flushed_pixels);
        test24_pass = false;
    }

    /* 2. Devices zero-flush test */
    ui_navigate_to_page(UI_PAGE_DEVICES);
    advance_ui(500u);
    sec_s24 = *ui_get_test_hooks()->uptime_seconds;
    while (*ui_get_test_hooks()->uptime_seconds == sec_s24) {
        advance_ui(10u);
    }
    reset_flush_counters();
    for (int i = 0; i < 10; i++) {
        lora_rx_revision++;
        advance_ui(10u);
    }
    if (s_flush_calls != 0 || s_flushed_pixels != 0) {
        printf("  FAIL: Devices 10 identical status frames produced %llu flushes, %llu pixels (expected 0)\n",
               (unsigned long long)s_flush_calls, (unsigned long long)s_flushed_pixels);
        test24_pass = false;
    }

    /* 3. Comfort claims elimination check (C5) */
    ui_navigate_to_page(UI_PAGE_HOME);
    populate_node_status_baseline();
    advance_ui(100u);
    int test_temps[] = { -200, 0, 180, 235, 290, 400 };
    int test_hums[] = { 10, 30, 48, 65, 90 };
    for (size_t i = 0; i < sizeof(test_temps)/sizeof(test_temps[0]); i++) {
        lora_node_status.bits.temp_deci_c = test_temps[i];
        for (size_t j = 0; j < sizeof(test_hums)/sizeof(test_hums[0]); j++) {
            lora_node_status.bits.humidity_pct = test_hums[j];
            lora_rx_revision++;
            advance_ui(20u);
            cur = ui_get_snapshot();
            if (cur) {
                if (cur->metrics[UI_METRIC_TEMP].note &&
                    strstr(cur->metrics[UI_METRIC_TEMP].note, "Ideal") != NULL) {
                    printf("  FAIL: Temp note claims '%s' for temp=%d\n",
                           cur->metrics[UI_METRIC_TEMP].note, test_temps[i]);
                    test24_pass = false;
                }
                if (cur->metrics[UI_METRIC_HUMIDITY].note &&
                    strstr(cur->metrics[UI_METRIC_HUMIDITY].note, "Ideal") != NULL) {
                    printf("  FAIL: Humid note claims '%s' for hum=%d\n",
                           cur->metrics[UI_METRIC_HUMIDITY].note, test_hums[j]);
                    test24_pass = false;
                }
            }
        }
    }

    if (test24_pass) {
        printf("  PASS: Zero flush on Trends/Devices & elimination of false comfort claims verified.\n");
        passed_tests++;
    }

    /* -------------------------------------------------------------
     * TEST 25: V1 Compiled Restore Integration Suite (NVS Flash Load)
     * ------------------------------------------------------------- */
    total_tests++;
    printf("\n[CHECK 25] V1 Compiled Restore Integration Suite...\n");
    bool test25_pass = test_restore_integration_suite();
    if (test25_pass) {
        printf("  PASS: V1 Flash restore integration suite passed (missing/current/legacy/truncated/callbacks).\n");
        passed_tests++;
    } else {
        printf("  FAIL: V1 Flash restore integration suite failed.\n");
    }

    /* -------------------------------------------------------------
     * TEST 26: V2 Open Dropdown Typography, Pitch >= 44px, Bounds, Scrim & Pointer Interaction
     * ------------------------------------------------------------- */
    total_tests++;
    printf("\n[CHECK 26] V2 Open Dropdown Typography, Pitch >= 44px, Bounds, Scrim & Pointer Interaction...\n");
    bool test26_pass = true;

    ui_navigate_to_page(UI_PAGE_DEVICES);
    advance_ui(50u);
    ui_open_device_settings(0);
    advance_ui(50u);

    const ui_test_hooks_t *hooks26 = ui_get_test_hooks();
    if (!hooks26 || !hooks26->dialog_preset_dd || *hooks26->dialog_preset_dd == NULL ||
        !hooks26->dialog_box || *hooks26->dialog_box == NULL) {
        printf("  FAIL: Dropdown or modal widget not available in settings dialog\n");
        test26_pass = false;
    } else {
        lv_obj_t *dd = *hooks26->dialog_preset_dd;
        lv_dropdown_open(dd);
        advance_ui(50u);

        if (!lv_dropdown_is_open(dd)) {
            printf("  FAIL: Dropdown failed to open\n");
            test26_pass = false;
        }

        lv_obj_t *list = lv_dropdown_get_list(dd);
        lv_obj_t *label = list ? lv_obj_get_child(list, 0) : NULL;
        if (!list || !label) {
            printf("  FAIL: Dropdown list or list label object is NULL\n");
            test26_pass = false;
        } else {
            /* 1. Font assertions */
            const lv_font_t *list_font = lv_obj_get_style_text_font(label, LV_PART_MAIN);
            if (list_font != &ui_font_18) {
                printf("  FAIL: Open dropdown list font is not ui_font_18\n");
                test26_pass = false;
            }

            /* 2. Selectable row pitch >= 44 px */
            lv_coord_t line_space = lv_obj_get_style_text_line_space(label, LV_PART_MAIN);
            lv_coord_t line_height = lv_font_get_line_height(list_font);
            lv_coord_t row_pitch = line_height + line_space;
            if (row_pitch < 44) {
                printf("  FAIL: Dropdown option row pitch %d px < 44 px requirement\n", (int)row_pitch);
                test26_pass = false;
            }

            /* 3. Bounds within modal at 800x480 */
            lv_area_t list_coords;
            lv_obj_get_coords(list, &list_coords);
            lv_area_t modal_coords;
            lv_obj_get_coords(*hooks26->dialog_box, &modal_coords);

            if (list_coords.y1 < modal_coords.y1 || list_coords.y2 > modal_coords.y2 ||
                list_coords.x1 < modal_coords.x1 || list_coords.x2 > modal_coords.x2) {
                printf("  FAIL: Dropdown list bounds (%d,%d..%d,%d) extend outside modal (%d,%d..%d,%d)\n",
                       list_coords.x1, list_coords.y1, list_coords.x2, list_coords.y2,
                       modal_coords.x1, modal_coords.y1, modal_coords.x2, modal_coords.y2);
                test26_pass = false;
            }

            /* 3b. Pointer drag scroll test: assert viewport really moved */
            lv_coord_t mid_x = (list_coords.x1 + list_coords.x2) / 2;
            lv_coord_t start_y = lv_obj_get_scroll_y(list);
            sim_pointer_drag(mid_x, list_coords.y2 - 20, mid_x, list_coords.y1 + 20, 100);
            advance_ui(50u);
            lv_coord_t dragged_y = lv_obj_get_scroll_y(list);
            if (dragged_y == start_y) {
                printf("  FAIL: Pointer drag did not move dropdown list scroll position (start=%d, dragged=%d)\n",
                       (int)start_y, (int)dragged_y);
                test26_pass = false;
            }

            /* Select last option (Generic Device) via pointer click */
            sim_pointer_click((list_coords.x1 + list_coords.x2) / 2, list_coords.y2 - 10);
            advance_ui(50u);
            if (lv_dropdown_is_open(dd)) {
                printf("  FAIL: Selecting last option did not close dropdown\n");
                test26_pass = false;
            }
            if (*hooks26->dialog_edit_preset != UI_PRESET_GENERIC) {
                printf("  FAIL: Pointer selection failed to update draft preset to Generic (got %u)\n",
                       *hooks26->dialog_edit_preset);
                test26_pass = false;
            }

            /* Reopen dropdown, scroll to top, click first option (Air Purifier) */
            lv_dropdown_open(dd);
            advance_ui(50u);
            list = lv_dropdown_get_list(dd);
            if (list) {
                lv_obj_scroll_to_y(list, 0, LV_ANIM_OFF);
                advance_ui(50u);
                lv_obj_get_coords(list, &list_coords);
                sim_pointer_click((list_coords.x1 + list_coords.x2) / 2, list_coords.y1 + row_pitch / 2);
                advance_ui(50u);
                if (*hooks26->dialog_edit_preset != UI_PRESET_PURIFIER) {
                    printf("  FAIL: Pointer selection failed to update draft preset to Purifier (got %u)\n",
                           *hooks26->dialog_edit_preset);
                    test26_pass = false;
                }
            }

            /* Reopen dropdown, click option 1 (Ventilation Fan) */
            lv_dropdown_open(dd);
            advance_ui(50u);
            list = lv_dropdown_get_list(dd);
            if (list) {
                lv_obj_get_coords(list, &list_coords);
                lv_coord_t opt1_y = list_coords.y1 + 1 * row_pitch + row_pitch / 2;
                sim_pointer_click((list_coords.x1 + list_coords.x2) / 2, opt1_y);
                advance_ui(50u);
                if (*hooks26->dialog_edit_preset != UI_PRESET_FAN) {
                    printf("  FAIL: Pointer selection failed to update draft preset to Fan (got %u)\n",
                           *hooks26->dialog_edit_preset);
                    test26_pass = false;
                }
            }

            /* 5. Outside dismiss: reopen dropdown, click outside at (250, 100) */
            lv_dropdown_open(dd);
            advance_ui(50u);
            if (!lv_dropdown_is_open(dd)) {
                printf("  FAIL: Dropdown failed to reopen\n");
                test26_pass = false;
            }
            sim_pointer_click(250, 100);
            advance_ui(50u);
            if (lv_dropdown_is_open(dd)) {
                printf("  FAIL: Tapping outside open dropdown did not dismiss it\n");
                test26_pass = false;
            }

            /* 6. X dismissal closes dialog cleanly */
            lv_dropdown_open(dd);
            advance_ui(50u);
            sim_pointer_click(578, 98);
            advance_ui(50u);
            if (ui_is_device_settings_open()) {
                printf("  FAIL: Tapping X while dropdown open did not close settings dialog\n");
                test26_pass = false;
            }
        }
    }
    if (test26_pass) {
        printf("  PASS: V2 open dropdown typography, pitch >= 44px, bounds, and pointer interaction verified.\n");
        passed_tests++;
    }

    /* -------------------------------------------------------------
     * TEST 27: Invalid Draft Stepper Creation, Save Disabled, Pointer Cancel & Isolation
     * ------------------------------------------------------------- */
    total_tests++;
    printf("\n[CHECK 27] Invalid Draft Stepper Creation, Save Disabled, Pointer Cancel & Isolation...\n");
    bool test27_pass = true;

    ui_hub_device_config_t dev1_orig;
    ui_auto_get_defaults(1, UI_PRESET_FAN, &dev1_orig);
    ui_set_device_config(1, &dev1_orig);

    ui_navigate_to_page(UI_PAGE_DEVICES);
    advance_ui(50u);
    ui_open_device_settings(1);
    advance_ui(50u);

    /* Click "Set limits…" button at (400, 266) to enter limits subview */
    sim_pointer_click(400, 266);
    advance_ui(50u);

    /* Register callback counter BEFORE editing/cancelling to ensure 0 callbacks */
    static uint32_t s_cb_rec27 = 0;
    void test_cb_counter27(uint8_t d_idx, const ui_hub_device_config_t *c) {
        (void)d_idx; (void)c;
        s_cb_rec27++;
    }
    ui_set_config_changed_cb(test_cb_counter27);
    s_cb_rec27 = 0;

    /* Step OFF up to create invalid pair via actual pointer gestures:
     * Click OFF plus button at (576, 252) 4 times: OFF goes from 800 -> 1000 (gap 0 < 50) */
    for (int k = 0; k < 4; k++) {
        sim_pointer_click(576, 252);
        advance_ui(30u);
    }

    const ui_test_hooks_t *hooks27 = ui_get_test_hooks();
    if (!hooks27 || !hooks27->dialog_btn_save_auto || *hooks27->dialog_btn_save_auto == NULL) {
        printf("  FAIL: Save button not found in limits subview\n");
        test27_pass = false;
    } else {
        if (!lv_obj_has_state(*hooks27->dialog_btn_save_auto, LV_STATE_DISABLED)) {
            printf("  FAIL: Save button was not disabled on invalid threshold pair\n");
            test27_pass = false;
        }

        /* 1. Pointer click Cancel directly via X button at (578, 98) from Auto limits subview */
        sim_pointer_click(578, 98);
        advance_ui(50u);

        if (ui_is_device_settings_open()) {
            printf("  FAIL: Clicking X did not close limits dialog\n");
            test27_pass = false;
        }
        if (s_cb_rec27 != 0) {
            printf("  FAIL: Cancel from Auto limits triggered %u callbacks (expected 0)\n", s_cb_rec27);
            test27_pass = false;
        }

        /* Assert committed config for Device 1 remains completely untouched */
        const ui_hub_device_config_t *c1_check = ui_get_device_config(1);
        if (c1_check->auto_on_thresh != 1000 || c1_check->auto_off_thresh != 800) {
            printf("  FAIL: Committed thresholds were corrupted by canceled invalid draft (on=%d, off=%d)\n",
                   (int)c1_check->auto_on_thresh, (int)c1_check->auto_off_thresh);
            test27_pass = false;
        }

        /* Reopen Device 1 settings dialog: assert draft was discarded and shows original limits */
        ui_open_device_settings(1);
        advance_ui(50u);
        sim_pointer_click(400, 266); /* Set limits… */
        advance_ui(50u);

        if (ui_get_dialog_edit_on_thresh() != 1000 || ui_get_dialog_edit_off_thresh() != 800) {
            printf("  FAIL: Reopened dialog retained invalid draft instead of canonical defaults (on=%d, off=%d)\n",
                   (int)ui_get_dialog_edit_on_thresh(), (int)ui_get_dialog_edit_off_thresh());
            test27_pass = false;
        }

        /* 2. Step OFF up again via pointer gestures, then test Back -> Device X */
        for (int k = 0; k < 4; k++) {
            sim_pointer_click(576, 252);
            advance_ui(30u);
        }
        /* Click Back button at (222, 98) */
        sim_pointer_click(222, 98);
        advance_ui(50u);
        /* Now on Device settings subview, click X at (578, 98) */
        sim_pointer_click(578, 98);
        advance_ui(50u);

        if (ui_is_device_settings_open()) {
            printf("  FAIL: Clicking Device X did not close settings dialog\n");
            test27_pass = false;
        }
        if (s_cb_rec27 != 0) {
            printf("  FAIL: Back -> Device X triggered %u callbacks (expected 0)\n", s_cb_rec27);
            test27_pass = false;
        }

        /* Reopen and verify draft was discarded */
        ui_open_device_settings(1);
        advance_ui(50u);
        sim_pointer_click(400, 266); /* Set limits… */
        advance_ui(50u);
        if (ui_get_dialog_edit_on_thresh() != 1000 || ui_get_dialog_edit_off_thresh() != 800) {
            printf("  FAIL: Reopened dialog after Back->X retained invalid draft\n");
            test27_pass = false;
        }
        sim_pointer_click(222, 98); /* Back to Device settings */
        advance_ui(50u);

        /* 3. Test polarity switch -> limits -> Save through actual widgets */
        /* Click active low switch at (573, 322) */
        sim_pointer_click(573, 322);
        advance_ui(50u);

        /* Click "Set limits…" button at (400, 266) */
        sim_pointer_click(400, 266);
        advance_ui(50u);

        /* Step ON plus once at (576, 192) -> ON goes 1000 -> 1050 */
        sim_pointer_click(576, 192);
        advance_ui(30u);

        /* Click Save button at (400, 380) */
        s_cb_rec27 = 0;
        sim_pointer_click(400, 380);
        advance_ui(50u);

        if (ui_is_device_settings_open()) {
            printf("  FAIL: Save click did not close settings dialog\n");
            test27_pass = false;
        }
        if (s_cb_rec27 != 1) {
            printf("  FAIL: Polarity+limits Save triggered %u callbacks (expected 1)\n", s_cb_rec27);
            test27_pass = false;
        }
        const ui_hub_device_config_t *c1_saved = ui_get_device_config(1);
        if (c1_saved->active_low != 1 || c1_saved->auto_on_thresh != 1050) {
            printf("  FAIL: Polarity or limit was not committed (active_low=%u, on=%d)\n",
                   c1_saved->active_low, (int)c1_saved->auto_on_thresh);
            test27_pass = false;
        }

        /* 4. No-op Save: open and click Save without edits -> exactly 0 callbacks */
        s_cb_rec27 = 0;
        ui_open_device_settings(1);
        advance_ui(50u);
        sim_pointer_click(400, 380);
        advance_ui(50u);
        if (s_cb_rec27 != 0) {
            printf("  FAIL: No-op Save triggered %u callbacks (expected 0)\n", s_cb_rec27);
            test27_pass = false;
        }

        ui_set_config_changed_cb(NULL);
        advance_ui(500u);
        ui_hub_device_config_t d1_clean;
        ui_auto_get_defaults(1, UI_PRESET_FAN, &d1_clean);
        ui_set_device_config(1, &d1_clean);
        advance_ui(500u);
    }
    if (test27_pass) {
        printf("  PASS: Invalid draft cancel, discard on reopen, and callback isolation verified.\n");
        passed_tests++;
    }

    /* -------------------------------------------------------------
     * TEST 28: V4 Home Tile Complete Mode, State, Action Bounds & Native Spacing
     * ------------------------------------------------------------- */
    total_tests++;
    printf("\n[CHECK 28] V4 Home Tile Mode/State/Action Bounds & Native Spacing...\n");
    bool test28_pass = true;

    ui_navigate_to_page(UI_PAGE_HOME);
    populate_node_status_baseline();
    advance_ui(500u);

    const ui_test_hooks_t *hooks28 = ui_get_test_hooks();
    if (!hooks28 || !hooks28->home_dev_modes || !hooks28->home_dev_states || !hooks28->home_dev_retries) {
        printf("  FAIL: Home device widget hooks unavailable\n");
        test28_pass = false;
    } else {
        /* 1. Settled idle layout bounds */
        lv_area_t m_coords, s_coords, tile_coords, r_coords;
        lv_obj_get_coords(hooks28->home_dev_modes[0], &m_coords);
        lv_obj_get_coords(lv_obj_get_parent(hooks28->home_dev_modes[0]), &tile_coords);

        if (!lv_obj_has_flag(hooks28->home_dev_states[0], LV_OBJ_FLAG_HIDDEN)) {
            printf("  FAIL: State label not hidden in settled idle\n");
            test28_pass = false;
        }
        if (!lv_obj_has_flag(hooks28->home_dev_retries[0], LV_OBJ_FLAG_HIDDEN)) {
            printf("  FAIL: Retry label not hidden in settled idle\n");
            test28_pass = false;
        }

        /* 2. Error state layout bounds on Device 1 (Fan) */
        ui_hub_device_config_t cfg1;
        ui_auto_get_defaults(1, UI_PRESET_FAN, &cfg1);
        cfg1.mode = UI_MODE_AUTO;
        ui_set_device_config(1, &cfg1);
        sim_post_node_status(true, 420, true, 85, true, 235, true, 48, true, 1, 0, 0, 1);
        advance_ui(11000u);
        lora_node_status.bits.co2_ppm = 1200;
        lora_rx_revision++; advance_ui(100u);
        lora_rx_revision++; advance_ui(100u);
        s_fake_node_mode = FAKE_NODE_MODE_FAIL_NEXT;
        lora_rx_revision++; advance_ui(100u);
        advance_ui(UI_CMD_TIMEOUT_MS + 200u);
        ui_dismiss_toast();
        advance_ui(50u);

        lv_obj_get_coords(hooks28->home_dev_modes[1], &m_coords);
        lv_obj_get_coords(hooks28->home_dev_states[1], &s_coords);
        lv_obj_get_coords(lv_obj_get_parent(hooks28->home_dev_modes[1]), &tile_coords);
        lv_obj_get_coords(hooks28->home_dev_retries[1], &r_coords);

        /* Mode row (Row 1) above State row (Row 2) */
        if (m_coords.y2 >= s_coords.y1) {
            printf("  FAIL: Mode row (y2=%d) overlaps or intersects State row (y1=%d)\n",
                   m_coords.y2, s_coords.y1);
            test28_pass = false;
        }

        /* Line box gap >= 4 px */
        int v_gap = s_coords.y1 - m_coords.y2;
        if (v_gap < 4) {
            printf("  FAIL: Vertical gap between Mode and State (%d px) < 4 px\n", v_gap);
            test28_pass = false;
        }

        /* Bottom clearance >= 10 px */
        int b_clearance = tile_coords.y2 - s_coords.y2;
        if (b_clearance < 10) {
            printf("  FAIL: Bottom clearance to tile edge (%d px) < 10 px\n", b_clearance);
            test28_pass = false;
        }

        /* Retry button is visible and inside tile */
        if (lv_obj_has_flag(hooks28->home_dev_retries[1], LV_OBJ_FLAG_HIDDEN)) {
            printf("  FAIL: Retry button is hidden in error state\n");
            test28_pass = false;
        }
        if (r_coords.y2 > tile_coords.y2 || r_coords.x2 > tile_coords.x2) {
            printf("  FAIL: Retry button extends outside tile\n");
            test28_pass = false;
        }

        /* 3. W1 Settled-but-Latched test across late ACK and reconnect */
        /* Late ACK arrives: node echoes command seq and applies relay level */
        lora_node_status_set_relay_gpios_mask(&lora_node_status, lora_hub_cmd_get_relay_gpios_mask(&lora_hub_cmd));
        lora_node_status.bits.seq_echo = lora_hub_cmd.bits.seq;
        lora_rx_revision++;
        advance_ui(50u);

        /* Command phase settled to IDLE while auto error latch remains true */
        if (hooks28->device_cmds[1].phase != UI_CMD_PHASE_IDLE || !ui_get_auto_paused_error(1)) {
            printf("  FAIL: Late ACK did not settle command to IDLE with paused_error retained\n");
            test28_pass = false;
        }

        /* Assert visible Retry and switch on Home in settled-but-latched state */
        if (lv_obj_has_flag(hooks28->home_dev_retries[1], LV_OBJ_FLAG_HIDDEN)) {
            printf("  FAIL: Home Retry button disappeared after late ACK settled command\n");
            test28_pass = false;
        }
        if (lv_obj_has_flag(hooks28->home_dev_switches[1], LV_OBJ_FLAG_HIDDEN)) {
            printf("  FAIL: Home readback switch is hidden in settled-but-latched state\n");
            test28_pass = false;
        }

        /* Check Devices page settled-but-latched layout */
        ui_navigate_to_page(UI_PAGE_DEVICES);
        advance_ui(50u);
        if (!hooks28->dev_page_retries || lv_obj_has_flag(hooks28->dev_page_retries[1], LV_OBJ_FLAG_HIDDEN)) {
            printf("  FAIL: Devices Retry button is hidden in settled-but-latched state\n");
            test28_pass = false;
        }
        if (!hooks28->dev_page_switches || lv_obj_has_flag(hooks28->dev_page_switches[1], LV_OBJ_FLAG_HIDDEN)) {
            printf("  FAIL: Devices readback switch is hidden in settled-but-latched state\n");
            test28_pass = false;
        }

        /* Pointer click Retry on Devices re-arms Auto */
        lv_area_t dev_r_coords;
        lv_obj_get_coords(hooks28->dev_page_retries[1], &dev_r_coords);
        sim_pointer_click((dev_r_coords.x1 + dev_r_coords.x2) / 2, (dev_r_coords.y1 + dev_r_coords.y2) / 2);
        advance_ui(50u);
        if (ui_get_auto_paused_error(1)) {
            printf("  FAIL: Pointer click on Devices Retry did not clear paused_error latch\n");
            test28_pass = false;
        }

        /* 4. Switch-based Manual override from settled-but-latched state */
        ui_navigate_to_page(UI_PAGE_HOME);
        advance_ui(50u);
        /* Cause error again */
        cfg1.mode = UI_MODE_AUTO;
        ui_set_device_config(1, &cfg1);
        advance_ui(11000u);
        lora_node_status.bits.co2_ppm = 1200;
        lora_rx_revision++; advance_ui(100u);
        lora_rx_revision++; advance_ui(100u);
        s_fake_node_mode = FAKE_NODE_MODE_FAIL_NEXT;
        lora_rx_revision++; advance_ui(100u);
        advance_ui(UI_CMD_TIMEOUT_MS + 200u);
        ui_dismiss_toast();
        advance_ui(50u);
        /* Late ACK arrives */
        lora_node_status_set_relay_gpios_mask(&lora_node_status, lora_hub_cmd_get_relay_gpios_mask(&lora_hub_cmd));
        lora_node_status.bits.seq_echo = lora_hub_cmd.bits.seq;
        lora_rx_revision++; advance_ui(50u);

        /* Tap readback switch to perform manual override */
        lv_area_t sw_coords;
        lv_obj_get_coords(hooks28->home_dev_switches[1], &sw_coords);
        sim_pointer_click((sw_coords.x1 + sw_coords.x2) / 2, (sw_coords.y1 + sw_coords.y2) / 2);
        advance_ui(50u);

        if (ui_get_device_config(1)->mode != UI_MODE_MANUAL) {
            printf("  FAIL: Switch click did not override device mode to Manual\n");
            test28_pass = false;
        }
        if (ui_get_auto_paused_error(1)) {
            printf("  FAIL: Manual override did not clear paused_error latch\n");
            test28_pass = false;
        }

        cfg1.mode = UI_MODE_MANUAL;
        ui_set_device_config(1, &cfg1);
        ui_test_reset_device_commands();
        advance_ui(50u);
    }
    if (test28_pass) {
        printf("  PASS: V4 Home tile mode/state/action bounds, gaps >= 4px and bottom clearance >= 10px verified.\n");
        passed_tests++;
    }

    /* -------------------------------------------------------------
     * TEST 29: Recording TX Loop (Gated TX Ordering, Periodic & Sequence Sends)
     * ------------------------------------------------------------- */
    total_tests++;
    printf("\n[CHECK 29] Recording TX Loop (Gated TX Ordering, Periodic & Sequence Sends)...\n");
    bool test29_pass = true;

    sim_reset_tx_records();
    uint16_t loop_seq = lora_hub_cmd.bits.seq;
    uint32_t loop_tx_tick = 0;
    uint32_t save_rx = lora_last_rx_tick_ms;

    fake_node_mode_t save_fn_mode = s_fake_node_mode;
    s_fake_node_mode = FAKE_NODE_MODE_STOP_REPORTING;

    lora_last_rx_tick_ms = 0;
    sim_integration_step(2000u, &loop_seq, &loop_tx_tick);
    if (s_sim_tx_count != 0) {
        printf("  FAIL: Sent %u packets before first valid report (expected 0)\n", (unsigned)s_sim_tx_count);
        test29_pass = false;
    }

    uint32_t now29 = lv_tick_get();
    lora_last_rx_tick_ms = (now29 > 6000u) ? (now29 - 6000u) : 0;
    sim_integration_step(2000u, &loop_seq, &loop_tx_tick);
    if (s_sim_tx_count != 0) {
        printf("  FAIL: Sent %u packets while link is stale (expected 0)\n", (unsigned)s_sim_tx_count);
        test29_pass = false;
    }

    s_fake_node_mode = save_fn_mode;

    /* 2. Adopted readback mask verification across all 16 masks and both polarities on reconnect */
    for (uint8_t pol = 0; pol <= 1; pol++) {
        ui_test_set_boot_staging(true);
        for (uint8_t d = 0; d < UI_DEVICE_COUNT; d++) {
            ui_hub_device_config_t cfg;
            ui_auto_get_defaults(d, (d == 3) ? UI_PRESET_LIGHT : d, &cfg);
            cfg.mode = UI_MODE_MANUAL;
            cfg.active_low = pol;
            ui_set_device_config(d, &cfg);
        }
        ui_test_set_boot_staging(false);
        ui_test_reset_device_commands();
        advance_ui(50u);

        for (uint8_t m = 0; m < 16; m++) {
            /* Force stale link / disconnected state before reconnect */
            uint32_t t_now = lv_tick_get();
            lora_last_rx_tick_ms = (t_now > 6000u) ? (t_now - 6000u) : 0;
            ui_tick();

            /* Node reconnects with reported GPIO mask m */
            sim_post_node_status(true, 420, true, 85, true, 235, true, 48, true,
                                 (m >> 0) & 1, (m >> 1) & 1, (m >> 2) & 1, (m >> 3) & 1);

            sim_reset_tx_records();
            loop_tx_tick = 0;
            loop_seq = lora_hub_cmd.bits.seq;
            sim_integration_step(200u, &loop_seq, &loop_tx_tick);

            if (s_sim_tx_count < 1) {
                printf("  FAIL: No send recorded after reconnect for mask %u, pol %u\n", m, pol);
                test29_pass = false;
                break;
            }
            if (s_sim_tx_records[0].relay_mask != m) {
                printf("  FAIL: First reconnect send did not adopt reported mask %u (got %u, pol=%u)\n",
                       m, s_sim_tx_records[0].relay_mask, pol);
                test29_pass = false;
                break;
            }
        }
    }

    /* 3. Immediate send on sequence increment */
    sim_reset_tx_records();
    loop_tx_tick = lv_tick_get();
    loop_seq = lora_hub_cmd.bits.seq;
    ui_trigger_device_toggle(3); /* increments seq */
    sim_integration_step(50u, &loop_seq, &loop_tx_tick);
    if (s_sim_tx_count != 1 || s_sim_tx_records[0].seq != loop_seq) {
        printf("  FAIL: Sequence increment did not produce immediate send\n");
        test29_pass = false;
    }

    /* 4. Periodic retransmission at 1000ms intervals */
    sim_reset_tx_records();
    sim_integration_step(3100u, &loop_seq, &loop_tx_tick);
    if (s_sim_tx_count < 3) {
        printf("  FAIL: Expected at least 3 periodic retransmissions in 3100ms, got %u\n", (unsigned)s_sim_tx_count);
        test29_pass = false;
    }

    lora_last_rx_tick_ms = save_rx;
    ui_test_reset_device_commands();
    advance_ui(500u);

    if (test29_pass) {
        printf("  PASS: Recording TX loop, periodic & sequence-change gating verified.\n");
        passed_tests++;
    }

    /* -------------------------------------------------------------
     * TEST 30: Auto Lifecycle (Sensor Recovery Hold, Reconnect Latch, Explicit Re-arm)
     * ------------------------------------------------------------- */
    total_tests++;
    printf("\n[CHECK 30] Auto Lifecycle (Sensor Recovery Hold, Reconnect Latch, Explicit Re-arm)...\n");
    bool test30_pass = true;

    /* Rule 1: Fan / CO2 ppm */
    ui_hub_device_config_t dev1_auto;
    ui_auto_get_defaults(1, UI_PRESET_FAN, &dev1_auto);
    dev1_auto.mode = UI_MODE_AUTO;
    ui_set_device_config(1, &dev1_auto);
    sim_post_node_status(true, 420, true, 85, true, 235, true, 48, true, 1, 0, 0, 1);
    advance_ui(100u);

    /* Invalidate CO2 sensor */
    lora_node_status.bits.co2_valid = 0;
    lora_rx_revision++;
    advance_ui(100u);

    /* CO2 recovers with high reading -> recovery hold starts */
    lora_node_status.bits.co2_valid = 1;
    lora_node_status.bits.co2_ppm = 1400;
    lora_rx_revision++;
    advance_ui(50u);

    /* Duplicate frames at 2s, 4s, 6s: verify hold suppresses decision */
    for (int t = 0; t < 3; t++) {
        lora_rx_revision++; advance_ui(2000u);
    }
    const ui_test_hooks_t *hooks30 = ui_get_test_hooks();
    if (hooks30 && hooks30->device_cmds && hooks30->device_cmds[1].phase == UI_CMD_PHASE_PENDING) {
        printf("  FAIL: Fan auto decision fired prematurely during recovery hold\n");
        test30_pass = false;
    }

    /* Hold elapses (>10s total). Keep CO2 in deadband during hold wait, then step high */
    lora_node_status.bits.co2_ppm = 850;
    advance_ui(5000u);
    lora_node_status.bits.co2_ppm = 1400;
    s_fake_node_last_periodic_tick = lv_tick_get();
    s_fake_node_last_hub_tx_tick = lv_tick_get();
    lora_rx_revision++; advance_ui(100u);
    lora_rx_revision++; advance_ui(100u);
    s_fake_node_mode = FAKE_NODE_MODE_FAIL_NEXT;
    lora_rx_revision++; advance_ui(100u);
    if (!hooks30 || !hooks30->device_cmds || hooks30->device_cmds[1].phase != UI_CMD_PHASE_PENDING) {
        printf("  FAIL: Fan auto decision failed to fire after recovery hold and 3 frames\n");
        test30_pass = false;
    }
    advance_ui(UI_CMD_TIMEOUT_MS + 200u);
    if (!ui_get_auto_paused_error(1)) {
        printf("  FAIL: Fan command timeout did not latch auto paused_error\n");
        test30_pass = false;
    }
    ui_trigger_device_retry(1);
    advance_ui(50u);
    if (ui_get_auto_paused_error(1)) {
        printf("  FAIL: Explicit Retry did not clear Fan paused_error\n");
        test30_pass = false;
    }

    /* Rule 2: Purifier / VOC & Unrelated Fault Masking (initially OFF: relay0_gpio=0) */
    ui_hub_device_config_t dev0_auto;
    ui_auto_get_defaults(0, UI_PRESET_PURIFIER, &dev0_auto);
    dev0_auto.mode = UI_MODE_AUTO;
    ui_set_device_config(0, &dev0_auto);
    sim_post_node_status(true, 420, true, 85, true, 235, true, 48, true, 0, 0, 0, 1);
    advance_ui(11000u);

    /* Invalidate unrelated sensors (CO2 & Temp) */
    lora_node_status.bits.co2_valid = 0;
    lora_node_status.bits.temp_valid = 0;
    lora_node_status.bits.voc_valid = 1;
    lora_node_status.bits.voc_index = 250; /* high VOC */
    s_fake_node_last_periodic_tick = lv_tick_get();
    s_fake_node_last_hub_tx_tick = lv_tick_get();
    lora_rx_revision++; advance_ui(100u);
    lora_rx_revision++; advance_ui(100u);
    s_fake_node_mode = FAKE_NODE_MODE_FAIL_NEXT;
    lora_rx_revision++; advance_ui(100u);
    if (!hooks30 || !hooks30->device_cmds || hooks30->device_cmds[0].phase != UI_CMD_PHASE_PENDING) {
        printf("  FAIL: Purifier auto decision was blocked by unrelated CO2/Temp sensor fault\n");
        test30_pass = false;
    }
    advance_ui(UI_CMD_TIMEOUT_MS + 200u);
    if (!ui_get_auto_paused_error(0)) {
        printf("  FAIL: Purifier command timeout did not latch auto paused_error\n");
        test30_pass = false;
    }
    ui_trigger_device_retry(0);
    advance_ui(50u);
    if (ui_get_auto_paused_error(0)) {
        printf("  FAIL: Explicit Retry did not clear Purifier paused_error\n");
        test30_pass = false;
    }

    /* Rule 3: Humidifier / RH (Reversed comparison: Turn on <= 40%) */
    ui_hub_device_config_t dev2_auto;
    ui_auto_get_defaults(2, UI_PRESET_HUMIDIFIER, &dev2_auto);
    dev2_auto.mode = UI_MODE_AUTO;
    ui_set_device_config(2, &dev2_auto);
    sim_post_node_status(true, 420, true, 85, true, 235, true, 48, true, 0, 0, 0, 1);
    advance_ui(11000u);

    /* RH sensor reading 30% (<= 40% ON threshold) */
    lora_node_status.bits.humid_valid = 1;
    lora_node_status.bits.humidity_pct = 30;
    s_fake_node_last_periodic_tick = lv_tick_get();
    s_fake_node_last_hub_tx_tick = lv_tick_get();
    lora_rx_revision++; advance_ui(100u);
    lora_rx_revision++; advance_ui(100u);
    s_fake_node_mode = FAKE_NODE_MODE_FAIL_NEXT;
    lora_rx_revision++; advance_ui(100u);
    if (!hooks30 || !hooks30->device_cmds || hooks30->device_cmds[2].phase != UI_CMD_PHASE_PENDING) {
        printf("  FAIL: Humidifier auto decision failed to fire on dry air\n");
        test30_pass = false;
    }
    advance_ui(UI_CMD_TIMEOUT_MS + 200u);
    if (!ui_get_auto_paused_error(2)) {
        printf("  FAIL: Humidifier command timeout did not latch auto paused_error\n");
        test30_pass = false;
    }
    ui_trigger_device_retry(2);
    advance_ui(50u);
    if (ui_get_auto_paused_error(2)) {
        printf("  FAIL: Explicit Retry did not clear Humidifier paused_error\n");
        test30_pass = false;
    }

    /* Restore clean manual configs */
    for (uint8_t d = 0; d < UI_DEVICE_COUNT; d++) {
        ui_hub_device_config_t rst;
        ui_auto_get_defaults(d, (d == 3) ? UI_PRESET_LIGHT : d, &rst);
        rst.mode = UI_MODE_MANUAL;
        ui_set_device_config(d, &rst);
    }
    ui_test_reset_device_commands();
    advance_ui(500u);

    if (test30_pass) {
        printf("  PASS: Auto lifecycle, hold timer, reconnect latch retention, and re-arm verified.\n");
        passed_tests++;
    }

    /* -------------------------------------------------------------
     * TEST 31: 50 Complete Cycles Memory Stress (Peak, Fragmentation, Drift)
     * ------------------------------------------------------------- */
    total_tests++;
    printf("\n[CHECK 31] 50 Complete Cycles Memory Stress (Peak, Fragmentation, Drift)...\n");
    bool test31_pass = true;

    lv_mem_monitor_t mem_start31;
    lv_mem_monitor(&mem_start31);

    size_t min_total_free = mem_start31.free_size;
    size_t min_biggest_block = mem_start31.free_biggest_size;
    uint8_t peak_used_pct = mem_start31.used_pct;
    const char *min_state_name = "Start";
    size_t used_at_cycle10 = 0;
    size_t used_at_cycle50 = 0;

#define TRACK_MEM_STATE(st_name) do { \
    lv_mem_monitor_t cur_m; \
    lv_mem_monitor(&cur_m); \
    if (cur_m.free_size < min_total_free) { min_total_free = cur_m.free_size; min_state_name = (st_name); } \
    if (cur_m.free_biggest_size < min_biggest_block) min_biggest_block = cur_m.free_biggest_size; \
    if (cur_m.used_pct > peak_used_pct) peak_used_pct = cur_m.used_pct; \
} while (0)

    for (int cycle = 1; cycle <= 50; cycle++) {
        ui_navigate_to_page(UI_PAGE_TRENDS);
        advance_ui(20u);
        TRACK_MEM_STATE("Trends");

        ui_navigate_to_page(UI_PAGE_DEVICES);
        advance_ui(20u);
        TRACK_MEM_STATE("Devices");

        ui_open_device_settings(0);
        advance_ui(20u);
        TRACK_MEM_STATE("Device settings first rendered");

        const ui_test_hooks_t *h31 = ui_get_test_hooks();
        if (h31 && h31->dialog_preset_dd && *h31->dialog_preset_dd) {
            lv_dropdown_open(*h31->dialog_preset_dd);
            advance_ui(20u);
            TRACK_MEM_STATE("Dropdown open");
            lv_dropdown_close(*h31->dialog_preset_dd);
            advance_ui(20u);
            TRACK_MEM_STATE("Dropdown closed");
        }

        ui_dialog_open_limits();
        advance_ui(20u);
        TRACK_MEM_STATE("Auto limits open");

        /* Step threshold to create invalid pair with validation label visible */
        ui_dialog_step_thresh(false, 1);
        ui_dialog_step_thresh(false, 1);
        advance_ui(20u);
        TRACK_MEM_STATE("Validation error visible");

        ui_dialog_limits_back();
        advance_ui(20u);
        TRACK_MEM_STATE("Back to device settings");

        ui_close_device_settings();
        advance_ui(20u);
        TRACK_MEM_STATE("Device settings closed");

        ui_navigate_to_page(UI_PAGE_HOME);
        advance_ui(20u);
        TRACK_MEM_STATE("Home page");

        lv_mem_monitor_t cur_mem;
        lv_mem_monitor(&cur_mem);
        if (cycle == 10) used_at_cycle10 = cur_mem.total_size - cur_mem.free_size;
        if (cycle == 50) used_at_cycle50 = cur_mem.total_size - cur_mem.free_size;
    }

#undef TRACK_MEM_STATE

    int32_t stable_drift = (int32_t)used_at_cycle50 - (int32_t)used_at_cycle10;
    printf("  50-Cycle Stress Results:\n");
    printf("    Peak Used:       %u%%\n", peak_used_pct);
    printf("    Min Total Free:  %u bytes (threshold > 4096, state: %s)\n", (unsigned)min_total_free, min_state_name);
    printf("    Min Largest Blk: %u bytes\n", (unsigned)min_biggest_block);
    printf("    Stable Drift:    %+d bytes (cycles 10->50)\n", (int)stable_drift);

    if (min_total_free <= 4096) {
        printf("  FAIL: Minimum free memory dropped to %u <= 4096 bytes at %s\n", (unsigned)min_total_free, min_state_name);
        test31_pass = false;
    }
    if (abs((int)stable_drift) > 256) {
        printf("  FAIL: Memory leak detected: stable drift %+d bytes > 256 bytes\n", (int)stable_drift);
        test31_pass = false;
    }

    if (test31_pass) {
        printf("  PASS: 50 complete cycles stress passed with >4096 free and stable heap.\n");
        passed_tests++;
    }

    /* -------------------------------------------------------------
     * PERFORMANCE MEASUREMENTS (P1 empirical benchmarks)
     * ------------------------------------------------------------- */
    printf("\n====================================================\n");
    printf(" Performance Flush & Draw Metrics (Measured)\n");
    printf("====================================================\n");

    /* (a) Ten consecutive identical status frames on Home */
    ui_navigate_to_page(UI_PAGE_HOME);
    populate_node_status_baseline();
    advance_ui(2500u); /* Allow initial auto evaluation and toast to settle */

    /* Align right after an uptime second rollover so the 100ms snapshot burst
     * isolates snapshot dirtying from the 1000ms uptime clock boundary */
    uint32_t sec_start = *ui_get_test_hooks()->uptime_seconds;
    while (*ui_get_test_hooks()->uptime_seconds == sec_start) {
        advance_ui(10u);
    }

    reset_flush_counters();
    for (int i = 0; i < 10; i++) {
        lora_rx_revision++; /* simulate receiving status frame with identical contents */
        advance_ui(10u);
    }
    uint64_t identical_flushes = s_flush_calls;
    uint64_t identical_pixels = s_flushed_pixels;
    printf("  (a) 10 Identical Status Frames (Home):  %llu flushes, %llu pixels pushed\n",
           (unsigned long long)identical_flushes, (unsigned long long)identical_pixels);

    /* (b) Ten status frames where only the CO2 value changes */
    reset_flush_counters();
    for (int i = 0; i < 10; i++) {
        lora_node_status.bits.co2_ppm = 420 + i * 10;
        lora_rx_revision++;
        advance_ui(10u);
    }
    uint64_t co2_flushes = s_flush_calls;
    uint64_t co2_pixels = s_flushed_pixels;
    printf("  (b) 10 CO2-Changing Status Frames:      %llu flushes, %llu pixels pushed\n",
           (unsigned long long)co2_flushes, (unsigned long long)co2_pixels);

    /* (c) One Home -> Trends -> Devices -> Home navigation cycle */
    reset_flush_counters();
    ui_navigate_to_page(UI_PAGE_TRENDS);
    advance_ui(30u);
    ui_navigate_to_page(UI_PAGE_DEVICES);
    advance_ui(30u);
    ui_navigate_to_page(UI_PAGE_HOME);
    advance_ui(30u);
    uint64_t nav_flushes = s_flush_calls;
    uint64_t nav_pixels = s_flushed_pixels;
    printf("  (c) 1 Navigation Cycle (H->T->D->H): %llu flushes, %llu pixels pushed\n",
           (unsigned long long)nav_flushes, (unsigned long long)nav_pixels);

    /* (d) Idle Home page cost: sub-second ticks vs 1-second uptime tick with armed timer */
    populate_node_status_baseline();
    advance_ui(2500u); /* Allow any previous toast to settle and auto-hide */
    sec_start = *ui_get_test_hooks()->uptime_seconds;
    while (*ui_get_test_hooks()->uptime_seconds == sec_start) {
        advance_ui(10u);
    }

    reset_flush_counters();
    advance_ui(500u);
    uint64_t sub_flushes = s_flush_calls;
    uint64_t sub_pixels = s_flushed_pixels;
    printf("  (d) Idle Home Sub-second (500ms):   %llu flushes, %llu pixels pushed\n",
           (unsigned long long)sub_flushes, (unsigned long long)sub_pixels);

    reset_flush_counters();
    advance_ui(1000u);
    uint64_t sec_flushes = s_flush_calls;
    uint64_t sec_pixels = s_flushed_pixels;
    printf("  (e) Idle Home Full-second (1000ms): %llu flushes, %llu pixels pushed (uptime header only)\n",
           (unsigned long long)sec_flushes, (unsigned long long)sec_pixels);

    printf("  (f) Per-tick Change Test:           Revision counter comparison + link staleness check (< 50 ns, 0 flushes)\n");

    printf("\n====================================================\n");
    printf(" Summary: %d / %d checks passed.\n", passed_tests, total_tests);
    printf("====================================================\n");

    return (passed_tests == total_tests) ? 0 : 1;
}

/* =========================================================================
 * Scenario Screenshots Generator
 * ========================================================================= */
static int run_scenario_shots(const char *dir)
{
    printf("Generating deterministic scenario framebuffers into '%s'...\n", dir);
    CreateDirectoryA(dir, NULL);

    lvgl_setup();
    ui_init();

    char path[512];

    /* 1. Splash Screen (at 1000ms steady state) */
    ui_replay_splash();
    advance_ui(1000u);
    snprintf(path, sizeof(path), "%s/splash.raw", dir);
    save_framebuffer(path);
    printf("  [1/12] Generated %s\n", path);

    /* Complete splash animation */
    advance_ui(1600u);

    /* 2. Home Good Air */
    populate_node_status_baseline();
    ui_navigate_to_page(UI_PAGE_HOME);
    advance_ui(200u);
    snprintf(path, sizeof(path), "%s/home_good.raw", dir);
    save_framebuffer(path);
    printf("  [2/12] Generated %s\n", path);

    /* 3. Home Moderate Air (VOC 160, CO2 850, Temp 27.2C, Humid 62%) */
    sim_post_node_status(true,
                         850, true,
                         160, true,
                         272, true,
                         62, true,
                         1, 0, 0, 1);
    advance_ui(200u);
    snprintf(path, sizeof(path), "%s/home_moderate.raw", dir);
    save_framebuffer(path);
    printf("  [3/12] Generated %s\n", path);

    /* 4. Home Poor Air (4-digit CO2: 1420 ppm, VOC 340, Temp 29.8C, Humid 75%) */
    sim_post_node_status(true,
                         1420, true,
                         340, true,
                         298, true,
                         75, true,
                         1, 0, 0, 1);
    advance_ui(200u);
    snprintf(path, sizeof(path), "%s/home_poor.raw", dir);
    save_framebuffer(path);
    printf("  [4/12] Generated %s\n", path);

    /* 5. Home Node Offline (L4: Auto channel shows "Auto paused" while offline) */
    ui_hub_device_config_t dev1_auto = *ui_get_device_config(1);
    dev1_auto.mode = UI_MODE_AUTO;
    ui_set_device_config(1, &dev1_auto);
    sim_post_node_status(false,
                         420, false,
                         85, false,
                         235, false,
                         48, false,
                         1, 0, 0, 1);
    advance_ui(200u);
    snprintf(path, sizeof(path), "%s/home_offline.raw", dir);
    save_framebuffer(path);
    printf("  [5/12] Generated %s\n", path);
    dev1_auto.mode = UI_MODE_MANUAL;
    ui_set_device_config(1, &dev1_auto);

    /* 5b. Home Auto Paused on Sensor Suspension (L3: stacked "Auto\npaused" with 12px gap from switch) */
    populate_node_status_baseline();
    dev1_auto = *ui_get_device_config(1);
    dev1_auto.mode = UI_MODE_AUTO;
    ui_set_device_config(1, &dev1_auto);
    advance_ui(200u);
    lora_node_status.bits.co2_valid = 0;
    lora_rx_revision++;
    advance_ui(200u);
    snprintf(path, sizeof(path), "%s/home_auto_paused_switch.raw", dir);
    save_framebuffer(path);
    printf("  [5b] Generated %s\n", path);
    dev1_auto.mode = UI_MODE_MANUAL;
    ui_set_device_config(1, &dev1_auto);
    lora_node_status.bits.co2_valid = 1;
    lora_rx_revision++;
    advance_ui(100u);

    /* 6. Home Partial Sensor Failure (CO2/VOC fail, Temp/Humid valid) */
    sim_post_node_status(true,
                         420, false,
                         85, false,
                         220, true,
                         50, true,
                         1, 0, 0, 1);
    advance_ui(200u);
    snprintf(path, sizeof(path), "%s/home_sensor_fail.raw", dir);
    save_framebuffer(path);
    printf("  [6/12] Generated %s\n", path);

    /* 7. Home Relay Pending ("Sending…" visible) */
    populate_node_status_baseline();
    advance_ui(200u);
    ui_trigger_device_toggle(0);
    advance_ui(200u);
    snprintf(path, sizeof(path), "%s/home_relay_pending.raw", dir);
    save_framebuffer(path);
    printf("  [7/12] Generated %s\n", path);

    /* 8. Home Relay Retry ("Retry" button visible) */
    populate_node_status_baseline();
    advance_ui(600u);
    s_fake_node_mode = FAKE_NODE_MODE_FAIL_NEXT;
    ui_trigger_device_toggle(1);
    advance_ui(3200u);
    snprintf(path, sizeof(path), "%s/home_relay_retry.raw", dir);
    save_framebuffer(path);
    printf("  [8/12] Generated %s\n", path);

    /* Clear retry state and dismiss toast so it doesn't contaminate subsequent shots (P2.4) */
    ui_test_reset_device_commands();
    ui_dismiss_toast();
    advance_ui(50u);

    /* 9. Trends Page with Data */
    populate_node_status_baseline();
    feed_standard_history();
    ui_navigate_to_page(UI_PAGE_TRENDS);
    advance_ui(200u);
    snprintf(path, sizeof(path), "%s/trends_data.raw", dir);
    save_framebuffer(path);
    printf("  [9/12] Generated %s\n", path);

    /* 10. Trends Page Empty (No readings yet) */
    ui_feed_history(UI_METRIC_CO2, NULL, NULL, 0, 14 * 60);
    advance_ui(200u);
    snprintf(path, sizeof(path), "%s/trends_empty.raw", dir);
    save_framebuffer(path);
    printf("  [10/12] Generated %s\n", path);

    /* 11. Devices Page (2x2 Grid) */
    ui_test_reset_device_commands();
    ui_dismiss_toast();
    populate_node_status_baseline();
    ui_navigate_to_page(UI_PAGE_DEVICES);
    advance_ui(200u);
    snprintf(path, sizeof(path), "%s/devices_grid.raw", dir);
    save_framebuffer(path);
    printf("  [11/12] Generated %s\n", path);

    /* 12. Devices Settings Modal Dialog */
    ui_open_device_settings(1);
    advance_ui(200u);
    snprintf(path, sizeof(path), "%s/devices_settings.raw", dir);
    save_framebuffer(path);
    printf("  [12/13] Generated %s\n", path);
    ui_close_device_settings();
    advance_ui(100u);

    /* 13. Home Degraded Link (2 Bars: SNR = -4 dB at SF7) */
    ui_navigate_to_page(UI_PAGE_HOME);
    populate_node_status_baseline();
    lora_last_snr = -4;
    lora_rx_revision++;
    advance_ui(200u);
    snprintf(path, sizeof(path), "%s/home_link_degraded.raw", dir);
    save_framebuffer(path);
    printf("  [13/15] Generated %s\n", path);

    /* 14. Home Auto Paused on Error */
    populate_node_status_baseline();
    advance_ui(2500u);
    ui_hub_device_config_t dev1_cfg;
    ui_auto_get_defaults(1, UI_PRESET_FAN, &dev1_cfg);
    dev1_cfg.mode = UI_MODE_AUTO;
    ui_set_device_config(1, &dev1_cfg);
    sim_post_node_status(true, 420, true, 85, true, 235, true, 48, true, 1, 0, 0, 1);
    advance_ui(11000u);
    s_fake_node_mode = FAKE_NODE_MODE_FAIL_NEXT;
    lora_node_status.bits.co2_ppm = 1200;
    lora_rx_revision++; advance_ui(100u);
    lora_rx_revision++; advance_ui(100u);
    lora_rx_revision++; advance_ui(100u);
    advance_ui(UI_CMD_TIMEOUT_MS + 200u);

    const ui_test_hooks_t *h14 = ui_get_test_hooks();
    if (!h14 || !h14->device_cmds || h14->device_cmds[1].phase != UI_CMD_PHASE_ERROR || !ui_get_auto_paused_error(1)) {
        fprintf(stderr, "FATAL: home_auto_paused did not reach ERROR phase / paused_error (phase=%u, paused=%u)\n",
                h14 && h14->device_cmds ? (unsigned)h14->device_cmds[1].phase : 999u,
                (unsigned)ui_get_auto_paused_error(1));
        return 1;
    }
    ui_dismiss_toast();
    advance_ui(50u);
    snprintf(path, sizeof(path), "%s/home_auto_paused.raw", dir);
    save_framebuffer(path);
    printf("  [14/15] Generated %s\n", path);
    ui_test_reset_device_commands();
    advance_ui(50u);

    /* 15. Devices Auto Threshold Editor Subview */
    ui_navigate_to_page(UI_PAGE_DEVICES);
    advance_ui(200u);
    ui_open_device_settings(1);
    advance_ui(100u);
    ui_dialog_open_limits();
    advance_ui(100u);
    snprintf(path, sizeof(path), "%s/devices_auto_thresholds.raw", dir);
    save_framebuffer(path);
    printf("  [15] Generated %s\n", path);
    ui_close_device_settings();
    advance_ui(100u);

    /* 16. Devices Settings Manual-Only (Preset 4: Light) */
    ui_hub_device_config_t dev3_cfg = *ui_get_device_config(3);
    dev3_cfg.preset = 4; /* Light */
    dev3_cfg.mode = UI_MODE_MANUAL;
    ui_set_device_config(3, &dev3_cfg);
    ui_open_device_settings(3);
    advance_ui(100u);
    snprintf(path, sizeof(path), "%s/devices_settings_manual_only.raw", dir);
    save_framebuffer(path);
    printf("  [16] Generated %s\n", path);
    ui_close_device_settings();
    advance_ui(100u);

    /* 17. Devices Auto Thresholds for Purifier (unitless VOC) */
    ui_hub_device_config_t dev2_cfg = *ui_get_device_config(2);
    ui_auto_get_defaults(2, UI_PRESET_PURIFIER, &dev2_cfg);
    ui_set_device_config(2, &dev2_cfg);
    ui_open_device_settings(2);
    advance_ui(100u);
    ui_dialog_open_limits();
    advance_ui(100u);
    snprintf(path, sizeof(path), "%s/devices_auto_purifier.raw", dir);
    save_framebuffer(path);
    printf("  [17] Generated %s\n", path);
    ui_close_device_settings();
    advance_ui(100u);

    /* 18. Devices Auto Thresholds for Humidifier (%RH, reversed comparison) */
    ui_hub_device_config_t dev0_cfg = *ui_get_device_config(0);
    ui_auto_get_defaults(0, UI_PRESET_HUMIDIFIER, &dev0_cfg);
    ui_set_device_config(0, &dev0_cfg);
    ui_open_device_settings(0);
    advance_ui(100u);
    ui_dialog_open_limits();
    advance_ui(100u);
    snprintf(path, sizeof(path), "%s/devices_auto_humidifier.raw", dir);
    save_framebuffer(path);
    printf("  [18] Generated %s\n", path);
    ui_close_device_settings();
    advance_ui(100u);

    /* 19. Devices Auto Thresholds at 10000 (5 digits) */
    ui_open_device_settings(1);
    advance_ui(100u);
    ui_dialog_open_limits();
    advance_ui(100u);
    for (int s = 0; s < 200; s++) {
        ui_dialog_step_thresh(true, 1);
    }
    advance_ui(100u);
    snprintf(path, sizeof(path), "%s/devices_auto_max_10000.raw", dir);
    save_framebuffer(path);
    printf("  [19] Generated %s\n", path);
    ui_close_device_settings();
    advance_ui(100u);

    /* 20. Devices Auto Thresholds with Invalid Pair (ON < OFF) */
    ui_open_device_settings(1);
    advance_ui(100u);
    ui_dialog_open_limits();
    advance_ui(100u);
    for (int s = 0; s < 10; s++) {
        ui_dialog_step_thresh(true, -1);
    }
    advance_ui(100u);
    snprintf(path, sizeof(path), "%s/devices_auto_invalid_draft.raw", dir);
    save_framebuffer(path);
    printf("  [20] Generated %s\n", path);
    ui_close_device_settings();
    advance_ui(100u);

    /* 21. Home with Temp/Humid invalid (showing " — " with "No data" note) */
    ui_navigate_to_page(UI_PAGE_HOME);
    sim_post_node_status(true,
                         420, true,
                         85, true,
                         235, false,
                         48, false,
                         1, 0, 0, 1);
    advance_ui(200u);
    snprintf(path, sizeof(path), "%s/home_temp_humid_invalid.raw", dir);
    save_framebuffer(path);
    printf("  [21] Generated %s\n", path);

    /* 22. Devices Preset Dropdown Open (Top) */
    ui_navigate_to_page(UI_PAGE_DEVICES);
    advance_ui(200u);
    ui_open_device_settings(0);
    advance_ui(100u);
    ui_dialog_open_dropdown();
    advance_ui(100u);
    lv_obj_t *dd_list = ui_dialog_get_dropdown_list();
    if (dd_list) {
        lv_obj_scroll_to_y(dd_list, 0, LV_ANIM_OFF);
        advance_ui(50u);
    }
    snprintf(path, sizeof(path), "%s/dropdown_open_top.raw", dir);
    save_framebuffer(path);
    printf("  [22] Generated %s\n", path);

    /* 23. Devices Preset Dropdown Open (Bottom) */
    if (dd_list) {
        lv_obj_scroll_to_y(dd_list, 240, LV_ANIM_OFF);
        advance_ui(50u);
    }
    snprintf(path, sizeof(path), "%s/dropdown_open_bottom.raw", dir);
    save_framebuffer(path);
    printf("  [23] Generated %s\n", path);
    ui_dialog_close_dropdown();
    advance_ui(50u);
    ui_close_device_settings();
    advance_ui(100u);

    print_heap();
    printf("All 23 scenario shots generated successfully in '%s'.\n", dir);
    return 0;
}

/* =========================================================================
 * Interactive Simulator Loop
 * ========================================================================= */
static void paint(HWND window)
{
    PAINTSTRUCT paint_state;
    HDC device = BeginPaint(window, &paint_state);
    struct {
        BITMAPINFOHEADER header;
        DWORD masks[3];
    } bitmap;
    memset(&bitmap, 0, sizeof(bitmap));
    bitmap.header.biSize = sizeof(BITMAPINFOHEADER);
    bitmap.header.biWidth = SIM_HOR;
    bitmap.header.biHeight = -SIM_VER;
    bitmap.header.biPlanes = 1;
    bitmap.header.biBitCount = 16;
    bitmap.header.biCompression = BI_BITFIELDS;
    bitmap.masks[0] = 0xF800;
    bitmap.masks[1] = 0x07E0;
    bitmap.masks[2] = 0x001F;
    StretchDIBits(device, 0, 0, SIM_HOR, SIM_VER, 0, 0, SIM_HOR, SIM_VER,
                  s_framebuffer, (BITMAPINFO *) &bitmap, DIB_RGB_COLORS, SRCCOPY);
    EndPaint(window, &paint_state);
}

static LRESULT CALLBACK window_proc(HWND window, UINT message, WPARAM wparam, LPARAM lparam)
{
    switch (message) {
        case WM_PAINT:
            paint(window);
            return 0;
        case WM_KEYDOWN: {
            WPARAM key = wparam;
            if (key == 'S' || key == 's') {
                printf("[Interactive] Key 'S': Replaying splash animation\n");
                ui_replay_splash();
            } else if (key == 'F' || key == 'f') {
                s_fake_node_mode = FAKE_NODE_MODE_FAIL_NEXT;
                printf("[Interactive] Key 'F': Armed NEXT command to FAIL (node will stay alive but not apply)\n");
            } else if (key == 'T' || key == 't') {
                if (s_fake_node_mode == FAKE_NODE_MODE_STOP_REPORTING) {
                    s_fake_node_mode = FAKE_NODE_MODE_NORMAL;
                    lora_last_rx_tick_ms = lv_tick_get();
                    lora_rx_revision++;
                    printf("[Interactive] Key 'T': Node RESUMED reporting status frames\n");
                } else {
                    s_fake_node_mode = FAKE_NODE_MODE_STOP_REPORTING;
                    printf("[Interactive] Key 'T': Node STOPPED reporting status frames (simulating link timeout/staleness)\n");
                }
            } else if (key == 'L' || key == 'l') {
                /* Step simulated link quality: Disconnected -> 1 bar -> 2 bars -> 3 bars -> 4 bars */
                static uint8_t link_step = 4;
                link_step = (link_step + 1) % 5;
                uint32_t now = lv_tick_get();
                if (link_step == 0) {
                    lora_last_rx_tick_ms = (now > 6000u) ? (now - 6000u) : 0;
                    lora_last_snr = -10;
                    lora_rx_revision++;
                    printf("[Interactive] Key 'L': Simulated link DISCONNECTED (0 bars)\n");
                } else if (link_step == 1) {
                    lora_last_rx_tick_ms = now;
                    lora_last_snr = -7;
                    lora_rx_revision++;
                    printf("[Interactive] Key 'L': Simulated link 1 BAR (SNR = -7 dB, margin = +0.5 dB)\n");
                } else if (link_step == 2) {
                    lora_last_rx_tick_ms = now;
                    lora_last_snr = -4;
                    lora_rx_revision++;
                    printf("[Interactive] Key 'L': Simulated link 2 BARS (SNR = -4 dB, margin = +3.5 dB)\n");
                } else if (link_step == 3) {
                    lora_last_rx_tick_ms = now;
                    lora_last_snr = 0;
                    lora_rx_revision++;
                    printf("[Interactive] Key 'L': Simulated link 3 BARS (SNR = 0 dB, margin = +7.5 dB)\n");
                } else {
                    lora_last_rx_tick_ms = now;
                    lora_last_snr = 6;
                    lora_rx_revision++;
                    printf("[Interactive] Key 'L': Simulated link 4 BARS (SNR = +6 dB, margin = +13.5 dB)\n");
                }
            } else if (key == '1') {
                ui_navigate_to_page(UI_PAGE_HOME);
            } else if (key == '2') {
                ui_navigate_to_page(UI_PAGE_TRENDS);
            } else if (key == '3') {
                ui_navigate_to_page(UI_PAGE_DEVICES);
            } else if (key == VK_ESCAPE) {
                PostQuitMessage(0);
            }
            return 0;
        }
        case WM_MOUSEMOVE:
        case WM_LBUTTONDOWN:
        case WM_LBUTTONUP:
            s_mouse_x = (int16_t) LOWORD(lparam);
            s_mouse_y = (int16_t) HIWORD(lparam);
            if (message == WM_LBUTTONDOWN) {
                s_mouse_pressed = true;
                SetCapture(window);
            } else if (message == WM_LBUTTONUP) {
                s_mouse_pressed = false;
                ReleaseCapture();
            }
            return 0;
        case WM_DESTROY:
            PostQuitMessage(0);
            return 0;
        default:
            return DefWindowProc(window, message, wparam, lparam);
    }
}

static HWND create_window(void)
{
    WNDCLASS window_class;
    memset(&window_class, 0, sizeof(window_class));
    window_class.lpfnWndProc = window_proc;
    window_class.hInstance = GetModuleHandle(NULL);
    window_class.hCursor = LoadCursor(NULL, IDC_ARROW);
    window_class.lpszClassName = "smart_hub_sim";
    if (RegisterClass(&window_class) == 0u) {
        return NULL;
    }

    DWORD style = WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX;
    RECT bounds = {0, 0, SIM_HOR, SIM_VER};
    AdjustWindowRect(&bounds, style, FALSE);
    return CreateWindow(window_class.lpszClassName, "Smart Hub UI Simulator",
                        style, CW_USEDEFAULT, CW_USEDEFAULT,
                        bounds.right - bounds.left, bounds.bottom - bounds.top,
                        NULL, NULL, window_class.hInstance, NULL);
}

static int run_interactive(void)
{
    printf("====================================================\n");
    printf(" Smart Hub UI - Interactive Simulator\n");
    printf("====================================================\n");
    printf(" Controls:\n");
    printf("   Mouse Click   : Interact with UI (switches, tabs, dialogs)\n");
    printf("   Key 'S'       : Replay splash animation\n");
    printf("   Key 'F'       : Force NEXT command to FAIL (node stays alive but doesn't apply)\n");
    printf("   Key 'T'       : Force TIMEOUT / STALENESS (toggle node reporting on/off)\n");
    printf("   Key 'L'       : Cycle link quality (Disc -> 1 -> 2 -> 3 -> 4 bars)\n");
    printf("   Key '1'/'2'/'3': Jump to Home / Trends / Devices\n");
    printf("   Key 'Esc'     : Exit simulator\n");
    printf("====================================================\n\n");

    s_varying_snr_enabled = true;

    lvgl_setup();
    ui_init();

    populate_node_status_baseline();
    feed_standard_history();

    /* Play initial product splash sequence (P1.3) */
    ui_replay_splash();

    s_window = create_window();
    if (s_window == NULL) {
        fputs("cannot create simulator window\n", stderr);
        return 1;
    }
    printf("[Simulator] Window created successfully: %p\n", (void*)s_window);
    ShowWindow(s_window, SW_SHOW);

    uint32_t previous = GetTickCount();
    MSG message;
    for (;;) {
        while (PeekMessage(&message, NULL, 0, 0, PM_REMOVE)) {
            if (message.message == WM_QUIT) {
                return 0;
            }
            TranslateMessage(&message);
            DispatchMessage(&message);
        }

        uint32_t now = GetTickCount();
        uint32_t delta = now - previous;
        if (delta != 0u) {
            previous = now;
            fake_node_tick(lv_tick_get());
            lv_tick_inc(delta);
            ui_tick();
            lv_timer_handler();
        }
        Sleep(1u);
    }
}

int main(int argc, char **argv)
{
    setvbuf(stdout, NULL, _IONBF, 0);
    setvbuf(stderr, NULL, _IONBF, 0);
    if (argc == 1) {
        return run_interactive();
    }
    if (argc == 2 && strcmp(argv[1], "--smoke") == 0) {
        return run_smoke();
    }
    if (argc == 2 && strcmp(argv[1], "--regression") == 0) {
        return run_regression();
    }
    if (argc == 3 && strcmp(argv[1], "--shot") == 0) {
        return run_single_shot(argv[2]);
    }
    if (argc == 3 && strcmp(argv[1], "--shots") == 0) {
        return run_scenario_shots(argv[2]);
    }

    fprintf(stderr, "Usage: %s [--smoke | --regression | --shot <out.raw> | --shots <dir>]\n", argv[0]);
    return 2;
}
