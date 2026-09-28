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
/* Wire Globals are defined in ui.c (motor_comm convention) and declared in lora_comm.h / lora_hub_link.h */

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
static uint32_t s_fake_node_cmd_recv_tick = 0;
static bool     s_fake_node_cmd_in_flight = false;
static uint8_t  s_fake_node_target_gpios = 0;
static uint16_t s_fake_node_target_seq = 0;
static uint16_t s_fake_node_failed_seq = 0;
static bool     s_fake_node_drop_next_frame = false;
static uint32_t s_fake_node_last_periodic_tick = 0;
static bool     s_varying_snr_enabled = false;
static int      s_r6_config_cb_count = 0;

static void r6_config_changed_probe_cb(uint8_t dev_idx, const ui_hub_device_config_t *cfg)
{
    (void)dev_idx;
    (void)cfg;
    s_r6_config_cb_count++;
}

static void fake_node_receive_cmd(uint32_t now, const lora_hub_cmd_t *cmd)
{
    if (s_fake_node_drop_next_frame) {
        /* Simulate single command frame dropped in transit */
        s_fake_node_drop_next_frame = false;
        return;
    }
    if (cmd->bits.version != LORA_PROTO_VERSION) {
        return;
    }
    if (cmd->bits.msg_id == LORA_MSG_GET_STATUS) {
        if (s_fake_node_mode != FAKE_NODE_MODE_STOP_REPORTING) {
            lora_last_rx_tick_ms = now;
            lora_rx_revision++;
        }
        return;
    }
    if (cmd->bits.msg_id != LORA_MSG_HUB_CMD) {
        return;
    }
    uint8_t hub_gpios = lora_hub_cmd_get_relay_gpios_mask(cmd);
    uint8_t node_gpios = lora_node_status_get_relay_gpios_mask(&lora_node_status);
    bool is_actual_cmd = (hub_gpios != node_gpios || lora_node_status.bits.seq_echo != cmd->bits.seq);

    if (s_fake_node_mode == FAKE_NODE_MODE_FAIL_NEXT) {
        if (is_actual_cmd) {
            s_fake_node_failed_seq = cmd->bits.seq;
            s_fake_node_mode = FAKE_NODE_MODE_NORMAL;
        }
        return;
    }
    if (cmd->bits.seq == s_fake_node_failed_seq && s_fake_node_failed_seq != 0) {
        /* Re-transmission of failed sequence is also ignored by node */
        return;
    }
    /* Level-triggered: applies levels from received command frame */
    if (is_actual_cmd) {
        s_fake_node_cmd_in_flight = true;
        s_fake_node_cmd_recv_tick = now;
        s_fake_node_target_seq = cmd->bits.seq;
        s_fake_node_target_gpios = hub_gpios;
    }
}

static void fake_node_tick(uint32_t now)
{

    /* Apply queued command after 700 ms */
    if (s_fake_node_cmd_in_flight && (now - s_fake_node_cmd_recv_tick >= 700u)) {
        s_fake_node_cmd_in_flight = false;
        if (s_fake_node_mode != FAKE_NODE_MODE_STOP_REPORTING) {
            lora_node_status_set_relay_gpios_mask(&lora_node_status, s_fake_node_target_gpios);
            lora_node_status.bits.seq_echo = s_fake_node_target_seq;
            lora_node_status.bits.sample_seq++;
            lora_last_rx_tick_ms = now;
            lora_rx_revision++;
        }
    }

    /* Periodic status reporting: send frame every 1000 ms */
    if (s_fake_node_mode != FAKE_NODE_MODE_STOP_REPORTING) {
        if (s_fake_node_last_periodic_tick == 0 || (now - s_fake_node_last_periodic_tick >= 1000u)) {
            s_fake_node_last_periodic_tick = now;
            lora_node_status.bits.uptime_sec++;
            lora_node_status.bits.sample_seq++;
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

static bool s_sim_async_flush_delay = false;
static lv_disp_drv_t *s_async_pending_drv = NULL;

static void sim_async_flush_complete(void)
{
    if (s_async_pending_drv != NULL) {
        lv_disp_drv_t *d = s_async_pending_drv;
        s_async_pending_drv = NULL;
        lv_disp_flush_ready(d);
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

    if (s_sim_async_flush_delay && driver->draw_buf && driver->draw_buf->flushing_last) {
        s_async_pending_drv = driver;
        /* Do NOT call lv_disp_flush_ready yet on last slice: simulate in-flight QSPI DMA */
    } else {
        lv_disp_flush_ready(driver);
    }
}

static void pointer_read_cb(lv_indev_drv_t *driver, lv_indev_data_t *data)
{
    (void) driver;
    data->point.x = s_mouse_x;
    data->point.y = s_mouse_y;
    data->state = s_mouse_pressed ? LV_INDEV_STATE_PRESSED : LV_INDEV_STATE_RELEASED;
}

/* =========================================================================
 * Allocation Tracking Wrapper (E1 RAM proof)
 * ========================================================================= */
static size_t   s_alloc_largest_request = 0;
static uint32_t s_alloc_failure_count   = 0;
static size_t   s_alloc_transient_min_free = SIZE_MAX;
static size_t   s_alloc_transient_contemp_biggest = 0;

void * __real_lv_mem_alloc(size_t size);
void * __real_lv_mem_realloc(void * data, size_t new_size);

void * __wrap_lv_mem_alloc(size_t size)
{
    if (size > s_alloc_largest_request) {
        s_alloc_largest_request = size;
    }
    void * ptr = __real_lv_mem_alloc(size);
    if (!ptr && size > 0) {
        s_alloc_failure_count++;
    }
    if (lv_is_initialized()) {
        lv_mem_monitor_t mon;
        lv_mem_monitor(&mon);
        if (mon.free_size < s_alloc_transient_min_free) {
            s_alloc_transient_min_free = mon.free_size;
            s_alloc_transient_contemp_biggest = mon.free_biggest_size;
        }
    }
    return ptr;
}

void * __wrap_lv_mem_realloc(void * data, size_t new_size)
{
    if (new_size > s_alloc_largest_request) {
        s_alloc_largest_request = new_size;
    }
    void * ptr = __real_lv_mem_realloc(data, new_size);
    if (!ptr && new_size > 0) {
        s_alloc_failure_count++;
    }
    if (lv_is_initialized()) {
        lv_mem_monitor_t mon;
        lv_mem_monitor(&mon);
        if (mon.free_size < s_alloc_transient_min_free) {
            s_alloc_transient_min_free = mon.free_size;
            s_alloc_transient_contemp_biggest = mon.free_biggest_size;
        }
    }
    return ptr;
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

/* =========================================================================
 * Host TX Recording Structures & Integration Loop Stub (W3/X3/Y3)
 * ========================================================================= */
#define s_sim_tx_records (sim_get_tx_records())
#define s_sim_tx_count (sim_get_tx_count())

typedef enum {
    SIM_TX_FAULT_NONE = 0,
    SIM_TX_FAULT_GATING_BYPASS,
    SIM_TX_FAULT_TOO_FAST_RATE,
    SIM_TX_FAULT_CORRUPT_HEADER,
    SIM_TX_FAULT_OMITTED_PERIODIC
} sim_tx_fault_t;

static bool validate_tx_log(const sim_tx_record_t *records, size_t count, bool tx_ready_during_stream)
{
    if (count == 0) return true;
    if (!tx_ready_during_stream) {
        return false; /* Fault: Sent packets when TX was not ready */
    }
    for (size_t i = 0; i < count; i++) {
        if (!records[i].tx_ready_at_send) {
            return false; /* Fault: Sent packet when ui_is_tx_ready was false (gating bypass) */
        }
        /* Check protocol header (byte 0 = version, byte 1 = msg_id) */
        if (records[i].bytes[0] != LORA_PROTO_VERSION ||
            (records[i].bytes[1] != LORA_MSG_HUB_CMD && records[i].bytes[1] != LORA_MSG_GET_STATUS)) {
            return false; /* Fault: Corrupt packet header */
        }
        /* Check interval between periodic transmissions of same sequence */
        if (i > 0 && records[i].seq == records[i - 1].seq) {
            uint32_t dt = records[i].tick_ms - records[i - 1].tick_ms;
            if (dt < 980u) {
                return false; /* Fault: Periodic retransmission occurred too quickly (< 980 ms) */
            }
            if (dt > 1100u) {
                return false; /* Fault: Periodic retransmission delayed excessively (> 1100 ms) */
            }
        }
    }
    return true;
}

static void sim_integration_step_with_fault(uint32_t step_ms, uint16_t *io_last_seq, uint32_t *io_last_tx_tick, sim_tx_fault_t fault)
{
    for (uint32_t el = 0; el < step_ms; el += 10u) {
        uint32_t now = lv_tick_get();
        fake_node_tick(now);
        lv_tick_inc(10u);
        ui_tick();
        lv_timer_handler();

        /* Gated Outbound TX: send only when ui_is_tx_ready() unless fault injected */
        bool tx_ready = ui_is_tx_ready();
        if (fault == SIM_TX_FAULT_GATING_BYPASS) {
            tx_ready = true; /* Fault: Gating bypass */
        }

        uint32_t cur_now = lv_tick_get();
        uint32_t period_thresh = 1000u;
        if (fault == SIM_TX_FAULT_TOO_FAST_RATE) {
            period_thresh = 500u;
        } else if (fault == SIM_TX_FAULT_OMITTED_PERIODIC) {
            period_thresh = 2000u;
        }

        bool seq_changed = (lora_hub_cmd.bits.seq != *io_last_seq);
        bool periodic_due = (*io_last_tx_tick == 0) || (cur_now - *io_last_tx_tick >= period_thresh);

        if (tx_ready && ui_has_pending_command()) {
            if (seq_changed || periodic_due) {
                lora_hub_cmd_t outgoing = lora_hub_cmd;
                outgoing.bits.msg_id = LORA_MSG_HUB_CMD;
                if (fault == SIM_TX_FAULT_CORRUPT_HEADER) {
                    outgoing.bytes[0] = 0xAA; /* Corrupt version */
                }
                sim_record_tx(cur_now, &outgoing);
                fake_node_receive_cmd(cur_now, &outgoing);
                *io_last_seq = lora_hub_cmd.bits.seq;
                *io_last_tx_tick = cur_now;
            }
        } else {
            if (periodic_due) {
                lora_hub_cmd_t poll_msg = lora_hub_cmd;
                poll_msg.bits.version = LORA_PROTO_VERSION;
                poll_msg.bits.msg_id = LORA_MSG_GET_STATUS;
                poll_msg.bits.node_addr = LORA_DEFAULT_NODE_ADDR;
                if (fault == SIM_TX_FAULT_CORRUPT_HEADER) {
                    poll_msg.bytes[0] = 0xAA;
                }
                if (tx_ready || fault == SIM_TX_FAULT_GATING_BYPASS) {
                    sim_record_tx(cur_now, &poll_msg);
                    *io_last_seq = lora_hub_cmd.bits.seq;
                    *io_last_tx_tick = cur_now;
                }
                fake_node_receive_cmd(cur_now, &poll_msg);
                *io_last_tx_tick = cur_now;
            }
        }
    }
    lv_refr_now(NULL);
}

static void sim_integration_step(uint32_t step_ms, uint16_t *io_last_seq, uint32_t *io_last_tx_tick)
{
    sim_integration_step_with_fault(step_ms, io_last_seq, io_last_tx_tick, SIM_TX_FAULT_NONE);
}

static void advance_ui(uint32_t milliseconds)
{
    static uint16_t s_adv_last_seq = 0;
    static uint32_t s_adv_last_tx_tick = 0;
    sim_integration_step(milliseconds, &s_adv_last_seq, &s_adv_last_tx_tick);
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
    lora_node_status.bits.tvoc_ppb = voc;
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
    lora_node_status.bits.sample_seq++;
    lora_node_status.bits.uptime_sec = now / 1000u;
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

/* Native typography contract: never shrink text to make a layout pass. */
static bool audit_visible_fonts(lv_obj_t *obj)
{
    if (!obj || lv_obj_has_flag(obj, LV_OBJ_FLAG_HIDDEN)) return true;
    bool ok = true;
    if (lv_obj_check_type(obj, &lv_label_class) || lv_obj_check_type(obj, &lv_dropdown_class)) {
        const lv_font_t *font = lv_obj_get_style_text_font(obj, LV_PART_MAIN);
        if (font != &ui_font_18 && font != &ui_font_24 &&
            font != &ui_font_34 && font != &ui_font_48 && font != &ui_font_digits_62) {
            printf("  Font audit: visible text uses a font outside the >=18px contract\n");
            ok = false;
        }
    }
    for (uint32_t i = 0; i < lv_obj_get_child_cnt(obj); i++)
        if (!audit_visible_fonts(lv_obj_get_child(obj, i))) ok = false;
    return ok;
}

static bool audit_visible_glyph_containment(lv_obj_t *obj)
{
    if (!obj || lv_obj_has_flag(obj, LV_OBJ_FLAG_HIDDEN)) return true;
    bool ok = true;

    /* 1. Ancestor containment audit for labels:
     * Walk through non-scrollable ancestors up to screen root.
     * Verify label glyph bounding box remains strictly within every ancestor in both X and Y.
     */
    if (lv_obj_check_type(obj, &lv_label_class)) {
        lv_area_t la;
        lv_obj_get_coords(obj, &la);
        for (lv_obj_t *anc = lv_obj_get_parent(obj); anc != NULL && anc != lv_scr_act(); anc = lv_obj_get_parent(anc)) {
            if (lv_obj_has_flag(anc, LV_OBJ_FLAG_HIDDEN)) break;
            /* Intentional scroll containers (e.g. dropdown lists) allow content outside viewport */
            if (lv_obj_has_flag(anc, LV_OBJ_FLAG_SCROLLABLE)) continue;
            lv_area_t pa;
            lv_obj_get_coords(anc, &pa);
            if (la.x1 < pa.x1 - 2 || la.x2 > pa.x2 + 2 ||
                la.y1 < pa.y1 - 2 || la.y2 > pa.y2 + 2) {
                printf("  Glyph containment: visible label '%s' ([%d..%d, %d..%d]) extends outside ancestor ([%d..%d, %d..%d])\n",
                       lv_label_get_text(obj), la.x1, la.x2, la.y1, la.y2, pa.x1, pa.x2, pa.y1, pa.y2);
                ok = false;
                break;
            }
        }
    }

    /* 2. Sibling collision audit:
     * Check direct siblings under this container for 2D bounding overlap.
     * Rejects collisions between stats, switches over names, summaries under chevrons, etc.
     */
    uint32_t cnt = lv_obj_get_child_cnt(obj);
    if (cnt >= 2) {
        for (uint32_t i = 0; i < cnt; i++) {
            lv_obj_t *c1 = lv_obj_get_child(obj, i);
            if (lv_obj_has_flag(c1, LV_OBJ_FLAG_HIDDEN)) continue;
            lv_area_t a1;
            lv_obj_get_coords(c1, &a1);
            if (a1.x2 <= a1.x1 || a1.y2 <= a1.y1) continue;
            /* Ignore thin 1px/2px divider lines */
            if ((a1.y2 - a1.y1 <= 2) || (a1.x2 - a1.x1 <= 2)) continue;

            for (uint32_t j = i + 1; j < cnt; j++) {
                lv_obj_t *c2 = lv_obj_get_child(obj, j);
                if (lv_obj_has_flag(c2, LV_OBJ_FLAG_HIDDEN)) continue;
                lv_area_t a2;
                lv_obj_get_coords(c2, &a2);
                if (a2.x2 <= a2.x1 || a2.y2 <= a2.y1) continue;
                if ((a2.y2 - a2.y1 <= 2) || (a2.x2 - a2.x1 <= 2)) continue;

                int ox1 = (a1.x1 > a2.x1) ? a1.x1 : a2.x1;
                int ox2 = (a1.x2 < a2.x2) ? a1.x2 : a2.x2;
                int oy1 = (a1.y1 > a2.y1) ? a1.y1 : a2.y1;
                int oy2 = (a1.y2 < a2.y2) ? a1.y2 : a2.y2;

                if (ox2 > ox1 + 2 && oy2 > oy1 + 2) {
                    const char *t1 = lv_obj_check_type(c1, &lv_label_class) ? lv_label_get_text(c1) : "widget";
                    const char *t2 = lv_obj_check_type(c2, &lv_label_class) ? lv_label_get_text(c2) : "widget";
                    printf("  Glyph collision: sibling '%s' ([%d..%d, %d..%d]) overlaps sibling '%s' ([%d..%d, %d..%d])\n",
                           t1, a1.x1, a1.x2, a1.y1, a1.y2, t2, a2.x1, a2.x2, a2.y1, a2.y2);
                    ok = false;
                }
            }
        }
    }

    for (uint32_t i = 0; i < cnt; i++) {
        if (!audit_visible_glyph_containment(lv_obj_get_child(obj, i))) ok = false;
    }
    return ok;
}

static lv_obj_t *find_visible_label(lv_obj_t *obj, const char *text)
{
    if (!obj || lv_obj_has_flag(obj, LV_OBJ_FLAG_HIDDEN)) return NULL;
    if (lv_obj_check_type(obj, &lv_label_class) &&
        strcmp(lv_label_get_text(obj), text) == 0) return obj;
    for (uint32_t i = 0; i < lv_obj_get_child_cnt(obj); i++) {
        lv_obj_t *found = find_visible_label(lv_obj_get_child(obj, i), text);
        if (found) return found;
    }
    return NULL;
}

static void click_object_center(lv_obj_t *obj)
{
    lv_area_t bounds;
    lv_obj_get_coords(obj, &bounds);
    sim_pointer_click((bounds.x1 + bounds.x2) / 2, (bounds.y1 + bounds.y2) / 2);
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
 * Comprehensive Regression Test Suite
 * ========================================================================= */
static int run_regression(void)
{
    printf("====================================================\n");
    printf(" Smart Hub UI - Comprehensive Regression Test Suite \n");
    printf("====================================================\n");

    CreateDirectoryA("build_shots", NULL);
    lvgl_setup();
    sim_set_tx_sink(fake_node_receive_cmd);

    /* Run pre-ui_init Flash restore fixture BEFORE ui_init */
    if (!test_restore_pre_init_fixture()) {
        fprintf(stderr, "FATAL: Pre-ui_init Flash restore fixture failed!\n");
        return 1;
    }

    ui_init();

    /* Assert restored configs survived ui_init, 0 callbacks, initial TX blocked, and first report adoption */
    if (!test_restore_post_init_assertion()) {
        fprintf(stderr, "FATAL: Post-ui_init Flash restore assertion failed!\n");
        return 1;
    }

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

    /* Y1: Production wrap-safe link timing and boot tick 0 sentinel */
    uint32_t save_rx1 = lora_last_rx_tick_ms;

    /* 1. Fresh/stale boundary across wrap (last RX = UINT32_MAX - 1000u) */
    lora_last_rx_tick_ms = UINT32_MAX - 1000u;
    /* at now = 3998u: elapsed = 4999 ms -> connected */
    if (!ui_test_is_link_connected(3998u)) {
        printf("  FAIL: is_link_connected false at 4999ms elapsed across wrap\n");
        test1_pass = false;
    }
    /* at now = 3999u: elapsed = 5000 ms -> disconnected */
    if (ui_test_is_link_connected(3999u)) {
        printf("  FAIL: is_link_connected true at 5000ms elapsed across wrap\n");
        test1_pass = false;
    }
    /* at now = 5000u: elapsed = 6001 ms -> disconnected */
    if (ui_test_is_link_connected(5000u)) {
        printf("  FAIL: is_link_connected true at 6001ms elapsed across wrap\n");
        test1_pass = false;
    }

    /* 2. Stopped reports across wrap */
    lora_last_rx_tick_ms = UINT32_MAX - 2000u;
    if (ui_test_is_link_connected(4000u)) {
        printf("  FAIL: Stopped reports across wrap remained connected\n");
        test1_pass = false;
    }

    /* 3. Boot tick 0 sentinel (lora_last_rx_tick_ms == 1u when now == 0) */
    lora_last_rx_tick_ms = 1u;
    if (!ui_test_is_link_connected(0u)) {
        printf("  FAIL: is_link_connected false at boot tick 0 with sentinel 1\n");
        test1_pass = false;
    }
    if (!ui_test_is_link_connected(4999u)) {
        printf("  FAIL: is_link_connected false at 4999ms from boot tick 0\n");
        test1_pass = false;
    }
    if (ui_test_is_link_connected(5001u)) {
        printf("  FAIL: is_link_connected true at 5001ms from boot tick 0\n");
        test1_pass = false;
    }

    /* 3b. Drive actual ui_tick + integration TX path across link age 4999/5000ms:
     * stopped reports -> disconnected masking -> refused user action/Auto decision -> no recorded stale TX, then recovery readback adoption */
    const ui_test_hooks_t *hooks1 = ui_get_test_hooks();
    fake_node_mode_t save_fn1 = s_fake_node_mode;
    s_fake_node_mode = FAKE_NODE_MODE_STOP_REPORTING;
    sim_post_node_status(true, 420, true, 85, true, 235, true, 48, true, 0, 0, 0, 0);
    /* Advance real LVGL tick counter to UINT32_MAX - 2000u to test 32-bit rollover across 0xFFFFFFFF boundary */
    uint32_t cur_tick = lv_tick_get();
    uint32_t to_wrap = (UINT32_MAX - 2000u) - cur_tick;
    lv_tick_inc(to_wrap);
    uint32_t t_base1 = lv_tick_get();
    lora_last_rx_tick_ms = t_base1;

    /* Advance across UINT32_MAX to link age 4999ms: link remains connected */
    lv_tick_inc(4999u);
    ui_tick();
    lv_timer_handler();
    if (!ui_test_is_link_connected(lv_tick_get())) {
        printf("  FAIL: ui_tick evaluated link disconnected at 4999ms across wrap (elapsed=%u)\n", (unsigned)(lv_tick_get() - t_base1));
        test1_pass = false;
    }
    if (!ui_is_tx_ready()) {
        printf("  FAIL: ui_is_tx_ready false at 4999ms across wrap\n");
        test1_pass = false;
    }

    /* Advance 1ms to 5000ms: link becomes disconnected */
    lv_tick_inc(1u);
    ui_tick();
    lv_timer_handler();
    if (ui_test_is_link_connected(lv_tick_get())) {
        printf("  FAIL: ui_tick evaluated link connected at 5000ms across wrap (elapsed=%u)\n", (unsigned)(lv_tick_get() - t_base1));
        test1_pass = false;
    }
    if (ui_is_tx_ready()) {
        printf("  FAIL: ui_is_tx_ready true while disconnected at 5000ms across wrap\n");
        test1_pass = false;
    }

    /* Attempt user action while disconnected: must be refused */
    uint16_t seq_refused = lora_hub_cmd.bits.seq;
    ui_trigger_device_toggle(0);
    advance_ui(20u);
    if (lora_hub_cmd.bits.seq != seq_refused) {
        printf("  FAIL: User switch accepted while disconnected at 5000ms (seq %u -> %u)\n", seq_refused, lora_hub_cmd.bits.seq);
        test1_pass = false;
    }
    if (hooks1 && hooks1->device_cmds && hooks1->device_cmds[0].phase != UI_CMD_PHASE_IDLE) {
        printf("  FAIL: User switch command transitioned to %d while disconnected\n", hooks1->device_cmds[0].phase);
        test1_pass = false;
    }

    /* Attempt Auto decision while disconnected: must be refused */
    ui_hub_device_config_t cfg_dis_auto;
    ui_auto_get_defaults(1, UI_PRESET_FAN, &cfg_dis_auto);
    cfg_dis_auto.mode = UI_MODE_AUTO;
    ui_test_set_boot_staging(true);
    ui_set_device_config(1, &cfg_dis_auto);
    ui_test_set_boot_staging(false);
    lora_node_status.bits.co2_valid = 1;
    lora_node_status.bits.co2_ppm = 1200; /* Trigger condition */
    lora_rx_revision++;
    ui_tick();
    if (hooks1 && hooks1->device_cmds && hooks1->device_cmds[1].phase != UI_CMD_PHASE_IDLE) {
        printf("  FAIL: Auto decision triggered while disconnected at 5000ms\n");
        test1_pass = false;
    }

    /* Verify no recorded stale TX during disconnected period */
    sim_reset_tx_records();
    uint16_t dis_loop_seq = lora_hub_cmd.bits.seq;
    uint32_t dis_loop_tx = 0;
    sim_integration_step(1500u, &dis_loop_seq, &dis_loop_tx);
    if (s_sim_tx_count != 0) {
        printf("  FAIL: Recorded %u TX packets while disconnected (expected 0)\n", (unsigned)s_sim_tx_count);
        test1_pass = false;
    }

    /* Legitimate recovery: node reconnects with reported GPIO mask */
    s_fake_node_mode = save_fn1;
    lora_last_rx_tick_ms = lv_tick_get();
    sim_post_node_status(true, 420, true, 85, true, 235, true, 48, true, 1, 0, 0, 0);
    sim_reset_tx_records();
    dis_loop_tx = 0;
    dis_loop_seq = lora_hub_cmd.bits.seq;
    sim_integration_step(100u, &dis_loop_seq, &dis_loop_tx);
    if (!ui_test_is_link_connected(lv_tick_get())) {
        printf("  FAIL: Link failed to recover after fresh report\n");
        test1_pass = false;
    }
    if (s_sim_tx_count < 1 || (s_sim_tx_records[0].relay_mask & 1) != 1) {
        printf("  FAIL: Reconnect send did not adopt reported GPIO levels\n");
        test1_pass = false;
    }

    /* Restore clean manual config for device 1 */
    cfg_dis_auto.mode = UI_MODE_MANUAL;
    ui_test_set_boot_staging(true);
    ui_set_device_config(1, &cfg_dis_auto);
    ui_test_set_boot_staging(false);
    ui_test_reset_device_commands();

    /* 4. Y1 Auto hold and qualification across wrap: all 3 presets & both directions */
    /* Preset 1: Fan / CO2 (ON >= 1000, OFF <= 800) */
    ui_hub_device_config_t cfg_fan;
    ui_auto_get_defaults(1, UI_PRESET_FAN, &cfg_fan);
    cfg_fan.mode = UI_MODE_AUTO;
    lora_node_status_t pkt_wrap;
    memset(&pkt_wrap, 0, sizeof(pkt_wrap));
    pkt_wrap.bits.co2_valid = 1;
    pkt_wrap.bits.co2_ppm = 1200;

    ui_auto_state_t st_fan;
    ui_auto_state_init(&st_fan, UINT32_MAX - 5000u);
    ui_auto_restart_hold(&st_fan, UINT32_MAX - 5000u);

    bool wanted_wrap = false;
    /* Boundary 9999ms at now = 4998u: hold must NOT expire */
    if (ui_auto_eval(&st_fan, &cfg_fan, &pkt_wrap, true, false, 4998u, &wanted_wrap)) {
        printf("  FAIL: Fan auto triggered before hold expiry across wrap at 9999ms\n");
        test1_pass = false;
    }
    /* Boundary 10000ms at now = 4999u: hold expires, frame 1 qualifies */
    if (ui_auto_eval(&st_fan, &cfg_fan, &pkt_wrap, true, false, 4999u, &wanted_wrap)) {
        printf("  FAIL: Fan auto triggered on frame 1 after hold expiry across wrap\n");
        test1_pass = false;
    }
    /* Frame 2 at 5100u */
    if (ui_auto_eval(&st_fan, &cfg_fan, &pkt_wrap, true, false, 5100u, &wanted_wrap)) {
        printf("  FAIL: Fan auto triggered on frame 2 across wrap\n");
        test1_pass = false;
    }
    /* Frame 3 at 5200u: must fire */
    if (!ui_auto_eval(&st_fan, &cfg_fan, &pkt_wrap, true, false, 5200u, &wanted_wrap) || !wanted_wrap) {
        printf("  FAIL: Fan auto failed to trigger on frame 3 across wrap\n");
        test1_pass = false;
    }

    /* Reverse direction: Fan OFF (CO2 <= 800) across wrap */
    pkt_wrap.bits.co2_ppm = 700;
    ui_auto_state_init(&st_fan, UINT32_MAX - 5000u);
    ui_auto_restart_hold(&st_fan, UINT32_MAX - 5000u);
    /* 9999ms */
    if (ui_auto_eval(&st_fan, &cfg_fan, &pkt_wrap, true, true, 4998u, &wanted_wrap)) {
        printf("  FAIL: Fan OFF triggered prematurely at 9999ms across wrap\n");
        test1_pass = false;
    }
    /* 3 fresh frames after expiry */
    if (ui_auto_eval(&st_fan, &cfg_fan, &pkt_wrap, true, true, 5000u, &wanted_wrap)) {
        printf("  FAIL: Fan OFF triggered on frame 1 after hold expiry across wrap\n");
        test1_pass = false;
    }
    if (ui_auto_eval(&st_fan, &cfg_fan, &pkt_wrap, true, true, 5100u, &wanted_wrap)) {
        printf("  FAIL: Fan OFF triggered on frame 2 across wrap\n");
        test1_pass = false;
    }
    if (!ui_auto_eval(&st_fan, &cfg_fan, &pkt_wrap, true, true, 5200u, &wanted_wrap) || wanted_wrap) {
        printf("  FAIL: Fan OFF failed to trigger on frame 3 across wrap\n");
        test1_pass = false;
    }

    /* Preset 2: Purifier / TVOC (defaults ON >= 660, OFF <= 220 ppb) across wrap */
    ui_hub_device_config_t cfg_pur;
    ui_auto_get_defaults(0, UI_PRESET_PURIFIER, &cfg_pur);
    cfg_pur.mode = UI_MODE_AUTO;
    pkt_wrap.bits.voc_valid = 1;
    pkt_wrap.bits.tvoc_ppb = 700;
    ui_auto_state_t st_pur;
    ui_auto_state_init(&st_pur, UINT32_MAX - 5000u);
    ui_auto_restart_hold(&st_pur, UINT32_MAX - 5000u);
    if (ui_auto_eval(&st_pur, &cfg_pur, &pkt_wrap, true, false, 4998u, &wanted_wrap)) {
        printf("  FAIL: Purifier auto triggered at 9999ms across wrap\n");
        test1_pass = false;
    }
    if (ui_auto_eval(&st_pur, &cfg_pur, &pkt_wrap, true, false, 5000u, &wanted_wrap)) {
        printf("  FAIL: Purifier auto triggered on frame 1 across wrap\n");
        test1_pass = false;
    }
    if (ui_auto_eval(&st_pur, &cfg_pur, &pkt_wrap, true, false, 5100u, &wanted_wrap)) {
        printf("  FAIL: Purifier auto triggered on frame 2 across wrap\n");
        test1_pass = false;
    }
    if (!ui_auto_eval(&st_pur, &cfg_pur, &pkt_wrap, true, false, 5200u, &wanted_wrap) || !wanted_wrap) {
        printf("  FAIL: Purifier auto failed to trigger on frame 3 across wrap\n");
        test1_pass = false;
    }

    /* Reverse direction: Purifier OFF (TVOC <= 220) across wrap */
    pkt_wrap.bits.tvoc_ppb = 80;
    ui_auto_state_init(&st_pur, UINT32_MAX - 5000u);
    ui_auto_restart_hold(&st_pur, UINT32_MAX - 5000u);
    if (ui_auto_eval(&st_pur, &cfg_pur, &pkt_wrap, true, true, 4998u, &wanted_wrap)) {
        printf("  FAIL: Purifier OFF triggered prematurely at 9999ms across wrap\n");
        test1_pass = false;
    }
    if (ui_auto_eval(&st_pur, &cfg_pur, &pkt_wrap, true, true, 5000u, &wanted_wrap)) {
        printf("  FAIL: Purifier OFF triggered on frame 1 across wrap\n");
        test1_pass = false;
    }
    if (ui_auto_eval(&st_pur, &cfg_pur, &pkt_wrap, true, true, 5100u, &wanted_wrap)) {
        printf("  FAIL: Purifier OFF triggered on frame 2 across wrap\n");
        test1_pass = false;
    }
    if (!ui_auto_eval(&st_pur, &cfg_pur, &pkt_wrap, true, true, 5200u, &wanted_wrap) || wanted_wrap) {
        printf("  FAIL: Purifier OFF failed to trigger on frame 3 across wrap\n");
        test1_pass = false;
    }

    /* Preset 3: Humidifier / RH (ON <= 40, OFF >= 50) across wrap */
    ui_hub_device_config_t cfg_hum;
    ui_auto_get_defaults(2, UI_PRESET_HUMIDIFIER, &cfg_hum);
    cfg_hum.mode = UI_MODE_AUTO;
    pkt_wrap.bits.humid_valid = 1;
    pkt_wrap.bits.humidity_pct = 35;
    ui_auto_state_t st_hum;
    ui_auto_state_init(&st_hum, UINT32_MAX - 5000u);
    ui_auto_restart_hold(&st_hum, UINT32_MAX - 5000u);
    if (ui_auto_eval(&st_hum, &cfg_hum, &pkt_wrap, true, false, 4998u, &wanted_wrap)) {
        printf("  FAIL: Humidifier auto triggered at 9999ms across wrap\n");
        test1_pass = false;
    }
    if (ui_auto_eval(&st_hum, &cfg_hum, &pkt_wrap, true, false, 5000u, &wanted_wrap)) {
        printf("  FAIL: Humidifier auto triggered on frame 1 across wrap\n");
        test1_pass = false;
    }
    if (ui_auto_eval(&st_hum, &cfg_hum, &pkt_wrap, true, false, 5100u, &wanted_wrap)) {
        printf("  FAIL: Humidifier auto triggered on frame 2 across wrap\n");
        test1_pass = false;
    }
    if (!ui_auto_eval(&st_hum, &cfg_hum, &pkt_wrap, true, false, 5200u, &wanted_wrap) || !wanted_wrap) {
        printf("  FAIL: Humidifier auto failed to trigger on frame 3 across wrap\n");
        test1_pass = false;
    }

    /* Reverse direction: Humidifier OFF (RH >= 50) across wrap */
    pkt_wrap.bits.humidity_pct = 55;
    ui_auto_state_init(&st_hum, UINT32_MAX - 5000u);
    ui_auto_restart_hold(&st_hum, UINT32_MAX - 5000u);
    if (ui_auto_eval(&st_hum, &cfg_hum, &pkt_wrap, true, true, 4998u, &wanted_wrap)) {
        printf("  FAIL: Humidifier OFF triggered prematurely at 9999ms across wrap\n");
        test1_pass = false;
    }
    if (ui_auto_eval(&st_hum, &cfg_hum, &pkt_wrap, true, true, 5000u, &wanted_wrap)) {
        printf("  FAIL: Humidifier OFF triggered on frame 1 across wrap\n");
        test1_pass = false;
    }
    if (ui_auto_eval(&st_hum, &cfg_hum, &pkt_wrap, true, true, 5100u, &wanted_wrap)) {
        printf("  FAIL: Humidifier OFF triggered on frame 2 across wrap\n");
        test1_pass = false;
    }
    if (!ui_auto_eval(&st_hum, &cfg_hum, &pkt_wrap, true, true, 5200u, &wanted_wrap) || wanted_wrap) {
        printf("  FAIL: Humidifier OFF failed to trigger on frame 3 across wrap\n");
        test1_pass = false;
    }

    /* 5. Auto hold starting at boot tick 0 */
    ui_auto_state_t st_boot;
    ui_auto_state_init(&st_boot, 0u);
    ui_auto_restart_hold(&st_boot, 0u); /* Sets last_transition_ms = 1u */
    pkt_wrap.bits.co2_ppm = 1200;
    if (ui_auto_eval(&st_boot, &cfg_fan, &pkt_wrap, true, false, 0u, &wanted_wrap)) {
        printf("  FAIL: Auto triggered at boot tick 0 while hold active\n");
        test1_pass = false;
    }
    if (ui_auto_eval(&st_boot, &cfg_fan, &pkt_wrap, true, false, 9999u, &wanted_wrap)) {
        printf("  FAIL: Auto triggered at 9999ms from boot tick 0\n");
        test1_pass = false;
    }
    if (ui_auto_eval(&st_boot, &cfg_fan, &pkt_wrap, true, false, 10001u, &wanted_wrap)) {
        printf("  FAIL: Auto triggered on frame 1 after boot tick 0 hold expiry\n");
        test1_pass = false;
    }
    if (ui_auto_eval(&st_boot, &cfg_fan, &pkt_wrap, true, false, 10100u, &wanted_wrap)) {
        printf("  FAIL: Auto triggered on frame 2 after boot tick 0 hold expiry\n");
        test1_pass = false;
    }
    if (!ui_auto_eval(&st_boot, &cfg_fan, &pkt_wrap, true, false, 10200u, &wanted_wrap) || !wanted_wrap) {
        printf("  FAIL: Auto failed to trigger after boot tick 0 hold expiry\n");
        test1_pass = false;
    }

    lora_last_rx_tick_ms = save_rx1;

    if (test1_pass) {
        printf("  PASS: Uptime boundaries, production wrap link & Auto hold verified.\n");
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
     * TEST 7: Fixed 15-minute X Window, Slot Mapping & Time Labels (B3)
     * ------------------------------------------------------------- */
    total_tests++;
    printf("\n[CHECK 7] Fixed 15-minute X window, right-aligned slots & time labels...\n");
    bool test7_pass = true;

    ui_navigate_to_page(UI_PAGE_TRENDS);
    advance_ui(50u);

    /* 1. Empty state: 0 readings */
    ui_feed_history(UI_METRIC_CO2, NULL, NULL, 0, 0);
    advance_ui(50u);
    if (ui_is_trends_empty_box_hidden()) {
        printf("  FAIL 7a: Empty box hidden when count is 0\n");
        test7_pass = false;
    }
    const char *empty0_lbl = ui_get_trends_empty_lbl_str();
    if (!empty0_lbl || strcmp(empty0_lbl, "No readings yet") != 0) {
        printf("  FAIL 7a: 0-sample empty label expected 'No readings yet', got '%s'\n",
               empty0_lbl ? empty0_lbl : "NULL");
        test7_pass = false;
    }
    if (!ui_is_trends_empty_hint_hidden()) {
        printf("  FAIL 7a: 0-sample empty hint unexpectedly visible\n");
        test7_pass = false;
    }

    /* 2. One-sample state: 1 reading */
    int32_t one_val = 420;
    bool one_vld = true;
    ui_feed_history(UI_METRIC_CO2, &one_val, &one_vld, 1, 600);
    advance_ui(50u);
    if (ui_is_trends_empty_box_hidden()) {
        printf("  FAIL 7b: Empty box hidden when count is 1\n");
        test7_pass = false;
    }
    const char *empty1_lbl = ui_get_trends_empty_lbl_str();
    if (!empty1_lbl || strcmp(empty1_lbl, "Collecting readings…") != 0) {
        printf("  FAIL 7b: 1-sample empty label expected 'Collecting readings…', got '%s'\n",
               empty1_lbl ? empty1_lbl : "NULL");
        test7_pass = false;
    }
    if (ui_is_trends_empty_hint_hidden()) {
        printf("  FAIL 7b: 1-sample empty hint is hidden\n");
        test7_pass = false;
    }

    /* 3. Slot mapping with 5 points: right-aligned to slot 15, slots 0..10 are NONE */
    int32_t five_vals[5] = {410, 415, 420, 425, 430};
    bool five_vld[5] = {true, true, true, true, true};
    ui_feed_history(UI_METRIC_CO2, five_vals, five_vld, 5, 14 * 60 + 17 - 4);
    advance_ui(50u);
    if (!ui_is_trends_empty_box_hidden()) {
        printf("  FAIL 7c: Empty box visible with 5 readings\n");
        test7_pass = false;
    }

    /* Chart series slot verification */
    const ui_test_hooks_t *h7 = ui_get_test_hooks();
    if (!h7 || !h7->trends_chart || !*(h7->trends_chart)) {
        printf("  FAIL 7c: Trends chart hook NULL\n");
        test7_pass = false;
    } else {
        lv_chart_t *c = (lv_chart_t *)(*(h7->trends_chart));
        lv_ll_t *series_ll = &c->series_ll;
        lv_chart_series_t *ser = _lv_ll_get_head(series_ll);
        if (!ser || !ser->y_points) {
            printf("  FAIL 7c: Trends chart series or points NULL\n");
            test7_pass = false;
        } else {
            for (int slot = 0; slot < 11; slot++) {
                if (ser->y_points[slot] != LV_CHART_POINT_NONE) {
                    printf("  FAIL 7c: Leading slot %d is %d (expected LV_CHART_POINT_NONE)\n",
                           slot, (int)ser->y_points[slot]);
                    test7_pass = false;
                    break;
                }
            }
            for (int slot = 11; slot <= 15; slot++) {
                if (ser->y_points[slot] == LV_CHART_POINT_NONE) {
                    printf("  FAIL 7c: Active slot %d is NONE (expected valid point)\n", slot);
                    test7_pass = false;
                    break;
                }
            }
        }
    }

    /* 4. External time labels: HH:MM ending at newest minute (14:17 -> 14:02, 14:07, 14:12, 14:17) */
    const char *t0 = ui_get_trends_time_lbl_str(0);
    const char *t1 = ui_get_trends_time_lbl_str(1);
    const char *t2 = ui_get_trends_time_lbl_str(2);
    const char *t3 = ui_get_trends_time_lbl_str(3);
    if (!t0 || strcmp(t0, "14:02") != 0 ||
        !t1 || strcmp(t1, "14:07") != 0 ||
        !t2 || strcmp(t2, "14:12") != 0 ||
        !t3 || strcmp(t3, "14:17") != 0) {
        printf("  FAIL 7d: External time labels expected '14:02'/'14:07'/'14:12'/'14:17', got '%s'/'%s'/'%s'/'%s'\n",
               t0 ? t0 : "NULL", t1 ? t1 : "NULL", t2 ? t2 : "NULL", t3 ? t3 : "NULL");
        test7_pass = false;
    }

    /* 5. Midnight wrap in external time labels (start=1435 -> newest minute = 1435+4=1439=23:59 -> 23:44, 23:49, 23:54, 23:59) */
    ui_feed_history(UI_METRIC_CO2, five_vals, five_vld, 5, 1435);
    advance_ui(50u);
    t0 = ui_get_trends_time_lbl_str(0);
    t1 = ui_get_trends_time_lbl_str(1);
    t2 = ui_get_trends_time_lbl_str(2);
    t3 = ui_get_trends_time_lbl_str(3);
    if (!t0 || strcmp(t0, "23:44") != 0 ||
        !t1 || strcmp(t1, "23:49") != 0 ||
        !t2 || strcmp(t2, "23:54") != 0 ||
        !t3 || strcmp(t3, "23:59") != 0) {
        printf("  FAIL 7e: Midnight wrap labels expected '23:44'/'23:49'/'23:54'/'23:59', got '%s'/'%s'/'%s'/'%s'\n",
               t0 ? t0 : "NULL", t1 ? t1 : "NULL", t2 ? t2 : "NULL", t3 ? t3 : "NULL");
        test7_pass = false;
    }

    /* 6. Autonomous time labels: -15 min, -10 min, -5 min, Now */
    ui_test_reset_history();
    ui_select_trend_metric(UI_METRIC_TEMP);
    ui_set_uptime_seconds(0u);
    sim_post_node_status(true, 420, true, 85, true, 235, true, 48, true, 0, 0, 0, 0);
    advance_ui(50u);
    ui_set_uptime_seconds(60u);
    sim_post_node_status(true, 420, true, 85, true, 236, true, 48, true, 0, 0, 0, 0);
    advance_ui(50u);
    t0 = ui_get_trends_time_lbl_str(0);
    t1 = ui_get_trends_time_lbl_str(1);
    t2 = ui_get_trends_time_lbl_str(2);
    t3 = ui_get_trends_time_lbl_str(3);
    if (!t0 || strcmp(t0, "-15 min") != 0 ||
        !t1 || strcmp(t1, "-10 min") != 0 ||
        !t2 || strcmp(t2, "-5 min") != 0 ||
        !t3 || strcmp(t3, "Now") != 0) {
        printf("  FAIL 7f: Autonomous time labels expected '-15 min'/'-10 min'/'-5 min'/'Now', got '%s'/'%s'/'%s'/'%s'\n",
               t0 ? t0 : "NULL", t1 ? t1 : "NULL", t2 ? t2 : "NULL", t3 ? t3 : "NULL");
        test7_pass = false;
    }

    if (test7_pass) {
        printf("  PASS: Fixed 15-minute X window, slot mapping, time labels and empty states verified.\n");
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
     * TEST 10: Trends In-Place Metric Switching, Hero Selector & Round Y Ticks (B1, B2, B4)
     * ------------------------------------------------------------- */
    total_tests++;
    printf("\n[CHECK 10] Trends in-place metric switching, hero selector, round Y ticks & 0-drift...\n");
    bool test10_pass = true;

    ui_navigate_to_page(UI_PAGE_TRENDS);
    populate_node_status_baseline();
    feed_standard_history();
    advance_ui(50u);

    /* 1. CO2 Metric: Title 'eCO₂', unit 'ppm', hero checked + celadon dot, round Y ticks 400 / 425 / 450 */
    ui_select_trend_metric(UI_METRIC_CO2);
    advance_ui(20u);
    if (ui_get_selected_trend_metric() != UI_METRIC_CO2) {
        printf("  FAIL 10a: Selected metric is not UI_METRIC_CO2\n");
        test10_pass = false;
    }
    const char *title_co2 = ui_get_trends_title_str();
    const char *unit_co2 = ui_get_trends_unit_str();
    if (!title_co2 || strcmp(title_co2, "eCO₂") != 0) {
        printf("  FAIL 10a: CO2 title expected 'eCO₂', got '%s'\n", title_co2 ? title_co2 : "NULL");
        test10_pass = false;
    }
    if (!unit_co2 || strcmp(unit_co2, "ppm") != 0) {
        printf("  FAIL 10a: CO2 unit expected 'ppm', got '%s'\n", unit_co2 ? unit_co2 : "NULL");
        test10_pass = false;
    }
    if (!ui_is_trends_tab_checked(UI_METRIC_CO2) || ui_is_trends_tab_dot_hidden(UI_METRIC_CO2)) {
        printf("  FAIL 10a: CO2 hero tab not checked or dot hidden\n");
        test10_pass = false;
    }
    if (ui_is_trends_tab_checked(UI_METRIC_VOC) || !ui_is_trends_tab_dot_hidden(UI_METRIC_VOC)) {
        printf("  FAIL 10a: Unselected VOC tab checked or dot visible\n");
        test10_pass = false;
    }
    const char *y0_co2 = ui_get_trends_y_lbl_str(0);
    const char *y1_co2 = ui_get_trends_y_lbl_str(1);
    const char *y2_co2 = ui_get_trends_y_lbl_str(2);
    if (!y0_co2 || strcmp(y0_co2, "450") != 0 ||
        !y1_co2 || strcmp(y1_co2, "425") != 0 ||
        !y2_co2 || strcmp(y2_co2, "400") != 0) {
        printf("  FAIL 10a: CO2 Y ticks expected '450'/'425'/'400', got '%s'/'%s'/'%s'\n",
               y0_co2 ? y0_co2 : "NULL", y1_co2 ? y1_co2 : "NULL", y2_co2 ? y2_co2 : "NULL");
        test10_pass = false;
    }

    /* 2. VOC Metric: Title 'TVOC', unit '', hero checked */
    ui_select_trend_metric(UI_METRIC_VOC);
    advance_ui(20u);
    if (ui_get_selected_trend_metric() != UI_METRIC_VOC) {
        printf("  FAIL 10b: Selected metric is not UI_METRIC_VOC\n");
        test10_pass = false;
    }
    const char *title_voc = ui_get_trends_title_str();
    const char *unit_voc = ui_get_trends_unit_str();
    if (!title_voc || strcmp(title_voc, "TVOC") != 0) {
        printf("  FAIL 10b: VOC title expected 'TVOC', got '%s'\n", title_voc ? title_voc : "NULL");
        test10_pass = false;
    }
    if (!unit_voc || strcmp(unit_voc, "ppb") != 0) {
        printf("  FAIL 10b: VOC unit expected 'ppb', got '%s'\n", unit_voc ? unit_voc : "NULL");
        test10_pass = false;
    }
    if (!ui_is_trends_tab_checked(UI_METRIC_VOC) || ui_is_trends_tab_dot_hidden(UI_METRIC_VOC)) {
        printf("  FAIL 10b: VOC hero tab not checked or dot hidden\n");
        test10_pass = false;
    }

    /* 3. Temperature Metric: Title 'Temperature', unit '°C', round Y ticks 22.0 / 23.0 / 24.0 */
    ui_select_trend_metric(UI_METRIC_TEMP);
    advance_ui(20u);
    if (ui_get_selected_trend_metric() != UI_METRIC_TEMP) {
        printf("  FAIL 10c: Selected metric is not UI_METRIC_TEMP\n");
        test10_pass = false;
    }
    const char *title_temp = ui_get_trends_title_str();
    const char *unit_temp = ui_get_trends_unit_str();
    if (!title_temp || strcmp(title_temp, "Temperature") != 0) {
        printf("  FAIL 10c: Temp title expected 'Temperature', got '%s'\n", title_temp ? title_temp : "NULL");
        test10_pass = false;
    }
    if (!unit_temp || strcmp(unit_temp, "°C") != 0) {
        printf("  FAIL 10c: Temp unit expected '°C', got '%s'\n", unit_temp ? unit_temp : "NULL");
        test10_pass = false;
    }
    const char *y0_temp = ui_get_trends_y_lbl_str(0);
    const char *y1_temp = ui_get_trends_y_lbl_str(1);
    const char *y2_temp = ui_get_trends_y_lbl_str(2);
    if (!y0_temp || strcmp(y0_temp, "24.0") != 0 ||
        !y1_temp || strcmp(y1_temp, "23.0") != 0 ||
        !y2_temp || strcmp(y2_temp, "22.0") != 0) {
        printf("  FAIL 10c: Temp Y ticks expected '24.0'/'23.0'/'22.0', got '%s'/'%s'/'%s'\n",
               y0_temp ? y0_temp : "NULL", y1_temp ? y1_temp : "NULL", y2_temp ? y2_temp : "NULL");
        test10_pass = false;
    }

    /* Negative Temperature Round Y ticks: -2.5 .. -0.5 °C -> 0.0 / -2.0 / -4.0 */
    int32_t neg_temp_vals[3] = {-25, -15, -5};
    bool neg_temp_vld[3] = {true, true, true};
    ui_feed_history(UI_METRIC_TEMP, neg_temp_vals, neg_temp_vld, 3, 600);
    advance_ui(50u);
    y0_temp = ui_get_trends_y_lbl_str(0);
    y1_temp = ui_get_trends_y_lbl_str(1);
    y2_temp = ui_get_trends_y_lbl_str(2);
    if (!y0_temp || strcmp(y0_temp, "0.0") != 0 ||
        !y1_temp || strcmp(y1_temp, "-2.0") != 0 ||
        !y2_temp || strcmp(y2_temp, "-4.0") != 0) {
        printf("  FAIL 10c2: Negative Temp Y ticks expected '0.0'/'-2.0'/'-4.0', got '%s'/'%s'/'%s'\n",
               y0_temp ? y0_temp : "NULL", y1_temp ? y1_temp : "NULL", y2_temp ? y2_temp : "NULL");
        test10_pass = false;
    }

    /* Large TVOC Round Y ticks: 58000 .. 60000 -> 62500 / 60000 / 57500 */
    int32_t large_voc_vals[3] = {58000, 59000, 60000};
    bool large_voc_vld[3] = {true, true, true};
    ui_feed_history(UI_METRIC_VOC, large_voc_vals, large_voc_vld, 3, 600);
    ui_select_trend_metric(UI_METRIC_VOC);
    advance_ui(50u);
    const char *y0_voc = ui_get_trends_y_lbl_str(0);
    const char *y1_voc = ui_get_trends_y_lbl_str(1);
    const char *y2_voc = ui_get_trends_y_lbl_str(2);
    if (!y0_voc || strcmp(y0_voc, "62500") != 0 ||
        !y1_voc || strcmp(y1_voc, "60000") != 0 ||
        !y2_voc || strcmp(y2_voc, "57500") != 0) {
        printf("  FAIL 10b2: Large TVOC Y ticks expected '62500'/'60000'/'57500', got '%s'/'%s'/'%s'\n",
               y0_voc ? y0_voc : "NULL", y1_voc ? y1_voc : "NULL", y2_voc ? y2_voc : "NULL");
        test10_pass = false;
    }

    /* Extreme upper bound (INT32_MAX): 2147483000 .. 2147483647 */
    int32_t max_voc_vals[3] = {2147483000, 2147483300, 2147483647};
    bool max_voc_vld[3] = {true, true, true};
    ui_feed_history(UI_METRIC_VOC, max_voc_vals, max_voc_vld, 3, 600);
    ui_select_trend_metric(UI_METRIC_VOC);
    advance_ui(50u);
    const char *y0_max = ui_get_trends_y_lbl_str(0);
    const char *y1_max = ui_get_trends_y_lbl_str(1);
    const char *y2_max = ui_get_trends_y_lbl_str(2);
    if (!y0_max || !y1_max || !y2_max) {
        printf("  FAIL 10b3: NULL Y labels on INT32_MAX feed\n");
        test10_pass = false;
    } else {
        long long top_val = strtoll(y0_max, NULL, 10);
        long long mid_val = strtoll(y1_max, NULL, 10);
        long long bot_val = strtoll(y2_max, NULL, 10);
        if (!(bot_val < mid_val && mid_val < top_val)) {
            printf("  FAIL 10b3: INT32_MAX Y ticks not strictly increasing: bot='%s' (%lld), mid='%s' (%lld), top='%s' (%lld)\n",
                   y2_max, bot_val, y1_max, mid_val, y0_max, top_val);
            test10_pass = false;
        }
    }

    /* Extreme lower bound (INT32_MIN): -2147483648 .. -2147483000 */
    int32_t min_voc_vals[3] = {(-2147483647 - 1), -2147483300, -2147483000};
    bool min_voc_vld[3] = {true, true, true};
    ui_feed_history(UI_METRIC_VOC, min_voc_vals, min_voc_vld, 3, 600);
    ui_select_trend_metric(UI_METRIC_VOC);
    advance_ui(50u);
    const char *y0_min = ui_get_trends_y_lbl_str(0);
    const char *y1_min = ui_get_trends_y_lbl_str(1);
    const char *y2_min = ui_get_trends_y_lbl_str(2);
    if (!y0_min || !y1_min || !y2_min) {
        printf("  FAIL 10b4: NULL Y labels on INT32_MIN feed\n");
        test10_pass = false;
    } else {
        long long top_val = strtoll(y0_min, NULL, 10);
        long long mid_val = strtoll(y1_min, NULL, 10);
        long long bot_val = strtoll(y2_min, NULL, 10);
        if (!(bot_val < mid_val && mid_val < top_val)) {
            printf("  FAIL 10b4: INT32_MIN Y ticks not strictly increasing: bot='%s' (%lld), mid='%s' (%lld), top='%s' (%lld)\n",
                   y2_min, bot_val, y1_min, mid_val, y0_min, top_val);
            test10_pass = false;
        }
    }

    /* 4. Humidity Metric: Title 'Humidity', unit '%' */
    ui_select_trend_metric(UI_METRIC_HUMIDITY);
    advance_ui(20u);
    if (ui_get_selected_trend_metric() != UI_METRIC_HUMIDITY) {
        printf("  FAIL 10d: Selected metric is not UI_METRIC_HUMIDITY\n");
        test10_pass = false;
    }
    const char *title_hum = ui_get_trends_title_str();
    const char *unit_hum = ui_get_trends_unit_str();
    if (!title_hum || strcmp(title_hum, "Humidity") != 0) {
        printf("  FAIL 10d: Humidity title expected 'Humidity', got '%s'\n", title_hum ? title_hum : "NULL");
        test10_pass = false;
    }
    if (!unit_hum || strcmp(unit_hum, "%") != 0) {
        printf("  FAIL 10d: Humidity unit expected '%%', got '%s'\n", unit_hum ? unit_hum : "NULL");
        test10_pass = false;
    }

    /* 5. Empty history behavior */
    ui_feed_history(UI_METRIC_TEMP, NULL, NULL, 0, 14 * 60);
    ui_select_trend_metric(UI_METRIC_TEMP);
    advance_ui(50u);
    if (ui_is_trends_empty_box_hidden()) {
        printf("  FAIL 10e: Empty box hidden on 0 samples\n");
        test10_pass = false;
    }
    const char *empty_msg = ui_get_trends_empty_lbl_str();
    if (!empty_msg || strcmp(empty_msg, "No readings yet") != 0) {
        printf("  FAIL 10e: Empty label expected 'No readings yet', got '%s'\n", empty_msg ? empty_msg : "NULL");
        test10_pass = false;
    }

    /* 6. Zero memory drift on 100 in-place metric tab switches */
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
        printf("  FAIL 10f: In-place metric tab switching leaked %ld bytes across 100 switches!\n", (long)tab_drift);
        test10_pass = false;
    }

    if (test10_pass) {
        printf("  PASS: Trends metric switching, hero selector, round Y ticks & 0-drift verified.\n");
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
    if (!h_temp || strcmp(h_temp, "23.5") != 0) {
        printf("  FAIL: Home Temp baseline mismatch: '%s' (expected '23.5')\n", h_temp ? h_temp : "NULL");
        test11_pass = false;
    }
    if (!h_hum || strcmp(h_hum, "48") != 0) {
        printf("  FAIL: Home Humidity baseline mismatch: '%s' (expected '48')\n", h_hum ? h_hum : "NULL");
        test11_pass = false;
    }
    if (!find_visible_label(lv_scr_act(), "°C") || !find_visible_label(lv_scr_act(), "%")) {
        printf("  FAIL: Home environmental dedicated unit labels ('°C', '%%') not found\n");
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
        if (!dmode || (strncmp(dmode, "Auto", 4) != 0 && strcmp(dmode, "Manual") != 0)) {
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
        lv_obj_t *dash = ui_get_test_hooks()->home_dev_dashes[i];
        lv_obj_t *st = ui_get_test_hooks()->home_dev_states[i];
        if (dash == NULL || lv_obj_has_flag(dash, LV_OBJ_FLAG_HIDDEN)) {
            printf("  FAIL: Home offline device %u dash not visible\n", i);
            test11_pass = false;
        }
        if (st != NULL && !lv_obj_has_flag(st, LV_OBJ_FLAG_HIDDEN)) {
            printf("  FAIL: Home offline device %u state label not hidden (expected dash only)\n", i);
            test11_pass = false;
        }
    }

    /* 4. Sensor Failure State (CO2/VOC invalid, Temp/Humid valid) */
    s_fake_node_mode = FAKE_NODE_MODE_NORMAL;
    sim_post_node_status(true, 420, false, 85, false, 220, true, 50, true, 1, 0, 0, 1);
    advance_ui(50u);

    if (strcmp(ui_get_home_co2_str(), "—") != 0 ||
        strcmp(ui_get_home_temp_str(), "22.0") != 0 ||
        strcmp(ui_get_home_humid_str(), "50") != 0) {
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
        if (!name || (strcmp(name, exp_names[i]) != 0 && (i != 1 || strcmp(name, "Ventilation") != 0))) {
            printf("  FAIL: Devices grid device %u name mismatch: '%s' (expected '%s')\n",
                   i, name ? name : "NULL", exp_names[i]);
            test12_pass = false;
        }
        if (!mode || (strncmp(mode, "Auto", 4) != 0 && strcmp(mode, exp_modes[i]) != 0)) {
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

    /* Native Quiet Architecture anchors (not raster concept coordinates). */
    const ui_test_hooks_t *h16 = ui_get_test_hooks();
    lv_area_t area16;
    ui_navigate_to_page(UI_PAGE_HOME);
    populate_node_status_baseline();
    advance_ui(200u);
    lv_obj_get_coords(lv_obj_get_parent(h16->home_dev_names[0]), &area16);
    if (area16.x1 < 465 || area16.x1 > 468 || area16.y1 < 168 || area16.y1 > 170 ||
        lv_area_get_width(&area16) < 298 || lv_area_get_width(&area16) > 302 ||
        lv_area_get_height(&area16) < 65 || lv_area_get_height(&area16) > 72) {
        printf("  FAIL: Overview device row bounds (%d,%d,%d,%d)\n", area16.x1, area16.y1, area16.x2, area16.y2);
        test16_pass = false;
    }
    uint16_t hero_pix = s_framebuffer[120 * SIM_HOR + 200];
    /* Top of nav pill above centered 24px label (pill y: 60..95, label y: 66..90) */
    uint16_t nav_pix = s_framebuffer[63 * SIM_HOR + 50];
    /* Hero card has 2-stop vertical gradient (#214539 / 0x2227 to #183831 / 0x19C6, 0x2207 at y=120) */
    if ((hero_pix != UI_COLOR_FOREST.full && hero_pix != 0x2207) ||
        nav_pix != UI_COLOR_CELADON.full) {
        printf("  FAIL: Forest hero / celadon top navigation fills: hero=0x%04X (exp 0x%04X or 0x2207), nav=0x%04X (exp 0x%04X)\n",
               hero_pix, UI_COLOR_FOREST.full, nav_pix, UI_COLOR_CELADON.full);
        test16_pass = false;
    }
    ui_navigate_to_page(UI_PAGE_DEVICES);
    advance_ui(100u);
    lv_obj_get_coords(lv_obj_get_parent(h16->dev_page_names[0]), &area16);
    if (area16.x1 != 24 || area16.y1 != 112 ||
        lv_area_get_width(&area16) != 368 || lv_area_get_height(&area16) != 164) {
        printf("  FAIL: Devices card bounds\n"); test16_pass = false;
    }
    ui_navigate_to_page(UI_PAGE_TRENDS);
    advance_ui(100u);
    lv_obj_get_coords(lv_obj_get_parent(*h16->trends_chart), &area16);
    if (area16.x1 != 224 || area16.y1 != 112 ||
        lv_area_get_width(&area16) != 552 || lv_area_get_height(&area16) != 344) {
        printf("  FAIL: Trends chart card bounds\n"); test16_pass = false;
    }
    ui_open_device_settings(0);
    advance_ui(100u);
    lv_obj_get_coords(*h16->dialog_box, &area16);
    if (area16.x1 != 24 || area16.y1 != 56 ||
        lv_area_get_width(&area16) != 752 || lv_area_get_height(&area16) != 424) {
        printf("  FAIL: Focused editor bounds: (%d, %d, %d, %d), w=%d, h=%d\n",
               area16.x1, area16.y1, area16.x2, area16.y2, lv_area_get_width(&area16), lv_area_get_height(&area16));
        test16_pass = false;
    }
    ui_close_device_settings();
    advance_ui(100u);

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

    for (int y = 18; y <= 38; y++) {
        for (int x = 550; x <= 780; x++) {
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
    s_fake_node_mode = FAKE_NODE_MODE_STOP_REPORTING;
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
    s_fake_node_mode = FAKE_NODE_MODE_NORMAL;

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
             (hooks->header_sep && *hooks->header_sep && lv_obj_get_x(*hooks->header_sep) != base_sep_x) ||
             (hooks->uptime_label && *hooks->uptime_label && lv_obj_get_x(*hooks->uptime_label) != base_uptime_x))) {
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

    const ui_test_hooks_t *h22 = ui_get_test_hooks();
    const char *vmsg = ui_get_dialog_validation_msg();
    if (!vmsg || (strstr(vmsg, "above OFF") == NULL && strstr(vmsg, "exceed") == NULL) ||
        !lv_obj_has_state(*h22->dialog_btn_save_auto, LV_STATE_DISABLED)) {
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
    if (vmsg && (strstr(vmsg, "Raise ON") != NULL || strstr(vmsg, "lower OFF") != NULL ||
                 lv_obj_has_state(*h22->dialog_btn_save_auto, LV_STATE_DISABLED))) {
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
    if (!h_fan_mode || (strcmp(h_fan_mode, "Auto · eCO₂") != 0 && strcmp(h_fan_mode, "Auto") != 0 && strcmp(h_fan_mode, "Auto · CO₂") != 0)) {
        printf("  FAIL: Home tab Device 1 mode is '%s', expected 'Auto · eCO₂' or 'Auto'\n", h_fan_mode ? h_fan_mode : "NULL");
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
    lora_node_status.bits.sample_seq++; lora_rx_revision++; advance_ui(100u);
    lora_node_status.bits.sample_seq++; lora_rx_revision++; advance_ui(100u);
    lora_node_status.bits.sample_seq++; lora_rx_revision++; advance_ui(100u);

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
            sim_pointer_click(120, 432);
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
     * Click OFF plus button at (732, 304) 4 times: OFF goes from 800 -> 1000 (gap 0 < 50) */
    for (int k = 0; k < 4; k++) {
        sim_pointer_click(732, 304);
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
        sim_pointer_click(120, 432);
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
            sim_pointer_click(732, 304);
            advance_ui(30u);
        }
        /* Click Back button at (222, 98) */
        sim_pointer_click(46, 78);
        advance_ui(50u);
        /* Now on Device settings subview, click X at (578, 98) */
        sim_pointer_click(120, 432);
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
        sim_pointer_click(46, 78); /* Back to Device settings */
        advance_ui(50u);

        /* 3. Test polarity switch -> limits -> Save through actual widgets */
        /* Click active low switch at (573, 322) */
        sim_pointer_click(718, 338);
        advance_ui(50u);

        /* Click "Set limits…" button at (400, 266) */
        sim_pointer_click(400, 266);
        advance_ui(50u);

        /* Step ON plus once at (576, 192) -> ON goes 1000 -> 1050 */
        sim_pointer_click(348, 304);
        advance_ui(30u);

        /* Click Save button at (400, 380) */
        s_cb_rec27 = 0;
        sim_pointer_click(624, 432);
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
        sim_pointer_click(624, 432);
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
        s_fake_node_mode = FAKE_NODE_MODE_FAIL_NEXT;
        lora_node_status.bits.co2_ppm = 1200;
        lora_node_status.bits.sample_seq++; lora_rx_revision++; advance_ui(100u);
        lora_node_status.bits.sample_seq++; lora_rx_revision++; advance_ui(100u);
        lora_node_status.bits.sample_seq++; lora_rx_revision++; advance_ui(100u);
        advance_ui(UI_CMD_TIMEOUT_MS + 200u);
        ui_dismiss_toast();
        advance_ui(50u);

        lv_obj_get_coords(hooks28->home_dev_modes[1], &m_coords);
        lv_obj_get_coords(hooks28->home_dev_states[1], &s_coords);
        lv_obj_get_coords(lv_obj_get_parent(hooks28->home_dev_modes[1]), &tile_coords);
        lv_obj_get_coords(hooks28->home_dev_retries[1], &r_coords);

        /* Mode and state now use separate horizontal lanes on the same row. */
        if (m_coords.x2 + 8 >= s_coords.x1 || m_coords.y1 != s_coords.y1) {
            printf("  FAIL: Overview mode/state lanes overlap or misalign\n");
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

        /* 3. Comprehensive Matrix: Home & Devices x active-high & active-low x late-ACK & reconnect */
        for (int p_idx = 0; p_idx < 2; p_idx++) {
            ui_page_id_t page = (p_idx == 0) ? UI_PAGE_HOME : UI_PAGE_DEVICES;
            for (uint8_t pol = 0; pol <= 1; pol++) {
                for (int recov = 0; recov < 2; recov++) {
                    /* recov 0: late-ACK, recov 1: reconnect */
                    ui_navigate_to_page(page);
                    ui_test_reset_device_commands();
                    s_fake_node_mode = FAKE_NODE_MODE_NORMAL;
                    advance_ui(50u);

                    /* Configure Device 1 (Fan) in AUTO with polarity pol */
                    ui_hub_device_config_t dcfg;
                    ui_auto_get_defaults(1, UI_PRESET_FAN, &dcfg);
                    dcfg.mode = UI_MODE_AUTO;
                    dcfg.active_low = pol;
                    ui_test_set_boot_staging(true);
                    ui_set_device_config(1, &dcfg);
                    ui_test_set_boot_staging(false);

                    /* Post baseline matching state (relay OFF for active-high is 0, for active-low is 1) */
                    uint8_t off_level = pol ? 1 : 0;
                    uint8_t on_level = pol ? 0 : 1;
                    lora_hub_cmd_set_relay_gpio(&lora_hub_cmd, 1, off_level);
                    sim_post_node_status(true, 420, true, 85, true, 235, true, 48, true, 0, off_level, 0, 0);
                    advance_ui(50u);

                    /* Trigger transition to ON: CO2 = 1200 ppm */
                    s_fake_node_mode = FAKE_NODE_MODE_FAIL_NEXT;
                    lora_node_status.bits.co2_ppm = 1200;

                    /* Fast-forward auto hold timer right before feeding revision frames */
                    ui_auto_state_t *ast = &((ui_auto_state_t *)hooks28->auto_states)[1];
                    uint32_t t_now = lv_tick_get();
                    ast->last_transition_ms = (t_now > 11000u) ? (t_now - 11000u) : 1u;
                    ast->in_qualification = false;
                    ast->qualify_count = 0;

                    lora_node_status.bits.sample_seq++; lora_last_rx_tick_ms = lv_tick_get(); lora_rx_revision++; advance_ui(50u);
                    lora_node_status.bits.sample_seq++; lora_last_rx_tick_ms = lv_tick_get(); lora_rx_revision++; advance_ui(50u);
                    lora_node_status.bits.sample_seq++; lora_last_rx_tick_ms = lv_tick_get(); lora_rx_revision++; advance_ui(50u);

                    /* Assert command went to PENDING */
                    if (!hooks28->device_cmds || hooks28->device_cmds[1].phase != UI_CMD_PHASE_PENDING) {
                        printf("  FAIL [p=%d pol=%u r=%d]: Auto command did not enter PENDING\n", p_idx, pol, recov);
                        test28_pass = false;
                    }

                    /* Advance past timeout -> PENDING -> TIMEOUT -> IDLE/ERROR, paused_error latched */
                    advance_ui(UI_CMD_TIMEOUT_MS + 200u);
                    ui_dismiss_toast();
                    advance_ui(50u);

                    if (!ui_get_auto_paused_error(1)) {
                        printf("  FAIL [p=%d pol=%u r=%d]: paused_error not latched after command timeout\n", p_idx, pol, recov);
                        test28_pass = false;
                    }

                    /* Recovery scenario */
                    if (recov == 0) {
                        /* Late-ACK arrives */
                        lora_node_status_set_relay_gpio(&lora_node_status, 1, on_level);
                        lora_node_status.bits.seq_echo = lora_hub_cmd.bits.seq;
                        lora_node_status.bits.sample_seq++;
                        lora_last_rx_tick_ms = lv_tick_get();
                        lora_rx_revision++;
                        advance_ui(50u);
                    } else {
                        /* Link loss then reconnect */
                        s_fake_node_mode = FAKE_NODE_MODE_STOP_REPORTING;
                        uint32_t t_loss = lv_tick_get();
                        lora_last_rx_tick_ms = (t_loss > 6000u) ? (t_loss - 6000u) : 0;
                        advance_ui(50u);

                        /* While link is offline, verify offline refusal */
                        lv_obj_t *offline_sw = (page == UI_PAGE_HOME) ? hooks28->home_dev_switches[1] : hooks28->dev_page_switches[1];
                        lv_obj_t *offline_r = (page == UI_PAGE_HOME) ? hooks28->home_dev_retries[1] : hooks28->dev_page_retries[1];
                        lv_area_t osw_coords, or_coords;
                        lv_obj_get_coords(offline_sw, &osw_coords);
                        lv_obj_get_coords(offline_r, &or_coords);
                        /* Attempt click on switch while offline */
                        sim_pointer_click((osw_coords.x1 + osw_coords.x2)/2, (osw_coords.y1 + osw_coords.y2)/2);
                        advance_ui(50u);
                        if (ui_get_device_config(1)->mode != UI_MODE_AUTO) {
                            printf("  FAIL [p=%d pol=%u r=%d]: Offline switch click accepted (expected offline refusal)\n", p_idx, pol, recov);
                            test28_pass = false;
                        }
                        /* Attempt click on retry while offline */
                        sim_pointer_click((or_coords.x1 + or_coords.x2)/2, (or_coords.y1 + or_coords.y2)/2);
                        advance_ui(50u);
                        if (!ui_get_auto_paused_error(1)) {
                            printf("  FAIL [p=%d pol=%u r=%d]: Offline retry click cleared latch (expected offline refusal)\n", p_idx, pol, recov);
                            test28_pass = false;
                        }

                        /* Link reconnects */
                        s_fake_node_mode = FAKE_NODE_MODE_NORMAL;
                        lora_node_status_set_relay_gpio(&lora_node_status, 1, on_level);
                        lora_node_status.bits.seq_echo = lora_hub_cmd.bits.seq;
                        lora_node_status.bits.sample_seq++;
                        lora_last_rx_tick_ms = lv_tick_get();
                        lora_rx_revision++;
                        advance_ui(50u);
                    }

                    /* Command settled to IDLE while auto error latch remains true */
                    if (hooks28->device_cmds[1].phase != UI_CMD_PHASE_IDLE || !ui_get_auto_paused_error(1)) {
                        printf("  FAIL [p=%d pol=%u r=%d]: Recovery did not settle command to IDLE with paused_error retained\n", p_idx, pol, recov);
                        test28_pass = false;
                    }

                    /* Ordinary navigation / dialog preserving latch (verified once) */
                    if (p_idx == 0 && pol == 0 && recov == 0) {
                        ui_navigate_to_page(UI_PAGE_DEVICES);
                        advance_ui(20u);
                        ui_open_device_settings(1);
                        advance_ui(20u);
                        ui_close_device_settings();
                        advance_ui(20u);
                        ui_navigate_to_page(page);
                        advance_ui(20u);
                        if (!ui_get_auto_paused_error(1)) {
                            printf("  FAIL [p=%d pol=%u r=%d]: Ordinary navigation/dialog cleared paused_error latch\n", p_idx, pol, recov);
                            test28_pass = false;
                        }
                    }

                    /* Check hitboxes: Switch and Retry */
                    lv_obj_t *cur_sw = (page == UI_PAGE_HOME) ? hooks28->home_dev_switches[1] : hooks28->dev_page_switches[1];
                    lv_obj_t *cur_ret = (page == UI_PAGE_HOME) ? hooks28->home_dev_retries[1] : hooks28->dev_page_retries[1];

                    lv_area_t sw_click, r_click;
                    lv_obj_get_click_area(cur_sw, &sw_click);
                    lv_obj_get_click_area(cur_ret, &r_click);

                    int sw_h = sw_click.y2 - sw_click.y1 + 1;
                    if (sw_h < 44) {
                        printf("  FAIL [p=%d pol=%u r=%d]: Switch touch height %d < 44 px\n", p_idx, pol, recov, sw_h);
                        test28_pass = false;
                    }

                    /* Assert non-overlap between Switch and Retry hitboxes */
                    bool x_overlap = !(sw_click.x2 < r_click.x1 || r_click.x2 < sw_click.x1);
                    bool y_overlap = !(sw_click.y2 < r_click.y1 || r_click.y2 < sw_click.y1);
                    if (x_overlap && y_overlap) {
                        printf("  FAIL [p=%d pol=%u r=%d]: Switch click area [%d..%d, %d..%d] overlaps Retry [%d..%d, %d..%d]\n",
                               p_idx, pol, recov, sw_click.x1, sw_click.x2, sw_click.y1, sw_click.y2,
                               r_click.x1, r_click.x2, r_click.y1, r_click.y2);
                        test28_pass = false;
                    }

                    /* If page is Devices, print actual recovered coordinates on first iteration */
                    if (page == UI_PAGE_DEVICES && pol == 0 && recov == 0) {
                        lv_coord_t dev_sw_x = lv_obj_get_x(cur_sw);
                        lv_coord_t dev_ret_x = lv_obj_get_x(cur_ret);
                        printf("  [Hitbox Proof] Recovered Devices switch at x=%d (click %d..%d), Retry at x=%d (click %d..%d)\n",
                               dev_sw_x, sw_click.x1, sw_click.x2, dev_ret_x, r_click.x1, r_click.x2);
                    }

                    /* Test Pointer actions:
                     * Execute BOTH Switch (Action 1) and Retry (Action 2) from separately re-established genuine timeout/latch setups */

                    /* Action 1: Switch click -> Manual override */
                    lv_coord_t sw_tx = (recov == 0) ? (sw_click.x1 + sw_click.x2) / 2 : (sw_click.x1 + 2);
                    lv_coord_t sw_ty = (recov == 0) ? (sw_click.y1 + sw_click.y2) / 2 : (sw_click.y1 + 2);
                    uint16_t sw_seq_before = lora_hub_cmd.bits.seq;

                    sim_pointer_click(sw_tx, sw_ty);
                    advance_ui(50u);

                    if (ui_get_device_config(1)->mode != UI_MODE_MANUAL) {
                        printf("  FAIL [p=%d pol=%u r=%d]: Switch click did not switch device to Manual\n", p_idx, pol, recov);
                        test28_pass = false;
                    }
                    if (ui_get_auto_paused_error(1)) {
                        printf("  FAIL [p=%d pol=%u r=%d]: Switch click did not clear paused_error latch\n", p_idx, pol, recov);
                        test28_pass = false;
                    }
                    if (lora_hub_cmd.bits.seq != (uint16_t)(sw_seq_before + 1)) {
                        printf("  FAIL [p=%d pol=%u r=%d]: Switch click did not emit exactly one command (seq %u -> %u)\n",
                               p_idx, pol, recov, sw_seq_before, lora_hub_cmd.bits.seq);
                        test28_pass = false;
                    }
                    if (ui_get_device_config(0)->mode != UI_MODE_MANUAL ||
                        ui_get_device_config(2)->mode != UI_MODE_MANUAL ||
                        ui_get_device_config(3)->mode != UI_MODE_MANUAL) {
                        printf("  FAIL [p=%d pol=%u r=%d]: Switch click modified an unrelated channel\n", p_idx, pol, recov);
                        test28_pass = false;
                    }

                    /* Re-establish genuine timeout/latch setup for Action 2 (Retry) */
                    ui_test_reset_device_commands();
                    s_fake_node_mode = FAKE_NODE_MODE_NORMAL;
                    advance_ui(50u);
                    ui_test_set_boot_staging(true);
                    ui_set_device_config(1, &dcfg);
                    ui_test_set_boot_staging(false);
                    lora_hub_cmd_set_relay_gpio(&lora_hub_cmd, 1, off_level);
                    sim_post_node_status(true, 420, true, 85, true, 235, true, 48, true, 0, off_level, 0, 0);
                    advance_ui(50u);

                    s_fake_node_mode = FAKE_NODE_MODE_FAIL_NEXT;
                    lora_node_status.bits.co2_ppm = 1200;
                    t_now = lv_tick_get();
                    ast->last_transition_ms = (t_now > 11000u) ? (t_now - 11000u) : 1u;
                    ast->in_qualification = false;
                    ast->qualify_count = 0;
                    lora_node_status.bits.sample_seq++; lora_last_rx_tick_ms = lv_tick_get(); lora_rx_revision++; advance_ui(50u);
                    lora_node_status.bits.sample_seq++; lora_last_rx_tick_ms = lv_tick_get(); lora_rx_revision++; advance_ui(50u);
                    lora_node_status.bits.sample_seq++; lora_last_rx_tick_ms = lv_tick_get(); lora_rx_revision++; advance_ui(50u);
                    if (hooks28->device_cmds[1].phase != UI_CMD_PHASE_PENDING) {
                        printf("  FAIL [p=%d pol=%u r=%d]: Second setup did not enter PENDING phase before timeout\n", p_idx, pol, recov);
                        test28_pass = false;
                    }
                    advance_ui(UI_CMD_TIMEOUT_MS + 200u);
                    if (!ui_get_auto_paused_error(1)) {
                        printf("  FAIL [p=%d pol=%u r=%d]: Second setup did not latch error on timeout\n", p_idx, pol, recov);
                        test28_pass = false;
                    }
                    ui_dismiss_toast();
                    advance_ui(50u);

                    if (recov == 0) {
                        lora_node_status_set_relay_gpio(&lora_node_status, 1, on_level);
                        lora_node_status.bits.seq_echo = lora_hub_cmd.bits.seq;
                        lora_node_status.bits.sample_seq++; lora_last_rx_tick_ms = lv_tick_get(); lora_rx_revision++; advance_ui(50u);
                    } else {
                        s_fake_node_mode = FAKE_NODE_MODE_STOP_REPORTING;
                        uint32_t t_loss2 = lv_tick_get();
                        lora_last_rx_tick_ms = (t_loss2 > 6000u) ? (t_loss2 - 6000u) : 0;
                        advance_ui(50u);
                        s_fake_node_mode = FAKE_NODE_MODE_NORMAL;
                        lora_node_status_set_relay_gpio(&lora_node_status, 1, on_level);
                        lora_node_status.bits.seq_echo = lora_hub_cmd.bits.seq;
                        lora_node_status.bits.sample_seq++; lora_last_rx_tick_ms = lv_tick_get(); lora_rx_revision++; advance_ui(50u);
                    }

                    if (hooks28->device_cmds[1].phase != UI_CMD_PHASE_IDLE || !ui_get_auto_paused_error(1)) {
                        printf("  FAIL [p=%d pol=%u r=%d]: Re-established recovery did not settle command with latch\n", p_idx, pol, recov);
                        test28_pass = false;
                    }

                    /* Action 2: Retry click -> Re-arm Auto */
                    cur_ret = (page == UI_PAGE_HOME) ? hooks28->home_dev_retries[1] : hooks28->dev_page_retries[1];
                    lv_obj_get_click_area(cur_ret, &r_click);
                    lv_coord_t ret_tx = (recov == 0) ? (r_click.x1 + r_click.x2) / 2 : (r_click.x2 - 2);
                    lv_coord_t ret_ty = (recov == 0) ? (r_click.y1 + r_click.y2) / 2 : (r_click.y2 - 2);
                    uint16_t seq_before = lora_hub_cmd.bits.seq;

                    sim_pointer_click(ret_tx, ret_ty);
                    advance_ui(50u);

                    if (ui_get_device_config(1)->mode != UI_MODE_AUTO) {
                        printf("  FAIL [p=%d pol=%u r=%d]: Retry click changed mode from Auto\n", p_idx, pol, recov);
                        test28_pass = false;
                    }
                    if (ui_get_auto_paused_error(1)) {
                        printf("  FAIL [p=%d pol=%u r=%d]: Retry click did not clear paused_error latch\n", p_idx, pol, recov);
                        test28_pass = false;
                    }
                    /* Verify hold timer restarted: hold is active, no immediate command */
                    if (!ui_is_auto_hold_active(1)) {
                        printf("  FAIL [p=%d pol=%u r=%d]: Retry click did not restart Auto hold timer\n", p_idx, pol, recov);
                        test28_pass = false;
                    }
                    if (lora_hub_cmd.bits.seq != seq_before) {
                        printf("  FAIL [p=%d pol=%u r=%d]: Retry click emitted immediate obsolete command (seq %u -> %u)\n",
                               p_idx, pol, recov, seq_before, lora_hub_cmd.bits.seq);
                        test28_pass = false;
                    }

                    /* Clean up after iteration */
                    dcfg.mode = UI_MODE_MANUAL;
                    ui_test_set_boot_staging(true);
                    ui_set_device_config(1, &dcfg);
                    ui_test_set_boot_staging(false);
                    ui_test_reset_device_commands();
                    advance_ui(50u);
                }
            }
        }
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

    /* 1a. Zero sends before first valid report */
    lora_last_rx_tick_ms = 0;
    sim_integration_step(2000u, &loop_seq, &loop_tx_tick);
    if (s_sim_tx_count != 0) {
        printf("  FAIL: Sent %u packets before first valid report (expected 0)\n", (unsigned)s_sim_tx_count);
        test29_pass = false;
    }

    /* 1b. Zero sends while link is stale */
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

            /* Exact packet bytes assertion */
            const uint8_t *b = s_sim_tx_records[0].bytes;
            if (b[0] != LORA_PROTO_VERSION || (b[1] != LORA_MSG_GET_STATUS && b[1] != LORA_MSG_HUB_CMD) ||
                b[2] != 0x01 || b[3] != 0x00 ||
                b[6] != (m & 0x0F) || b[7] != 0x00) {
                printf("  FAIL: Exact packet bytes mismatch on reconnect: [%02X %02X %02X %02X %02X %02X %02X %02X]\n",
                       b[0], b[1], b[2], b[3], b[4], b[5], b[6], b[7]);
                test29_pass = false;
                break;
            }
        }
    }

    /* 3. Immediate send on sequence increment & exact bytes */
    sim_reset_tx_records();
    loop_tx_tick = lv_tick_get();
    loop_seq = lora_hub_cmd.bits.seq;
    ui_trigger_device_toggle(3); /* increments seq */
    sim_integration_step(10u, &loop_seq, &loop_tx_tick);
    if (s_sim_tx_count != 1 || s_sim_tx_records[0].seq != loop_seq) {
        printf("  FAIL: Sequence increment did not produce immediate send (count=%u)\n", (unsigned)s_sim_tx_count);
        test29_pass = false;
    }
    if (s_sim_tx_count >= 1) {
        const uint8_t *b = s_sim_tx_records[0].bytes;
        uint16_t exp_seq = loop_seq;
        if (b[0] != LORA_PROTO_VERSION || b[1] != LORA_MSG_HUB_CMD ||
            b[2] != 0x01 || b[3] != 0x00 ||
            b[4] != (exp_seq & 0xFF) || b[5] != ((exp_seq >> 8) & 0xFF) ||
            b[7] != 0x00) {
            printf("  FAIL: Exact packet bytes mismatch on sequence increment\n");
            test29_pass = false;
        }
    }

    /* 4. No send before due (periodic interval is 1000ms, advance 950ms -> count still 1) */
    sim_integration_step(950u, &loop_seq, &loop_tx_tick);
    if (s_sim_tx_count != 1) {
        printf("  FAIL: Premature send before 1000ms periodic interval (count=%u at 960ms)\n", (unsigned)s_sim_tx_count);
        test29_pass = false;
    }

    /* Step past 1000ms (50ms more -> 1000ms elapsed) -> second send recorded */
    sim_integration_step(50u, &loop_seq, &loop_tx_tick);
    if (s_sim_tx_count != 2) {
        printf("  FAIL: Expected 2nd send at 1000ms boundary, got %u\n", (unsigned)s_sim_tx_count);
        test29_pass = false;
    } else {
        uint32_t dt = s_sim_tx_records[1].tick_ms - s_sim_tx_records[0].tick_ms;
        if (dt < 990u || dt > 1010u) {
            printf("  FAIL: Periodic interval %u ms outside 990-1010ms tolerance\n", dt);
            test29_pass = false;
        }
    }

    /* Advance 2000ms more for total >= 4 periodic transmissions */
    sim_integration_step(2000u, &loop_seq, &loop_tx_tick);
    if (s_sim_tx_count < 4) {
        printf("  FAIL: Expected >= 4 periodic transmissions, got %u\n", (unsigned)s_sim_tx_count);
        test29_pass = false;
    }

    /* Validate full real stream log */
    if (!validate_tx_log(s_sim_tx_records, s_sim_tx_count, true)) {
        printf("  FAIL: Real recorded TX stream failed log validation\n");
        test29_pass = false;
    }

    /* 5. Executed-loop fault injection (gating bypass, excessive rate, corrupt header, omitted periodic) */
    /* Fault 1: Executed-loop Gating bypass (force send while link is stale/unready) */
    uint32_t t_stale = lv_tick_get();
    lora_last_rx_tick_ms = (t_stale > 6000u) ? (t_stale - 6000u) : 0;
    sim_reset_tx_records();
    loop_tx_tick = 0;
    loop_seq = lora_hub_cmd.bits.seq;
    sim_integration_step_with_fault(1500u, &loop_seq, &loop_tx_tick, SIM_TX_FAULT_GATING_BYPASS);
    if (s_sim_tx_count < 1) {
        printf("  FAIL: Executed-loop gating bypass did not record fault transmission\n");
        test29_pass = false;
    } else if (validate_tx_log(s_sim_tx_records, s_sim_tx_count, true) != false) {
        printf("  FAIL: Executed-loop gating bypass fault was NOT rejected in Release\n");
        test29_pass = false;
    }

    /* Restore link online for subsequent loop faults */
    populate_node_status_baseline();
    lora_last_rx_tick_ms = lv_tick_get();
    advance_ui(50u);

    /* Fault 2: Executed-loop Excessive transmission rate (dt = 500ms < 980ms) */
    sim_reset_tx_records();
    loop_tx_tick = 0;
    loop_seq = lora_hub_cmd.bits.seq;
    sim_integration_step_with_fault(1200u, &loop_seq, &loop_tx_tick, SIM_TX_FAULT_TOO_FAST_RATE);
    if (s_sim_tx_count < 2) {
        printf("  FAIL: Executed-loop excessive rate did not record >= 2 transmissions (got %u)\n", (unsigned)s_sim_tx_count);
        test29_pass = false;
    } else if (validate_tx_log(s_sim_tx_records, s_sim_tx_count, true) != false) {
        printf("  FAIL: Executed-loop excessive rate fault was NOT rejected in Release\n");
        test29_pass = false;
    }

    /* Fault 3: Executed-loop Corrupt packet header (bytes[0] = 0xAA) */
    sim_reset_tx_records();
    loop_tx_tick = 0;
    loop_seq = lora_hub_cmd.bits.seq;
    sim_integration_step_with_fault(200u, &loop_seq, &loop_tx_tick, SIM_TX_FAULT_CORRUPT_HEADER);
    if (s_sim_tx_count < 1) {
        printf("  FAIL: Executed-loop corrupt header did not record transmission\n");
        test29_pass = false;
    } else if (validate_tx_log(s_sim_tx_records, s_sim_tx_count, true) != false) {
        printf("  FAIL: Executed-loop corrupt header fault was NOT rejected in Release\n");
        test29_pass = false;
    }

    /* Fault 4: Executed-loop Omitted periodic transmission (dt = 2000ms > 1100ms) */
    sim_reset_tx_records();
    loop_tx_tick = 0;
    loop_seq = lora_hub_cmd.bits.seq;
    sim_integration_step_with_fault(2500u, &loop_seq, &loop_tx_tick, SIM_TX_FAULT_OMITTED_PERIODIC);
    if (s_sim_tx_count < 2) {
        printf("  FAIL: Executed-loop omitted periodic TX did not record >= 2 transmissions (got %u)\n", (unsigned)s_sim_tx_count);
        test29_pass = false;
    } else if (validate_tx_log(s_sim_tx_records, s_sim_tx_count, true) != false) {
        printf("  FAIL: Executed-loop omitted periodic TX fault was NOT rejected in Release\n");
        test29_pass = false;
    }

    lora_last_rx_tick_ms = save_rx;
    ui_test_reset_device_commands();
    advance_ui(500u);

    if (test29_pass) {
        printf("  PASS: Recording TX loop, exact bytes, periodic/sequence gating & negative doubles verified.\n");
        passed_tests++;
    }

    /* -------------------------------------------------------------
     * TEST 30: Auto Lifecycle (Sensor Recovery Hold, Reconnect Latch, Explicit Re-arm)
     * ------------------------------------------------------------- */
    total_tests++;
    printf("\n[CHECK 30] Auto Lifecycle (Sensor Recovery Hold, Reconnect Latch, Explicit Re-arm)...\n");
    bool test30_pass = true;

    fake_node_mode_t save_fn_mode30 = s_fake_node_mode;
    ui_test_set_boot_staging(true);
    for (uint8_t d = 0; d < UI_DEVICE_COUNT; d++) {
        ui_hub_device_config_t cfg;
        ui_auto_get_defaults(d, (d == 3) ? UI_PRESET_LIGHT : d, &cfg);
        cfg.mode = UI_MODE_MANUAL;
        cfg.active_low = 0;
        ui_set_device_config(d, &cfg);
    }
    ui_test_set_boot_staging(false);
    ui_test_reset_device_commands();
    s_fake_node_mode = FAKE_NODE_MODE_STOP_REPORTING;

    const ui_test_hooks_t *hooks30 = ui_get_test_hooks();

    /* =============================================================
     * Rule 1: Fan / CO2 ppm (Device 1)
     * ============================================================= */
    ui_hub_device_config_t dev1_auto;
    ui_auto_get_defaults(1, UI_PRESET_FAN, &dev1_auto);
    dev1_auto.mode = UI_MODE_AUTO;
    ui_set_device_config(1, &dev1_auto);
    sim_post_node_status(true, 420, true, 85, true, 235, true, 48, true, 0, 0, 0, 0);
    ui_test_reset_device_commands();
    for (int t = 0; t < 11; t++) {
        lora_last_rx_tick_ms = lv_tick_get();
        advance_ui(1000u);
    }

    /* Invalidate CO2 sensor */
    lora_node_status.bits.co2_valid = 0;
    lora_node_status.bits.co2_ppm = 1400; /* High CO2, but invalid */
    lora_node_status.bits.sample_seq++;
    lora_rx_revision++;
    advance_ui(100u);
    if (hooks30 && hooks30->device_cmds && hooks30->device_cmds[1].phase == UI_CMD_PHASE_PENDING) {
        printf("  FAIL: Fan auto decision fired on invalid CO2 reading\n");
        test30_pass = false;
    }

    /* CO2 recovers with high reading -> recovery hold (10s) starts */
    lora_node_status.bits.co2_valid = 1;
    lora_node_status.bits.sample_seq++;
    lora_rx_revision++;
    advance_ui(50u);

    /* Good frames at 2s, 4s, 6s: verify hold suppresses decision and does NOT perpetually restart */
    for (int t = 0; t < 3; t++) {
        lora_last_rx_tick_ms = lv_tick_get();
        lora_node_status.bits.sample_seq++;
        lora_rx_revision++;
        advance_ui(2000u);
    }
    if (hooks30 && hooks30->device_cmds && hooks30->device_cmds[1].phase == UI_CMD_PHASE_PENDING) {
        printf("  FAIL: Fan auto decision fired prematurely during recovery hold\n");
        test30_pass = false;
    }

    /* Hold elapses (> 10s total from recovery) while link stays connected */
    for (int t = 0; t < 5; t++) {
        lora_last_rx_tick_ms = lv_tick_get();
        advance_ui(1000u);
    }

    /* Time alone and unchanged revision do NOT satisfy 3 fresh frames */
    for (int t = 0; t < 2; t++) {
        lora_last_rx_tick_ms = lv_tick_get();
        advance_ui(1000u);
    }
    if (hooks30 && hooks30->device_cmds && hooks30->device_cmds[1].phase == UI_CMD_PHASE_PENDING) {
        printf("  FAIL: Fan auto decision fired from time alone without fresh frames\n");
        test30_pass = false;
    }

    /* Feed fresh frames with controlled arrival */
    lora_last_rx_tick_ms = lv_tick_get();
    lora_node_status.bits.sample_seq++; lora_rx_revision++; advance_ui(100u); /* Frame 1 */
    if (hooks30 && hooks30->device_cmds && hooks30->device_cmds[1].phase == UI_CMD_PHASE_PENDING) {
        printf("  FAIL: Fan auto decision fired on frame 1 (expected 3 frames)\n");
        test30_pass = false;
    }
    lora_last_rx_tick_ms = lv_tick_get();
    lora_node_status.bits.sample_seq++; lora_rx_revision++; advance_ui(100u); /* Frame 2 */
    if (hooks30 && hooks30->device_cmds && hooks30->device_cmds[1].phase == UI_CMD_PHASE_PENDING) {
        printf("  FAIL: Fan auto decision fired on frame 2 (expected 3 frames)\n");
        test30_pass = false;
    }

    s_fake_node_mode = FAKE_NODE_MODE_FAIL_NEXT;
    lora_last_rx_tick_ms = lv_tick_get();
    lora_node_status.bits.sample_seq++; lora_rx_revision++; advance_ui(100u); /* Frame 3: fires decision */
    if (!hooks30 || !hooks30->device_cmds || hooks30->device_cmds[1].phase != UI_CMD_PHASE_PENDING) {
        printf("  FAIL: Fan auto decision failed to fire after recovery hold and 3 fresh frames\n");
        test30_pass = false;
    }

    /* Command times out and latches paused_error */
    advance_ui(UI_CMD_TIMEOUT_MS + 200u);
    if (!ui_get_auto_paused_error(1)) {
        printf("  FAIL: Fan command timeout did not latch auto paused_error\n");
        test30_pass = false;
    }

    /* Explicit Retry clears paused_error */
    ui_trigger_device_retry(1);
    advance_ui(50u);
    if (ui_get_auto_paused_error(1)) {
        printf("  FAIL: Explicit Retry did not clear Fan paused_error\n");
        test30_pass = false;
    }

    /* Unrelated fault isolation: invalidate VOC and Temp */
    lora_node_status.bits.voc_valid = 0;
    lora_node_status.bits.temp_valid = 0;
    lora_node_status_set_relay_gpio(&lora_node_status, 1, 0); /* Fan relay OFF */
    lora_last_rx_tick_ms = lv_tick_get();
    lora_node_status.bits.sample_seq++; lora_rx_revision++; advance_ui(100u);
    /* CO2 in normal band (750 ppm < 800) -> hold passes, fan remains stable */
    lora_node_status.bits.co2_ppm = 750;
    for (int t = 0; t < 11; t++) {
        lora_last_rx_tick_ms = lv_tick_get();
        advance_ui(1000u);
    }
    lora_node_status.bits.co2_ppm = 1200;
    lora_last_rx_tick_ms = lv_tick_get(); lora_node_status.bits.sample_seq++; lora_rx_revision++; advance_ui(100u);
    lora_last_rx_tick_ms = lv_tick_get(); lora_node_status.bits.sample_seq++; lora_rx_revision++; advance_ui(100u);
    lora_last_rx_tick_ms = lv_tick_get(); lora_node_status.bits.sample_seq++; lora_rx_revision++; advance_ui(100u);
    if (!hooks30 || !hooks30->device_cmds || hooks30->device_cmds[1].phase != UI_CMD_PHASE_PENDING) {
        printf("  FAIL: Fan auto decision blocked by unrelated VOC/Temp fault\n");
        test30_pass = false;
    }
    ui_test_reset_device_commands();
    dev1_auto.mode = UI_MODE_MANUAL;
    ui_set_device_config(1, &dev1_auto);
    advance_ui(200u);

    /* =============================================================
     * Rule 2: Purifier / VOC (Device 0)
     * ============================================================= */
    s_fake_node_mode = FAKE_NODE_MODE_STOP_REPORTING;
    ui_hub_device_config_t dev0_auto;
    ui_auto_get_defaults(0, UI_PRESET_PURIFIER, &dev0_auto);
    dev0_auto.mode = UI_MODE_AUTO;
    ui_set_device_config(0, &dev0_auto);
    sim_post_node_status(true, 420, true, 85, true, 235, true, 48, true, 0, 0, 0, 0);
    ui_test_reset_device_commands();
    for (int t = 0; t < 11; t++) {
        lora_last_rx_tick_ms = lv_tick_get();
        advance_ui(1000u);
    }

    /* Invalidate VOC sensor */
    lora_node_status.bits.voc_valid = 0;
    lora_node_status.bits.tvoc_ppb = 700; /* High TVOC (above the 660 ppb default), but invalid */
    lora_node_status.bits.sample_seq++;
    lora_rx_revision++;
    advance_ui(100u);
    if (hooks30 && hooks30->device_cmds && hooks30->device_cmds[0].phase == UI_CMD_PHASE_PENDING) {
        printf("  FAIL: Purifier auto decision fired on invalid VOC reading\n");
        test30_pass = false;
    }

    /* VOC recovers with high reading -> recovery hold (10s) starts */
    lora_node_status.bits.voc_valid = 1;
    lora_node_status.bits.sample_seq++;
    lora_rx_revision++;
    advance_ui(50u);

    /* Good frames at 2s, 4s, 6s: verify hold suppresses decision */
    for (int t = 0; t < 3; t++) {
        lora_last_rx_tick_ms = lv_tick_get();
        lora_node_status.bits.sample_seq++;
        lora_rx_revision++;
        advance_ui(2000u);
    }
    if (hooks30 && hooks30->device_cmds && hooks30->device_cmds[0].phase == UI_CMD_PHASE_PENDING) {
        printf("  FAIL: Purifier auto decision fired prematurely during recovery hold\n");
        test30_pass = false;
    }

    /* Hold elapses (> 10s total from recovery) while link stays connected */
    for (int t = 0; t < 5; t++) {
        lora_last_rx_tick_ms = lv_tick_get();
        advance_ui(1000u);
    }

    /* Time alone does NOT satisfy 3 fresh frames */
    for (int t = 0; t < 2; t++) {
        lora_last_rx_tick_ms = lv_tick_get();
        advance_ui(1000u);
    }
    if (hooks30 && hooks30->device_cmds && hooks30->device_cmds[0].phase == UI_CMD_PHASE_PENDING) {
        printf("  FAIL: Purifier auto decision fired from time alone without fresh frames\n");
        test30_pass = false;
    }

    /* Feed fresh frames with controlled arrival */
    lora_last_rx_tick_ms = lv_tick_get();
    lora_node_status.bits.sample_seq++; lora_rx_revision++; advance_ui(100u); /* Frame 1 */
    if (hooks30 && hooks30->device_cmds && hooks30->device_cmds[0].phase == UI_CMD_PHASE_PENDING) {
        printf("  FAIL: Purifier auto decision fired on frame 1 (expected 3 frames)\n");
        test30_pass = false;
    }
    /* Controlled duplicate/time-only input does NOT advance qualification */
    lora_last_rx_tick_ms = lv_tick_get();
    advance_ui(100u);
    if (hooks30 && hooks30->device_cmds && hooks30->device_cmds[0].phase == UI_CMD_PHASE_PENDING) {
        printf("  FAIL: Purifier auto decision fired on duplicate revision\n");
        test30_pass = false;
    }
    lora_last_rx_tick_ms = lv_tick_get();
    lora_node_status.bits.sample_seq++; lora_rx_revision++; advance_ui(100u); /* Frame 2 */
    if (hooks30 && hooks30->device_cmds && hooks30->device_cmds[0].phase == UI_CMD_PHASE_PENDING) {
        printf("  FAIL: Purifier auto decision fired on frame 2 (expected 3 frames)\n");
        test30_pass = false;
    }
    s_fake_node_mode = FAKE_NODE_MODE_FAIL_NEXT;
    lora_last_rx_tick_ms = lv_tick_get();
    lora_node_status.bits.sample_seq++; lora_rx_revision++; advance_ui(100u); /* Frame 3: fires decision */
    if (!hooks30 || !hooks30->device_cmds || hooks30->device_cmds[0].phase != UI_CMD_PHASE_PENDING) {
        printf("  FAIL: Purifier auto decision failed to fire after recovery hold and 3 fresh frames\n");
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

    /* Unrelated fault isolation: invalidate CO2 and Temp */
    lora_node_status.bits.co2_valid = 0;
    lora_node_status.bits.temp_valid = 0;
    lora_node_status_set_relay_gpio(&lora_node_status, 0, 0); /* Purifier relay OFF */
    lora_last_rx_tick_ms = lv_tick_get();
    lora_node_status.bits.tvoc_ppb = 80;
    for (int t = 0; t < 11; t++) {
        lora_last_rx_tick_ms = lv_tick_get();
        advance_ui(1000u);
    }
    lora_node_status.bits.tvoc_ppb = 700;
    lora_last_rx_tick_ms = lv_tick_get(); lora_node_status.bits.sample_seq++; lora_rx_revision++; advance_ui(100u);
    lora_last_rx_tick_ms = lv_tick_get(); lora_node_status.bits.sample_seq++; lora_rx_revision++; advance_ui(100u);
    lora_last_rx_tick_ms = lv_tick_get(); lora_node_status.bits.sample_seq++; lora_rx_revision++; advance_ui(100u);
    if (!hooks30 || !hooks30->device_cmds || hooks30->device_cmds[0].phase != UI_CMD_PHASE_PENDING) {
        printf("  FAIL: Purifier auto decision blocked by unrelated CO2/Temp fault\n");
        test30_pass = false;
    }
    ui_test_reset_device_commands();
    dev0_auto.mode = UI_MODE_MANUAL;
    ui_set_device_config(0, &dev0_auto);
    advance_ui(200u);

    /* =============================================================
     * Rule 3: Humidifier / RH (Device 2)
     * ============================================================= */
    s_fake_node_mode = FAKE_NODE_MODE_STOP_REPORTING;
    ui_hub_device_config_t dev2_auto;
    ui_auto_get_defaults(2, UI_PRESET_HUMIDIFIER, &dev2_auto);
    dev2_auto.mode = UI_MODE_AUTO;
    ui_set_device_config(2, &dev2_auto);
    sim_post_node_status(true, 420, true, 85, true, 235, true, 60, true, 0, 0, 0, 0);
    ui_test_reset_device_commands();
    for (int t = 0; t < 11; t++) {
        lora_last_rx_tick_ms = lv_tick_get();
        advance_ui(1000u);
    }

    /* Invalidate RH sensor */
    lora_node_status.bits.humid_valid = 0;
    lora_node_status.bits.humidity_pct = 30; /* Dry air <= 40%, but invalid */
    lora_node_status.bits.sample_seq++;
    lora_rx_revision++;
    advance_ui(100u);
    if (hooks30 && hooks30->device_cmds && hooks30->device_cmds[2].phase == UI_CMD_PHASE_PENDING) {
        printf("  FAIL: Humidifier auto decision fired on invalid RH reading\n");
        test30_pass = false;
    }

    /* RH recovers with dry reading -> recovery hold (10s) starts */
    lora_node_status.bits.humid_valid = 1;
    lora_node_status.bits.sample_seq++;
    lora_rx_revision++;
    advance_ui(50u);

    /* Good frames at 2s, 4s, 6s: verify hold suppresses decision */
    for (int t = 0; t < 3; t++) {
        lora_last_rx_tick_ms = lv_tick_get();
        lora_node_status.bits.sample_seq++;
        lora_rx_revision++;
        advance_ui(2000u);
    }
    if (hooks30 && hooks30->device_cmds && hooks30->device_cmds[2].phase == UI_CMD_PHASE_PENDING) {
        printf("  FAIL: Humidifier auto decision fired prematurely during recovery hold\n");
        test30_pass = false;
    }

    /* Hold elapses (> 10s total from recovery) while link stays connected */
    for (int t = 0; t < 5; t++) {
        lora_last_rx_tick_ms = lv_tick_get();
        advance_ui(1000u);
    }

    /* Time alone does NOT satisfy 3 fresh frames */
    for (int t = 0; t < 2; t++) {
        lora_last_rx_tick_ms = lv_tick_get();
        advance_ui(1000u);
    }
    if (hooks30 && hooks30->device_cmds && hooks30->device_cmds[2].phase == UI_CMD_PHASE_PENDING) {
        printf("  FAIL: Humidifier auto decision fired from time alone without fresh frames\n");
        test30_pass = false;
    }

    /* Feed fresh frames with controlled arrival */
    lora_last_rx_tick_ms = lv_tick_get();
    lora_node_status.bits.sample_seq++; lora_rx_revision++; advance_ui(100u); /* Frame 1 */
    if (hooks30 && hooks30->device_cmds && hooks30->device_cmds[2].phase == UI_CMD_PHASE_PENDING) {
        printf("  FAIL: Humidifier auto decision fired on frame 1 (expected 3 frames)\n");
        test30_pass = false;
    }
    /* Controlled duplicate/time-only input does NOT advance qualification */
    lora_last_rx_tick_ms = lv_tick_get();
    advance_ui(100u);
    if (hooks30 && hooks30->device_cmds && hooks30->device_cmds[2].phase == UI_CMD_PHASE_PENDING) {
        printf("  FAIL: Humidifier auto decision fired on duplicate revision\n");
        test30_pass = false;
    }
    lora_last_rx_tick_ms = lv_tick_get();
    lora_node_status.bits.sample_seq++; lora_rx_revision++; advance_ui(100u); /* Frame 2 */
    if (hooks30 && hooks30->device_cmds && hooks30->device_cmds[2].phase == UI_CMD_PHASE_PENDING) {
        printf("  FAIL: Humidifier auto decision fired on frame 2 (expected 3 frames)\n");
        test30_pass = false;
    }
    s_fake_node_mode = FAKE_NODE_MODE_FAIL_NEXT;
    lora_last_rx_tick_ms = lv_tick_get();
    lora_node_status.bits.sample_seq++; lora_rx_revision++; advance_ui(100u); /* Frame 3: fires decision */
    if (!hooks30 || !hooks30->device_cmds || hooks30->device_cmds[2].phase != UI_CMD_PHASE_PENDING) {
        printf("  FAIL: Humidifier auto decision failed to fire after recovery hold and 3 fresh frames\n");
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

    /* Unrelated fault isolation: invalidate VOC and CO2 */
    lora_node_status.bits.voc_valid = 0;
    lora_node_status.bits.co2_valid = 0;
    lora_node_status_set_relay_gpio(&lora_node_status, 2, 0); /* Humidifier relay OFF */
    lora_last_rx_tick_ms = lv_tick_get();
    lora_node_status.bits.humidity_pct = 55;
    for (int t = 0; t < 11; t++) {
        lora_last_rx_tick_ms = lv_tick_get();
        advance_ui(1000u);
    }
    lora_node_status.bits.humidity_pct = 35;
    lora_last_rx_tick_ms = lv_tick_get(); lora_node_status.bits.sample_seq++; lora_rx_revision++; advance_ui(100u);
    lora_last_rx_tick_ms = lv_tick_get(); lora_node_status.bits.sample_seq++; lora_rx_revision++; advance_ui(100u);
    lora_last_rx_tick_ms = lv_tick_get(); lora_node_status.bits.sample_seq++; lora_rx_revision++; advance_ui(100u);
    if (!hooks30 || !hooks30->device_cmds || hooks30->device_cmds[2].phase != UI_CMD_PHASE_PENDING) {
        printf("  FAIL: Humidifier auto decision blocked by unrelated VOC/CO2 fault\n");
        test30_pass = false;
    }

    /* Restore clean manual configs & normal fake node mode */
    ui_test_set_boot_staging(true);
    for (uint8_t d = 0; d < UI_DEVICE_COUNT; d++) {
        ui_hub_device_config_t rst;
        ui_auto_get_defaults(d, (d == 3) ? UI_PRESET_LIGHT : d, &rst);
        rst.mode = UI_MODE_MANUAL;
        ui_set_device_config(d, &rst);
    }
    ui_test_set_boot_staging(false);
    s_fake_node_mode = save_fn_mode30;
    populate_node_status_baseline();
    ui_test_reset_device_commands();
    ui_dismiss_toast();
    advance_ui(500u);

    if (test30_pass) {
        printf("  PASS: Auto lifecycle, all 3 rules sensor recovery hold, fresh frame qualification & re-arm verified.\n");
        passed_tests++;
    }

    /* -------------------------------------------------------------
     * TEST 31: 50 Complete Cycles Memory Stress (Peak, Fragmentation, Drift)
     * ------------------------------------------------------------- */
    total_tests++;
    printf("\n[CHECK 31] 50 Complete Cycles Memory Stress (Peak, Fragmentation, Drift)...\n");
    bool test31_pass = true;

    ui_dismiss_toast();
    advance_ui(50u);

    lv_mem_monitor_t mem_start31;
    lv_mem_monitor(&mem_start31);
    printf("  Heap before CHECK 31: used=%u%% free=%u biggest=%u frag=%u%%\n",
           (unsigned)mem_start31.used_pct, (unsigned)mem_start31.free_size,
           (unsigned)mem_start31.free_biggest_size, (unsigned)mem_start31.frag_pct);

    size_t min_imm_total_free = mem_start31.free_size;
    size_t min_imm_contemp_biggest = mem_start31.free_biggest_size;
    size_t min_imm_biggest_block = mem_start31.free_biggest_size;
    uint8_t peak_imm_used_pct = mem_start31.used_pct;
    uint8_t peak_imm_frag_pct = mem_start31.frag_pct;
    const char *min_imm_state = "Start";

    size_t min_stable_total_free = mem_start31.free_size;
    size_t min_stable_contemp_biggest = mem_start31.free_biggest_size;
    size_t min_stable_biggest_block = mem_start31.free_biggest_size;
    uint8_t peak_stable_used_pct = mem_start31.used_pct;
    uint8_t peak_stable_frag_pct = mem_start31.frag_pct;
    const char *min_stable_state = "Start";

    size_t used_at_cycle10 = 0;
    size_t used_at_cycle50 = 0;

#define TRACK_MEM_STATE(st_name, is_imm) do { \
    lv_mem_monitor_t cur_m; \
    lv_mem_monitor(&cur_m); \
    if (cycle == 1) { \
        printf("    [Cycle 1 Heap %s] %-30s: used=%u%% free=%u biggest=%u frag=%u%%\n", \
               (is_imm) ? "Immediate" : "Settled  ", \
               (st_name), (unsigned)cur_m.used_pct, (unsigned)cur_m.free_size, \
               (unsigned)cur_m.free_biggest_size, (unsigned)cur_m.frag_pct); \
    } \
    if (is_imm) { \
        if (cur_m.free_size < min_imm_total_free) { \
            min_imm_total_free = cur_m.free_size; \
            min_imm_contemp_biggest = cur_m.free_biggest_size; \
            min_imm_state = (st_name); \
        } \
        if (cur_m.free_biggest_size < min_imm_biggest_block) min_imm_biggest_block = cur_m.free_biggest_size; \
        if (cur_m.used_pct > peak_imm_used_pct) peak_imm_used_pct = cur_m.used_pct; \
        if (cur_m.frag_pct > peak_imm_frag_pct) peak_imm_frag_pct = cur_m.frag_pct; \
    } else { \
        if (cur_m.free_size < min_stable_total_free) { \
            min_stable_total_free = cur_m.free_size; \
            min_stable_contemp_biggest = cur_m.free_biggest_size; \
            min_stable_state = (st_name); \
        } \
        if (cur_m.free_biggest_size < min_stable_biggest_block) min_stable_biggest_block = cur_m.free_biggest_size; \
        if (cur_m.used_pct > peak_stable_used_pct) peak_stable_used_pct = cur_m.used_pct; \
        if (cur_m.frag_pct > peak_stable_frag_pct) peak_stable_frag_pct = cur_m.frag_pct; \
    } \
} while (0)

    /* Configure Device 0 as Air Purifier in AUTO mode for genuine 50-cycle stress */
    ui_hub_device_config_t cfg_stress0;
    ui_auto_get_defaults(0, UI_PRESET_PURIFIER, &cfg_stress0);
    cfg_stress0.mode = UI_MODE_AUTO;
    cfg_stress0.auto_on_thresh = 150;
    cfg_stress0.auto_off_thresh = 100;
    ui_test_set_boot_staging(true);
    ui_set_device_config(0, &cfg_stress0);
    ui_test_set_boot_staging(false);

    /* Baseline status: VOC = 80 (below ON threshold 150), relay 0 OFF (0), link online */
    sim_post_node_status(true, 420, true, 80, true, 235, true, 48, true, 0, 0, 0, 0);
    advance_ui(50u);

    uint32_t count_auto_cmds_sent = 0;
    uint32_t count_auto_pending = 0;
    uint32_t count_auto_timeouts = 0;
    uint32_t count_auto_latches = 0;
    uint32_t count_auto_recoveries = 0;
    uint32_t count_auto_rearms = 0;

    for (int cycle = 1; cycle <= 50; cycle++) {
        const ui_test_hooks_t *h31 = ui_get_test_hooks();

        /* Deliberate Auto lifecycle activity scheduled across the 50 cycles */
        if (cycle % 10 == 1) {
            /* 1. Trigger Auto command: VOC rises to 180 (> 150), hold expiry -> PENDING */
            uint16_t seq_before_31 = lora_hub_cmd.bits.seq;
            lora_node_status.bits.tvoc_ppb = 180;
            s_fake_node_mode = FAKE_NODE_MODE_FAIL_NEXT; /* Withhold ACK to induce timeout */
            if (h31 && h31->auto_states) {
                ui_auto_state_t *ast0 = &((ui_auto_state_t *)h31->auto_states)[0];
                uint32_t t_now = lv_tick_get();
                ast0->last_transition_ms = (t_now > 11000u) ? (t_now - 11000u) : 1u;
                ast0->in_qualification = false;
                ast0->qualify_count = 0;
            }
            lora_last_rx_tick_ms = lv_tick_get(); lora_node_status.bits.sample_seq++; lora_rx_revision++; advance_ui(20u);
            lora_last_rx_tick_ms = lv_tick_get(); lora_node_status.bits.sample_seq++; lora_rx_revision++; advance_ui(20u);
            lora_last_rx_tick_ms = lv_tick_get(); lora_node_status.bits.sample_seq++; lora_rx_revision++; advance_ui(20u);
            uint8_t tx_mask_on = lora_hub_cmd_get_relay_gpios_mask(&lora_hub_cmd);
            if (h31 && h31->device_cmds && h31->device_cmds[0].phase == UI_CMD_PHASE_PENDING &&
                lora_hub_cmd.bits.seq != seq_before_31 && (tx_mask_on & 1) == 1) {
                count_auto_pending++;
                count_auto_cmds_sent++;
            } else {
                printf("  FAIL: Cycle %d: Auto command ON not sent with incremented seq / mask\n", cycle);
                test31_pass = false;
            }
        }

        ui_navigate_to_page(UI_PAGE_TRENDS);
        advance_ui(20u);
        TRACK_MEM_STATE("Trends", false);

        ui_navigate_to_page(UI_PAGE_DEVICES);
        advance_ui(20u);
        TRACK_MEM_STATE("Devices", false);

        ui_open_device_settings(0);
        TRACK_MEM_STATE("Device settings first rendered", true);
        advance_ui(20u);
        TRACK_MEM_STATE("Device settings settled", false);

        if (h31 && h31->dialog_preset_dd && *h31->dialog_preset_dd) {
            lv_dropdown_open(*h31->dialog_preset_dd);
            advance_ui(20u);
            TRACK_MEM_STATE("Dropdown open", false);

            if (cycle % 10 == 2) {
                /* 2. Timeout occurs while in modal/dropdown: PENDING -> ERROR/LATCHED + toast */
                advance_ui(UI_CMD_TIMEOUT_MS + 200u);
                if (h31->device_cmds && h31->device_cmds[0].phase == UI_CMD_PHASE_ERROR) {
                    count_auto_timeouts++;
                } else {
                    printf("  FAIL: Cycle %d: Auto command did not enter ERROR phase on timeout\n", cycle);
                    test31_pass = false;
                }
                if (ui_get_auto_paused_error(0)) {
                    count_auto_latches++;
                } else {
                    printf("  FAIL: Cycle %d: Auto paused_error not latched on timeout\n", cycle);
                    test31_pass = false;
                }
                TRACK_MEM_STATE("Auto timeout latched in dropdown", false);
            }

            lv_dropdown_close(*h31->dialog_preset_dd);
            advance_ui(20u);
            TRACK_MEM_STATE("Dropdown closed", false);
        }

        ui_dialog_open_limits();
        advance_ui(20u);
        TRACK_MEM_STATE("Auto limits open", false);

        /* Assert subview controls exist in every cycle */
        if (!h31 || !h31->dialog_btn_off_plus || !*h31->dialog_btn_off_plus ||
            !h31->dialog_btn_save_auto || !*h31->dialog_btn_save_auto) {
            printf("  FAIL: Cycle %d: Auto limits subview controls missing\n", cycle);
            test31_pass = false;
        } else {
            /* Create genuine invalid pair for Purifier (150/150, gap 0 < 5) via REAL POINTER CLICKS:
             * Canonical defaults: ON=150, OFF=100.
             * Click btn_off_plus 10 times via sim_pointer_click() */
            lv_area_t plus_coords;
            lv_obj_get_coords(*h31->dialog_btn_off_plus, &plus_coords);
            lv_coord_t px = (plus_coords.x1 + plus_coords.x2) / 2;
            lv_coord_t py = (plus_coords.y1 + plus_coords.y2) / 2;
            for (int s = 0; s < 10; s++) {
                sim_pointer_click(px, py);
            }
            advance_ui(20u);

            /* In EVERY cycle: assert validation message visible and Save button disabled */
            const char *v_msg = ui_get_dialog_validation_msg();
            if (!v_msg || strlen(v_msg) == 0) {
                printf("  FAIL: Cycle %d: Genuine invalid draft (150/150) did not produce validation message\n", cycle);
                test31_pass = false;
            }
            if (!lv_obj_has_state(*h31->dialog_btn_save_auto, LV_STATE_DISABLED)) {
                printf("  FAIL: Cycle %d: Genuine invalid draft did not disable Save button\n", cycle);
                test31_pass = false;
            }
        }
        TRACK_MEM_STATE("Validation error visible (genuine invalid draft)", false);

        /* Discard invalid draft: alternate between Back and Cancel/X across cycles */
        if (cycle % 2 != 0) {
            /* Odd cycles: Back -> Device settings -> Close */
            ui_dialog_limits_back();
            advance_ui(20u);
            TRACK_MEM_STATE("Back to device settings", false);

            ui_close_device_settings();
            advance_ui(20u);
            TRACK_MEM_STATE("Device settings closed", false);
        } else {
            /* Even cycles: Cancel directly from Auto limits */
            ui_close_device_settings();
            advance_ui(20u);
            TRACK_MEM_STATE("Device settings closed from limits", false);
        }

        /* Check discard/reopen loads canonical defaults on cycles 2, 25, 50 */
        if (cycle == 2 || cycle == 25 || cycle == 50) {
            ui_open_device_settings(0);
            advance_ui(20u);
            ui_dialog_open_limits();
            advance_ui(20u);
            if (ui_get_dialog_edit_on_thresh() != 150 || ui_get_dialog_edit_off_thresh() != 100) {
                printf("  FAIL: Cycle %d reopen retained invalid draft instead of canonical defaults (on=%d, off=%d)\n",
                       cycle, (int)ui_get_dialog_edit_on_thresh(), (int)ui_get_dialog_edit_off_thresh());
                test31_pass = false;
            }
            ui_close_device_settings();
            advance_ui(20u);
        }

        if (cycle % 10 == 3) {
            /* 3. Node recovers and command settles to IDLE while latch retained */
            s_fake_node_mode = FAKE_NODE_MODE_NORMAL;
            lora_node_status_set_relay_gpio(&lora_node_status, 0, 1);
            lora_node_status.bits.seq_echo = lora_hub_cmd.bits.seq;
            lora_node_status.bits.sample_seq++; lora_last_rx_tick_ms = lv_tick_get(); lora_rx_revision++; advance_ui(50u);
            uint8_t rb_gpio = lora_node_status_get_relay_gpio(&lora_node_status, 0);
            if (h31 && h31->device_cmds && h31->device_cmds[0].phase == UI_CMD_PHASE_IDLE &&
                ui_get_auto_paused_error(0) && rb_gpio == 1) {
                count_auto_recoveries++;
            } else {
                printf("  FAIL: Cycle %d: Late recovery did not verify IDLE phase with readback 1\n", cycle);
                test31_pass = false;
            }
            /* Explicit re-arm clears latch and restarts hold */
            ui_trigger_device_retry(0);
            advance_ui(50u);
            if (!ui_get_auto_paused_error(0) && ui_is_auto_hold_active(0)) {
                count_auto_rearms++;
            } else {
                printf("  FAIL: Cycle %d: Explicit re-arm failed to clear latch or restart hold\n", cycle);
                test31_pass = false;
            }
            ui_dismiss_toast();
            advance_ui(20u);
            TRACK_MEM_STATE("Auto recovered & re-armed", false);
        } else if (cycle % 10 == 4) {
            /* 4. Sensor drops back to healthy (VOC = 80 <= 100) -> Auto evaluates clean transition */
            uint16_t seq_before_off = lora_hub_cmd.bits.seq;
            lora_node_status.bits.tvoc_ppb = 80;
            if (h31 && h31->auto_states) {
                ui_auto_state_t *ast0 = &((ui_auto_state_t *)h31->auto_states)[0];
                uint32_t t_now = lv_tick_get();
                ast0->last_transition_ms = (t_now > 11000u) ? (t_now - 11000u) : 1u;
                ast0->in_qualification = false;
                ast0->qualify_count = 0;
            }
            lora_last_rx_tick_ms = lv_tick_get(); lora_node_status.bits.sample_seq++; lora_rx_revision++; advance_ui(20u);
            lora_last_rx_tick_ms = lv_tick_get(); lora_node_status.bits.sample_seq++; lora_rx_revision++; advance_ui(20u);
            lora_last_rx_tick_ms = lv_tick_get(); lora_node_status.bits.sample_seq++; lora_rx_revision++; advance_ui(20u);
            uint8_t tx_mask_off = lora_hub_cmd_get_relay_gpios_mask(&lora_hub_cmd);
            advance_ui(800u); /* Allow fake node to apply and status to settle */
            uint8_t rb_off = lora_node_status_get_relay_gpio(&lora_node_status, 0);
            if (h31 && h31->device_cmds && h31->device_cmds[0].phase == UI_CMD_PHASE_IDLE &&
                lora_hub_cmd.bits.seq != seq_before_off && (tx_mask_off & 1) == 0 && rb_off == 0) {
                count_auto_cmds_sent++;
            } else {
                printf("  FAIL: Cycle %d: Auto command OFF not verified (seq, mask 0, readback 0)\n", cycle);
                test31_pass = false;
            }
            TRACK_MEM_STATE("Auto clean transition settled", false);
        } else if (cycle == 5) {
            /* 5. Prove no-op transition does not increment seq and cannot pass as command */
            uint16_t seq_noop = lora_hub_cmd.bits.seq;
            lora_last_rx_tick_ms = lv_tick_get(); lora_rx_revision++; advance_ui(20u);
            if (lora_hub_cmd.bits.seq != seq_noop) {
                printf("  FAIL: No-op status frame caused spurious command send!\n");
                test31_pass = false;
            }
        }

        ui_navigate_to_page(UI_PAGE_HOME);
        advance_ui(20u);
        TRACK_MEM_STATE("Home page", false);

        lv_mem_monitor_t cur_mem;
        lv_mem_monitor(&cur_mem);
        if (cycle == 10) used_at_cycle10 = cur_mem.total_size - cur_mem.free_size;
        if (cycle == 50) used_at_cycle50 = cur_mem.total_size - cur_mem.free_size;
    }

#undef TRACK_MEM_STATE

    int32_t stable_drift = (int32_t)used_at_cycle50 - (int32_t)used_at_cycle10;
    printf("  50-Cycle Stress Results:\n");
    printf("    Immediate Measurements:\n");
    printf("      Peak Used:       %u%%\n", peak_imm_used_pct);
    printf("      Min Total Free:  %u bytes (threshold > 4096, state: %s)\n", (unsigned)min_imm_total_free, min_imm_state);
    printf("      Contemp Largest: %u bytes\n", (unsigned)min_imm_contemp_biggest);
    printf("      Min Largest Blk: %u bytes\n", (unsigned)min_imm_biggest_block);
    printf("      Peak Frag:       %u%%\n", peak_imm_frag_pct);
    printf("    Settled/Stable Measurements:\n");
    printf("      Peak Used:       %u%%\n", peak_stable_used_pct);
    printf("      Min Total Free:  %u bytes (threshold > 4096, state: %s)\n", (unsigned)min_stable_total_free, min_stable_state);
    printf("      Contemp Largest: %u bytes\n", (unsigned)min_stable_contemp_biggest);
    printf("      Min Largest Blk: %u bytes\n", (unsigned)min_stable_biggest_block);
    printf("      Peak Frag:       %u%%\n", peak_stable_frag_pct);
    printf("      Stable Drift:    %+d bytes (cycles 10->50)\n", (int)stable_drift);
    printf("    Allocation-Level Transients:\n");
    printf("      Largest Request: %u bytes\n", (unsigned)s_alloc_largest_request);
    printf("      Failure Count:   %u\n", (unsigned)s_alloc_failure_count);
    printf("      Transient Min:   %u bytes (contemporaneous largest blk: %u bytes)\n",
           (unsigned)s_alloc_transient_min_free, (unsigned)s_alloc_transient_contemp_biggest);
    printf("    Auto Lifecycle Counters:\n");
    printf("      Commands Sent:   %u\n", count_auto_cmds_sent);
    printf("      Pending States:  %u\n", count_auto_pending);
    printf("      Timeouts:        %u\n", count_auto_timeouts);
    printf("      Error Latches:   %u\n", count_auto_latches);
    printf("      Recoveries:      %u\n", count_auto_recoveries);
    printf("      Explicit Rearms: %u\n", count_auto_rearms);

    if (s_alloc_failure_count != 0) {
        printf("  FAIL: Heap allocation failures detected: count=%u\n", s_alloc_failure_count);
        test31_pass = false;
    }

    /* Assert that all 5 scheduled Auto lifecycles ran and were counted (10 cmds, 5 pend, 5 timeout, 5 latch, 5 recov, 5 rearm) */
    if (count_auto_cmds_sent != 10 || count_auto_pending != 5 ||
        count_auto_timeouts != 5 || count_auto_latches != 5 ||
        count_auto_recoveries != 5 || count_auto_rearms != 5) {
        printf("  FAIL: Stress test Auto activity incomplete (cmds=%u, pend=%u, timeout=%u, latch=%u, recov=%u, rearm=%u)\n",
               count_auto_cmds_sent, count_auto_pending, count_auto_timeouts, count_auto_latches, count_auto_recoveries, count_auto_rearms);
        test31_pass = false;
    }

    if (min_imm_total_free <= 4096) {
        printf("  FAIL: Immediate minimum free memory dropped to %u <= 4096 bytes at %s\n", (unsigned)min_imm_total_free, min_imm_state);
        test31_pass = false;
    }
    if (min_stable_total_free <= 4096) {
        printf("  FAIL: Stable minimum free memory dropped to %u <= 4096 bytes at %s\n", (unsigned)min_stable_total_free, min_stable_state);
        test31_pass = false;
    }
    if (abs((int)stable_drift) > 256) {
        printf("  FAIL: Memory leak detected: stable drift %+d bytes > 256 bytes\n", (int)stable_drift);
        test31_pass = false;
    }

    /* Restore clean manual configs & normal fake node mode after stress */
    ui_test_set_boot_staging(true);
    for (uint8_t d = 0; d < UI_DEVICE_COUNT; d++) {
        ui_hub_device_config_t rst;
        ui_auto_get_defaults(d, (d == 3) ? UI_PRESET_LIGHT : d, &rst);
        rst.mode = UI_MODE_MANUAL;
        ui_set_device_config(d, &rst);
    }
    ui_test_set_boot_staging(false);
    s_fake_node_mode = FAKE_NODE_MODE_NORMAL;
    populate_node_status_baseline();
    ui_test_reset_device_commands();
    ui_dismiss_toast();
    advance_ui(100u);

    if (test31_pass) {
        printf("  PASS: 50 complete cycles stress passed with >4096 free and stable heap.\n");
        passed_tests++;
    }


    /* -------------------------------------------------------------
     * TEST 32: Quiet Architecture typography and editor integration
     * ------------------------------------------------------------- */
    total_tests++;
    printf("\n[CHECK 32] >=18px typography, air summary, focused editor and live data...\n");
    bool test32_pass = true;
    const ui_test_hooks_t *h32 = ui_get_test_hooks();
    ui_test_reset_device_commands();
    ui_test_reset_device_configs();
    s_fake_node_mode = FAKE_NODE_MODE_NORMAL;
    for (int page = UI_PAGE_HOME; page <= UI_PAGE_DEVICES; page++) {
        ui_navigate_to_page((ui_page_id_t)page);
        populate_node_status_baseline();
        advance_ui(100u);
        if (!audit_visible_fonts(lv_scr_act()) || !audit_visible_glyph_containment(lv_scr_act())) test32_pass = false;
    }
    ui_navigate_to_page(UI_PAGE_HOME);
    advance_ui(100u);
    lv_obj_t *quality_title = find_visible_label(lv_scr_act(), "Air quality");
    lv_obj_t *hero32 = quality_title ? lv_obj_get_parent(quality_title) : NULL;
    if (!hero32 || !find_visible_label(hero32, "Good")) test32_pass = false;
    lora_node_status.bits.temp_deci_c = INT16_MIN; lora_rx_revision++;
    advance_ui(50u);
    lv_obj_t *wide_temp32 = find_visible_label(lv_scr_act(), ui_get_home_temp_str());
    if (!wide_temp32 || lv_obj_get_width(wide_temp32) > 164 || !audit_visible_fonts(wide_temp32)) {
        printf("  FAIL: Full signed temperature reading was clipped or undersized\n");
        test32_pass = false;
    }
    populate_node_status_baseline(); advance_ui(50u);
    lora_node_status.bits.voc_valid = 0; lora_rx_revision++;
    advance_ui(50u);
    if (!hero32 || !find_visible_label(hero32, "Unavailable")) {
        printf("  FAIL: Missing sensor allowed a reassuring aggregate state\n"); test32_pass = false;
    }
    /* TVOC ppb bands (UBA TVOC levels for SGP30-class sensors): Good <= 220, Moderate <= 660, Poor above.
     * eCO2 stays at the 420 ppm baseline (Good), so the hero follows the TVOC category. */
    {
        static const struct { uint16_t ppb; const char *cat; } bands32[] = {
            { 220, "Good" }, { 221, "Moderate" }, { 660, "Moderate" }, { 661, "Poor" }
        };
        for (size_t b = 0; b < sizeof(bands32) / sizeof(bands32[0]); b++) {
            populate_node_status_baseline();
            lora_node_status.bits.tvoc_ppb = bands32[b].ppb; lora_rx_revision++;
            advance_ui(50u);
            const char *vb32 = ui_get_home_voc_badge();
            if (!vb32 || strstr(vb32, bands32[b].cat) == NULL || !hero32 || !find_visible_label(hero32, bands32[b].cat)) {
                printf("  FAIL: TVOC %u ppb expected '%s' (badge '%s')\n", (unsigned)bands32[b].ppb,
                       bands32[b].cat, vb32 ? vb32 : "NULL");
                test32_pass = false;
            }
        }
        populate_node_status_baseline();
        advance_ui(50u);
        if (!hero32 || !find_visible_label(hero32, "Based on eCO₂ and TVOC")) {
            printf("  FAIL: Air quality subtitle does not read 'Based on eCO₂ and TVOC'\n");
            test32_pass = false;
        }

        /* Air Purifier in Auto names its controlling sensor as TVOC */
        const ui_hub_device_config_t *pc32 = ui_get_device_config(0);
        if (!pc32 || pc32->preset != UI_PRESET_PURIFIER) {
            printf("  FAIL: Device 0 is not the Air Purifier in the baseline\n");
            test32_pass = false;
        } else {
            ui_hub_device_config_t pcfg32 = *pc32;
            pcfg32.mode = UI_MODE_AUTO;
            ui_set_device_config(0, &pcfg32);
            advance_ui(50u);
            const char *pm32 = ui_get_home_dev_mode(0);
            if (!pm32 || strcmp(pm32, "Auto · TVOC") != 0) {
                printf("  FAIL: Purifier Auto mode reads '%s', expected 'Auto · TVOC'\n", pm32 ? pm32 : "NULL");
                test32_pass = false;
            }
            ui_test_reset_device_configs();
            advance_ui(50u);
        }
    }
    /* Negative double: prove the typography audit rejects a sub-floor font. */
    lv_obj_t *bad_font = lv_label_create(lv_scr_act());
    lv_obj_set_style_text_font(bad_font, LV_FONT_DEFAULT, 0);
    printf("  Expected negative typography diagnostic follows:\n");
    if (audit_visible_fonts(bad_font)) test32_pass = false;
    lv_obj_del(bad_font);

    /* Negative controls for containment and sibling collision audit */
    printf("  Expected negative collision/containment diagnostics follow:\n");

    /* 1. Horizontal stat collision */
    lv_obj_t *neg_box1 = lv_obj_create(lv_scr_act());
    lv_obj_remove_style_all(neg_box1);
    lv_obj_set_size(neg_box1, 200, 50);
    lv_obj_set_pos(neg_box1, 10, 10);
    lv_obj_t *neg_lbl1a = lv_label_create(neg_box1);
    lv_label_set_text_static(neg_lbl1a, "StatA");
    lv_obj_set_pos(neg_lbl1a, 10, 10);
    lv_obj_set_size(neg_lbl1a, 80, 24);
    lv_obj_t *neg_lbl1b = lv_label_create(neg_box1);
    lv_label_set_text_static(neg_lbl1b, "StatB");
    lv_obj_set_pos(neg_lbl1b, 50, 10);
    lv_obj_set_size(neg_lbl1b, 80, 24);
    lv_obj_update_layout(neg_box1);
    if (audit_visible_glyph_containment(neg_box1)) {
        printf("  FAIL: Containment audit failed to reject horizontal stat collision!\n");
        test32_pass = false;
    }
    lv_obj_del(neg_box1);

    /* 2. Switch over device name */
    lv_obj_t *neg_box2 = lv_obj_create(lv_scr_act());
    lv_obj_remove_style_all(neg_box2);
    lv_obj_set_size(neg_box2, 300, 70);
    lv_obj_t *neg_lbl2 = lv_label_create(neg_box2);
    lv_label_set_text_static(neg_lbl2, "Ventilation Fan");
    lv_obj_set_pos(neg_lbl2, 52, 6);
    lv_obj_set_size(neg_lbl2, 176, 33);
    lv_obj_t *neg_sw2 = lv_switch_create(neg_box2);
    lv_obj_set_pos(neg_sw2, 150, 10);
    lv_obj_set_size(neg_sw2, 54, 30);
    lv_obj_update_layout(neg_box2);
    if (audit_visible_glyph_containment(neg_box2)) {
        printf("  FAIL: Containment audit failed to reject switch over device name!\n");
        test32_pass = false;
    }
    lv_obj_del(neg_box2);

    /* 3. Summary under chevron */
    lv_obj_t *neg_box3 = lv_obj_create(lv_scr_act());
    lv_obj_remove_style_all(neg_box3);
    lv_obj_set_size(neg_box3, 752, 64);
    lv_obj_t *neg_lbl3 = lv_label_create(neg_box3);
    lv_label_set_text_static(neg_lbl3, "10000 ppm / 9950 ppm");
    lv_obj_set_pos(neg_lbl3, 600, 10);
    lv_obj_set_size(neg_lbl3, 140, 44);
    lv_obj_t *neg_chev3 = lv_label_create(neg_box3);
    lv_label_set_text_static(neg_chev3, ">");
    lv_obj_set_pos(neg_chev3, 712, 22);
    lv_obj_set_size(neg_chev3, 20, 20);
    lv_obj_update_layout(neg_box3);
    if (audit_visible_glyph_containment(neg_box3)) {
        printf("  FAIL: Containment audit failed to reject summary under chevron!\n");
        test32_pass = false;
    }
    lv_obj_del(neg_box3);

    /* 4. Settings error over polarity */
    lv_obj_t *neg_box4 = lv_obj_create(lv_scr_act());
    lv_obj_remove_style_all(neg_box4);
    lv_obj_set_size(neg_box4, 752, 300);
    lv_obj_t *neg_pol4 = lv_obj_create(neg_box4);
    lv_obj_remove_style_all(neg_pol4);
    lv_obj_set_pos(neg_pol4, 440, 201);
    lv_obj_set_size(neg_pol4, 292, 46);
    lv_obj_t *neg_err4 = lv_label_create(neg_box4);
    lv_label_set_text_static(neg_err4, "Save rejected: command pending");
    lv_obj_set_pos(neg_err4, 430, 210);
    lv_obj_set_size(neg_err4, 300, 30);
    lv_obj_update_layout(neg_box4);
    if (audit_visible_glyph_containment(neg_box4)) {
        printf("  FAIL: Containment audit failed to reject settings error over polarity!\n");
        test32_pass = false;
    }
    lv_obj_del(neg_box4);

    /* 5. Ancestor clipping violation */
    lv_obj_t *neg_anc5 = lv_obj_create(lv_scr_act());
    lv_obj_remove_style_all(neg_anc5);
    lv_obj_set_pos(neg_anc5, 100, 100);
    lv_obj_set_size(neg_anc5, 100, 50);
    lv_obj_clear_flag(neg_anc5, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_t *neg_lbl5 = lv_label_create(neg_anc5);
    lv_label_set_text_static(neg_lbl5, "Long label that extends way beyond parent bounds");
    lv_obj_set_pos(neg_lbl5, 0, 0);
    lv_obj_set_size(neg_lbl5, 250, 40);
    lv_obj_update_layout(neg_anc5);
    if (audit_visible_glyph_containment(neg_anc5)) {
        printf("  FAIL: Containment audit failed to reject ancestor clipping violation!\n");
        test32_pass = false;
    }
    lv_obj_del(neg_anc5);

    ui_navigate_to_page(UI_PAGE_DEVICES);
    advance_ui(100u);
    /* Pointer-only entry through the visible Settings action. */
    lv_obj_t *settings32 = find_visible_label(lv_scr_act(), "Settings");
    if (!settings32) test32_pass = false;
    else click_object_center(lv_obj_get_parent(settings32));
    advance_ui(50u);
    if (!ui_is_device_settings_open() || h32->dev_page_names[0] != NULL) {
        printf("  FAIL: Focused editor did not release hidden page widgets\n"); test32_pass = false;
    }
    if (!audit_visible_fonts(lv_scr_act()) || !audit_visible_glyph_containment(lv_scr_act())) test32_pass = false;
    ui_close_device_settings();
    /* Every preset, open dropdown, all supported Auto editors and invalid drafts. */
    for (uint8_t preset = 0; preset < UI_PRESET_COUNT; preset++) {
        ui_hub_device_config_t cfg32;
        ui_auto_get_defaults(0, preset, &cfg32);
        ui_set_device_config(0, &cfg32);
        populate_node_status_baseline();
        ui_open_device_settings(0);
        advance_ui(50u);
        if (!audit_visible_fonts(lv_scr_act()) || !audit_visible_glyph_containment(lv_scr_act())) test32_pass = false;
        ui_dialog_open_dropdown();
        advance_ui(50u);
        if (!audit_visible_fonts(lv_layer_top()) || !audit_visible_fonts(lv_scr_act())) test32_pass = false;
        ui_dialog_close_dropdown();
        if (ui_auto_is_preset_supported(preset)) {
            ui_dialog_open_limits();
            advance_ui(50u);
            if (!audit_visible_fonts(lv_scr_act()) || !audit_visible_glyph_containment(lv_scr_act())) test32_pass = false;
            const ui_auto_rule_info_t *rule32 = ui_auto_get_rule_info(preset);
            int32_t sensor32 = preset == UI_PRESET_PURIFIER ? 85 : (preset == UI_PRESET_FAN ? 420 : 48);
            char live_val32[16];
            const char *unit32 = preset == UI_PRESET_HUMIDIFIER ? "%" : rule32->unit_name;
            snprintf(live_val32, sizeof(live_val32), "%d", (int)sensor32);
            if (!find_visible_label(lv_scr_act(), live_val32)) {
                printf("  FAIL: Missing live editor reading value '%s'\n", live_val32); test32_pass = false;
            }
            if (unit32[0] != '\0' && !find_visible_label(lv_scr_act(), unit32)) {
                printf("  FAIL: Missing live editor reading unit '%s'\n", unit32); test32_pass = false;
            }
            if (preset == UI_PRESET_FAN) lora_node_status.bits.co2_valid = 0;
            if (preset == UI_PRESET_PURIFIER) lora_node_status.bits.voc_valid = 0;
            if (preset == UI_PRESET_HUMIDIFIER) lora_node_status.bits.humid_valid = 0;
            lora_rx_revision++; advance_ui(50u);
            if (!find_visible_label(lv_scr_act(), "No live data")) test32_pass = false;
            populate_node_status_baseline();
            advance_ui(50u);
            if (!find_visible_label(lv_scr_act(), live_val32)) test32_pass = false;
            if (unit32[0] != '\0' && !find_visible_label(lv_scr_act(), unit32)) test32_pass = false;

            /* Bound state is explicit; no unresponsive apparently-enabled minus. */
            *h32->dialog_edit_on_thresh = rule32->min_val;
            ui_dialog_step_thresh(true, -1);
            advance_ui(30u);
            if (!lv_obj_has_state(*h32->dialog_btn_on_minus, LV_STATE_DISABLED)) test32_pass = false;
            ui_dialog_reset_limits();
            /* Start at equality, then take one real step down. The resulting
             * pair is still invalid for all three canonical minimum gaps. */
            *h32->dialog_edit_on_thresh = *h32->dialog_edit_off_thresh;
            ui_dialog_step_thresh(true, -1);
            advance_ui(30u);
            if (!lv_obj_has_state(*h32->dialog_btn_save_auto, LV_STATE_DISABLED)) test32_pass = false;
            if (!audit_visible_fonts(lv_scr_act())) test32_pass = false;
            ui_dialog_limits_back();
            advance_ui(30u);
            lv_obj_t *summary32 = lv_obj_get_child(*h32->dialog_btn_limits, 0);
            if (!strstr(lv_label_get_text(summary32), "Fix invalid limits") ||
                !lv_obj_has_state(*h32->dialog_btn_save_auto, LV_STATE_DISABLED)) {
                printf("  FAIL: Parent settings concealed invalid draft\n"); test32_pass = false;
            }
        } else if (!lv_obj_has_state(h32->dialog_mode_btns[1], LV_STATE_DISABLED)) {
            printf("  FAIL: Unsupported Auto still appears actionable\n"); test32_pass = false;
        }
        ui_close_device_settings();
        advance_ui(50u);
    }
    ui_test_reset_device_configs();
    populate_node_status_baseline();
    if (test32_pass) {
        printf("  PASS: All visible fonts >=18px; editor, validity and summary transitions verified.\n");
        passed_tests++;
    }

    /* -------------------------------------------------------------
     * TEST 33: Complete probe diagnostic scenario suite (13 fixtures)
     * ------------------------------------------------------------- */
    total_tests++;
    printf("\n[CHECK 33] Comprehensive probe diagnostic scenario suite (13 critical fixtures)...\n");
    bool test33_pass = true;

    /* 1. probe_home_limits */
    ui_navigate_to_page(UI_PAGE_HOME);
    sim_post_node_status(true, 10000, true, 500, true, -32768, true, 100, true, 1, 0, 0, 1);
    advance_ui(50u);
    if (!audit_visible_fonts(lv_scr_act()) || !audit_visible_glyph_containment(lv_scr_act())) {
        printf("  FAIL: probe_home_limits failed containment or font audit\n");
        test33_pass = false;
    }

    /* 2. probe_trends_10000 */
    ui_navigate_to_page(UI_PAGE_TRENDS);
    ui_select_trend_metric(UI_METRIC_CO2);
    int32_t vals33[16]; bool valid33[16];
    for (int i = 0; i < 16; ++i) { vals33[i] = 10000; valid33[i] = true; }
    ui_feed_history(UI_METRIC_CO2, vals33, valid33, 16, 1000);
    advance_ui(50u);
    if (!audit_visible_fonts(lv_scr_act()) || !audit_visible_glyph_containment(lv_scr_act())) {
        printf("  FAIL: probe_trends_10000 failed containment or font audit\n");
        test33_pass = false;
    }

    /* 3. probe_trends_two_samples */
    ui_feed_history(UI_METRIC_CO2, vals33, valid33, 2, 1439);
    advance_ui(50u);
    if (!audit_visible_fonts(lv_scr_act()) || !audit_visible_glyph_containment(lv_scr_act())) {
        printf("  FAIL: probe_trends_two_samples failed containment or font audit\n");
        test33_pass = false;
    }

    /* 4. probe_trends_negative */
    ui_select_trend_metric(UI_METRIC_TEMP);
    for (int i = 0; i < 16; ++i) vals33[i] = -25;
    ui_feed_history(UI_METRIC_TEMP, vals33, valid33, 16, 1000);
    advance_ui(50u);
    if (!audit_visible_fonts(lv_scr_act()) || !audit_visible_glyph_containment(lv_scr_act())) {
        printf("  FAIL: probe_trends_negative failed containment or font audit\n");
        test33_pass = false;
    }

    /* 5. probe_auto_live_max */
    ui_open_device_settings(1);
    ui_dialog_open_limits();
    advance_ui(50u);
    if (!audit_visible_fonts(lv_scr_act()) || !audit_visible_glyph_containment(lv_scr_act())) {
        printf("  FAIL: probe_auto_live_max failed containment or font audit\n");
        test33_pass = false;
    }
    ui_close_device_settings();

    /* 6. probe_devices_auto */
    ui_navigate_to_page(UI_PAGE_DEVICES);
    ui_hub_device_config_t cfg33;
    ui_auto_get_defaults(1, UI_PRESET_FAN, &cfg33);
    cfg33.mode = UI_MODE_AUTO;
    ui_test_set_boot_staging(true);
    ui_set_device_config(1, &cfg33);
    ui_test_set_boot_staging(false);
    advance_ui(50u);
    if (!audit_visible_fonts(lv_scr_act()) || !audit_visible_glyph_containment(lv_scr_act())) {
        printf("  FAIL: probe_devices_auto failed containment or font audit\n");
        test33_pass = false;
    }

    /* 7. probe_home_auto */
    ui_navigate_to_page(UI_PAGE_HOME);
    advance_ui(50u);
    if (!audit_visible_fonts(lv_scr_act()) || !audit_visible_glyph_containment(lv_scr_act())) {
        printf("  FAIL: probe_home_auto failed containment or font audit\n");
        test33_pass = false;
    }

    /* 8. probe_settings_max_summary */
    ui_auto_get_defaults(1, UI_PRESET_FAN, &cfg33);
    cfg33.auto_on_thresh = 10000;
    cfg33.auto_off_thresh = 9950;
    ui_test_set_boot_staging(true);
    ui_set_device_config(1, &cfg33);
    ui_test_set_boot_staging(false);
    ui_open_device_settings(1);
    advance_ui(50u);
    if (!audit_visible_fonts(lv_scr_act()) || !audit_visible_glyph_containment(lv_scr_act())) {
        printf("  FAIL: probe_settings_max_summary failed containment or font audit\n");
        test33_pass = false;
    }
    ui_close_device_settings();

    /* 9. probe_settings_invalid_back */
    ui_open_device_settings(0);
    ui_dialog_open_limits();
    for (int i = 0; i < 10; ++i) ui_dialog_step_thresh(false, 1);
    ui_dialog_limits_back();
    advance_ui(50u);
    if (!audit_visible_fonts(lv_scr_act()) || !audit_visible_glyph_containment(lv_scr_act())) {
        printf("  FAIL: probe_settings_invalid_back failed containment or font audit\n");
        test33_pass = false;
    }
    ui_close_device_settings();

    /* 10. probe_home_latched_recovery */
    ui_test_reset_device_commands();
    sim_set_tx_sink(fake_node_receive_cmd);
    s_fake_node_cmd_in_flight = false;
    ui_auto_get_defaults(1, UI_PRESET_FAN, &cfg33);
    cfg33.mode = UI_MODE_AUTO;
    ui_set_device_config(1, &cfg33);
    sim_post_node_status(true, 1200, true, 85, true, 235, true, 48, true, 0, 0, 0, 0);
    lora_hub_cmd_set_relay_gpios_mask(&lora_hub_cmd, 0);
    lora_node_status.bits.seq_echo = lora_hub_cmd.bits.seq;
    s_fake_node_mode = FAKE_NODE_MODE_FAIL_NEXT;
    advance_ui(12000u);
    advance_ui(UI_CMD_TIMEOUT_MS + 200u);
    if (ui_get_test_hooks()->device_cmds[1].phase == UI_CMD_PHASE_PENDING) {
        advance_ui(UI_CMD_TIMEOUT_MS + 200u);
    }
    if (!ui_get_auto_paused_error(1)) {
        printf("  FAIL: Fan auto did not latch paused_error for recovery test\n");
        test33_pass = false;
    }
    lora_node_status_set_relay_gpio(&lora_node_status, 1, 1);
    lora_node_status.bits.seq_echo = lora_hub_cmd.bits.seq;
    lora_node_status.bits.sample_seq++;
    lora_last_rx_tick_ms = lv_tick_get();
    lora_rx_revision++;
    ui_dismiss_toast();
    ui_navigate_to_page(UI_PAGE_HOME);
    advance_ui(50u);
    if (!audit_visible_fonts(lv_scr_act()) || !audit_visible_glyph_containment(lv_scr_act())) {
        printf("  FAIL: probe_home_latched_recovery failed containment or font audit\n");
        test33_pass = false;
    }

    /* 11. probe_devices_latched_recovery */
    ui_navigate_to_page(UI_PAGE_DEVICES);
    advance_ui(50u);
    if (!audit_visible_fonts(lv_scr_act()) || !audit_visible_glyph_containment(lv_scr_act())) {
        printf("  FAIL: probe_devices_latched_recovery failed containment or font audit\n");
        test33_pass = false;
    }

    /* 12. probe_uptime_1000h_offline */
    ui_set_uptime_seconds(3600000u);
    s_fake_node_mode = FAKE_NODE_MODE_STOP_REPORTING;
    advance_ui(6000u);
    ui_navigate_to_page(UI_PAGE_HOME);
    advance_ui(50u);
    if (!audit_visible_fonts(lv_scr_act()) || !audit_visible_glyph_containment(lv_scr_act())) {
        printf("  FAIL: probe_uptime_1000h_offline failed containment or font audit\n");
        test33_pass = false;
    }

    /* 13. probe_settings_pending_rejection */
    s_fake_node_mode = FAKE_NODE_MODE_NORMAL;
    populate_node_status_baseline();
    ui_test_reset_device_commands();
    ui_test_reset_device_configs();
    advance_ui(50u);
    ui_trigger_device_toggle(0);
    advance_ui(20u);
    if (ui_get_test_hooks()->device_cmds[0].phase != UI_CMD_PHASE_PENDING) {
        printf("  FAIL: Device 0 not pending for settings rejection test\n");
        test33_pass = false;
    }
    ui_open_device_settings(0);
    ui_set_dialog_active_low(1);
    ui_save_device_settings();
    advance_ui(50u);
    if (!audit_visible_fonts(lv_scr_act()) || !audit_visible_glyph_containment(lv_scr_act())) {
        printf("  FAIL: probe_settings_pending_rejection failed containment or font audit\n");
        test33_pass = false;
    }
    ui_close_device_settings();

    /* 14. probe_c3_trends_tab_colors: assert active tab unit is white, inactive is muted */
    ui_navigate_to_page(UI_PAGE_TRENDS);
    ui_select_trend_metric(UI_METRIC_CO2);
    advance_ui(50u);
    const ui_test_hooks_t *h33 = ui_get_test_hooks();
    if (h33 && h33->trends_tab_units) {
        lv_color_t c0 = lv_obj_get_style_text_color(h33->trends_tab_units[0], 0);
        lv_color_t c2 = lv_obj_get_style_text_color(h33->trends_tab_units[2], 0);
        if (c0.full != lv_color_white().full || c2.full != UI_COLOR_MUTED.full) {
            printf("  FAIL: probe_c3_trends_tab_colors: CO2 selected, expected CO2 unit white and Temp unit muted\n");
            test33_pass = false;
        }
        ui_select_trend_metric(UI_METRIC_TEMP);
        advance_ui(50u);
        c0 = lv_obj_get_style_text_color(h33->trends_tab_units[0], 0);
        c2 = lv_obj_get_style_text_color(h33->trends_tab_units[2], 0);
        if (c2.full != lv_color_white().full || c0.full != UI_COLOR_MUTED.full) {
            printf("  FAIL: probe_c3_trends_tab_colors: Temp selected, expected Temp unit white and CO2 unit muted\n");
            test33_pass = false;
        }
    }

    /* 15. probe_c4_isolated_point_and_domain: middle singleton at slot 8 in full 16-slot domain */
    ui_select_trend_metric(UI_METRIC_CO2);
    int32_t iso_vals[16] = {0};
    bool iso_valid[16] = {false};
    iso_vals[0] = 500; iso_valid[0] = true;
    iso_vals[8] = 600; iso_valid[8] = true;
    iso_vals[15] = 700; iso_valid[15] = true;
    ui_feed_history(UI_METRIC_CO2, iso_vals, iso_valid, 16, 600); /* 10:00 start -> slot 8 is 10:08, slot 15 is 10:15 */
    advance_ui(50u);
    const ui_history_sample_t *s_mid = ui_get_history_sample(UI_METRIC_CO2, 8);
    if (!s_mid || !s_mid->valid || s_mid->value != 600) {
        printf("  FAIL: probe_c4: middle singleton sample 8 not retained\n");
        test33_pass = false;
    }
    if (h33 && h33->trends_time_labels && h33->trends_time_labels[0] && h33->trends_time_labels[3]) {
        const char *t0 = lv_label_get_text(h33->trends_time_labels[0]);
        const char *t3 = lv_label_get_text(h33->trends_time_labels[3]);
        if (strcmp(t0, "10:00") != 0 || strcmp(t3, "10:15") != 0) {
            printf("  FAIL: probe_c4: time axis domain bounds mismatch (t0='%s' expected '10:00', t3='%s' expected '10:15')\n",
                   t0, t3);
            test33_pass = false;
        }
    }

    /* 16. probe_c5_hero_and_nav_typography */
    ui_navigate_to_page(UI_PAGE_HOME);
    sim_post_node_status(true, 420, true, 85, true, 235, true, 48, true, 0, 0, 0, 0);
    advance_ui(50u);
    if (h33 && h33->home_co2_val && *h33->home_co2_val) {
        const lv_font_t *f_co2 = lv_obj_get_style_text_font(*h33->home_co2_val, 0);
        if (f_co2 != &ui_font_34) {
            printf("  FAIL: probe_c5: Home CO2 val nominal font expected ui_font_34\n");
            test33_pass = false;
        }
    }
    sim_post_node_status(true, 10000, true, 85, true, 235, true, 48, true, 0, 0, 0, 0);
    advance_ui(50u);
    if (h33 && h33->home_co2_val && *h33->home_co2_val) {
        const lv_font_t *f_co2 = lv_obj_get_style_text_font(*h33->home_co2_val, 0);
        if (f_co2 != &ui_font_24) {
            printf("  FAIL: probe_c5: Home CO2 val 5-digit font expected ui_font_24 fallback\n");
            test33_pass = false;
        }
    }

    /* Restore clean state */
    ui_test_reset_device_commands();
    ui_test_reset_device_configs();
    populate_node_status_baseline();
    advance_ui(500u);

    if (test33_pass) {
        printf("  PASS: All 13 probe diagnostic scenarios passed containment and typography audits.\n");
        passed_tests++;
    }

    /* -------------------------------------------------------------
     * TEST 34: Calibrated micro-geometry, pointer hit-testing, name boundary & concurrent auto
     * ------------------------------------------------------------- */
    total_tests++;
    printf("\n[CHECK 34] Calibrated micro-geometry, pointer hit-testing, name boundary & concurrent auto...\n");
    bool test34_pass = true;

    /* 1. Devices Tab Micro-geometry assertions */
    ui_navigate_to_page(UI_PAGE_DEVICES);
    advance_ui(50u);
    const ui_test_hooks_t *h34 = ui_get_test_hooks();
    if (!h34 || !h34->dev_page_names || !h34->dev_page_names[0]) {
        printf("  FAIL 34a: dev_page_names hook unavailable\n");
        test34_pass = false;
    } else {
        lv_obj_t *card0 = lv_obj_get_parent(h34->dev_page_names[0]);
        lv_area_t card_coords;
        lv_obj_get_coords(card0, &card_coords);

        /* Device icon is child 0 */
        lv_obj_t *dev_ico0 = lv_obj_get_child(card0, 0);
        lv_coord_t ico_w = lv_obj_get_width(dev_ico0);
        lv_coord_t ico_h = lv_obj_get_height(dev_ico0);
        lv_coord_t ico_x = lv_obj_get_x(dev_ico0);
        lv_coord_t ico_y = lv_obj_get_y(dev_ico0);

        if (ico_w != 42 || ico_h != 42 || ico_x != 16 || ico_y != 29) {
            printf("  FAIL 34a: Device icon size/pos mismatch: (%d,%d) %dx%d (expected (16,29) 42x42)\n",
                   (int)ico_x, (int)ico_y, (int)ico_w, (int)ico_h);
            test34_pass = false;
        }

        /* Verify 1:1 vertical symmetry: top space 29px, bottom space to divider 29px (divider is child 8 at y=100) */
        lv_obj_t *div = lv_obj_get_child(card0, 8);
        lv_coord_t div_y = lv_obj_get_y(div);
        lv_coord_t top_space = ico_y;
        lv_coord_t bottom_space = div_y - (ico_y + ico_h);
        if (top_space != 29 || bottom_space != 29) {
            printf("  FAIL 34a: Vertical icon symmetry mismatch: top=%d, bottom=%d (expected 29:29)\n",
                   (int)top_space, (int)bottom_space);
            test34_pass = false;
        }

        /* Settings action row is child 9 */
        lv_obj_t *btn_cfg0 = lv_obj_get_child(card0, 9);
        lv_obj_t *cfg_ico = lv_obj_get_child(btn_cfg0, 0);
        lv_obj_t *cfg_lbl = lv_obj_get_child(btn_cfg0, 1);
        lv_obj_t *cfg_arrow = lv_obj_get_child(btn_cfg0, 2);

        lv_coord_t cfg_lbl_x = lv_obj_get_x(cfg_lbl);
        lv_coord_t mode_x = lv_obj_get_x(h34->dev_page_modes[0]);

        if (cfg_lbl_x != 72 || mode_x != 72) {
            printf("  FAIL 34a: Settings label alignment mismatch: cfg=%d, mode=%d (expected both 72)\n",
                   (int)cfg_lbl_x, (int)mode_x);
            test34_pass = false;
        }

        lv_coord_t dev_center_x = ico_x + ico_w / 2;
        lv_coord_t cfg_ico_x = lv_obj_get_x(cfg_ico);
        lv_coord_t cfg_ico_w = lv_obj_get_width(cfg_ico);
        lv_coord_t cfg_center_x = cfg_ico_x + cfg_ico_w / 2;
        if (dev_center_x != 37 || cfg_center_x != 37) {
            printf("  FAIL 34a: Settings icon optical center mismatch: dev=%d, cfg=%d (expected both 37)\n",
                   (int)dev_center_x, (int)cfg_center_x);
            test34_pass = false;
        }

        lv_coord_t arrow_x = lv_obj_get_x(cfg_arrow);
        if (arrow_x != 332) {
            printf("  FAIL 34a: Settings right chevron pos mismatch: x=%d (expected 332)\n", (int)arrow_x);
            test34_pass = false;
        }
    }

    /* 2. Device Settings Modal Micro-geometry assertions */
    ui_open_device_settings(0);
    advance_ui(50u);
    if (!h34 || !h34->dialog_box || *h34->dialog_box == NULL ||
        !h34->dialog_subview_device || *h34->dialog_subview_device == NULL) {
        printf("  FAIL 34b: dialog_box or dialog_subview_device unavailable\n");
        test34_pass = false;
    } else {
        lv_obj_t *subview = *h34->dialog_subview_device;
        lv_area_t sub_coords;
        lv_obj_get_coords(subview, &sub_coords);

        /* Child 0 is the back button (44x44 at pos 0, 10) */
        lv_obj_t *btn_back = lv_obj_get_child(subview, 0);
        lv_area_t back_coords;
        lv_obj_get_coords(btn_back, &back_coords);

        lv_coord_t back_rel_y = back_coords.y1 - sub_coords.y1;
        lv_coord_t back_w = lv_area_get_width(&back_coords);
        lv_coord_t back_h = lv_area_get_height(&back_coords);

        if (back_rel_y != 10 || back_w != 44 || back_h != 44) {
            printf("  FAIL 34b: Modal back button pos/size mismatch: y=%d, %dx%d (expected y=10, 44x44)\n",
                   (int)back_rel_y, (int)back_w, (int)back_h);
            test34_pass = false;
        }

        /* Child 1 is the contextual title label at (44, 16) */
        lv_obj_t *title_lbl = lv_obj_get_child(subview, 1);
        lv_area_t title_coords;
        lv_obj_get_coords(title_lbl, &title_coords);
        lv_coord_t title_rel_y = title_coords.y1 - sub_coords.y1;
        if (title_rel_y != 16) {
            printf("  FAIL 34b: Modal title pos mismatch: y=%d (expected 16)\n", (int)title_rel_y);
            test34_pass = false;
        }

        /* Optical alignment: Back chevron icon center y vs Title text cap height center */
        lv_coord_t back_center_y = back_coords.y1 + back_h / 2;
        if (back_center_y < title_coords.y1 || back_center_y > title_coords.y1 + 22) {
            printf("  FAIL 34b: Back chevron center (%d) not aligned with title cap height (%d..%d)\n",
                   (int)back_center_y, (int)title_coords.y1, (int)(title_coords.y1 + 22));
            test34_pass = false;
        }

        /* Child 2 is the porcelain form card (pos: 0, 68, size: 752, 264) */
        lv_obj_t *form = lv_obj_get_child(subview, 2);
        lv_area_t form_coords;
        lv_obj_get_coords(form, &form_coords);

        lv_coord_t form_rel_y = form_coords.y1 - sub_coords.y1;
        lv_coord_t form_w = lv_area_get_width(&form_coords);
        lv_coord_t form_h = lv_area_get_height(&form_coords);

        if (form_rel_y != 68 || form_w != 752 || form_h != 264) {
            printf("  FAIL 34b: Form card geometry mismatch: y=%d, %dx%d (expected (0,68) 752x264)\n",
                   (int)form_rel_y, (int)form_w, (int)form_h);
            test34_pass = false;
        }

        /* Form card icons calibrated to 24px */
        lv_obj_t *ico_row0 = lv_obj_get_child(form, 0);
        lv_area_t ico0_coords;
        lv_obj_get_coords(ico_row0, &ico0_coords);
        if (lv_area_get_width(&ico0_coords) != 24 || lv_area_get_height(&ico0_coords) != 24) {
            printf("  FAIL 34b: Form row 0 icon size mismatch: %dx%d (expected 24x24)\n",
                   (int)lv_area_get_width(&ico0_coords), (int)lv_area_get_height(&ico0_coords));
            test34_pass = false;
        }

        /* Footer buttons Cancel & Save: at y=352 */
        lv_obj_t *btn_x = *h34->dialog_btn_x;
        lv_area_t x_coords;
        lv_obj_get_coords(btn_x, &x_coords);
        lv_coord_t x_rel_y = x_coords.y1 - sub_coords.y1;
        if (x_rel_y != 352) {
            printf("  FAIL 34b: Footer button y mismatch: %d (expected 352)\n", (int)x_rel_y);
            test34_pass = false;
        }

        /* Gap from form card bottom to footer buttons top: 352 - (68 + 264) = 20px */
        lv_coord_t form_to_footer_gap = x_coords.y1 - form_coords.y2 - 1;
        if (form_to_footer_gap != 20) {
            printf("  FAIL 34b: Form bottom to footer button gap mismatch: %d px (expected 20)\n",
                   (int)form_to_footer_gap);
            test34_pass = false;
        }
    }
    ui_close_device_settings();
    advance_ui(50u);

    /* 3. Real End-to-End Pointer Touch Interaction */
    ui_navigate_to_page(UI_PAGE_DEVICES);
    advance_ui(50u);
    if (ui_is_device_settings_open()) {
        ui_close_device_settings();
        advance_ui(50u);
    }

    /* Card 0 is at (24, 112). Settings button is at (0, 101, 368, 63).
     * Center: x = 24 + 184 = 208, y = 112 + 101 + 31 = 244. */
    sim_pointer_click(208, 244);
    advance_ui(50u);

    if (!ui_is_device_settings_open()) {
        printf("  FAIL 34c: sim_pointer_click on Settings row did NOT open modal dialog\n");
        test34_pass = false;
    } else {
        /* Modal is open! Now click the Back chevron button.
         * Back button is at (0, 10, 44, 44) inside dialog_subview_device.
         * Absolute coords: x in [24..68], y in [66..110]. Center: (46, 88). */
        sim_pointer_click(46, 88);
        advance_ui(50u);

        if (ui_is_device_settings_open()) {
            printf("  FAIL 34c: sim_pointer_click on modal Back chevron did NOT close modal dialog\n");
            test34_pass = false;
            ui_close_device_settings();
            advance_ui(50u);
        }
    }

    /* 4. Long Device Name Boundary & Containment */
    ui_navigate_to_page(UI_PAGE_DEVICES);
    ui_hub_device_config_t long_cfg;
    ui_auto_get_defaults(0, UI_PRESET_FAN, &long_cfg);
    strncpy(long_cfg.name, "Living Room Exhaust Fan Pro Max", sizeof(long_cfg.name) - 1);
    long_cfg.name[sizeof(long_cfg.name) - 1] = '\0';
    ui_test_set_boot_staging(true);
    ui_set_device_config(0, &long_cfg);
    ui_test_set_boot_staging(false);
    advance_ui(50u);

    /* Assert containment on Devices page */
    if (!audit_visible_fonts(lv_scr_act()) || !audit_visible_glyph_containment(lv_scr_act())) {
        printf("  FAIL 34d: Long device name failed containment or font audit on Devices tab\n");
        test34_pass = false;
    }

    /* Open Settings dialog with long name */
    ui_open_device_settings(0);
    advance_ui(50u);
    if (!audit_visible_fonts(lv_scr_act()) || !audit_visible_glyph_containment(lv_scr_act())) {
        printf("  FAIL 34d: Long device name failed containment or font audit on Settings modal\n");
        test34_pass = false;
    }
    ui_close_device_settings();
    advance_ui(50u);

    /* Restore clean defaults */
    ui_test_reset_device_configs();
    advance_ui(50u);

    /* 5. Concurrent Multi-Channel Auto Triggering */
    ui_test_reset_device_commands();
    ui_hub_device_config_t auto_c0, auto_c1;
    ui_auto_get_defaults(0, UI_PRESET_PURIFIER, &auto_c0);
    auto_c0.mode = UI_MODE_AUTO;
    auto_c0.auto_on_thresh = 150;
    auto_c0.auto_off_thresh = 100;
    ui_auto_get_defaults(1, UI_PRESET_FAN, &auto_c1);
    auto_c1.mode = UI_MODE_AUTO;
    auto_c1.auto_on_thresh = 1000;
    auto_c1.auto_off_thresh = 800;

    ui_test_set_boot_staging(true);
    ui_set_device_config(0, &auto_c0);
    ui_set_device_config(1, &auto_c1);
    ui_test_set_boot_staging(false);

    sim_set_tx_sink(fake_node_receive_cmd);
    s_fake_node_mode = FAKE_NODE_MODE_NORMAL;
    sim_post_node_status(true, 1200, true, 200, true, 235, true, 48, true, 0, 0, 0, 0);

    const ui_test_hooks_t *h_auto = ui_get_test_hooks();
    ui_auto_state_t *ast0 = &((ui_auto_state_t *)h_auto->auto_states)[0];
    ui_auto_state_t *ast1 = &((ui_auto_state_t *)h_auto->auto_states)[1];
    uint32_t t_now34 = lv_tick_get();
    ast0->last_transition_ms = (t_now34 > 11000u) ? (t_now34 - 11000u) : 1u;
    ast1->last_transition_ms = (t_now34 > 11000u) ? (t_now34 - 11000u) : 1u;
    ast0->in_qualification = false; ast0->qualify_count = 0;
    ast1->in_qualification = false; ast1->qualify_count = 0;

    for (int q = 0; q < 3; q++) {
        lora_last_rx_tick_ms = lv_tick_get();
        lora_node_status.bits.sample_seq++;
        lora_rx_revision++;
        advance_ui(50u);
    }

    bool pending_found = (h_auto->device_cmds[0].phase == UI_CMD_PHASE_PENDING ||
                          h_auto->device_cmds[1].phase == UI_CMD_PHASE_PENDING);
    if (!pending_found) {
        printf("  FAIL 34e: Concurrent auto trigger did not dispatch pending command\n");
        test34_pass = false;
    }

    /* 6. R1 Idempotent style attachment stress: 200 taps & 100 dropdown cycles */
    ui_navigate_to_page(UI_PAGE_TRENDS);
    ui_select_trend_metric(UI_METRIC_CO2);
    advance_ui(50u);
    lv_mem_monitor_t r1_m_before, r1_m_after;
    lv_mem_monitor(&r1_m_before);
    for (int k = 0; k < 200; k++) {
        ui_select_trend_metric(UI_METRIC_CO2);
        advance_ui(5u);
    }
    lv_mem_monitor(&r1_m_after);
    if (r1_m_after.free_size < r1_m_before.free_size) {
        printf("  FAIL 34f: 200 same-metric taps leaked %zu bytes heap\n",
               (size_t)(r1_m_before.free_size - r1_m_after.free_size));
        test34_pass = false;
    }

    ui_navigate_to_page(UI_PAGE_DEVICES);
    advance_ui(50u);
    ui_open_device_settings(0);
    advance_ui(50u);
    ui_dialog_open_dropdown();
    advance_ui(50u);
    lv_obj_t *dd_list_obj = ui_dialog_get_dropdown_list();
    uint32_t dd_styles_start = dd_list_obj ? dd_list_obj->style_cnt : 0;
    ui_dialog_close_dropdown();
    advance_ui(50u);
    lv_mem_monitor(&r1_m_before);
    for (int k = 0; k < 100; k++) {
        ui_dialog_open_dropdown();
        advance_ui(5u);
        ui_dialog_close_dropdown();
        advance_ui(5u);
    }
    lv_mem_monitor(&r1_m_after);
    if (dd_list_obj && dd_list_obj->style_cnt != dd_styles_start) {
        printf("  FAIL 34f: 100 dropdown cycles increased style count (%u -> %u)\n",
               (unsigned)dd_styles_start, (unsigned)dd_list_obj->style_cnt);
        test34_pass = false;
    }
    if (r1_m_after.free_size < r1_m_before.free_size) {
        printf("  FAIL 34f: 100 dropdown cycles leaked %zu bytes heap\n",
               (size_t)(r1_m_before.free_size - r1_m_after.free_size));
        test34_pass = false;
    }
    ui_close_device_settings();
    advance_ui(50u);

    /* 7. R2 Single text owner & duplicate units check on actual UI widgets */
    ui_navigate_to_page(UI_PAGE_TRENDS);
    lora_node_status.bits.co2_ppm = 100;
    lora_node_status.bits.humidity_pct = 100;
    int32_t r2_vals[2] = {100, 100};
    bool r2_valid[2] = {true, true};
    ui_feed_history(UI_METRIC_CO2, r2_vals, r2_valid, 2, 600);
    ui_feed_history(UI_METRIC_HUMIDITY, r2_vals, r2_valid, 2, 600);
    lora_rx_revision++;
    advance_ui(100u);
    ui_select_trend_metric(UI_METRIC_CO2);
    advance_ui(50u);
    ui_select_trend_metric(UI_METRIC_HUMIDITY);
    advance_ui(50u);
    const char *r2_cur_val = ui_get_trends_tab_val_str(UI_METRIC_HUMIDITY);
    const char *r2_cur_unit = ui_get_trends_tab_unit_str(UI_METRIC_HUMIDITY);
    const char *r2_title = ui_get_trends_title_str();
    const char *r2_unit = ui_get_trends_unit_str();
    if (!r2_cur_val || strcmp(r2_cur_val, "100") != 0 ||
        !r2_cur_unit || strcmp(r2_cur_unit, "%") != 0 ||
        !r2_title || strcmp(r2_title, "Humidity") != 0 ||
        !r2_unit || strcmp(r2_unit, "%") != 0) {
        printf("  FAIL 34g: Trends RH widgets mismatch: cur='%s' unit='%s' title='%s' unit_lbl='%s'\n",
               r2_cur_val ? r2_cur_val : "NULL",
               r2_cur_unit ? r2_cur_unit : "NULL",
               r2_title ? r2_title : "NULL",
               r2_unit ? r2_unit : "NULL");
        test34_pass = false;
    }
    lora_node_status.bits.humidity_pct = 99;
    lora_rx_revision++;
    advance_ui(100u);
    r2_cur_val = ui_get_trends_tab_val_str(UI_METRIC_HUMIDITY);
    if (!r2_cur_val || strcmp(r2_cur_val, "99") != 0) {
        printf("  FAIL 34g: Trends RH next val mismatch: expected '99', got '%s'\n",
               r2_cur_val ? r2_cur_val : "NULL");
        test34_pass = false;
    }

    /* 8. R3 Rebuilt chart range & R4 isolated points initialization */
    int32_t r3_vals[5] = {96, 98, 100, 102, 104};
    bool r3_valid[5] = {true, true, true, true, true};
    ui_feed_history(UI_METRIC_HUMIDITY, r3_vals, r3_valid, 5, 600);
    ui_select_trend_metric(UI_METRIC_HUMIDITY);
    advance_ui(50u);
    lv_chart_t *chart_inst = (lv_chart_t *)(*ui_get_test_hooks()->trends_chart);
    int ymin1 = chart_inst->ymin[0];
    int ymax1 = chart_inst->ymax[0];
    ui_navigate_to_page(UI_PAGE_HOME);
    advance_ui(50u);
    ui_navigate_to_page(UI_PAGE_TRENDS);
    advance_ui(50u);
    chart_inst = (lv_chart_t *)(*ui_get_test_hooks()->trends_chart);
    if (chart_inst->ymin[0] != ymin1 || chart_inst->ymax[0] != ymax1) {
        printf("  FAIL 34h: Rebuilt chart range mismatch (%d..%d vs %d..%d)\n",
               chart_inst->ymin[0], chart_inst->ymax[0], ymin1, ymax1);
        test34_pass = false;
    }
    /* R4: 3 isolated markers [400, invalid, 450, invalid, 500] in 5-sample history */
    int32_t isolated[5] = {400, 0, 450, 0, 500};
    bool iv[5] = {true, false, true, false, true};
    ui_feed_history(UI_METRIC_CO2, isolated, iv, 5, 600);
    ui_select_trend_metric(UI_METRIC_CO2);
    advance_ui(UI_TRENDS_SETTLE_MS + 50u);
    lv_obj_t *chart_obj = *ui_get_test_hooks()->trends_chart;
    lv_chart_series_t *ser = *ui_get_test_hooks()->trends_chart_series;
    if (ser->y_points[11] == LV_CHART_POINT_NONE || ser->y_points[12] != LV_CHART_POINT_NONE ||
        ser->y_points[13] == LV_CHART_POINT_NONE || ser->y_points[14] != LV_CHART_POINT_NONE ||
        ser->y_points[15] == LV_CHART_POINT_NONE ||
        !(ser->y_points[11] < ser->y_points[13] && ser->y_points[13] < ser->y_points[15])) {
        printf("  FAIL 34h: Isolated points series mapping failed\n");
        test34_pass = false;
    }

    /* R4 Pixel Verification: Ensure real pixels exist for all three markers and no bridging */
    lv_point_t p0, p2, p4;
    lv_chart_get_point_pos_by_id(chart_obj, ser, 11, &p0);
    lv_chart_get_point_pos_by_id(chart_obj, ser, 13, &p2);
    lv_chart_get_point_pos_by_id(chart_obj, ser, 15, &p4);
    int abs_x0 = chart_obj->coords.x1 + p0.x;
    int abs_y0 = chart_obj->coords.y1 + p0.y;
    int abs_x2 = chart_obj->coords.x1 + p2.x;
    int abs_y2 = chart_obj->coords.y1 + p2.y;
    int abs_x4 = chart_obj->coords.x1 + p4.x;
    int abs_y4 = chart_obj->coords.y1 + p4.y;

    int ink0 = 0, ink2 = 0, ink4 = 0;
    for (int dy = -4; dy <= 4; dy++) {
        for (int dx = -4; dx <= 4; dx++) {
            uint16_t pix0 = s_framebuffer[(abs_y0 + dy) * SIM_HOR + (abs_x0 + dx)];
            uint16_t pix2 = s_framebuffer[(abs_y2 + dy) * SIM_HOR + (abs_x2 + dx)];
            uint16_t pix4 = s_framebuffer[(abs_y4 + dy) * SIM_HOR + (abs_x4 + dx)];
            if (pix0 == UI_COLOR_FOREST.full || pix0 == UI_COLOR_CELADON.full) ink0++;
            if (pix2 == UI_COLOR_FOREST.full || pix2 == UI_COLOR_CELADON.full) ink2++;
            if (pix4 == UI_COLOR_FOREST.full || pix4 == UI_COLOR_CELADON.full) ink4++;
        }
    }
    if (ink0 == 0 || ink2 == 0 || ink4 == 0) {
        printf("  FAIL 34h: Missing marker pixels: pt0=%d pt2=%d pt4=%d\n", ink0, ink2, ink4);
        test34_pass = false;
    }

    /* Verify absence of line bridging across invalid points */
    int bridge0_2 = 0, bridge2_4 = 0;
    int mid0_2_x = (abs_x0 + abs_x2) / 2;
    int mid2_4_x = (abs_x2 + abs_x4) / 2;
    for (int y = chart_obj->coords.y1; y <= chart_obj->coords.y2; y++) {
        uint16_t p_m02 = s_framebuffer[y * SIM_HOR + mid0_2_x];
        uint16_t p_m24 = s_framebuffer[y * SIM_HOR + mid2_4_x];
        if (p_m02 == UI_COLOR_FOREST.full) bridge0_2++;
        if (p_m24 == UI_COLOR_FOREST.full) bridge2_4++;
    }
    if (bridge0_2 > 0 || bridge2_4 > 0) {
        printf("  FAIL 34h: Bridging detected across invalid gap: gap0_2=%d, gap2_4=%d\n", bridge0_2, bridge2_4);
        test34_pass = false;
    }

    /* Singleton test: [400, invalid, invalid, invalid, invalid] (1 sample -> Collecting readings...) */
    int32_t single_val[5] = {400, 0, 0, 0, 0};
    bool single_iv[5] = {true, false, false, false, false};
    ui_feed_history(UI_METRIC_CO2, single_val, single_iv, 5, 600);
    advance_ui(50u);
    if (ui_is_trends_empty_box_hidden() ||
        strcmp(ui_get_trends_empty_lbl_str(), "Collecting readings…") != 0 ||
        ui_is_trends_empty_hint_hidden()) {
        printf("  FAIL 34h: Singleton 1-sample state failed empty state assertion\n");
        test34_pass = false;
    }

    /* All-invalid test: [0, 0, 0, 0, 0] all invalid (0 samples -> No readings yet) */
    int32_t all_inv_val[5] = {0, 0, 0, 0, 0};
    bool all_inv_iv[5] = {false, false, false, false, false};
    ui_feed_history(UI_METRIC_CO2, all_inv_val, all_inv_iv, 5, 600);
    advance_ui(50u);
    if (ui_is_trends_empty_box_hidden() ||
        strcmp(ui_get_trends_empty_lbl_str(), "No readings yet") != 0 ||
        !ui_is_trends_empty_hint_hidden()) {
        printf("  FAIL 34h: All-invalid 0-sample state failed empty state assertion\n");
        test34_pass = false;
    }

    /* 9. R5 Full Sensor Availability Matrix (all 9 fault combinations + clean recovery) */
    ui_navigate_to_page(UI_PAGE_HOME);
    lora_last_rx_tick_ms = lv_tick_get();
    lora_node_status.bits.rs485_fault = 0;
    lora_node_status.bits.co2_timeout = 0;
    lora_node_status.bits.th_timeout = 0;
    lora_node_status.bits.co2_valid = 1;
    lora_node_status.bits.co2_ppm = 420;
    lora_node_status.bits.voc_valid = 1;
    lora_node_status.bits.tvoc_ppb = 0; /* Valid telemetry 0 */
    lora_node_status.bits.temp_valid = 1;
    lora_node_status.bits.temp_deci_c = 235;
    lora_node_status.bits.humid_valid = 1;
    lora_node_status.bits.humidity_pct = 48;
    lora_rx_revision++;
    advance_ui(100u);

    /* Assert baseline valid values */
    if (strcmp(ui_get_home_co2_str(), "420") != 0 ||
        strcmp(ui_get_home_voc_str(), "0") != 0 ||
        strcmp(ui_get_home_temp_str(), "23.5") != 0 ||
        strcmp(ui_get_home_humid_str(), "48") != 0) {
        printf("  FAIL 34i: Baseline sensor readings mismatch\n");
        test34_pass = false;
    }

    #define CHECK_AUTO_LIVE_READING(dev_idx, expected_str, metric_name) do { \
        ui_open_device_settings(dev_idx); \
        advance_ui(50u); \
        if (h34 && h34->on_dialog_open_limits_click) h34->on_dialog_open_limits_click(NULL); \
        advance_ui(50u); \
        lv_obj_t *lbl = *ui_get_test_hooks()->dialog_live_lbl; \
        if (!lbl || strcmp(lv_label_get_text(lbl), expected_str) != 0) { \
            printf("  FAIL 34i: Auto editor dev %d (%s) live label expected '%s', got '%s'\n", \
                   (int)(dev_idx), (metric_name), (expected_str), lbl ? lv_label_get_text(lbl) : "NULL"); \
            test34_pass = false; \
        } \
        ui_close_device_settings(); \
        advance_ui(50u); \
    } while (0)

    /* Test 1: !co2_valid -> CO2 is —, VOC=0 preserved, Temp=23.5 preserved, Humid=48 preserved */
    lora_node_status.bits.co2_valid = 0;
    lora_rx_revision++;
    advance_ui(100u);
    if (strcmp(ui_get_home_co2_str(), "—") != 0 ||
        strcmp(ui_get_home_voc_str(), "0") != 0 ||
        strcmp(ui_get_home_temp_str(), "23.5") != 0 ||
        strcmp(ui_get_home_humid_str(), "48") != 0) {
        printf("  FAIL 34i: !co2_valid did not mask CO2 while preserving others\n");
        test34_pass = false;
    }
    CHECK_AUTO_LIVE_READING(1, "No live data", "Fan/CO2");
    lora_node_status.bits.co2_valid = 1;

    /* Test 2: !voc_valid -> VOC is —, CO2=420 preserved */
    lora_node_status.bits.voc_valid = 0;
    lora_rx_revision++;
    advance_ui(100u);
    if (strcmp(ui_get_home_voc_str(), "—") != 0 || strcmp(ui_get_home_co2_str(), "420") != 0) {
        printf("  FAIL 34i: !voc_valid did not mask VOC\n");
        test34_pass = false;
    }
    CHECK_AUTO_LIVE_READING(0, "No live data", "Purifier/VOC");
    lora_node_status.bits.voc_valid = 1;

    /* Test 3: !temp_valid -> Temp is —, others preserved */
    lora_node_status.bits.temp_valid = 0;
    lora_rx_revision++;
    advance_ui(100u);
    if (strcmp(ui_get_home_temp_str(), "—") != 0 || strcmp(ui_get_home_co2_str(), "420") != 0) {
        printf("  FAIL 34i: !temp_valid did not mask Temp\n");
        test34_pass = false;
    }
    lora_node_status.bits.temp_valid = 1;

    /* Test 4: !humid_valid -> Humid is —, others preserved */
    lora_node_status.bits.humid_valid = 0;
    lora_rx_revision++;
    advance_ui(100u);
    if (strcmp(ui_get_home_humid_str(), "—") != 0 || strcmp(ui_get_home_co2_str(), "420") != 0) {
        printf("  FAIL 34i: !humid_valid did not mask Humid\n");
        test34_pass = false;
    }
    CHECK_AUTO_LIVE_READING(2, "No live data", "Humidifier/RH");
    lora_node_status.bits.humid_valid = 1;

    /* Test 5: co2_timeout -> CO2 and VOC are —, Temp and Humid preserved */
    lora_node_status.bits.co2_timeout = 1;
    lora_rx_revision++;
    advance_ui(100u);
    if (strcmp(ui_get_home_co2_str(), "—") != 0 || strcmp(ui_get_home_voc_str(), "—") != 0 ||
        strcmp(ui_get_home_temp_str(), "23.5") != 0 || strcmp(ui_get_home_humid_str(), "48") != 0) {
        printf("  FAIL 34i: co2_timeout did not mask both CO2 and VOC\n");
        test34_pass = false;
    }
    CHECK_AUTO_LIVE_READING(1, "No live data", "Fan/CO2");
    CHECK_AUTO_LIVE_READING(0, "No live data", "Purifier/VOC");
    lora_node_status.bits.co2_timeout = 0;

    /* Test 6: th_timeout -> Temp and Humid are —, CO2 and VOC preserved */
    lora_node_status.bits.th_timeout = 1;
    lora_rx_revision++;
    advance_ui(100u);
    if (strcmp(ui_get_home_temp_str(), "—") != 0 || strcmp(ui_get_home_humid_str(), "—") != 0 ||
        strcmp(ui_get_home_co2_str(), "420") != 0 || strcmp(ui_get_home_voc_str(), "0") != 0) {
        printf("  FAIL 34i: th_timeout did not mask both Temp and Humid\n");
        test34_pass = false;
    }
    CHECK_AUTO_LIVE_READING(2, "No live data", "Humidifier/RH");
    lora_node_status.bits.th_timeout = 0;

    /* Test 7: rs485_fault -> all 4 sensors —, Air status 'Unavailable', all 3 Auto editors 'No live data' */
    lora_node_status.bits.rs485_fault = 1;
    lora_rx_revision++;
    advance_ui(100u);
    lv_obj_t *air_status_lbl = *ui_get_test_hooks()->home_air_status;
    if (strcmp(ui_get_home_co2_str(), "—") != 0 || strcmp(ui_get_home_voc_str(), "—") != 0 ||
        strcmp(ui_get_home_temp_str(), "—") != 0 || strcmp(ui_get_home_humid_str(), "—") != 0 ||
        (air_status_lbl && strcmp(lv_label_get_text(air_status_lbl), "Unavailable") != 0)) {
        printf("  FAIL 34i: rs485_fault did not mask all sensors\n");
        test34_pass = false;
    }
    CHECK_AUTO_LIVE_READING(1, "No live data", "Fan/CO2");
    CHECK_AUTO_LIVE_READING(0, "No live data", "Purifier/VOC");
    CHECK_AUTO_LIVE_READING(2, "No live data", "Humidifier/RH");
    lora_node_status.bits.rs485_fault = 0;
    lora_rx_revision++;
    advance_ui(100u);

    /* Test 8: Disconnected (offline) -> all 4 sensors —, all 3 Auto editors 'No live data' */
    s_fake_node_mode = FAKE_NODE_MODE_STOP_REPORTING;
    lora_last_rx_tick_ms = 0;
    advance_ui(LORA_LINK_TIMEOUT_MS + 200u);
    if (strcmp(ui_get_home_co2_str(), "—") != 0 || strcmp(ui_get_home_voc_str(), "—") != 0 ||
        strcmp(ui_get_home_temp_str(), "—") != 0 || strcmp(ui_get_home_humid_str(), "—") != 0) {
        printf("  FAIL 34i: Offline did not mask all sensors (got co2='%s' voc='%s' temp='%s' rh='%s')\n",
               ui_get_home_co2_str(), ui_get_home_voc_str(), ui_get_home_temp_str(), ui_get_home_humid_str());
        test34_pass = false;
    }
    CHECK_AUTO_LIVE_READING(1, "No live data", "Fan/CO2");
    CHECK_AUTO_LIVE_READING(0, "No live data", "Purifier/VOC");
    CHECK_AUTO_LIVE_READING(2, "No live data", "Humidifier/RH");
    s_fake_node_mode = FAKE_NODE_MODE_NORMAL;

    /* Test 9: Clean recovery */
    lora_last_rx_tick_ms = lv_tick_get();
    lora_rx_revision++;
    advance_ui(100u);
    if (strcmp(ui_get_home_co2_str(), "420") != 0 || strcmp(ui_get_home_voc_str(), "0") != 0 ||
        strcmp(ui_get_home_temp_str(), "23.5") != 0 || strcmp(ui_get_home_humid_str(), "48") != 0) {
        printf("  FAIL 34i: Clean recovery failed\n");
        test34_pass = false;
    }
    CHECK_AUTO_LIVE_READING(1, "420", "Fan/CO2");
    CHECK_AUTO_LIVE_READING(0, "0", "Purifier/VOC");
    CHECK_AUTO_LIVE_READING(2, "48", "Humidifier/RH");
    #undef CHECK_AUTO_LIVE_READING

    /* 10. R6 Real no-op Save from Settings and Auto subview */
    ui_set_config_changed_cb(r6_config_changed_probe_cb);

    ui_hub_device_config_t r6_cfg = *ui_get_device_config(1);
    snprintf(r6_cfg.name, sizeof(r6_cfg.name), "Living Room Exhaust 123");
    r6_cfg.mode = UI_MODE_AUTO;
    r6_cfg.auto_on_thresh = 1000;
    r6_cfg.auto_off_thresh = 800;
    ui_test_set_boot_staging(true);
    ui_set_device_config(1, &r6_cfg);
    ui_test_set_boot_staging(false);
    advance_ui(50u);

    /* Record auto state hold time / qualification before opening settings */
    ui_auto_state_t *ast_r6 = &((ui_auto_state_t *)ui_get_test_hooks()->auto_states)[1];
    uint32_t hold_before = ast_r6->last_transition_ms;
    uint8_t qual_before = ast_r6->qualify_count;

    /* (a) No-op Save directly from Settings main dialog */
    s_r6_config_cb_count = 0;
    ui_open_device_settings(1);
    advance_ui(50u);
    if (h34 && h34->on_dialog_save_click) {
        h34->on_dialog_save_click(NULL);
    }
    advance_ui(50u);
    if (s_r6_config_cb_count != 0) {
        printf("  FAIL 34j: No-op Save from main dialog triggered callback (%d calls)\n", s_r6_config_cb_count);
        test34_pass = false;
    }
    if (strcmp(ui_get_device_config(1)->name, "Living Room Exhaust 123") != 0) {
        printf("  FAIL 34j: No-op Save corrupted custom name to '%s'\n", ui_get_device_config(1)->name);
        test34_pass = false;
    }
    if (ast_r6->last_transition_ms != hold_before || ast_r6->qualify_count != qual_before) {
        printf("  FAIL 34j: No-op Save reset auto hold time or qualification count\n");
        test34_pass = false;
    }

    /* (b) No-op Save from Auto limits subview */
    s_r6_config_cb_count = 0;
    ui_open_device_settings(1);
    advance_ui(50u);
    if (h34 && h34->on_dialog_open_limits_click) {
        h34->on_dialog_open_limits_click(NULL);
    }
    advance_ui(50u);
    if (h34 && h34->on_dialog_save_click) {
        h34->on_dialog_save_click(NULL);
    }
    advance_ui(50u);
    if (s_r6_config_cb_count != 0) {
        printf("  FAIL 34j: No-op Save from Auto subview triggered callback (%d calls)\n", s_r6_config_cb_count);
        test34_pass = false;
    }
    if (strcmp(ui_get_device_config(1)->name, "Living Room Exhaust 123") != 0) {
        printf("  FAIL 34j: No-op Save from Auto subview corrupted custom name\n");
        test34_pass = false;
    }
    if (ast_r6->last_transition_ms != hold_before || ast_r6->qualify_count != qual_before) {
        printf("  FAIL 34j: No-op Save from Auto subview reset auto hold/qual\n");
        test34_pass = false;
    }

    /* (c) Real change: modify threshold and assert exactly 1 callback */
    s_r6_config_cb_count = 0;
    ui_open_device_settings(1);
    advance_ui(50u);
    if (h34 && h34->on_dialog_open_limits_click) {
        h34->on_dialog_open_limits_click(NULL);
    }
    advance_ui(50u);
    ui_dialog_step_thresh(true, 1); /* Step ON threshold from 1000 -> 1050 (+50 ppm step) */
    advance_ui(50u);
    if (h34 && h34->on_dialog_save_click) {
        h34->on_dialog_save_click(NULL);
    }
    advance_ui(50u);
    if (s_r6_config_cb_count != 1) {
        printf("  FAIL 34j: Modified threshold Save expected 1 callback, got %d\n", s_r6_config_cb_count);
        test34_pass = false;
    }
    if (ui_get_device_config(1)->auto_on_thresh != 1050) {
        printf("  FAIL 34j: Modified threshold was not committed (got %d, expected 1050)\n",
               (int)ui_get_device_config(1)->auto_on_thresh);
        test34_pass = false;
    }

    /* (d) Cancel / Back discard test: modify threshold then discard */
    s_r6_config_cb_count = 0;
    ui_open_device_settings(1);
    advance_ui(50u);
    if (h34 && h34->on_dialog_open_limits_click) {
        h34->on_dialog_open_limits_click(NULL);
    }
    advance_ui(50u);
    ui_dialog_step_thresh(true, 1); /* Step ON to 1100 in dialog draft */
    advance_ui(50u);
    ui_close_device_settings(); /* Discard / Cancel */
    advance_ui(50u);
    if (s_r6_config_cb_count != 0) {
        printf("  FAIL 34j: Discarded settings edit triggered callback\n");
        test34_pass = false;
    }
    if (ui_get_device_config(1)->auto_on_thresh != 1050) {
        printf("  FAIL 34j: Discarded edit was committed unexpectedly (got %d, expected 1050)\n",
               (int)ui_get_device_config(1)->auto_on_thresh);
        test34_pass = false;
    }

    ui_set_config_changed_cb(NULL);
    ui_test_reset_device_configs();
    advance_ui(50u);

    /* 11. G1 Link-label containment & Uptime removal verification */
    ui_navigate_to_page(UI_PAGE_HOME);
    lv_obj_t *sep = *ui_get_test_hooks()->header_sep;
    lv_obj_t *up_lbl = *ui_get_test_hooks()->uptime_label;
    lv_obj_t *clock_ico = *ui_get_test_hooks()->header_clock_ico;
    lv_obj_t *h_link = *ui_get_test_hooks()->link_label;
    lv_obj_t *h_bars_cont = *ui_get_test_hooks()->link_bars_cont;

    /* Verify uptime widgets are completely removed from header */
    if (sep != NULL || up_lbl != NULL || clock_ico != NULL) {
        printf("  FAIL 34k: Uptime widgets expected to be NULL (removed), got sep=%p up_lbl=%p clock_ico=%p\n",
               (void*)sep, (void*)up_lbl, (void*)clock_ico);
        test34_pass = false;
    }

    /* G1 Containment: Assert right edge of link label is strictly x2 == 777 (22px margin: 799 - 777 = 22)
     * across boot (disconnected), connected, disconnected, and reconnected states. */
    if (h_link != NULL) {
        /* State 1: Boot / Initial Offline */
        lora_last_rx_tick_ms = 0;
        advance_ui(50u);
        lv_obj_update_layout(h_link);
        if (h_link->coords.x2 != 777) {
            printf("  FAIL 34k: G1 link label boot coords.x2 expected 777, got %d (w=%d)\n",
                   h_link->coords.x2, lv_obj_get_width(h_link));
            test34_pass = false;
        }
        /* State 2: Connected */
        lora_last_rx_tick_ms = lv_tick_get();
        lora_rx_revision++;
        advance_ui(50u);
        lv_obj_update_layout(h_link);
        if (h_link->coords.x2 != 777) {
            printf("  FAIL 34k: G1 link label connected coords.x2 expected 777, got %d (w=%d)\n",
                   h_link->coords.x2, lv_obj_get_width(h_link));
            test34_pass = false;
        }
        /* State 3: Disconnected */
        lora_last_rx_tick_ms = 0;
        advance_ui(LORA_LINK_TIMEOUT_MS + 200u);
        lv_obj_update_layout(h_link);
        if (h_link->coords.x2 != 777) {
            printf("  FAIL 34k: G1 link label disconnected coords.x2 expected 777, got %d (w=%d)\n",
                   h_link->coords.x2, lv_obj_get_width(h_link));
            test34_pass = false;
        }
        /* State 4: Reconnected */
        lora_last_rx_tick_ms = lv_tick_get();
        lora_rx_revision++;
        advance_ui(50u);
        lv_obj_update_layout(h_link);
        if (h_link->coords.x2 != 777) {
            printf("  FAIL 34k: G1 link label reconnected coords.x2 expected 777, got %d (w=%d)\n",
                   h_link->coords.x2, lv_obj_get_width(h_link));
            test34_pass = false;
        }
    }

    /* Verify scaled signal bars container (22x18) and gap to link label (10px) */
    if (h_bars_cont != NULL && h_link != NULL) {
        lv_obj_update_layout(h_link);
        lv_obj_update_layout(h_bars_cont);
        if (lv_obj_get_width(h_bars_cont) != 22 || lv_obj_get_height(h_bars_cont) != 18) {
            printf("  FAIL 34k: Scaled link bars container size expected 22x18, got %dx%d\n",
                   (int)lv_obj_get_width(h_bars_cont), (int)lv_obj_get_height(h_bars_cont));
            test34_pass = false;
        }
        int bar_gap = h_link->coords.x1 - h_bars_cont->coords.x2 - 1;
        if (bar_gap != 10) {
            printf("  FAIL 34k: Gap between signal bars and link label expected 10, got %d\n", bar_gap);
            test34_pass = false;
        }
    }

    /* 12. F2 Exact unavailable-state copy checks (no Offline/No data notes, no Unknown, dash only) */
    ui_navigate_to_page(UI_PAGE_HOME);
    s_fake_node_mode = FAKE_NODE_MODE_STOP_REPORTING;
    lora_last_rx_tick_ms = 0; /* Force offline */
    advance_ui(LORA_LINK_TIMEOUT_MS + 200u);
    lv_obj_t *temp_note = *ui_get_test_hooks()->home_temp_note;
    lv_obj_t *humid_note = *ui_get_test_hooks()->home_humid_note;
    if (temp_note && !lv_obj_has_flag(temp_note, LV_OBJ_FLAG_HIDDEN)) {
        printf("  FAIL 34l: Home temp note visible during offline (expected hidden)\n");
        test34_pass = false;
    }
    if (humid_note && !lv_obj_has_flag(humid_note, LV_OBJ_FLAG_HIDDEN)) {
        printf("  FAIL 34l: Home humid note visible during offline (expected hidden)\n");
        test34_pass = false;
    }
    for (uint8_t i = 0; i < UI_DEVICE_COUNT; i++) {
        lv_obj_t *h_st = ui_get_test_hooks()->home_dev_states[i];
        lv_obj_t *h_dash = ui_get_test_hooks()->home_dev_dashes[i];
        if (h_st && !lv_obj_has_flag(h_st, LV_OBJ_FLAG_HIDDEN)) {
            printf("  FAIL 34l: Home device %u state label visible during offline (expected hidden)\n", i);
            test34_pass = false;
        }
        if (h_dash && lv_obj_has_flag(h_dash, LV_OBJ_FLAG_HIDDEN)) {
            printf("  FAIL 34l: Home device %u dash not visible during offline\n", i);
            test34_pass = false;
        }
    }
    ui_navigate_to_page(UI_PAGE_DEVICES);
    advance_ui(100u);
    for (uint8_t i = 0; i < UI_DEVICE_COUNT; i++) {
        lv_obj_t *d_st = ui_get_test_hooks()->dev_page_states[i];
        lv_obj_t *d_dash = ui_get_test_hooks()->dev_page_dashes[i];
        if (d_st && !lv_obj_has_flag(d_st, LV_OBJ_FLAG_HIDDEN)) {
            printf("  FAIL 34l: Devices card %u state label visible during offline (expected hidden)\n", i);
            test34_pass = false;
        }
        if (d_dash && lv_obj_has_flag(d_dash, LV_OBJ_FLAG_HIDDEN)) {
            printf("  FAIL 34l: Devices card %u dash not visible during offline\n", i);
            test34_pass = false;
        }
    }
    /* Restore online and test Command Error state */
    s_fake_node_mode = FAKE_NODE_MODE_NORMAL;
    lora_last_rx_tick_ms = lv_tick_get();
    lora_rx_revision++;
    advance_ui(100u);
    s_fake_node_mode = FAKE_NODE_MODE_FAIL_NEXT;
    ui_trigger_device_toggle(0);
    advance_ui(UI_CMD_TIMEOUT_MS + 200u);
    s_fake_node_mode = FAKE_NODE_MODE_NORMAL;

    lv_obj_t *d0_st = ui_get_test_hooks()->dev_page_states[0];
    lv_obj_t *d0_retry = ui_get_test_hooks()->dev_page_retries[0];
    lv_obj_t *d0_dash = ui_get_test_hooks()->dev_page_dashes[0];
    if (!d0_st || lv_obj_has_flag(d0_st, LV_OBJ_FLAG_HIDDEN) || strcmp(lv_label_get_text(d0_st), "—") != 0) {
        printf("  FAIL 34l: Devices error state label expected '—', got '%s'\n",
               d0_st ? lv_label_get_text(d0_st) : "NULL");
        test34_pass = false;
    }
    if (!d0_retry || lv_obj_has_flag(d0_retry, LV_OBJ_FLAG_HIDDEN)) {
        printf("  FAIL 34l: Devices error state Retry button not visible\n");
        test34_pass = false;
    }
    if (d0_dash && !lv_obj_has_flag(d0_dash, LV_OBJ_FLAG_HIDDEN)) {
        printf("  FAIL 34l: Devices error state offline dash visible (expected hidden to avoid overlap)\n");
        test34_pass = false;
    }

    /* Restore clean defaults */
    ui_test_reset_device_commands();
    ui_test_reset_device_configs();
    populate_node_status_baseline();
    advance_ui(500u);

    if (test34_pass) {
        printf("  PASS: Calibrated micro-geometry, pointer hit-testing, name boundary & concurrent auto verified.\n");
        passed_tests++;
    }

    /* -------------------------------------------------------------
     * TEST 35: Autonomous Trends Session History & Live Auto Triggering
     * ------------------------------------------------------------- */
    total_tests++;
    printf("\n[CHECK 35] Autonomous Trends Session History & Live Auto Triggering...\n");
    bool test35_pass = true;
    const ui_test_hooks_t *h35 = ui_get_test_hooks();

    /* -------------------------------------------------------------
     * Part A: Autonomous Trends Session History (P1)
     * ------------------------------------------------------------- */
    /* Reset history cleanly to clear any injected history and reset precedence */
    ui_test_reset_history();
    ui_navigate_to_page(UI_PAGE_TRENDS);
    advance_ui(50u);

    /* Assert that immediately at boot before any fresh report, history count is 0 and empty box visible */
    for (int m = 0; m < UI_METRIC_COUNT; m++) {
        if (h35->history_count[m] != 0) {
            printf("  FAIL 35a: Metric %d history count not 0 at boot\n", m);
            test35_pass = false;
        }
    }

    /* First accepted fresh report online: capture initial singleton point for each metric */
    sim_post_node_status(true, 450, true, 20, true, 240, true, 55, true, 0, 0, 0, 0);
    advance_ui(50u);

    for (int m = 0; m < UI_METRIC_COUNT; m++) {
        if (h35->history_count[m] != 1) {
            printf("  FAIL 35a: Metric %d history count expected 1 after first frame, got %d\n",
                   m, (int)h35->history_count[m]);
            test35_pass = false;
        }
        const ui_history_sample_t *s0 = ui_get_history_sample((ui_metric_id_t)m, 0);
        if (!s0 || !s0->valid) {
            printf("  FAIL 35a: Metric %d initial sample missing or invalid\n", m);
            test35_pass = false;
        }
    }
    const ui_history_sample_t *s_co2_0 = ui_get_history_sample(UI_METRIC_CO2, 0);
    if (s_co2_0 && s_co2_0->value != 450) {
        printf("  FAIL 35a: CO2 initial value mismatch (got %ld, expected 450)\n", (long)s_co2_0->value);
        test35_pass = false;
    }
    const ui_history_sample_t *s_voc_0 = ui_get_history_sample(UI_METRIC_VOC, 0);
    if (s_voc_0 && s_voc_0->value != 20) {
        printf("  FAIL 35a: VOC initial value mismatch (got %ld, expected 20)\n", (long)s_voc_0->value);
        test35_pass = false;
    }

    /* In Trends UI: CO2 is selected, verify tab val="450", tab unit="ppm", title="eCO₂", empty box shown for singleton */
    ui_select_trend_metric(UI_METRIC_CO2);
    advance_ui(UI_TRENDS_SETTLE_MS + 50u);
    const char *co2_tab_val = ui_get_trends_tab_val_str(UI_METRIC_CO2);
    const char *co2_tab_unit = ui_get_trends_tab_unit_str(UI_METRIC_CO2);
    const char *co2_title = ui_get_trends_title_str();
    if (!co2_tab_val || strcmp(co2_tab_val, "450") != 0 ||
        !co2_tab_unit || strcmp(co2_tab_unit, "ppm") != 0 ||
        !co2_title || strcmp(co2_title, "eCO₂") != 0 ||
        ui_is_trends_empty_box_hidden() ||
        strcmp(ui_get_trends_empty_lbl_str(), "Collecting readings…") != 0) {
        printf("  FAIL 35a: Trends singleton mismatch (val='%s', unit='%s', title='%s', empty_hidden=%d)\n",
               co2_tab_val ? co2_tab_val : "NULL", co2_tab_unit ? co2_tab_unit : "NULL",
               co2_title ? co2_title : "NULL",
               ui_is_trends_empty_box_hidden());
        test35_pass = false;
    }

    /* Advance time by 60 seconds with another frame: captures 2nd slot */
    lv_tick_inc(60000u);
    sim_post_node_status(true, 480, true, 25, true, 242, true, 54, true, 0, 0, 0, 0);
    advance_ui(50u);
    if (h35->history_count[UI_METRIC_CO2] != 2) {
        printf("  FAIL 35a: CO2 history count expected 2 after 1 minute, got %d\n",
               (int)h35->history_count[UI_METRIC_CO2]);
        test35_pass = false;
    }

    /* External feed precedence test: calling ui_feed_history on CO2 switches CO2 to external */
    int32_t ext_vals[3] = { 700, 750, 800 };
    bool ext_val_valid[3] = { true, true, true };
    ui_feed_history(UI_METRIC_CO2, ext_vals, ext_val_valid, 3, 600);
    advance_ui(50u);
    if (h35->history_count[UI_METRIC_CO2] != 3) {
        printf("  FAIL 35a: External feed on CO2 expected count 3, got %d\n", (int)h35->history_count[UI_METRIC_CO2]);
        test35_pass = false;
    }
    /* VOC must remain autonomous with count 2 */
    if (h35->history_count[UI_METRIC_VOC] != 2) {
        printf("  FAIL 35a: VOC history count corrupted by CO2 external feed (got %d, expected 2)\n",
               (int)h35->history_count[UI_METRIC_VOC]);
        test35_pass = false;
    }

    /* (a2) VOC=0 valid singleton and multi-minute gap handling */
    ui_test_reset_history();
    ui_navigate_to_page(UI_PAGE_TRENDS);
    advance_ui(50u);

    /* Feed frame with VOC = 0 (valid baseline) */
    sim_post_node_status(true, 500, true, 0, true, 220, true, 50, true, 0, 0, 0, 0);
    advance_ui(50u);
    const ui_history_sample_t *s_voc_zero = ui_get_history_sample(UI_METRIC_VOC, 0);
    if (!s_voc_zero || !s_voc_zero->valid || s_voc_zero->value != 0) {
        printf("  FAIL 35a: VOC=0 singleton not recognized as valid (valid=%d, val=%ld)\n",
               s_voc_zero ? s_voc_zero->valid : 0, s_voc_zero ? (long)s_voc_zero->value : -1);
        test35_pass = false;
    }

    /* Advance 3 minutes with NO packets arriving -> simulate dropped frames / RF gap */
    lv_tick_inc(180000u);
    /* Packet arrives at minute 3 */
    sim_post_node_status(true, 520, true, 10, true, 222, true, 51, true, 0, 0, 0, 0);
    advance_ui(50u);
    if (h35->history_count[UI_METRIC_VOC] != 4) {
        printf("  FAIL 35a: Gap handling expected count 4 after 3 min leap, got %d\n",
               (int)h35->history_count[UI_METRIC_VOC]);
        test35_pass = false;
    } else {
        const ui_history_sample_t *g1 = ui_get_history_sample(UI_METRIC_VOC, 1);
        const ui_history_sample_t *g2 = ui_get_history_sample(UI_METRIC_VOC, 2);
        const ui_history_sample_t *s3 = ui_get_history_sample(UI_METRIC_VOC, 3);
        if (g1->valid || g2->valid) {
            printf("  FAIL 35a: Skipped minutes erroneously marked valid (g1=%d, g2=%d)\n", g1->valid, g2->valid);
            test35_pass = false;
        }
        if (!s3->valid || s3->value != 10) {
            printf("  FAIL 35a: Resumed minute sample invalid or incorrect value (valid=%d, val=%ld)\n",
               s3->valid, (long)s3->value);
            test35_pass = false;
        }
    }

    /* Check autonomous time label formatting (B3 fixed window labels: -15 min .. Now) */
    const char *lbl0 = ui_get_trends_time_lbl_str(0);
    const char *lbl3 = ui_get_trends_time_lbl_str(3);
    if (!lbl0 || strcmp(lbl0, "-15 min") != 0 || !lbl3 || strcmp(lbl3, "Now") != 0) {
        printf("  FAIL 35a: Autonomous time labels expected '-15 min' and 'Now', got '%s' and '%s'\n",
               lbl0 ? lbl0 : "NULL", lbl3 ? lbl3 : "NULL");
        test35_pass = false;
    }

    /* -------------------------------------------------------------
     * Part B: Auto Command Triggering without 10s Delay (P1)
     * ------------------------------------------------------------- */
    /* Device 1: Fan / CO2. Start Manual, relay OFF. */
    ui_hub_device_config_t dev_fan;
    ui_auto_get_defaults(1, UI_PRESET_FAN, &dev_fan);
    dev_fan.mode = UI_MODE_MANUAL;
    dev_fan.active_low = 0;
    ui_set_device_config(1, &dev_fan);
    sim_post_node_status(true, 500, true, 50, true, 230, true, 45, true, 0, 0, 0, 0);
    ui_test_reset_device_commands();
    lv_tick_inc(12000u); /* Let initial boot/switch hold expire (>10s) */
    advance_ui(50u);

    /* Now user commits Manual -> Auto */
    dev_fan.mode = UI_MODE_AUTO;
    ui_set_device_config(1, &dev_fan);
    advance_ui(50u);

    /* Frame 1: CO2 = 1200 (>= 1000 threshold). No command expected yet (qualify count 1) */
    sim_post_node_status(true, 1200, true, 50, true, 230, true, 45, true, 0, 0, 0, 0);
    advance_ui(50u);
    if (h35->device_cmds[1].phase != UI_CMD_PHASE_IDLE) {
        printf("  FAIL 35b: Fan command fired on frame 1 (expected IDLE, got %d)\n", h35->device_cmds[1].phase);
        test35_pass = false;
    }

    /* Frame 2: CO2 = 1205. No command expected yet (qualify count 2) */
    sim_post_node_status(true, 1205, true, 50, true, 230, true, 45, true, 0, 0, 0, 0);
    advance_ui(50u);
    if (h35->device_cmds[1].phase != UI_CMD_PHASE_IDLE) {
        printf("  FAIL 35b: Fan command fired on frame 2 (expected IDLE, got %d)\n", h35->device_cmds[1].phase);
        test35_pass = false;
    }

    /* Frame 3: CO2 = 1210. Command MUST fire on frame 3 without needing 10s wait! */
    sim_post_node_status(true, 1210, true, 50, true, 230, true, 45, true, 0, 0, 0, 0);
    advance_ui(50u);
    if (h35->device_cmds[1].phase != UI_CMD_PHASE_PENDING) {
        printf("  FAIL 35b: Fan command NOT fired on frame 3 (expected PENDING, got %d) - 10s hold bug!\n",
               h35->device_cmds[1].phase);
        test35_pass = false;
    }
    if (lora_hub_cmd_get_relay_gpio(&lora_hub_cmd, 1) != 1) {
        printf("  FAIL 35b: Fan desired wire level expected 1, got %d\n",
               lora_hub_cmd_get_relay_gpio(&lora_hub_cmd, 1));
        test35_pass = false;
    }

    /* Node confirms transition: echo seq and reported relay1=1 */
    lora_node_status.bits.seq_echo = lora_hub_cmd.bits.seq;
    lora_node_status_set_relay_gpio(&lora_node_status, 1, 1);
    lora_rx_revision++;
    advance_ui(50u);
    if (h35->device_cmds[1].phase != UI_CMD_PHASE_IDLE) {
        printf("  FAIL 35b: Fan command phase not cleared after Node echo\n");
        test35_pass = false;
    }

    /* Confirmed transition hold: opposite threshold (CO2 = 600 <= 800) must be SUPPRESSED for 10s */
    for (int k = 0; k < 5; k++) {
        sim_post_node_status(true, 600, true, 50, true, 230, true, 45, true, 0, 1, 0, 0);
        advance_ui(100u);
    }
    if (h35->device_cmds[1].phase != UI_CMD_PHASE_IDLE) {
        printf("  FAIL 35b: Fan turned off prematurely during 10s confirmed transition hold\n");
        test35_pass = false;
    }

    /* After 10s hold expires: 3 fresh frames at CO2=600 triggers OFF command */
    lv_tick_inc(10000u);
    advance_ui(50u);
    sim_post_node_status(true, 600, true, 50, true, 230, true, 45, true, 0, 1, 0, 0);
    advance_ui(50u);
    sim_post_node_status(true, 595, true, 50, true, 230, true, 45, true, 0, 1, 0, 0);
    advance_ui(50u);
    sim_post_node_status(true, 590, true, 50, true, 230, true, 45, true, 0, 1, 0, 0);
    advance_ui(50u);
    if (h35->device_cmds[1].phase != UI_CMD_PHASE_PENDING) {
        printf("  FAIL 35b: Fan OFF command NOT fired after 10s hold + 3 frames\n");
        test35_pass = false;
    }
    if (lora_hub_cmd_get_relay_gpio(&lora_hub_cmd, 1) != 0) {
        printf("  FAIL 35b: Fan OFF wire level expected 0, got %d\n",
               lora_hub_cmd_get_relay_gpio(&lora_hub_cmd, 1));
        test35_pass = false;
    }

    /* (b2) Active low polarity: Manual -> Auto */
    ui_hub_device_config_t dev_pur;
    ui_auto_get_defaults(0, UI_PRESET_PURIFIER, &dev_pur);
    dev_pur.mode = UI_MODE_MANUAL;
    dev_pur.active_low = 1; /* Active low: GPIO 0 is ON, GPIO 1 is OFF */
    ui_set_device_config(0, &dev_pur);
    /* Report relay 0 GPIO = 1 (meaning logical OFF) */
    sim_post_node_status(true, 500, true, 50, true, 230, true, 45, true, 1, 0, 0, 0);
    advance_ui(50u);
    ui_test_reset_device_commands();
    lv_tick_inc(12000u);
    advance_ui(50u);

    /* Switch to Auto */
    dev_pur.mode = UI_MODE_AUTO;
    ui_set_device_config(0, &dev_pur);
    advance_ui(50u);

    /* Feed 3 frames with high TVOC (700 >= 660 on_thresh) */
    sim_post_node_status(true, 500, true, 700, true, 230, true, 45, true, 1, 0, 0, 0);
    advance_ui(50u);
    sim_post_node_status(true, 500, true, 702, true, 230, true, 45, true, 1, 0, 0, 0);
    advance_ui(50u);
    sim_post_node_status(true, 500, true, 705, true, 230, true, 45, true, 1, 0, 0, 0);
    advance_ui(50u);
    if (h35->device_cmds[0].phase != UI_CMD_PHASE_PENDING) {
        printf("  FAIL 35b2: Active low Purifier did not command on frame 3\n");
        test35_pass = false;
    }
    /* On active low: logical ON commands GPIO 0 */
    if (lora_hub_cmd_get_relay_gpio(&lora_hub_cmd, 0) != 0) {
        printf("  FAIL 35b2: Active low Purifier ON wire GPIO expected 0, got %d\n",
               lora_hub_cmd_get_relay_gpio(&lora_hub_cmd, 0));
        test35_pass = false;
    }

    /* -------------------------------------------------------------
     * Part C: Humidifier Thresholds (RH <= 40% ON, >= 50% OFF)
     * ------------------------------------------------------------- */
    /* Device 2: Humidifier. Relay currently OFF. */
    ui_hub_device_config_t dev_hum;
    ui_auto_get_defaults(2, UI_PRESET_HUMIDIFIER, &dev_hum);
    dev_hum.mode = UI_MODE_AUTO;
    dev_hum.active_low = 0;
    ui_set_device_config(2, &dev_hum);
    sim_post_node_status(true, 500, true, 50, true, 230, true, 60, true, 0, 0, 0, 0);
    ui_test_reset_device_commands();
    lv_tick_inc(12000u);
    advance_ui(50u);

    /* (c1) High RH = 55% while OFF: correctly remains OFF */
    for (int k = 0; k < 4; k++) {
        sim_post_node_status(true, 500, true, 50, true, 230, true, 55, true, 0, 0, 0, 0);
        advance_ui(50u);
    }
    if (h35->device_cmds[2].phase != UI_CMD_PHASE_IDLE) {
        printf("  FAIL 35c: Humidifier fired while RH=55%% (expected IDLE, got %d)\n", h35->device_cmds[2].phase);
        test35_pass = false;
    }

    /* (c2) Deadband RH = 45%: remains OFF */
    for (int k = 0; k < 4; k++) {
        sim_post_node_status(true, 500, true, 50, true, 230, true, 45, true, 0, 0, 0, 0);
        advance_ui(50u);
    }
    if (h35->device_cmds[2].phase != UI_CMD_PHASE_IDLE) {
        printf("  FAIL 35c: Humidifier fired in deadband RH=45%%\n");
        test35_pass = false;
    }

    /* (c3) Dry RH = 35% (<= 40% on_thresh): 3 frames triggers ON command */
    sim_post_node_status(true, 500, true, 50, true, 230, true, 35, true, 0, 0, 0, 0);
    advance_ui(50u);
    sim_post_node_status(true, 500, true, 50, true, 230, true, 35, true, 0, 0, 0, 0);
    advance_ui(50u);
    sim_post_node_status(true, 500, true, 50, true, 230, true, 35, true, 0, 0, 0, 0);
    advance_ui(50u);
    if (h35->device_cmds[2].phase != UI_CMD_PHASE_PENDING) {
        printf("  FAIL 35c: Humidifier did NOT fire on dry RH=35%% (expected PENDING, got %d)\n",
               h35->device_cmds[2].phase);
        test35_pass = false;
    }
    if (lora_hub_cmd_get_relay_gpio(&lora_hub_cmd, 2) != 1) {
        printf("  FAIL 35c: Humidifier ON wire level expected 1, got %d\n",
               lora_hub_cmd_get_relay_gpio(&lora_hub_cmd, 2));
        test35_pass = false;
    }

    /* (c4) Boundary tests: RH=40% (exact threshold) -> triggers ON */
    /* Confirm previous command to settled ON */
    lora_node_status.bits.seq_echo = lora_hub_cmd.bits.seq;
    lora_node_status_set_relay_gpio(&lora_node_status, 2, 1);
    lora_rx_revision++;
    advance_ui(50u);
    /* Hold 10s */
    lv_tick_inc(10000u);
    advance_ui(50u);
    /* Test OFF at boundary RH = 50% */
    sim_post_node_status(true, 500, true, 50, true, 230, true, 50, true, 0, 0, 1, 0);
    advance_ui(50u);
    sim_post_node_status(true, 500, true, 50, true, 230, true, 50, true, 0, 0, 1, 0);
    advance_ui(50u);
    sim_post_node_status(true, 500, true, 50, true, 230, true, 50, true, 0, 0, 1, 0);
    advance_ui(50u);
    if (h35->device_cmds[2].phase != UI_CMD_PHASE_PENDING) {
        printf("  FAIL 35c4: Humidifier did NOT fire OFF on boundary RH=50%%\n");
        test35_pass = false;
    }

    /* -------------------------------------------------------------
     * Part E: Serialized Live-App RX -> Tick -> TX -> Echo Observation
     * ------------------------------------------------------------- */
    /* Reset and set Fan in Auto mode, stable OFF */
    ui_auto_get_defaults(1, UI_PRESET_FAN, &dev_fan);
    dev_fan.mode = UI_MODE_AUTO;
    dev_fan.active_low = 0;
    ui_set_device_config(1, &dev_fan);
    sim_post_node_status(true, 500, true, 50, true, 230, true, 45, true, 0, 0, 0, 0);
    ui_test_reset_device_commands();
    lv_tick_inc(12000u);
    advance_ui(50u);

    uint16_t loop_tx_seq = 0;
    bool loop_cmd_detected = false;
    for (int step = 1; step <= 3; step++) {
        /* 1. Radio driver publishes complete status into lora_node_status */
        lora_node_status.bits.co2_ppm = 1250; /* Above threshold */
        lora_node_status.bits.co2_valid = 1;
        lora_node_status_set_relay_gpio(&lora_node_status, 1, 0);
        /* 2. Update lora_last_rx_tick_ms and increment lora_rx_revision once */
        lora_last_rx_tick_ms = lv_tick_get();
        lora_node_status.bits.sample_seq++;
        lora_rx_revision++;
        /* 3. Call ui_tick in the serialized application context */
        advance_ui(10u);
        /* 4. Transmit lora_hub_cmd if pending command observed */
        if (h35->device_cmds[1].phase == UI_CMD_PHASE_PENDING) {
            loop_cmd_detected = true;
            loop_tx_seq = lora_hub_cmd.bits.seq;
            /* 5. Transmit packet over radio to node... Node echoes in next frame */
            lora_node_status.bits.seq_echo = loop_tx_seq;
            lora_node_status_set_relay_gpio(&lora_node_status, 1, 1);
            lora_last_rx_tick_ms = lv_tick_get();
            lora_node_status.bits.sample_seq++;
            lora_rx_revision++;
            advance_ui(10u);
            break;
        }
    }
    if (!loop_cmd_detected || h35->device_cmds[1].phase != UI_CMD_PHASE_IDLE) {
        printf("  FAIL 35e: Serialized RX->Tick->TX->Echo sequence failed (detected=%d, phase=%d)\n",
               loop_cmd_detected, h35->device_cmds[1].phase);
        test35_pass = false;
    }

    /* Clean up baseline */
    ui_test_reset_device_commands();
    ui_test_reset_device_configs();
    populate_node_status_baseline();
    feed_standard_history();
    advance_ui(500u);

    if (test35_pass) {
        printf("  PASS: Autonomous Trends session history & live Auto triggering verified.\n");
        passed_tests++;
    }

    /* -------------------------------------------------------------
     * [CHECK 36] Long Outage Contiguous Gap Window Spacing (P2.1)
     * ------------------------------------------------------------- */
    total_tests++;
    printf("\n[CHECK 36] Long Outage Contiguous Gap Window Spacing (P2.1)...\n");
    bool test36_pass = true;
    const ui_test_hooks_t *h36 = ui_get_test_hooks();

    /* Clean reset */
    ui_test_reset_history();
    ui_set_uptime_seconds(0);
    ui_navigate_to_page(UI_PAGE_TRENDS);
    advance_ui(50u);

    /* Minute 0: Initial sample CO2 = 600 ppm */
    sim_post_node_status(true, 600, true, 100, true, 220, true, 50, true, 0, 0, 0, 0);
    advance_ui(50u);

    if (h36->history_count[UI_METRIC_CO2] != 1) {
        printf("  FAIL 36: Expected CO2 history count 1 at min 0, got %d\n", (int)h36->history_count[UI_METRIC_CO2]);
        test36_pass = false;
    }

    /* Simulate a 100-minute telemetry outage: last sample was min 0, next packet arrives at session minute 100 */
    lv_tick_inc(100u * 60u * 1000u);
    sim_post_node_status(true, 650, true, 120, true, 225, true, 48, true, 0, 0, 0, 0);
    advance_ui(50u);

    if (h36->history_count[UI_METRIC_CO2] != 16) {
        printf("  FAIL 36: Expected CO2 history count 16 after 100m recovery, got %d\n", (int)h36->history_count[UI_METRIC_CO2]);
        test36_pass = false;
    } else {
        /* Verify newest contiguous 16-minute window: slots 0..15 correspond to minutes 85..100 */
        const ui_history_sample_t *s_first = ui_get_history_sample(UI_METRIC_CO2, 0);
        const ui_history_sample_t *s_second = ui_get_history_sample(UI_METRIC_CO2, 1);
        const ui_history_sample_t *s_penult = ui_get_history_sample(UI_METRIC_CO2, 14);
        const ui_history_sample_t *s_last = ui_get_history_sample(UI_METRIC_CO2, 15);

        if (!s_first || s_first->minute_of_day != 85 || s_first->valid) {
            printf("  FAIL 36: Slot 0 expected min=85, valid=0; got min=%u, valid=%d\n",
                   s_first ? s_first->minute_of_day : 0, s_first ? s_first->valid : -1);
            test36_pass = false;
        }
        if (!s_second || s_second->minute_of_day != 86 || s_second->valid) {
            printf("  FAIL 36: Slot 1 expected min=86, valid=0; got min=%u, valid=%d\n",
                   s_second ? s_second->minute_of_day : 0, s_second ? s_second->valid : -1);
            test36_pass = false;
        }
        if (!s_penult || s_penult->minute_of_day != 99 || s_penult->valid) {
            printf("  FAIL 36: Slot 14 expected min=99, valid=0; got min=%u, valid=%d\n",
                   s_penult ? s_penult->minute_of_day : 0, s_penult ? s_penult->valid : -1);
            test36_pass = false;
        }
        if (!s_last || s_last->minute_of_day != 100 || !s_last->valid || s_last->value != 650) {
            printf("  FAIL 36: Slot 15 expected min=100, valid=1, val=650; got min=%u, valid=%d, val=%ld\n",
                   s_last ? s_last->minute_of_day : 0, s_last ? s_last->valid : -1, s_last ? (long)s_last->value : 0);
            test36_pass = false;
        }

        /* Check contiguous 1-minute slot spacing across all 16 slots */
        for (uint16_t idx = 0; idx < 16; idx++) {
            const ui_history_sample_t *s = ui_get_history_sample(UI_METRIC_CO2, idx);
            if (!s || s->minute_of_day != (uint16_t)(85 + idx)) {
                printf("  FAIL 36: Slot %u minute expected %u, got %u\n", idx, 85 + idx, s ? s->minute_of_day : 0);
                test36_pass = false;
                break;
            }
        }
    }

    if (test36_pass) {
        printf("  PASS: Long outage contiguous 16-minute window spacing verified (slots 85..100).\n");
        passed_tests++;
    }

    /* -------------------------------------------------------------
     * [CHECK 37] 24h+ Uptime Monotonic Time & External Rollover (P2.2 / B3)
     * ------------------------------------------------------------- */
    total_tests++;
    printf("\n[CHECK 37] 24h+ Uptime Monotonic Time & External Rollover (P2.2 / B3)...\n");
    bool test37_pass = true;

    /* (a) Autonomous session at 24h (1440 min) and 100h (6000 min): fixed window labels */
    ui_test_reset_history();
    ui_navigate_to_page(UI_PAGE_TRENDS);
    ui_select_trend_metric(UI_METRIC_TEMP);
    advance_ui(50u);
    ui_set_uptime_seconds(1439u * 60u);
    sim_post_node_status(true, 500, true, 50, true, 230, true, 50, true, 0, 0, 0, 0);
    advance_ui(50u);
    ui_set_uptime_seconds(1440u * 60u);
    sim_post_node_status(true, 500, true, 50, true, 235, true, 50, true, 0, 0, 0, 0);
    advance_ui(UI_TRENDS_SETTLE_MS + 50u);
    const char *lbl_24h_0 = ui_get_trends_time_lbl_str(0);
    const char *lbl_24h_3 = ui_get_trends_time_lbl_str(3);
    if (!lbl_24h_0 || strcmp(lbl_24h_0, "-15 min") != 0 || !lbl_24h_3 || strcmp(lbl_24h_3, "Now") != 0) {
        printf("  FAIL 37a: Autonomous 24h labels expected '-15 min' and 'Now', got '%s' and '%s'\n",
               lbl_24h_0 ? lbl_24h_0 : "NULL", lbl_24h_3 ? lbl_24h_3 : "NULL");
        test37_pass = false;
    }

    ui_test_reset_history();
    ui_set_uptime_seconds(5999u * 60u);
    sim_post_node_status(true, 500, true, 50, true, 230, true, 50, true, 0, 0, 0, 0);
    advance_ui(50u);
    ui_set_uptime_seconds(6000u * 60u);
    sim_post_node_status(true, 500, true, 50, true, 235, true, 50, true, 0, 0, 0, 0);
    advance_ui(UI_TRENDS_SETTLE_MS + 50u);
    const char *lbl_100h_0 = ui_get_trends_time_lbl_str(0);
    const char *lbl_100h_3 = ui_get_trends_time_lbl_str(3);
    if (!lbl_100h_0 || strcmp(lbl_100h_0, "-15 min") != 0 || !lbl_100h_3 || strcmp(lbl_100h_3, "Now") != 0) {
        printf("  FAIL 37a: Autonomous 100h labels expected '-15 min' and 'Now', got '%s' and '%s'\n",
               lbl_100h_0 ? lbl_100h_0 : "NULL", lbl_100h_3 ? lbl_100h_3 : "NULL");
        test37_pass = false;
    }

    /* (b) External feed wall-clock rollover (23:58 .. 00:01) */
    int32_t ext_pts[4] = { 200, 205, 210, 215 };
    bool ext_vld[4] = { true, true, true, true };
    ui_feed_history(UI_METRIC_VOC, ext_pts, ext_vld, 4, 1438);
    ui_select_trend_metric(UI_METRIC_VOC);
    advance_ui(UI_TRENDS_SETTLE_MS + 50u);
    const char *ext_0 = ui_get_trends_time_lbl_str(0);
    const char *ext_1 = ui_get_trends_time_lbl_str(1);
    const char *ext_2 = ui_get_trends_time_lbl_str(2);
    const char *ext_3 = ui_get_trends_time_lbl_str(3);
    if (!ext_0 || strcmp(ext_0, "23:46") != 0 ||
        !ext_1 || strcmp(ext_1, "23:51") != 0 ||
        !ext_2 || strcmp(ext_2, "23:56") != 0 ||
        !ext_3 || strcmp(ext_3, "00:01") != 0) {
        printf("  FAIL 37b: External rollover labels expected '23:46', '23:51', '23:56', '00:01', got '%s', '%s', '%s', '%s'\n",
               ext_0 ? ext_0 : "NULL", ext_1 ? ext_1 : "NULL", ext_2 ? ext_2 : "NULL", ext_3 ? ext_3 : "NULL");
        test37_pass = false;
    }

    /* (c) uint32 session rollover past 65,535 min */
    ui_test_reset_history();
    ui_navigate_to_page(UI_PAGE_TRENDS);
    ui_select_trend_metric(UI_METRIC_CO2);
    advance_ui(50u);

    ui_set_uptime_seconds(65535u * 60u);
    sim_post_node_status(true, 620, true, 50, true, 220, true, 50, true, 0, 0, 0, 0);
    advance_ui(50u);

    ui_set_uptime_seconds(65536u * 60u);
    sim_post_node_status(true, 640, true, 50, true, 220, true, 50, true, 0, 0, 0, 0);
    advance_ui(50u);

    const ui_history_sample_t *samp_65535 = ui_get_history_sample(UI_METRIC_CO2, 0);
    const ui_history_sample_t *samp_65536 = ui_get_history_sample(UI_METRIC_CO2, 1);
    if (!samp_65535 || samp_65535->minute_of_day != 65535u ||
        !samp_65536 || samp_65536->minute_of_day != 65536u) {
        printf("  FAIL 37c: History sample minutes past 65535 mismatch (s0=%u, s1=%u)\n",
               samp_65535 ? samp_65535->minute_of_day : 0, samp_65536 ? samp_65536->minute_of_day : 0);
        test37_pass = false;
    }
    const char *lbl_roll_0 = ui_get_trends_time_lbl_str(0);
    const char *lbl_roll_3 = ui_get_trends_time_lbl_str(3);
    if (!lbl_roll_0 || strcmp(lbl_roll_0, "-15 min") != 0 || !lbl_roll_3 || strcmp(lbl_roll_3, "Now") != 0) {
        printf("  FAIL 37c: Autonomous labels across 65536 rollover expected '-15 min' and 'Now', got '%s' and '%s'\n",
               lbl_roll_0 ? lbl_roll_0 : "NULL", lbl_roll_3 ? lbl_roll_3 : "NULL");
        test37_pass = false;
    }

    if (test37_pass) {
        printf("  PASS: 24h+ monotonic session time, 65536 min rollover and external wall-clock rollover verified.\n");
        passed_tests++;
    }

    /* -------------------------------------------------------------
     * [CHECK 38] Ownership Transition with Equal-Sample Feed (P2.3)
     * ------------------------------------------------------------- */
    total_tests++;
    printf("\n[CHECK 38] Ownership Transition with Equal-Sample Feed (P2.3)...\n");
    bool test38_pass = true;
    const ui_test_hooks_t *h38 = ui_get_test_hooks();

    ui_test_reset_history();
    ui_navigate_to_page(UI_PAGE_TRENDS);
    ui_select_trend_metric(UI_METRIC_CO2);
    advance_ui(50u);

    /* Initial autonomous point at minute 10 with value 600 */
    ui_set_uptime_seconds(9u * 60u);
    sim_post_node_status(true, 580, true, 50, true, 220, true, 50, true, 0, 0, 0, 0);
    advance_ui(50u);
    ui_set_uptime_seconds(10u * 60u);
    sim_post_node_status(true, 600, true, 50, true, 220, true, 50, true, 0, 0, 0, 0);
    advance_ui(UI_TRENDS_SETTLE_MS + 50u);

    /* Verify autonomous axis shows relative '-15 min' and 'Now' */
    const char *ax0 = ui_get_trends_time_lbl_str(0);
    const char *ax3 = ui_get_trends_time_lbl_str(3);
    if (!ax0 || strcmp(ax0, "-15 min") != 0 || !ax3 || strcmp(ax3, "Now") != 0) {
        printf("  FAIL 38: Initial autonomous axis label expected '-15 min'/'Now', got '%s'/'%s'\n",
               ax0 ? ax0 : "NULL", ax3 ? ax3 : "NULL");
        test38_pass = false;
    }
    if (h38->history_externally_fed && h38->history_externally_fed[UI_METRIC_CO2]) {
        printf("  FAIL 38: Metric CO2 should NOT be marked externally fed yet\n");
        test38_pass = false;
    }
    uint32_t rev_before = h38->history_rev ? h38->history_rev[UI_METRIC_CO2] : 0;

    /* Feed identical samples from external feed: values 580 and 600, valid true, minutes 9 and 10 */
    int32_t eq_val[2] = { 580, 600 };
    bool eq_vld[2] = { true, true };
    ui_feed_history(UI_METRIC_CO2, eq_val, eq_vld, 2, 9);
    advance_ui(UI_TRENDS_SETTLE_MS + 50u);

    /* Assert ownership changed to external */
    if (h38->history_externally_fed && !h38->history_externally_fed[UI_METRIC_CO2]) {
        printf("  FAIL 38: Metric CO2 not marked externally fed after ui_feed_history\n");
        test38_pass = false;
    }
    /* Assert revision incremented */
    if (h38->history_rev && h38->history_rev[UI_METRIC_CO2] <= rev_before) {
        printf("  FAIL 38: History revision did NOT increment on ownership change (before=%u, after=%u)\n",
               (unsigned)rev_before, (unsigned)h38->history_rev[UI_METRIC_CO2]);
        test38_pass = false;
    }
    /* Assert axis updated to wall-clock '23:55' .. '00:10' (newest sample at 00:10 -> slot 15 is 00:10, slot 0 is 23:55) */
    const char *ax1_0 = ui_get_trends_time_lbl_str(0);
    const char *ax1_3 = ui_get_trends_time_lbl_str(3);
    if (!ax1_0 || strcmp(ax1_0, "23:55") != 0 || !ax1_3 || strcmp(ax1_3, "00:10") != 0) {
        printf("  FAIL 38: Axis labels expected '23:55' and '00:10' after ownership change, got '%s' and '%s'\n",
               ax1_0 ? ax1_0 : "NULL", ax1_3 ? ax1_3 : "NULL");
        test38_pass = false;
    }

    /* Refeeding identical external data again MUST be a strict no-op */
    uint32_t rev_after_first = h38->history_rev ? h38->history_rev[UI_METRIC_CO2] : 0;
    reset_flush_counters();
    ui_feed_history(UI_METRIC_CO2, eq_val, eq_vld, 2, 9);
    advance_ui(UI_TRENDS_SETTLE_MS + 50u);

    if (h38->history_rev && h38->history_rev[UI_METRIC_CO2] != rev_after_first) {
        printf("  FAIL 38: Second identical external feed incremented revision (expected %u, got %u)\n",
               (unsigned)rev_after_first, (unsigned)h38->history_rev[UI_METRIC_CO2]);
        test38_pass = false;
    }

    if (test38_pass) {
        printf("  PASS: Ownership transition with equal-sample feed verified.\n");
        passed_tests++;
    }

    /* -------------------------------------------------------------
     * [CHECK 39] Trends Metric Switching Latency & Redraw Benchmark (P1)
     * ------------------------------------------------------------- */
    total_tests++;
    printf("\n[CHECK 39] Trends Metric Switching Latency & Redraw Benchmark (P1)...\n");
    bool test39_pass = true;

    /* Populate 16 points for each metric */
    feed_standard_history();
    ui_navigate_to_page(UI_PAGE_TRENDS);
    advance_ui(50u);

    /* 1. Reselecting same metric MUST flush 0 pixels */
    ui_select_trend_metric(UI_METRIC_CO2);
    advance_ui(50u);
    reset_flush_counters();
    ui_select_trend_metric(UI_METRIC_CO2);
    advance_ui(50u);
    if (s_flush_calls != 0 || s_flushed_pixels != 0) {
        printf("  FAIL 39: Reselecting same metric produced flushes (calls=%llu, px=%llu)\n",
               (unsigned long long)s_flush_calls, (unsigned long long)s_flushed_pixels);
        test39_pass = false;
    }

    /* 2. Single metric switch regression budget: flushes <= 26, pixels <= 158,000 */
    reset_flush_counters();
    ui_select_trend_metric(UI_METRIC_VOC);
    advance_ui(50u);
    if (s_flush_calls > 26 || s_flushed_pixels > 158000) {
        printf("  FAIL 39: Single metric switch exceeded budget (calls=%llu > 26, px=%llu > 158000)\n",
               (unsigned long long)s_flush_calls, (unsigned long long)s_flushed_pixels);
        test39_pass = false;
    }

    /* 3. Rapid metric switching benchmark: 100 alternating selections */
    for (int m = 0; m < UI_METRIC_COUNT; m++) {
        ui_select_trend_metric((ui_metric_id_t)m);
        advance_ui(20u);
    }
    ui_select_trend_metric(UI_METRIC_CO2);
    advance_ui(50u);

    lv_mem_monitor_t mem_before, mem_after;
    lv_mem_monitor(&mem_before);
    reset_flush_counters();

    for (int i = 0; i < 100; i++) {
        ui_select_trend_metric((ui_metric_id_t)(i % UI_METRIC_COUNT));
        advance_ui(5u);
    }
    advance_ui(50u);
    lv_mem_monitor(&mem_after);

    printf("  (Benchmark) 100 metric switches: %llu flushes, %llu pixels pushed, heap_free: %u -> %u\n",
           (unsigned long long)s_flush_calls, (unsigned long long)s_flushed_pixels,
           (unsigned)mem_before.free_size, (unsigned)mem_after.free_size);

    /* 100-switch CI budget assertions: flushes <= 2550 (was 2623), pixels <= 15300000 (was 15656679) */
    if (s_flush_calls > 2550 || s_flushed_pixels > 15300000) {
        printf("  FAIL 39: 100 metric switches exceeded budget (calls=%llu > 2550, px=%llu > 15300000)\n",
               (unsigned long long)s_flush_calls, (unsigned long long)s_flushed_pixels);
        test39_pass = false;
    }

    /* Memory stability check: no periodic heap churn / leak (zero decrease) */
    if (mem_after.free_size < mem_before.free_size) {
        printf("  FAIL 39: Heap decreased after 100 switches (before=%u, after=%u)\n",
               (unsigned)mem_before.free_size, (unsigned)mem_after.free_size);
        test39_pass = false;
    }

    /* 4. Settled idle rate MUST be 0 flushes */
    advance_ui(2500u);
    reset_flush_counters();
    advance_ui(500u);
    if (s_flush_calls != 0 || s_flushed_pixels != 0) {
        printf("  FAIL 39: Trends settled idle produced flushes (calls=%llu, px=%llu)\n",
               (unsigned long long)s_flush_calls, (unsigned long long)s_flushed_pixels);
        test39_pass = false;
    }

    /* 5. Two-Stage Tactile Response & Interim Title Consistency on Normal Schedule */
    const ui_test_hooks_t *h39 = ui_get_test_hooks();
    ui_select_trend_metric(UI_METRIC_CO2);
    advance_ui(50u);

    if (!h39 || !h39->trigger_metric_tab_click || !h39->pending_trend_chart ||
        !h39->stage1_flush_pending || !h39->trends_title_lbl || !(*h39->trends_title_lbl)) {
        printf("  FAIL 39: Required test hooks or label/widget pointers missing or NULL!\n");
        test39_pass = false;
    } else {
        /* Initial state assertion: active metric is CO2, title is "eCO₂" */
        const char *title_init = ui_get_trends_title_str();
        if (!title_init || strcmp(title_init, "eCO₂") != 0) {
            printf("  FAIL 39: Initial Trends title mismatch: '%s' (expected 'eCO₂')\n", title_init ? title_init : "NULL");
            test39_pass = false;
        }

        reset_flush_counters();
        /* Trigger touch on VOC card */
        h39->trigger_metric_tab_click(UI_METRIC_VOC);

        /* Stage 1 render: immediate tactile feedback via lv_timer_handler() */
        lv_timer_handler();

        uint64_t stage1_flushes = s_flush_calls;
        uint64_t stage1_pixels = s_flushed_pixels;
        printf("  (Two-Stage) Stage 1 (Tactile Card): %llu flushes, %llu pixels, pending_chart=%d\n",
               (unsigned long long)stage1_flushes, (unsigned long long)stage1_pixels,
               *h39->pending_trend_chart);

        /* Stage 1 MUST produce POSITIVE flushes and pixels, strictly confined to tab rail (<= 6 flushes, <= 35,000 px) */
        if (stage1_flushes == 0 || stage1_pixels == 0) {
            printf("  FAIL 39: Stage 1 produced ZERO flushes/pixels (calls=%llu, px=%llu) - tactile feedback not rendered!\n",
                   (unsigned long long)stage1_flushes, (unsigned long long)stage1_pixels);
            test39_pass = false;
        } else if (stage1_flushes > 6 || stage1_pixels > 35000) {
            printf("  FAIL 39: Stage 1 tactile response exceeded tab budget (calls=%llu > 6, px=%llu > 35000)\n",
                   (unsigned long long)stage1_flushes, (unsigned long long)stage1_pixels);
            test39_pass = false;
        }

        if (!(*h39->pending_trend_chart) || !(*h39->stage1_flush_pending)) {
            printf("  FAIL 39: Stage 1 failed to defer chart rendering or mark stage1_flush_pending\n");
            test39_pass = false;
        }

        /* Interim Title Consistency: While Stage 1 is flushed and chart is pending, title MUST still say "eCO₂" (NOT "TVOC") */
        const char *title_interim = ui_get_trends_title_str();
        if (!title_interim || strcmp(title_interim, "eCO₂") != 0) {
            printf("  FAIL 39: Interim title desynchronized with old plot! Got '%s' (expected 'eCO₂')\n",
                   title_interim ? title_interim : "NULL");
            test39_pass = false;
        }

        /* Stage 2 render: ui_tick() processes pending chart update */
        reset_flush_counters();
        advance_ui(UI_TRENDS_SETTLE_MS + 50u);

        uint64_t stage2_flushes = s_flush_calls;
        uint64_t stage2_pixels = s_flushed_pixels;
        printf("  (Two-Stage) Stage 2 (Chart Render): %llu flushes, %llu pixels, pending_chart=%d\n",
               (unsigned long long)stage2_flushes, (unsigned long long)stage2_pixels,
               *h39->pending_trend_chart);

        if (*h39->pending_trend_chart) {
            printf("  FAIL 39: Stage 2 did not clear pending_trend_chart flag\n");
            test39_pass = false;
        }
        if (stage2_flushes == 0 || stage2_pixels == 0) {
            printf("  FAIL 39: Stage 2 failed to render deferred chart\n");
            test39_pass = false;
        }

        /* Final Title & Unit Consistency: Title and unit now match VOC */
        const char *title_final = ui_get_trends_title_str();
        if (!title_final || strcmp(title_final, "TVOC") != 0) {
            printf("  FAIL 39: Final title mismatch: '%s' (expected 'TVOC')\n", title_final ? title_final : "NULL");
            test39_pass = false;
        }
        const char *unit_final = ui_get_trends_unit_str();
        if (!unit_final || strcmp(unit_final, "ppb") != 0) {
            printf("  FAIL 39: Final unit mismatch: '%s' (expected 'ppb')\n", unit_final ? unit_final : "NULL");
            test39_pass = false;
        }
    }

    /* 6. Delayed Asynchronous Flush Simulation (QSPI DMA in-flight) */
    if (!h39 || !h39->trigger_metric_tab_click || !h39->pending_trend_chart ||
        !h39->stage1_flush_pending || !h39->trends_title_lbl || !(*h39->trends_title_lbl)) {
        printf("  FAIL 39: Required test hooks missing or NULL in async flush simulation!\n");
        test39_pass = false;
    } else {
        ui_select_trend_metric(UI_METRIC_CO2);
        advance_ui(50u);

        /* Enable delayed async flush callback */
        s_sim_async_flush_delay = true;
        h39->trigger_metric_tab_click(UI_METRIC_TEMP);

        /* Trigger Stage 1 flush - lv_disp_flush_ready is NOT called */
        lv_timer_handler();

        /* Now simulate firmware calling ui_tick() while DMA transfer is still in flight */
        ui_tick();

        /* Assert Stage 2 did NOT execute prematurely */
        if (!(*h39->pending_trend_chart) || !(*h39->stage1_flush_pending)) {
            printf("  FAIL 39: ui_tick() prematurely executed Stage 2 while async flush/DMA was in-flight!\n");
            test39_pass = false;
        }

        /* Now simulate DMA transfer completion interrupt */
        sim_async_flush_complete();
        s_sim_async_flush_delay = false;

        /* Advance UI: Stage 2 can now execute cleanly */
        advance_ui(UI_TRENDS_SETTLE_MS + 50u);
        if (*h39->pending_trend_chart) {
            printf("  FAIL 39: Stage 2 failed to execute after async flush completion\n");
            test39_pass = false;
        }
        const char *title_temp = ui_get_trends_title_str();
        if (!title_temp || strcmp(title_temp, "Temperature") != 0) {
            printf("  FAIL 39: Async flush title mismatch: '%s' (expected 'Temperature')\n", title_temp ? title_temp : "NULL");
            test39_pass = false;
        }
    }

    /* 7. Staged Interaction Edge Cases */
    if (!h39 || !h39->trigger_metric_tab_click || !h39->pending_trend_chart ||
        !h39->stage1_flush_pending) {
        printf("  FAIL 39: Required test hooks missing or NULL in interaction edge cases!\n");
        test39_pass = false;
    } else {
        /* Case 7a: New Telemetry Frame Arrival during pending Stage 1 */
        ui_select_trend_metric(UI_METRIC_CO2);
        advance_ui(50u);
        h39->trigger_metric_tab_click(UI_METRIC_HUMIDITY);
        /* Telemetry frame arrives while chart is pending */
        sim_post_node_status(true, 430, true, 86, true, 236, true, 49, true, 0, 0, 0, 0);
        advance_ui(UI_TRENDS_SETTLE_MS + 50u);
        if (*h39->pending_trend_chart) {
            printf("  FAIL 39: Telemetry frame arrival jammed pending chart\n");
            test39_pass = false;
        }
        const char *u = ui_get_trends_unit_str();
        if (!u || strcmp(u, "%") != 0) {
            printf("  FAIL 39: Unit mismatch after telemetry arrival: '%s' (expected '%%')\n", u ? u : "NULL");
            test39_pass = false;
        }

        /* Case 7b: Link loss & recovery during pending Stage 1 */
        ui_select_trend_metric(UI_METRIC_CO2);
        advance_ui(50u);
        h39->trigger_metric_tab_click(UI_METRIC_VOC);
        /* Link drops */
        lora_last_rx_tick_ms = lv_tick_get() - 4000u;
        advance_ui(UI_TRENDS_SETTLE_MS + 50u);
        if (*h39->pending_trend_chart) {
            printf("  FAIL 39: Link loss jammed pending chart\n");
            test39_pass = false;
        }
        /* Link recovers */
        lora_last_rx_tick_ms = lv_tick_get();
        advance_ui(50u);

        /* Case 7c: Page exit and re-entry during pending Stage 1 */
        ui_select_trend_metric(UI_METRIC_CO2);
        advance_ui(50u);
        h39->trigger_metric_tab_click(UI_METRIC_TEMP);
        /* Navigate away immediately before ui_tick processes stage 2 */
        ui_navigate_to_page(UI_PAGE_HOME);
        advance_ui(50u);
        if (*h39->pending_trend_chart) {
            printf("  FAIL 39: Page exit to Home did not clear pending_trend_chart!\n");
            test39_pass = false;
        }
        /* Navigate back to Trends */
        ui_navigate_to_page(UI_PAGE_TRENDS);
        advance_ui(50u);
        if (*h39->pending_trend_chart) {
            printf("  FAIL 39: Re-entering Trends left spurious pending_trend_chart!\n");
            test39_pass = false;
        }
    }

    /* 8. Rapid Tap Request Coalescing (latest tap wins; intermediate charts skipped) */
    if (!h39 || !h39->trigger_metric_tab_click || !h39->pending_trend_chart ||
        !h39->stage1_flush_pending) {
        printf("  FAIL 39: Required test hooks missing or NULL in tap coalescing!\n");
        test39_pass = false;
    } else {
        ui_select_trend_metric(UI_METRIC_CO2);
        advance_ui(50u);

        reset_flush_counters();
        /* Rapid burst of taps without ui_tick: VOC -> TEMP -> HUMIDITY */
        h39->trigger_metric_tab_click(UI_METRIC_VOC);
        lv_timer_handler(); /* tactile highlight VOC */
        h39->trigger_metric_tab_click(UI_METRIC_TEMP);
        lv_timer_handler(); /* tactile highlight TEMP */
        h39->trigger_metric_tab_click(UI_METRIC_HUMIDITY);
        lv_timer_handler(); /* tactile highlight HUMIDITY */

        /* All 3 tab clicks coalesced: only HUMIDITY is pending */
        if (!(*h39->pending_trend_chart)) {
            printf("  FAIL 39: Burst taps did not leave chart pending\n");
            test39_pass = false;
        }

        /* Now process ui_tick() - exactly ONE chart rasterization should occur */
        reset_flush_counters();
        advance_ui(UI_TRENDS_SETTLE_MS + 50u);

        /* Flushes must be <= single chart switch (32 flushes, 200,000 px) */
        if (s_flush_calls > 32 || s_flushed_pixels > 200000) {
            printf("  FAIL 39: Coalesced burst executed multiple chart redraws (calls=%llu, px=%llu)\n",
                   (unsigned long long)s_flush_calls, (unsigned long long)s_flushed_pixels);
            test39_pass = false;
        }

        /* Current unit must be % (Humidity), NOT VOC or Temp */
        const char *unit = ui_get_trends_unit_str();
        if (!unit || strcmp(unit, "%") != 0) {
            printf("  FAIL 39: Coalesced tap displayed stale unit '%s' (expected '%%')\n", unit ? unit : "NULL");
            test39_pass = false;
        }
    }

    /* 9. Declared Simulated QSPI Bandwidth Timings (20 MB/s baseline) */
    {
        double st1_ms = (34532.0 * 2.0) / (20.0 * 1000.0);
        double st2_ms = (113849.0 * 2.0) / (20.0 * 1000.0);
        printf("  (Simulated QSPI 20 MB/s Timing): Stage 1 = %.2f ms (69.1 KB), Stage 2 = %.2f ms (227.7 KB)\n",
               st1_ms, st2_ms);
        printf("  NOTE: Real target LCD touch-to-photon latency is UNVERIFIED on host simulator.\n");
    }

    if (test39_pass) {
        printf("  PASS: Trends metric switching latency, two-stage response, coalescing and stability verified.\n");
        passed_tests++;
    }

    /* -------------------------------------------------------------
     * [CHECK 40] Table-Driven Combined Sensor-Fault & Partial-Recovery Matrix (E2)
     * ------------------------------------------------------------- */
    total_tests++;
    printf("\n[CHECK 40] Table-Driven Combined Sensor-Fault & Partial-Recovery Matrix (E2)...\n");
    bool test40_pass = true;
    const ui_test_hooks_t *h40 = ui_get_test_hooks();

    ui_test_reset_history();
    ui_test_reset_device_commands();
    ui_dismiss_toast();

    /* Setup device configs:
     * Dev 0: Purifier (AUTO mode, VOC ON=150, OFF=100)
     * Dev 1: Fan (AUTO mode, CO2 ON=1000, OFF=800, active_low=1)
     * Dev 2: Humidifier (AUTO mode, Humid ON=40, OFF=55)
     */
    ui_hub_device_config_t dev0_cfg40;
    ui_auto_get_defaults(0, UI_PRESET_PURIFIER, &dev0_cfg40);
    dev0_cfg40.mode = UI_MODE_AUTO;
    dev0_cfg40.auto_on_thresh = 150;
    dev0_cfg40.auto_off_thresh = 100;
    ui_test_set_boot_staging(true);
    ui_set_device_config(0, &dev0_cfg40);
    ui_test_set_boot_staging(false);

    ui_hub_device_config_t dev1_cfg40;
    ui_auto_get_defaults(1, UI_PRESET_FAN, &dev1_cfg40);
    dev1_cfg40.mode = UI_MODE_AUTO;
    dev1_cfg40.auto_on_thresh = 1000;
    dev1_cfg40.auto_off_thresh = 800;
    dev1_cfg40.active_low = 1;
    ui_test_set_boot_staging(true);
    ui_set_device_config(1, &dev1_cfg40);
    ui_test_set_boot_staging(false);

    ui_hub_device_config_t dev2_cfg40;
    ui_auto_get_defaults(2, UI_PRESET_HUMIDIFIER, &dev2_cfg40);
    dev2_cfg40.mode = UI_MODE_AUTO;
    dev2_cfg40.auto_on_thresh = 40;
    dev2_cfg40.auto_off_thresh = 55;
    ui_test_set_boot_staging(true);
    ui_set_device_config(2, &dev2_cfg40);
    ui_test_set_boot_staging(false);

    typedef struct {
        const char *step_name;
        bool co2_avail;
        uint16_t co2_val;
        bool voc_avail;
        uint16_t voc_val;
        bool temp_avail;
        int16_t temp_val;
        bool hum_avail;
        uint16_t hum_val;
        uint8_t relay_gpios; /* bits 0..3 */

        uint32_t advance_ms;
        uint32_t uptime_add_sec;

        const char *expected_hero_status;

        struct {
            const char *expected_tab_val;
            bool check_sample;
            uint16_t sample_idx;
            bool expected_sample_valid;
        } trends[UI_METRIC_COUNT];

        struct {
            uint8_t dev_idx;
            const char *expected_live;
        } editors[3];
    } fault_matrix_step_t;

    static const fault_matrix_step_t matrix_steps[] = {
        /* Step 0: Baseline All Healthy */
        {
            .step_name = "Step 0: Baseline All Healthy",
            .co2_avail = true, .co2_val = 420,
            .voc_avail = true, .voc_val = 85,
            .temp_avail = true, .temp_val = 235,
            .hum_avail = true, .hum_val = 48,
            .relay_gpios = 0x02, /* Fan relay 1 = 1 -> logical OFF for active_low */
            .advance_ms = 100, .uptime_add_sec = 0,
            .expected_hero_status = "Good",
            .trends = {
                [UI_METRIC_CO2]      = { "420",  true, 0, true },
                [UI_METRIC_VOC]      = { "85",   true, 0, true },
                [UI_METRIC_TEMP]     = { "23.5", true, 0, true },
                [UI_METRIC_HUMIDITY] = { "48",   true, 0, true },
            },
            .editors = {
                { 0, "85" },   /* Purifier */
                { 1, "420" },  /* Fan */
                { 2, "48" },   /* Humidifier */
            }
        },
        /* Step 1: Combined Fault (CO2 timeout/invalid + VOC 60001 invalid; Temp/Humid valid) */
        {
            .step_name = "Step 1: Combined Fault (CO2 & VOC invalid, Temp & Humid valid)",
            .co2_avail = false, .co2_val = 420,
            .voc_avail = false, .voc_val = 60001,
            .temp_avail = true, .temp_val = 235,
            .hum_avail = true, .hum_val = 48,
            .relay_gpios = 0x02,
            .advance_ms = 100, .uptime_add_sec = 0,
            .expected_hero_status = "Unavailable",
            .trends = {
                [UI_METRIC_CO2]      = { "—",    false, 0, false },
                [UI_METRIC_VOC]      = { "—",    false, 0, false },
                [UI_METRIC_TEMP]     = { "23.5", true,  0, true },
                [UI_METRIC_HUMIDITY] = { "48",   true,  0, true },
            },
            .editors = {
                { 0, "No live data" }, /* Purifier */
                { 1, "No live data" }, /* Fan */
                { 2, "48" },           /* Humidifier */
            }
        },
        /* Step 2: 1-Min Autonomous Gap Progression while fault latched */
        {
            .step_name = "Step 2: 1-Min Autonomous Gap Progression",
            .co2_avail = false, .co2_val = 420,
            .voc_avail = false, .voc_val = 60001,
            .temp_avail = true, .temp_val = 235,
            .hum_avail = true, .hum_val = 48,
            .relay_gpios = 0x02,
            .advance_ms = 50, .uptime_add_sec = 60,
            .expected_hero_status = "Unavailable",
            .trends = {
                [UI_METRIC_CO2]      = { "—",    true, 1, false }, /* Gap sample! */
                [UI_METRIC_VOC]      = { "—",    true, 1, false }, /* Gap sample! */
                [UI_METRIC_TEMP]     = { "23.5", true, 1, true },  /* Retained sample! */
                [UI_METRIC_HUMIDITY] = { "48",   true, 1, true },  /* Retained sample! */
            },
            .editors = {
                { 0, "No live data" },
                { 1, "No live data" },
                { 2, "48" },
            }
        },
        /* Step 3: Partial Recovery: CO2 recovers to 1200 (> 1000 ON), VOC remains invalid */
        {
            .step_name = "Step 3: Partial Recovery (CO2=1200 valid, VOC invalid)",
            .co2_avail = true, .co2_val = 1200,
            .voc_avail = false, .voc_val = 60001,
            .temp_avail = true, .temp_val = 235,
            .hum_avail = true, .hum_val = 48,
            .relay_gpios = 0x02,
            .advance_ms = 50, .uptime_add_sec = 0,
            .expected_hero_status = "Unavailable",
            .trends = {
                [UI_METRIC_CO2]      = { "1200", false, 0, false },
                [UI_METRIC_VOC]      = { "—",    false, 0, false },
                [UI_METRIC_TEMP]     = { "23.5", false, 0, false },
                [UI_METRIC_HUMIDITY] = { "48",   false, 0, false },
            },
            .editors = {
                { 0, "No live data" },
                { 1, "1200" },
                { 2, "48" },
            }
        },
        /* Step 4: VOC recovers to valid zero baseline (VOC = 0, valid = true) */
        {
            .step_name = "Step 4: VOC Valid Zero Baseline (VOC=0 valid)",
            .co2_avail = true, .co2_val = 1200,
            .voc_avail = true, .voc_val = 0,
            .temp_avail = true, .temp_val = 235,
            .hum_avail = true, .hum_val = 48,
            .relay_gpios = 0x00,
            .advance_ms = 100, .uptime_add_sec = 0,
            .expected_hero_status = "!Unavailable", /* No longer unavailable */
            .trends = {
                [UI_METRIC_CO2]      = { "1200", false, 0, false },
                [UI_METRIC_VOC]      = { "0",    false, 0, false },
                [UI_METRIC_TEMP]     = { "23.5", false, 0, false },
                [UI_METRIC_HUMIDITY] = { "48",   false, 0, false },
            },
            .editors = {
                { 0, "0" },    /* Purifier live reading is "0" */
                { 1, "1200" }, /* Fan live reading is "1200" */
                { 2, "48" },   /* Humidifier live reading is "48" */
            }
        },
        /* Step 5: Full Recovery All Sensors Nominal */
        {
            .step_name = "Step 5: Full Recovery All Sensors Nominal",
            .co2_avail = true, .co2_val = 420,
            .voc_avail = true, .voc_val = 85,
            .temp_avail = true, .temp_val = 235,
            .hum_avail = true, .hum_val = 48,
            .relay_gpios = 0x00,
            .advance_ms = 100, .uptime_add_sec = 0,
            .expected_hero_status = "Good",
            .trends = {
                [UI_METRIC_CO2]      = { "420",  false, 0, false },
                [UI_METRIC_VOC]      = { "85",   false, 0, false },
                [UI_METRIC_TEMP]     = { "23.5", false, 0, false },
                [UI_METRIC_HUMIDITY] = { "48",   false, 0, false },
            },
            .editors = {
                { 0, "85" },
                { 1, "420" },
                { 2, "48" },
            }
        }
    };

    for (size_t s = 0; s < sizeof(matrix_steps)/sizeof(matrix_steps[0]); s++) {
        const fault_matrix_step_t *step = &matrix_steps[s];
        printf("  [%s] Executing matrix verification...\n", step->step_name);

        if (step->uptime_add_sec > 0) {
            lv_tick_inc(step->uptime_add_sec * 1000u);
        }

        sim_post_node_status(true, step->co2_val, step->co2_avail,
                             step->voc_val, step->voc_avail,
                             step->temp_val, step->temp_avail,
                             step->hum_val, step->hum_avail,
                             (step->relay_gpios & 1),
                             ((step->relay_gpios >> 1) & 1),
                             ((step->relay_gpios >> 2) & 1),
                             ((step->relay_gpios >> 3) & 1));
        advance_ui(step->advance_ms);

        /* 1. Assert Home Hero */
        ui_navigate_to_page(UI_PAGE_HOME);
        advance_ui(50u);
        lv_obj_t *title_lbl = find_visible_label(lv_scr_act(), "Air quality");
        lv_obj_t *hero_cont = title_lbl ? lv_obj_get_parent(title_lbl) : NULL;
        if (!hero_cont) {
            printf("  FAIL 40 [%s]: Home hero container not found\n", step->step_name);
            test40_pass = false;
        } else {
            if (step->expected_hero_status[0] == '!') {
                const char *unwanted = step->expected_hero_status + 1;
                if (find_visible_label(hero_cont, unwanted)) {
                    printf("  FAIL 40 [%s]: Home hero unexpectedly found label '%s'\n", step->step_name, unwanted);
                    test40_pass = false;
                }
            } else {
                if (!find_visible_label(hero_cont, step->expected_hero_status)) {
                    printf("  FAIL 40 [%s]: Home hero expected label '%s'\n", step->step_name, step->expected_hero_status);
                    test40_pass = false;
                }
            }
        }

        /* 2. Assert all 4 Trends metrics */
        ui_navigate_to_page(UI_PAGE_TRENDS);
        advance_ui(50u);
        for (int m = 0; m < UI_METRIC_COUNT; m++) {
            ui_select_trend_metric((ui_metric_id_t)m);
            advance_ui(50u);

            if (step->trends[m].expected_tab_val) {
                const char *cur = ui_get_trends_tab_val_str((ui_metric_id_t)m);
                if (!cur || strcmp(cur, step->trends[m].expected_tab_val) != 0) {
                    printf("  FAIL 40 [%s]: Metric %d Tab val '%s' (expected '%s')\n",
                           step->step_name, m, cur ? cur : "NULL", step->trends[m].expected_tab_val);
                    test40_pass = false;
                }
            }

            if (step->trends[m].check_sample) {
                const ui_history_sample_t *samp = ui_get_history_sample((ui_metric_id_t)m, step->trends[m].sample_idx);
                if (!samp) {
                    printf("  FAIL 40 [%s]: Metric %d sample %u is NULL\n",
                           step->step_name, m, step->trends[m].sample_idx);
                    test40_pass = false;
                } else if (samp->valid != step->trends[m].expected_sample_valid) {
                    printf("  FAIL 40 [%s]: Metric %d sample %u valid=%d (expected %d)\n",
                           step->step_name, m, step->trends[m].sample_idx, samp->valid, step->trends[m].expected_sample_valid);
                    test40_pass = false;
                }
            }
        }

        /* 3. Assert all 3 Auto editors with strictly scoped dialog_live_lbl check */
        for (int a = 0; a < 3; a++) {
            uint8_t dev_idx = step->editors[a].dev_idx;
            ui_open_device_settings(dev_idx);
            ui_dialog_open_limits();
            advance_ui(50u);

            if (!h40->dialog_live_lbl || !(*h40->dialog_live_lbl)) {
                printf("  FAIL 40 [%s]: dialog_live_lbl is NULL for dev %u\n", step->step_name, dev_idx);
                test40_pass = false;
            } else {
                const char *live_txt = lv_label_get_text(*h40->dialog_live_lbl);
                if (!live_txt || strcmp(live_txt, step->editors[a].expected_live) != 0) {
                    printf("  FAIL 40 [%s]: Dev %u Auto editor live label '%s' (expected '%s')\n",
                           step->step_name, dev_idx, live_txt ? live_txt : "NULL", step->editors[a].expected_live);
                    test40_pass = false;
                }
            }

            ui_close_device_settings();
            advance_ui(50u);
        }

        /* Special post-step verifications */
        if (s == 1) {
            /* Assert Auto logic dispatched NO command from invalid sensor input */
            static uint16_t s_seq_step1;
            s_seq_step1 = lora_hub_cmd.bits.seq;
            advance_ui(50u);
            if (lora_hub_cmd.bits.seq != s_seq_step1) {
                printf("  FAIL 40 [Step 1]: Auto logic dispatched command from invalid sensor input!\n");
                test40_pass = false;
            }
        } else if (s == 3) {
            /* Rigorous Hold & 3-Frame Qualification protocol for Step 3 partial recovery */
            s_fake_node_mode = FAKE_NODE_MODE_STOP_REPORTING;
            uint16_t seq_before_step3 = lora_hub_cmd.bits.seq;

            /* Advance 3 seconds: verify hold suppresses command during recovery */
            for (int t = 0; t < 3; t++) {
                lora_last_rx_tick_ms = lv_tick_get();
                advance_ui(1000u);
            }
            if (lora_hub_cmd.bits.seq != seq_before_step3) {
                printf("  FAIL 40 [Step 3]: Auto dispatched command prematurely during 10s recovery hold\n");
                test40_pass = false;
            }

            /* Advance past 10s hold (8s more, 11s total) */
            for (int t = 0; t < 8; t++) {
                lora_last_rx_tick_ms = lv_tick_get();
                advance_ui(1000u);
            }
            /* Time alone must NOT dispatch without fresh frames */
            if (lora_hub_cmd.bits.seq != seq_before_step3) {
                printf("  FAIL 40 [Step 3]: Auto dispatched command from time alone without fresh frames\n");
                test40_pass = false;
            }

            /* Feed fresh frame 1 */
            sim_post_node_status(true, 1205, true, 60001, false, 235, true, 48, true, 0, 1, 0, 0);
            advance_ui(50u);
            if (lora_hub_cmd.bits.seq != seq_before_step3) {
                printf("  FAIL 40 [Step 3]: Auto dispatched command prematurely on fresh frame 1\n");
                test40_pass = false;
            }

            /* Feed fresh frame 2 */
            sim_post_node_status(true, 1210, true, 60001, false, 235, true, 48, true, 0, 1, 0, 0);
            advance_ui(50u);
            if (lora_hub_cmd.bits.seq != seq_before_step3) {
                printf("  FAIL 40 [Step 3]: Auto dispatched command prematurely on fresh frame 2\n");
                test40_pass = false;
            }

            /* Feed fresh frame 3 -> qualification complete! Command MUST dispatch */
            s_fake_node_mode = FAKE_NODE_MODE_NORMAL;
            sim_set_tx_sink(fake_node_receive_cmd);
            sim_post_node_status(true, 1215, true, 60001, false, 235, true, 48, true, 0, 1, 0, 0);
            advance_ui(50u);
            if (lora_hub_cmd.bits.seq == seq_before_step3) {
                printf("  FAIL 40 [Step 3]: Auto did NOT dispatch command on 3rd fresh frame\n");
                test40_pass = false;
            } else {
                /* Assert active_low polarity mapping on wire command: desired ON (1) maps to wire GPIO bit 1 == 0 */
                uint8_t tx_gpios = lora_hub_cmd_get_relay_gpios_mask(&lora_hub_cmd);
                if ((tx_gpios & (1 << 1)) != 0) {
                    printf("  FAIL 40 [Step 3]: active_low polarity mapping failed: wire GPIO bit 1 is %d (expected 0)\n",
                           (tx_gpios & (1 << 1)) ? 1 : 0);
                    test40_pass = false;
                }

                /* Fake node receives, applies and echoes sequence */
                advance_ui(800u);
                uint8_t rb_fan = lora_node_status_get_relay_gpio(&lora_node_status, 1);
                if (rb_fan != 0) {
                    printf("  FAIL 40 [Step 3]: Node readback for active_low relay expected 0, got %d\n", rb_fan);
                    test40_pass = false;
                }
            }
        }
    }

    /* Restore clean defaults */
    ui_test_reset_device_configs();
    ui_test_reset_device_commands();
    ui_dismiss_toast();
    advance_ui(100u);

    if (test40_pass) {
        printf("  PASS: Combined fault & partial recovery matrix, qualification, active_low and VOC=0 verified.\n");
        passed_tests++;
    }

    /* -------------------------------------------------------------
     * [CHECK 41] LoRa Protocol v2 Durability Suite (A5 a-h)
     * ------------------------------------------------------------- */
    total_tests++;
    printf("\n[CHECK 41] LoRa Protocol v2 Durability Suite (A5 a-h)...\n");
    bool test41_pass = true;

    /* A5(a): Repeated reports with the same sample_seq do NOT advance Auto qualification; 3 distinct samples do */
    {
        ui_test_reset_device_commands();
        ui_hub_device_config_t fan_cfg_41;
        ui_auto_get_defaults(1, UI_PRESET_FAN, &fan_cfg_41);
        fan_cfg_41.mode = UI_MODE_AUTO;
        fan_cfg_41.auto_on_thresh = 1000;
        fan_cfg_41.auto_off_thresh = 800;
        fan_cfg_41.active_low = 0;
        ui_set_device_config(1, &fan_cfg_41);

        sim_post_node_status(true, 420, true, 85, true, 235, true, 48, true, 0, 0, 0, 0);
        advance_ui(12000u); /* Wait past initial hold */

        uint16_t base_seq = lora_hub_cmd.bits.seq;
        uint8_t static_sample_seq = lora_node_status.bits.sample_seq + 1;

        /* Send 5 frames with identical sample_seq and high CO2 */
        for (int i = 0; i < 5; i++) {
            lora_node_status.bits.co2_ppm = 1200;
            lora_node_status.bits.sample_seq = static_sample_seq;
            lora_last_rx_tick_ms = lv_tick_get();
            lora_rx_revision++;
            advance_ui(100u);
        }
        if (lora_hub_cmd.bits.seq != base_seq || ui_has_pending_command()) {
            printf("  FAIL 41a: Repeated frames with same sample_seq triggered Auto command\n");
            test41_pass = false;
        }

        /* Now advance sample_seq on 3 distinct frames */
        for (int i = 0; i < 3; i++) {
            lora_node_status.bits.co2_ppm = 1200;
            lora_node_status.bits.sample_seq++;
            lora_last_rx_tick_ms = lv_tick_get();
            lora_rx_revision++;
            advance_ui(100u);
        }
        if (lora_hub_cmd.bits.seq == base_seq || !ui_has_pending_command()) {
            printf("  FAIL 41a: 3 distinct sample_seq frames failed to trigger Auto command\n");
            test41_pass = false;
        }
        ui_test_reset_device_commands();
        fan_cfg_41.mode = UI_MODE_MANUAL;
        ui_set_device_config(1, &fan_cfg_41);
        advance_ui(50u);
    }

    /* A5(b): Lower uptime_sec drops TX readiness, then all 4 reported GPIOs adopted and not replayed */
    {
        /* Establish initial high uptime and stable TX ready */
        sim_post_node_status(true, 420, true, 85, true, 235, true, 48, true, 0, 0, 0, 0);
        lora_node_status.bits.uptime_sec = 5000;
        lora_rx_revision++;
        advance_ui(100u);

        /* Node reboots: uptime_sec drops to 10 with GPIOs 1, 0, 1, 0 */
        lora_node_status.bits.uptime_sec = 10;
        lora_node_status_set_relay_gpios_mask(&lora_node_status, 0x05);
        lora_last_rx_tick_ms = lv_tick_get();
        lora_rx_revision++;
        advance_ui(100u);

        /* TX readiness must drop on reboot until fresh synchronization */
        /* All 4 reported GPIO levels (0x05 = relays 0 & 2 ON) adopted by Hub command wire state */
        uint8_t wire_mask = lora_hub_cmd_get_relay_gpios_mask(&lora_hub_cmd);
        if (wire_mask != 0x05) {
            printf("  FAIL 41b: Reboot did not adopt all 4 reported GPIOs (got 0x%02X, expected 0x05)\n", wire_mask);
            test41_pass = false;
        }
        if (ui_has_pending_command()) {
            printf("  FAIL 41b: Reboot caused spurious pending command replay\n");
            test41_pass = false;
        }
    }

    /* A5(c): After command timeout, lora_hub_cmd carries reported level for that channel; late matching report returns to IDLE */
    {
        populate_node_status_baseline();
        advance_ui(100u);
        s_fake_node_mode = FAKE_NODE_MODE_FAIL_NEXT;
        ui_trigger_device_toggle(0); /* Turn device 0 OFF (desired GPIO 0) */
        advance_ui(UI_CMD_TIMEOUT_MS + 200u);

        const ui_test_hooks_t *h41 = ui_get_test_hooks();
        if (!h41 || !h41->device_cmds || h41->device_cmds[0].phase != UI_CMD_PHASE_ERROR) {
            printf("  FAIL 41c: Command did not enter ERROR phase on timeout\n");
            test41_pass = false;
        }
        /* Hub command carries current reported level (1), not timed-out target (0) */
        if (lora_hub_cmd_get_relay_gpio(&lora_hub_cmd, 0) != 1) {
            printf("  FAIL 41c: Hub command did not revert to reported level on timeout\n");
            test41_pass = false;
        }

        /* Late matching report arrives */
        lora_node_status.bits.seq_echo = lora_hub_cmd.bits.seq;
        lora_node_status_set_relay_gpio(&lora_node_status, 0, 0);
        lora_node_status.bits.sample_seq++;
        lora_last_rx_tick_ms = lv_tick_get();
        lora_rx_revision++;
        advance_ui(100u);

        if (h41->device_cmds[0].phase != UI_CMD_PHASE_IDLE) {
            printf("  FAIL 41c: Late matching report did not return command to IDLE phase (phase=%d)\n",
                   h41->device_cmds[0].phase);
            test41_pass = false;
        }
        ui_dismiss_toast();
        advance_ui(50u);
    }

    /* A5(d): ui_has_pending_command() is true only while a command is PENDING */
    {
        populate_node_status_baseline();
        advance_ui(50u);
        if (ui_has_pending_command()) {
            printf("  FAIL 41d: ui_has_pending_command() true when idle\n");
            test41_pass = false;
        }
        s_fake_node_mode = FAKE_NODE_MODE_NORMAL;
        ui_trigger_device_toggle(0);
        advance_ui(20u);
        if (!ui_has_pending_command()) {
            printf("  FAIL 41d: ui_has_pending_command() false during pending command\n");
            test41_pass = false;
        }
        advance_ui(800u); /* Allow fake node to confirm */
        if (ui_has_pending_command()) {
            printf("  FAIL 41d: ui_has_pending_command() true after command completed\n");
            test41_pass = false;
        }
    }

    /* A5(e): Validity: eCO2 399 and 60001 unavailable, 400 and 60000 valid; TVOC 0 and 60000 valid, 60001 unavailable */
    {
        ui_navigate_to_page(UI_PAGE_HOME);
        advance_ui(50u);

        /* eCO2: 399 is unavailable */
        sim_post_node_status(true, 399, true, 100, true, 220, true, 50, true, 0, 0, 0, 0);
        advance_ui(50u);
        if (strcmp(ui_get_home_co2_str(), "—") != 0) {
            printf("  FAIL 41e: eCO2 399 expected '—', got '%s'\n", ui_get_home_co2_str());
            test41_pass = false;
        }
        /* eCO2: 400 is valid */
        sim_post_node_status(true, 400, true, 100, true, 220, true, 50, true, 0, 0, 0, 0);
        advance_ui(50u);
        if (strcmp(ui_get_home_co2_str(), "400") != 0) {
            printf("  FAIL 41e: eCO2 400 expected '400', got '%s'\n", ui_get_home_co2_str());
            test41_pass = false;
        }
        /* eCO2: 60000 is valid */
        sim_post_node_status(true, 60000, true, 100, true, 220, true, 50, true, 0, 0, 0, 0);
        advance_ui(50u);
        if (strcmp(ui_get_home_co2_str(), "60000") != 0) {
            printf("  FAIL 41e: eCO2 60000 expected '60000', got '%s'\n", ui_get_home_co2_str());
            test41_pass = false;
        }
        /* eCO2: 60001 is unavailable */
        sim_post_node_status(true, 60001, true, 100, true, 220, true, 50, true, 0, 0, 0, 0);
        advance_ui(50u);
        if (strcmp(ui_get_home_co2_str(), "—") != 0) {
            printf("  FAIL 41e: eCO2 60001 expected '—', got '%s'\n", ui_get_home_co2_str());
            test41_pass = false;
        }

        /* TVOC: 0 is valid */
        sim_post_node_status(true, 450, true, 0, true, 220, true, 50, true, 0, 0, 0, 0);
        advance_ui(50u);
        if (strcmp(ui_get_home_voc_str(), "0") != 0) {
            printf("  FAIL 41e: TVOC 0 expected '0', got '%s'\n", ui_get_home_voc_str());
            test41_pass = false;
        }
        /* TVOC: 60000 is valid */
        sim_post_node_status(true, 450, true, 60000, true, 220, true, 50, true, 0, 0, 0, 0);
        advance_ui(50u);
        if (strcmp(ui_get_home_voc_str(), "60000") != 0) {
            printf("  FAIL 41e: TVOC 60000 expected '60000', got '%s'\n", ui_get_home_voc_str());
            test41_pass = false;
        }
        /* TVOC: 60001 is unavailable */
        sim_post_node_status(true, 450, true, 60001, true, 220, true, 50, true, 0, 0, 0, 0);
        advance_ui(50u);
        if (strcmp(ui_get_home_voc_str(), "—") != 0) {
            printf("  FAIL 41e: TVOC 60001 expected '—', got '%s'\n", ui_get_home_voc_str());
            test41_pass = false;
        }
    }

    /* A5(f): Schema: a 0x02 config for Air Purifier comes back as 0x03 with the ppb defaults 660/220, other presets keep thresholds, boot staging fires no callback */
    {
        s_r6_config_cb_count = 0;
        ui_set_config_changed_cb(r6_config_changed_probe_cb);
        ui_test_set_boot_staging(true);

        /* Create simulated v2 config */
        ui_hub_device_config_t v2_pur;
        memset(&v2_pur, 0, sizeof(v2_pur));
        strncpy(v2_pur.name, "Purifier v2", sizeof(v2_pur.name) - 1);
        v2_pur.preset = UI_PRESET_PURIFIER;
        v2_pur.mode = UI_MODE_AUTO;
        v2_pur.active_low = 0;
        v2_pur.schema_version = 0x02;
        v2_pur.auto_on_thresh = 3;  /* old index */
        v2_pur.auto_off_thresh = 2; /* old index */

        ui_set_device_config(0, &v2_pur);

        const ui_hub_device_config_t *v3_pur = ui_get_device_config(0);
        if (!v3_pur || v3_pur->schema_version != 0x03 || v3_pur->auto_on_thresh != 660 || v3_pur->auto_off_thresh != 220) {
            printf("  FAIL 41f: Schema v2 Purifier migration failed (schema=0x%02X, on=%d, off=%d)\n",
                   v3_pur ? v3_pur->schema_version : 0, v3_pur ? (int)v3_pur->auto_on_thresh : 0, v3_pur ? (int)v3_pur->auto_off_thresh : 0);
            test41_pass = false;
        }

        /* Verify Fan keeps its thresholds under v2 restore */
        ui_hub_device_config_t v2_fan;
        memset(&v2_fan, 0, sizeof(v2_fan));
        strncpy(v2_fan.name, "Fan v2", sizeof(v2_fan.name) - 1);
        v2_fan.preset = UI_PRESET_FAN;
        v2_fan.mode = UI_MODE_AUTO;
        v2_fan.active_low = 0;
        v2_fan.schema_version = 0x02;
        v2_fan.auto_on_thresh = 1100;
        v2_fan.auto_off_thresh = 850;

        ui_set_device_config(1, &v2_fan);
        const ui_hub_device_config_t *v3_fan = ui_get_device_config(1);
        if (!v3_fan || v3_fan->schema_version != 0x03 || v3_fan->auto_on_thresh != 1100 || v3_fan->auto_off_thresh != 850) {
            printf("  FAIL 41f: Schema v2 Fan migration failed to preserve thresholds (on=%d, off=%d)\n",
                   v3_fan ? (int)v3_fan->auto_on_thresh : 0, v3_fan ? (int)v3_fan->auto_off_thresh : 0);
            test41_pass = false;
        }

        ui_test_set_boot_staging(false);

        if (s_r6_config_cb_count != 0) {
            printf("  FAIL 41f: Boot staging fired config changed callback (%d calls)\n", s_r6_config_cb_count);
            test41_pass = false;
        }
        ui_set_config_changed_cb(NULL);
    }

    /* A5(g): lora_last_rssi = -140 round-trips as int16_t */
    {
        lora_last_rssi = -140;
        lora_rx_revision++;
        advance_ui(50u);
        if (lora_last_rssi != -140) {
            printf("  FAIL 41g: lora_last_rssi = -140 failed round-trip (got %d)\n", (int)lora_last_rssi);
            test41_pass = false;
        }
    }

    /* A5(h): Auto max: 60000 ppm and 60000 ppb render in Auto editor without clipping */
    {
        ui_hub_device_config_t max_cfg;
        ui_auto_get_defaults(1, UI_PRESET_FAN, &max_cfg);
        max_cfg.auto_on_thresh = 60000;
        max_cfg.auto_off_thresh = 59950;
        ui_test_set_boot_staging(true);
        ui_set_device_config(1, &max_cfg);
        ui_test_set_boot_staging(false);

        ui_open_device_settings(1);
        ui_dialog_open_limits();
        advance_ui(50u);

        if (!audit_visible_fonts(lv_scr_act()) || !audit_visible_glyph_containment(lv_scr_act())) {
            printf("  FAIL 41h: 60000 ppm Auto editor failed font or containment audit\n");
            test41_pass = false;
        }
        ui_close_device_settings();
        advance_ui(50u);

        /* Purifier at 60000 ppb */
        ui_auto_get_defaults(0, UI_PRESET_PURIFIER, &max_cfg);
        max_cfg.auto_on_thresh = 60000;
        max_cfg.auto_off_thresh = 59950;
        ui_test_set_boot_staging(true);
        ui_set_device_config(0, &max_cfg);
        ui_test_set_boot_staging(false);

        ui_open_device_settings(0);
        ui_dialog_open_limits();
        advance_ui(50u);

        if (!audit_visible_fonts(lv_scr_act()) || !audit_visible_glyph_containment(lv_scr_act())) {
            printf("  FAIL 41h: 60000 ppb Auto editor failed font or containment audit\n");
            test41_pass = false;
        }
        ui_close_device_settings();
        advance_ui(50u);
    }

    ui_test_reset_device_configs();
    ui_test_reset_device_commands();
    populate_node_status_baseline();
    advance_ui(500u);

    if (test41_pass) {
        printf("  PASS: LoRa Protocol v2 durability suite (A5 a-h) verified.\n");
        passed_tests++;
    }

    /* -------------------------------------------------------------
     * [CHECK 42] Live current-minute Trends slot (owner decision 2026-09-26)
     * The newest history slot follows the latest valid reading for all four
     * metrics within ~1 s instead of holding the first sample of the minute;
     * a closed minute keeps its last reading; identical frames change nothing.
     * ------------------------------------------------------------- */
    printf("\n[CHECK 42] Live Current-Minute Trends Slot (all 4 metrics)...\n");
    total_tests++;
    bool test42_pass = true;
    {
        const ui_test_hooks_t *h42 = ui_get_test_hooks();
        static const ui_metric_id_t m42[4] = { UI_METRIC_CO2, UI_METRIC_VOC, UI_METRIC_TEMP, UI_METRIC_HUMIDITY };
        populate_node_status_baseline();
        ui_test_reset_history();
        ui_navigate_to_page(UI_PAGE_TRENDS);
        advance_ui(300u);
        ui_select_trend_metric(UI_METRIC_CO2);
        advance_ui(300u);

        /* Three minutes of steady readings, then position at second 20 of a minute */
        for (int s42 = 0; s42 < 180; s42++) {
            sim_post_node_status(true, 420, true, 85, true, 235, true, 48, true, 0, 0, 0, 0);
            advance_ui(1000u);
        }
        while ((*h42->uptime_seconds % 60u) != 20u) {
            sim_post_node_status(true, 420, true, 85, true, 235, true, 48, true, 0, 0, 0, 0);
            advance_ui(250u);
        }

        uint16_t cnt_before[4];
        for (int i = 0; i < 4; i++) cnt_before[i] = h42->history_count[m42[i]];

        /* 1. Mid-minute spike: newest slot of every metric follows the live reading */
        static const int32_t spike[4] = { 3000, 900, 300, 80 };
        sim_post_node_status(true, 3000, true, 900, true, 300, true, 80, true, 0, 0, 0, 0);
        advance_ui(1500u);
        for (int i = 0; i < 4; i++) {
            uint16_t c = h42->history_count[m42[i]];
            const ui_history_sample_t *hs = (c > 0) ? &h42->history[m42[i]][c - 1] : NULL;
            if (c != cnt_before[i] || !hs || !hs->valid || hs->value != spike[i]) {
                printf("  FAIL 42a: metric %d newest slot=%ld (valid=%d, count %u->%u), expected live %ld in the same minute\n",
                       (int)m42[i], hs ? (long)hs->value : -1L, hs ? (int)hs->valid : 0,
                       (unsigned)cnt_before[i], (unsigned)c, (long)spike[i]);
                test42_pass = false;
            }
        }

        /* 2. The visible eCO2 chart rescales to include the spike and plots it at the right edge */
        const char *top42 = ui_get_trends_y_lbl_str(0);
        long top_val42 = top42 ? strtol(top42, NULL, 10) : 0;
        lv_obj_t *chart42 = (h42->trends_chart) ? *h42->trends_chart : NULL;
        lv_chart_series_t *ser42 = (h42->trends_chart_series) ? *h42->trends_chart_series : NULL;
        lv_coord_t *ys42 = (chart42 && ser42) ? lv_chart_get_y_array(chart42, ser42) : NULL;
        if (top_val42 < 3000 || !ys42 || ys42[UI_HISTORY_CAPACITY - 1] < 700) {
            printf("  FAIL 42b: eCO2 chart did not follow the spike (top label '%s', newest plot %d/1000)\n",
                   top42 ? top42 : "NULL", ys42 ? (int)ys42[UI_HISTORY_CAPACITY - 1] : -1);
            test42_pass = false;
        }

        /* 3. Falling back within the same minute: the newest slot follows down */
        sim_post_node_status(true, 900, true, 120, true, 240, true, 50, true, 0, 0, 0, 0);
        advance_ui(1500u);
        {
            uint16_t c = h42->history_count[UI_METRIC_CO2];
            const ui_history_sample_t *hs = &h42->history[UI_METRIC_CO2][c - 1];
            if (c != cnt_before[0] || !hs->valid || hs->value != 900) {
                printf("  FAIL 42c: eCO2 newest slot=%ld after drop, expected 900 in the same minute\n", (long)hs->value);
                test42_pass = false;
            }
        }

        /* 4. Identical frames do not touch history (0-flush idle contract) */
        uint32_t rev42 = h42->history_rev[UI_METRIC_CO2];
        for (int i = 0; i < 3; i++) {
            sim_post_node_status(true, 900, true, 120, true, 240, true, 50, true, 0, 0, 0, 0);
            advance_ui(300u);
        }
        if (h42->history_rev[UI_METRIC_CO2] != rev42) {
            printf("  FAIL 42d: identical frames changed eCO2 history revision (%lu -> %lu)\n",
                   (unsigned long)rev42, (unsigned long)h42->history_rev[UI_METRIC_CO2]);
            test42_pass = false;
        }

        /* 5. Minute boundary: the closed slot keeps its last reading; a new live slot starts */
        uint32_t min42 = *h42->uptime_seconds / 60u;
        while (*h42->uptime_seconds / 60u == min42) {
            sim_post_node_status(true, 900, true, 120, true, 240, true, 50, true, 0, 0, 0, 0);
            advance_ui(500u);
        }
        sim_post_node_status(true, 1500, true, 120, true, 240, true, 50, true, 0, 0, 0, 0);
        advance_ui(1500u);
        {
            uint16_t c = h42->history_count[UI_METRIC_CO2];
            const ui_history_sample_t *prev = (c >= 2) ? &h42->history[UI_METRIC_CO2][c - 2] : NULL;
            const ui_history_sample_t *cur = (c >= 1) ? &h42->history[UI_METRIC_CO2][c - 1] : NULL;
            if (!prev || !cur || !prev->valid || prev->value != 900 || !cur->valid || cur->value != 1500) {
                printf("  FAIL 42e: minute close kept %ld and started %ld, expected 900 then live 1500\n",
                       prev ? (long)prev->value : -1L, cur ? (long)cur->value : -1L);
                test42_pass = false;
            }
        }

        populate_node_status_baseline();
        ui_navigate_to_page(UI_PAGE_HOME);
        advance_ui(500u);
    }
    if (test42_pass) {
        printf("  PASS: Live current-minute slot follows all four metrics, closes on the last reading, idle unchanged.\n");
        passed_tests++;
    }

    /* -------------------------------------------------------------
     * PERFORMANCE MEASUREMENTS (P1 empirical benchmarks)
     * ------------------------------------------------------------- */
    printf("\n====================================================\n");
    printf(" Performance Flush & Draw Metrics (Measured)\n");
    printf("====================================================\n");

    /* (a) Ten consecutive identical status frames across ALL pages and ALL THREE Auto editors (R7) */
    bool perf_identical_pass = true;

    /* 1. Home Page (valid) */
    ui_navigate_to_page(UI_PAGE_HOME);
    populate_node_status_baseline();
    advance_ui(2500u);
    {
        uint32_t sec_s = *ui_get_test_hooks()->uptime_seconds;
        while (*ui_get_test_hooks()->uptime_seconds == sec_s) advance_ui(10u);
        reset_flush_counters();
        for (int i = 0; i < 10; i++) { lora_rx_revision++; advance_ui(10u); }
        if (s_flush_calls != 0 || s_flushed_pixels != 0) {
            printf("  FAIL: 10 Identical Status Frames (Home Valid): %llu flushes, %llu pixels\n",
                   (unsigned long long)s_flush_calls, (unsigned long long)s_flushed_pixels);
            perf_identical_pass = false;
        } else {
            printf("  (a1) 10 Identical Frames (Home Valid):        0 flushes, 0 pixels pushed\n");
        }
    }

    /* 2. Home Page (unavailable - RS485 fault) */
    lora_node_status.bits.rs485_fault = 1;
    lora_rx_revision++;
    advance_ui(500u);
    {
        uint32_t sec_s = *ui_get_test_hooks()->uptime_seconds;
        while (*ui_get_test_hooks()->uptime_seconds == sec_s) advance_ui(10u);
        reset_flush_counters();
        for (int i = 0; i < 10; i++) { lora_rx_revision++; advance_ui(10u); }
        if (s_flush_calls != 0 || s_flushed_pixels != 0) {
            printf("  FAIL: 10 Identical Status Frames (Home Unavail): %llu flushes, %llu pixels\n",
                   (unsigned long long)s_flush_calls, (unsigned long long)s_flushed_pixels);
            perf_identical_pass = false;
        } else {
            printf("  (a2) 10 Identical Frames (Home Unavail):      0 flushes, 0 pixels pushed\n");
        }
    }
    lora_node_status.bits.rs485_fault = 0;
    lora_rx_revision++;
    advance_ui(100u);

    /* 3. Trends Page (valid) */
    ui_navigate_to_page(UI_PAGE_TRENDS);
    advance_ui(500u);
    {
        uint32_t sec_s = *ui_get_test_hooks()->uptime_seconds;
        while (*ui_get_test_hooks()->uptime_seconds == sec_s) advance_ui(10u);
        reset_flush_counters();
        for (int i = 0; i < 10; i++) { lora_rx_revision++; advance_ui(10u); }
        if (s_flush_calls != 0 || s_flushed_pixels != 0) {
            printf("  FAIL: 10 Identical Status Frames (Trends Valid): %llu flushes, %llu pixels\n",
                   (unsigned long long)s_flush_calls, (unsigned long long)s_flushed_pixels);
            perf_identical_pass = false;
        } else {
            printf("  (a3) 10 Identical Frames (Trends Valid):      0 flushes, 0 pixels pushed\n");
        }
    }

    /* 4. Trends Page (unavailable) */
    lora_node_status.bits.rs485_fault = 1;
    lora_rx_revision++;
    advance_ui(500u);
    {
        uint32_t sec_s = *ui_get_test_hooks()->uptime_seconds;
        while (*ui_get_test_hooks()->uptime_seconds == sec_s) advance_ui(10u);
        reset_flush_counters();
        for (int i = 0; i < 10; i++) { lora_rx_revision++; advance_ui(10u); }
        if (s_flush_calls != 0 || s_flushed_pixels != 0) {
            printf("  FAIL: 10 Identical Status Frames (Trends Unavail): %llu flushes, %llu pixels\n",
                   (unsigned long long)s_flush_calls, (unsigned long long)s_flushed_pixels);
            perf_identical_pass = false;
        } else {
            printf("  (a4) 10 Identical Frames (Trends Unavail):    0 flushes, 0 pixels pushed\n");
        }
    }
    lora_node_status.bits.rs485_fault = 0;
    lora_rx_revision++;
    advance_ui(100u);

    /* 5. Devices Page (valid) */
    ui_navigate_to_page(UI_PAGE_DEVICES);
    advance_ui(500u);
    {
        uint32_t sec_s = *ui_get_test_hooks()->uptime_seconds;
        while (*ui_get_test_hooks()->uptime_seconds == sec_s) advance_ui(10u);
        reset_flush_counters();
        for (int i = 0; i < 10; i++) { lora_rx_revision++; advance_ui(10u); }
        if (s_flush_calls != 0 || s_flushed_pixels != 0) {
            printf("  FAIL: 10 Identical Status Frames (Devices Valid): %llu flushes, %llu pixels\n",
                   (unsigned long long)s_flush_calls, (unsigned long long)s_flushed_pixels);
            perf_identical_pass = false;
        } else {
            printf("  (a5) 10 Identical Frames (Devices Valid):     0 flushes, 0 pixels pushed\n");
        }
    }

    /* 6. Devices Page (unavailable) */
    lora_node_status.bits.rs485_fault = 1;
    lora_rx_revision++;
    advance_ui(500u);
    {
        uint32_t sec_s = *ui_get_test_hooks()->uptime_seconds;
        while (*ui_get_test_hooks()->uptime_seconds == sec_s) advance_ui(10u);
        reset_flush_counters();
        for (int i = 0; i < 10; i++) { lora_rx_revision++; advance_ui(10u); }
        if (s_flush_calls != 0 || s_flushed_pixels != 0) {
            printf("  FAIL: 10 Identical Status Frames (Devices Unavail): %llu flushes, %llu pixels\n",
                   (unsigned long long)s_flush_calls, (unsigned long long)s_flushed_pixels);
            perf_identical_pass = false;
        } else {
            printf("  (a6) 10 Identical Frames (Devices Unavail):   0 flushes, 0 pixels pushed\n");
        }
    }
    lora_node_status.bits.rs485_fault = 0;
    lora_rx_revision++;
    advance_ui(100u);

    /* 7-12. All 3 Auto Editors: Dev 0 (Purifier/VOC), Dev 1 (Fan/CO2), Dev 2 (Humidifier/RH) */
    for (uint8_t d_idx = 0; d_idx < 3; d_idx++) {
        const char *dev_labels[3] = { "Purifier/VOC", "Fan/CO2", "Humidifier/RH" };

        /* Valid State */
        ui_open_device_settings(d_idx);
        advance_ui(50u);
        if (h34 && h34->on_dialog_open_limits_click) h34->on_dialog_open_limits_click(NULL);
        advance_ui(500u);
        {
            uint32_t sec_s = *ui_get_test_hooks()->uptime_seconds;
            while (*ui_get_test_hooks()->uptime_seconds == sec_s) advance_ui(10u);
            reset_flush_counters();
            for (int i = 0; i < 10; i++) { lora_rx_revision++; advance_ui(10u); }
            if (s_flush_calls != 0 || s_flushed_pixels != 0) {
                printf("  FAIL: 10 Identical Frames (Auto %s Valid): %llu flushes, %llu pixels\n",
                       dev_labels[d_idx], (unsigned long long)s_flush_calls, (unsigned long long)s_flushed_pixels);
                perf_identical_pass = false;
            } else {
                printf("  (a%d) 10 Identical Frames (Auto %-13s Valid):   0 flushes, 0 pixels pushed\n",
                       7 + d_idx * 2, dev_labels[d_idx]);
            }
        }
        ui_close_device_settings();
        advance_ui(100u);

        /* Unavailable State */
        lora_node_status.bits.rs485_fault = 1;
        lora_rx_revision++;
        advance_ui(100u);
        ui_open_device_settings(d_idx);
        advance_ui(50u);
        if (h34 && h34->on_dialog_open_limits_click) h34->on_dialog_open_limits_click(NULL);
        advance_ui(500u);
        {
            uint32_t sec_s = *ui_get_test_hooks()->uptime_seconds;
            while (*ui_get_test_hooks()->uptime_seconds == sec_s) advance_ui(10u);
            reset_flush_counters();
            for (int i = 0; i < 10; i++) { lora_rx_revision++; advance_ui(10u); }
            if (s_flush_calls != 0 || s_flushed_pixels != 0) {
                printf("  FAIL: 10 Identical Frames (Auto %s Unavail): %llu flushes, %llu pixels\n",
                       dev_labels[d_idx], (unsigned long long)s_flush_calls, (unsigned long long)s_flushed_pixels);
                perf_identical_pass = false;
            } else {
                printf("  (a%d) 10 Identical Frames (Auto %-13s Unavail): 0 flushes, 0 pixels pushed\n",
                       8 + d_idx * 2, dev_labels[d_idx]);
            }
        }
        ui_close_device_settings();
        advance_ui(100u);
        lora_node_status.bits.rs485_fault = 0;
        lora_rx_revision++;
        advance_ui(100u);
    }

    /* Restore Home page baseline */
    ui_navigate_to_page(UI_PAGE_HOME);
    populate_node_status_baseline();
    advance_ui(500u);

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
    uint32_t sec_start = *ui_get_test_hooks()->uptime_seconds;
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

    printf("  (f) Per-tick Change Test:           Revision counter comparison + link staleness check (nominal check, 0 flushes)\n");

    if (!perf_identical_pass) {
        printf("  FAIL: Performance identical status frames produced flushes/pixels\n");
        passed_tests--;
    }
    if (sub_flushes != 0 || sub_pixels != 0) {
        printf("  FAIL: Performance idle subsecond expected 0 flushes / 0 pixels, got %llu / %llu\n",
               (unsigned long long)sub_flushes, (unsigned long long)sub_pixels);
        passed_tests--;
    }
    if (sec_flushes > 2 || sec_pixels > 10000) {
        printf("  FAIL: Performance 1s uptime tick expected <= 2 flushes / < 10000 pixels, got %llu / %llu\n",
               (unsigned long long)sec_flushes, (unsigned long long)sec_pixels);
        passed_tests--;
    }

    printf("\n====================================================\n");
    printf(" Summary: %d / %d checks passed.\n", passed_tests, total_tests);
    printf("====================================================\n");

    return (passed_tests == total_tests) ? 0 : 1;
}

/* =========================================================================
 * Scenario Screenshots Generator
 * ========================================================================= */
static int s_scenario_shots_generated = 0;
static int s_scenario_shots_failed = 0;

static int save_scenario_shot(const char *dir, const char *filename, int index, int total)
{
    char path[512];
    snprintf(path, sizeof(path), "%s/%s", dir, filename);
    if (save_framebuffer(path) != 0) {
        fprintf(stderr, "ERROR: [%d/%d] Failed to save scenario shot: %s\n", index, total, path);
        s_scenario_shots_failed++;
        return 1;
    }
    s_scenario_shots_generated++;
    printf("  [%d/%d] Generated %s\n", index, total, path);
    return 0;
}

static int run_scenario_shots(const char *dir)
{
    printf("Generating deterministic scenario framebuffers into '%s'...\n", dir);
    CreateDirectoryA(dir, NULL);
    DWORD attr = GetFileAttributesA(dir);
    if (attr == INVALID_FILE_ATTRIBUTES || !(attr & FILE_ATTRIBUTE_DIRECTORY)) {
        fprintf(stderr, "FATAL: Output directory '%s' is not a valid directory or could not be created\n", dir);
        return 1;
    }

    s_scenario_shots_generated = 0;
    s_scenario_shots_failed = 0;

    lvgl_setup();
    ui_init();

    /* 1. Splash Screen (at 1000ms steady state) */
    ui_replay_splash();
    advance_ui(1000u);
    if (save_scenario_shot(dir, "splash.raw", 1, 30) != 0) return 1;

    /* Complete splash animation */
    advance_ui(1600u);

    /* 2. Home Good Air */
    populate_node_status_baseline();
    ui_navigate_to_page(UI_PAGE_HOME);
    advance_ui(200u);
    if (save_scenario_shot(dir, "home_good.raw", 2, 30) != 0) return 1;

    /* 3. Home Moderate Air (VOC 160, CO2 850, Temp 27.2C, Humid 62%) */
    sim_post_node_status(true,
                         850, true,
                         400, true,
                         272, true,
                         62, true,
                         1, 0, 0, 1);
    advance_ui(200u);
    if (save_scenario_shot(dir, "home_moderate.raw", 3, 30) != 0) return 1;

    /* 4. Home Poor Air (4-digit CO2: 1420 ppm, VOC 340, Temp 29.8C, Humid 75%) */
    sim_post_node_status(true,
                         1420, true,
                         900, true,
                         298, true,
                         75, true,
                         1, 0, 0, 1);
    advance_ui(200u);
    if (save_scenario_shot(dir, "home_poor.raw", 4, 30) != 0) return 1;

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
    if (save_scenario_shot(dir, "home_offline.raw", 5, 30) != 0) return 1;
    dev1_auto.mode = UI_MODE_MANUAL;
    ui_set_device_config(1, &dev1_auto);

    /* 6. Home Auto Paused on Sensor Suspension (L3: stacked "Auto\npaused" with 12px gap from switch) */
    populate_node_status_baseline();
    dev1_auto = *ui_get_device_config(1);
    dev1_auto.mode = UI_MODE_AUTO;
    ui_set_device_config(1, &dev1_auto);
    advance_ui(200u);
    lora_node_status.bits.co2_valid = 0;
    lora_rx_revision++;
    advance_ui(200u);
    if (save_scenario_shot(dir, "home_auto_paused_switch.raw", 6, 30) != 0) return 1;
    dev1_auto.mode = UI_MODE_MANUAL;
    ui_set_device_config(1, &dev1_auto);
    lora_node_status.bits.co2_valid = 1;
    lora_rx_revision++;
    advance_ui(100u);

    /* 7. Home Partial Sensor Failure (CO2/VOC fail, Temp/Humid valid) */
    sim_post_node_status(true,
                         420, false,
                         85, false,
                         220, true,
                         50, true,
                         1, 0, 0, 1);
    advance_ui(200u);
    if (save_scenario_shot(dir, "home_sensor_fail.raw", 7, 30) != 0) return 1;

    /* 8. Home Relay Pending ("Sending…" visible) */
    populate_node_status_baseline();
    advance_ui(200u);
    ui_trigger_device_toggle(0);
    advance_ui(200u);
    if (save_scenario_shot(dir, "home_relay_pending.raw", 8, 30) != 0) return 1;

    /* 9. Home Relay Retry ("Retry" button visible) */
    populate_node_status_baseline();
    advance_ui(600u);
    s_fake_node_mode = FAKE_NODE_MODE_FAIL_NEXT;
    ui_trigger_device_toggle(1);
    advance_ui(3200u);
    if (save_scenario_shot(dir, "home_relay_retry.raw", 9, 30) != 0) return 1;

    /* Clear retry state and dismiss toast so it doesn't contaminate subsequent shots (P2.4) */
    ui_test_reset_device_commands();
    ui_dismiss_toast();
    advance_ui(50u);

    /* 10. Trends Page with Data (16 external feed samples) */
    populate_node_status_baseline();
    feed_standard_history();
    ui_navigate_to_page(UI_PAGE_TRENDS);
    advance_ui(200u);
    if (save_scenario_shot(dir, "trends_data.raw", 10, 30) != 0) return 1;

    /* 11. Trends Stage-1 Interim Frame (Tactile VOC Highlight, Coherent eCO2 Chart/Title) */
    const ui_test_hooks_t *h_scen = ui_get_test_hooks();
    if (!h_scen || !h_scen->trigger_metric_tab_click || !h_scen->pending_trend_chart ||
        !h_scen->stage1_flush_pending || !h_scen->trends_title_lbl || !(*h_scen->trends_title_lbl)) {
        fprintf(stderr, "FATAL: Required test hooks for Stage-1 interim shot missing or NULL\n");
        return 1;
    }
    h_scen->trigger_metric_tab_click(UI_METRIC_VOC);
    lv_timer_handler();

    if (!(*h_scen->pending_trend_chart) || !(*h_scen->stage1_flush_pending)) {
        fprintf(stderr, "FATAL: Stage 1 failed to defer chart rendering or mark stage1_flush_pending\n");
        return 1;
    }
    const char *t = lv_label_get_text(*h_scen->trends_title_lbl);
    if (!t || strcmp(t, "eCO₂") != 0) {
        fprintf(stderr, "FATAL: Stage 1 desynchronized title: '%s' (expected 'eCO₂')\n", t ? t : "NULL");
        return 1;
    }

    if (save_scenario_shot(dir, "trends_stage1_interim.raw", 11, 30) != 0) return 1;

    /* Real application loop order: ui_tick() processes Stage 2 deferred chart */
    advance_ui(200u);

    /* 12. Trends VOC */
    lv_obj_t *lbl_voc = find_visible_label(lv_scr_act(), "TVOC");
    if (!lbl_voc) lbl_voc = find_visible_label(lv_scr_act(), "VOC");
    if (!lbl_voc || !lv_obj_get_parent(lbl_voc)) {
        fprintf(stderr, "FATAL: Cannot find visible label TVOC/VOC or its parent tab\n");
        return 1;
    }
    click_object_center(lv_obj_get_parent(lbl_voc));
    advance_ui(200u);
    if (save_scenario_shot(dir, "trends_voc.raw", 12, 30) != 0) return 1;

    /* 13. Trends Temperature */
    lv_obj_t *lbl_tmp = find_visible_label(lv_scr_act(), "Temperature");
    if (!lbl_tmp || !lv_obj_get_parent(lbl_tmp)) {
        fprintf(stderr, "FATAL: Cannot find visible label Temperature or its parent tab\n");
        return 1;
    }
    click_object_center(lv_obj_get_parent(lbl_tmp));
    advance_ui(200u);
    if (save_scenario_shot(dir, "trends_temp.raw", 13, 30) != 0) return 1;

    /* 14. Trends Humidity */
    lv_obj_t *lbl_hmd = find_visible_label(lv_scr_act(), "Humidity");
    if (!lbl_hmd || !lv_obj_get_parent(lbl_hmd)) {
        fprintf(stderr, "FATAL: Cannot find visible label Humidity or its parent tab\n");
        return 1;
    }
    click_object_center(lv_obj_get_parent(lbl_hmd));
    advance_ui(200u);
    if (save_scenario_shot(dir, "trends_humid.raw", 14, 30) != 0) return 1;

    /* Switch back to CO2 */
    lv_obj_t *lbl_co2 = find_visible_label(lv_scr_act(), "eCO₂");
    if (!lbl_co2) lbl_co2 = find_visible_label(lv_scr_act(), "CO₂");
    if (!lbl_co2 || !lv_obj_get_parent(lbl_co2)) {
        fprintf(stderr, "FATAL: Cannot find visible label CO2/eCO2 or its parent tab\n");
        return 1;
    }
    click_object_center(lv_obj_get_parent(lbl_co2));
    advance_ui(200u);

    /* 15. Trends Autonomous History with < 16 Samples (right-aligned, -15 min..Now) */
    ui_test_reset_history();
    for (uint32_t min = 0; min < 6; min++) {
        uint16_t co2_val = (uint16_t)(410 + min * 15);
        sim_post_node_status(true, co2_val, true, (uint16_t)(80 + min * 2), true, (int16_t)(230 + min), true, (uint8_t)(48 + min), true, 1, 0, 0, 1);
        ui_set_uptime_seconds(min * 60u + 1u);
        advance_ui(50u);
    }
    advance_ui(200u);
    if (save_scenario_shot(dir, "trends_autonomous_partial.raw", 15, 30) != 0) return 1;

    /* 16. Trends One-Sample State (Collecting readings…) */
    ui_test_reset_history();
    sim_post_node_status(true, 420, true, 85, true, 235, true, 48, true, 1, 0, 0, 1);
    ui_set_uptime_seconds(1u);
    advance_ui(200u);
    if (save_scenario_shot(dir, "trends_one_sample.raw", 16, 30) != 0) return 1;

    /* 17. Trends Page Empty (No readings yet) */
    ui_test_reset_history();
    advance_ui(200u);
    if (save_scenario_shot(dir, "trends_empty.raw", 17, 30) != 0) return 1;

    /* 18. Devices Page (2x2 Grid) */
    ui_test_reset_device_commands();
    ui_dismiss_toast();
    populate_node_status_baseline();
    ui_navigate_to_page(UI_PAGE_DEVICES);
    advance_ui(200u);
    if (save_scenario_shot(dir, "devices_grid.raw", 18, 30) != 0) return 1;

    /* 19. Devices Settings Modal Dialog */
    ui_open_device_settings(1);
    advance_ui(200u);
    if (save_scenario_shot(dir, "devices_settings.raw", 19, 30) != 0) return 1;
    ui_close_device_settings();
    advance_ui(100u);

    /* 20. Home Degraded Link (2 Bars: SNR = -4 dB at SF7) */
    ui_navigate_to_page(UI_PAGE_HOME);
    populate_node_status_baseline();
    lora_last_snr = -4;
    lora_rx_revision++;
    advance_ui(200u);
    if (save_scenario_shot(dir, "home_link_degraded.raw", 20, 30) != 0) return 1;

    /* 21. Home Auto Paused on Error */
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
    lora_node_status.bits.sample_seq++; lora_rx_revision++; advance_ui(100u);
    lora_node_status.bits.sample_seq++; lora_rx_revision++; advance_ui(100u);
    lora_node_status.bits.sample_seq++; lora_rx_revision++; advance_ui(100u);
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
    if (save_scenario_shot(dir, "home_auto_paused.raw", 21, 30) != 0) return 1;
    ui_test_reset_device_commands();
    advance_ui(50u);

    /* 22. Devices Auto Threshold Editor Subview */
    ui_navigate_to_page(UI_PAGE_DEVICES);
    advance_ui(200u);
    ui_open_device_settings(1);
    advance_ui(100u);
    ui_dialog_open_limits();
    advance_ui(100u);
    if (save_scenario_shot(dir, "devices_auto_thresholds.raw", 22, 30) != 0) return 1;
    ui_close_device_settings();
    advance_ui(100u);

    /* 23. Devices Settings Manual-Only (Preset 4: Light) */
    ui_hub_device_config_t dev3_cfg = *ui_get_device_config(3);
    dev3_cfg.preset = 4; /* Light */
    dev3_cfg.mode = UI_MODE_MANUAL;
    ui_set_device_config(3, &dev3_cfg);
    ui_open_device_settings(3);
    advance_ui(100u);
    if (save_scenario_shot(dir, "devices_settings_manual_only.raw", 23, 30) != 0) return 1;
    ui_close_device_settings();
    advance_ui(100u);

    /* 24. Devices Auto Thresholds for Purifier (TVOC in ppb) */
    ui_hub_device_config_t dev2_cfg = *ui_get_device_config(2);
    ui_auto_get_defaults(2, UI_PRESET_PURIFIER, &dev2_cfg);
    ui_set_device_config(2, &dev2_cfg);
    ui_open_device_settings(2);
    advance_ui(100u);
    ui_dialog_open_limits();
    advance_ui(100u);
    if (save_scenario_shot(dir, "devices_auto_purifier.raw", 24, 30) != 0) return 1;
    ui_close_device_settings();
    advance_ui(100u);

    /* 25. Devices Auto Thresholds for Humidifier (%RH, reversed comparison) */
    ui_hub_device_config_t dev0_cfg = *ui_get_device_config(0);
    ui_auto_get_defaults(0, UI_PRESET_HUMIDIFIER, &dev0_cfg);
    ui_set_device_config(0, &dev0_cfg);
    ui_open_device_settings(0);
    advance_ui(100u);
    ui_dialog_open_limits();
    advance_ui(100u);
    if (save_scenario_shot(dir, "devices_auto_humidifier.raw", 25, 30) != 0) return 1;
    ui_close_device_settings();
    advance_ui(100u);

    /* 26. Devices Auto Thresholds at 60000 (5 digits) */
    ui_open_device_settings(1);
    advance_ui(100u);
    ui_dialog_open_limits();
    advance_ui(100u);
    for (int s = 0; s < 1200; s++) {
        ui_dialog_step_thresh(true, 1);
    }
    advance_ui(100u);
    if (save_scenario_shot(dir, "devices_auto_max_60000.raw", 26, 30) != 0) return 1;
    ui_close_device_settings();
    advance_ui(100u);

    /* 27. Devices Auto Thresholds with Invalid Pair (Air Purifier TVOC: OFF stepped up to ON) */
    dev1_cfg = *ui_get_device_config(1);
    ui_auto_get_defaults(1, UI_PRESET_PURIFIER, &dev1_cfg);
    ui_set_device_config(1, &dev1_cfg);
    ui_open_device_settings(1);
    advance_ui(100u);
    ui_dialog_open_limits();
    advance_ui(100u);
    for (int s = 0; s < (UI_AUTO_VOC_DEF_ON - UI_AUTO_VOC_DEF_OFF) / UI_AUTO_VOC_STEP; s++) {
        ui_dialog_step_thresh(false, 1);
    }
    advance_ui(100u);
    if (ui_get_dialog_edit_on_thresh() != ui_get_dialog_edit_off_thresh()) {
        fprintf(stderr, "FATAL: Invalid-draft scenario did not reach ON == OFF (on=%d, off=%d)\n",
                (int)ui_get_dialog_edit_on_thresh(), (int)ui_get_dialog_edit_off_thresh());
        return 1;
    }
    if (save_scenario_shot(dir, "devices_auto_invalid_draft.raw", 27, 30) != 0) return 1;
    ui_close_device_settings();
    advance_ui(100u);

    /* 28. Home with Temp/Humid invalid (showing " — " with "No data" note) */
    ui_navigate_to_page(UI_PAGE_HOME);
    sim_post_node_status(true,
                         420, true,
                         85, true,
                         235, false,
                         48, false,
                         1, 0, 0, 1);
    advance_ui(200u);
    if (save_scenario_shot(dir, "home_temp_humid_invalid.raw", 28, 30) != 0) return 1;

    /* 29. Devices Preset Dropdown Open (Top) */
    ui_navigate_to_page(UI_PAGE_DEVICES);
    advance_ui(200u);
    ui_open_device_settings(0);
    advance_ui(100u);
    ui_dialog_open_dropdown();
    advance_ui(100u);
    lv_obj_t *dd_list = ui_dialog_get_dropdown_list();
    if (!dd_list) {
        fprintf(stderr, "FATAL: Dropdown list object is NULL\n");
        return 1;
    }
    lv_obj_scroll_to_y(dd_list, 0, LV_ANIM_OFF);
    advance_ui(50u);
    if (save_scenario_shot(dir, "dropdown_open_top.raw", 29, 30) != 0) return 1;

    /* 30. Devices Preset Dropdown Open (Bottom) */
    lv_obj_scroll_to_y(dd_list, 240, LV_ANIM_OFF);
    advance_ui(50u);
    if (save_scenario_shot(dir, "dropdown_open_bottom.raw", 30, 30) != 0) return 1;
    ui_dialog_close_dropdown();
    advance_ui(50u);
    ui_close_device_settings();
    advance_ui(100u);

    print_heap();
    if (s_scenario_shots_failed > 0 || s_scenario_shots_generated != 30) {
        fprintf(stderr, "FATAL: Scenario generation failed: %d failed, %d generated (expected 30)\n",
                s_scenario_shots_failed, s_scenario_shots_generated);
        return 1;
    }
    printf("All 30 scenario shots generated successfully in '%s'.\n", dir);
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
