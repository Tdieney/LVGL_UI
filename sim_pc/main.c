/**
 * PC simulator for the BLDC HMI (Windows GDI backend, no SDL needed).
 *
 * Modes:
 *   sim_pc.exe                            Interactive 800x480 window, mouse acts as touch.
 *   sim_pc.exe --shot <tab> <out.raw>     Headless: render <tab>, advance a virtual clock,
 *                                         dump the RGB565 framebuffer and exit.
 *   sim_pc.exe --layout <tab>             Headless: boot to <tab>, then print the live
 *                                         widget tree (class, visibility, coordinates,
 *                                         label text / slider-arc-bar values) as text.
 *                                         <tab> = dashboard|monitor|control|graphs|diagnostics|settings
 *   Extra options:
 *     --ms <n>       Virtual milliseconds to run before the dump (default 4000).
 *     --demo          Enable Demo telemetry.
 *     --demo-run      Enable Demo and apply a representative non-zero command.
 *     --mode <name>   Start Control in speed|torque|position (default speed).
 *
 * Convert the raw dump with: python tools/raw2png.py out.raw out.png
 */
#include <math.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <windows.h>

#include "lvgl.h"
#include "ui.h"
#include "actions.h"
#include "demo_sim.h"
#include "screens.h"
#include "ui_mcu_profile.h"

#define SIM_HOR UI_DISPLAY_HOR_RES
#define SIM_VER UI_DISPLAY_VER_RES

// --demo: start with the runtime demo simulator ON (it boots OFF now). Handy
// for headless screenshots / a quick lively demo without tapping the brand.
static int s_start_demo = 0;
static int s_start_demo_run = 0;
static uint8_t s_start_mode = OP_MODE_SPEED;
static bool s_profile_drag = false;

static uint16_t s_fb[SIM_HOR * SIM_VER];
static lv_color_t s_draw_buf[UI_DRAW_BUF_PIXELS];
static HWND s_hwnd = NULL;
static bool s_mouse_pressed = false;
static int16_t s_mouse_x = 0, s_mouse_y = 0;

static uint64_t s_flush_pixels = 0;
static uint32_t s_flush_calls = 0;
static uint32_t s_frame_pixels = 0;
static uint32_t s_max_frame_pixels = 0;
static uint32_t s_frames_drawn = 0;
static uint32_t s_first_frame_pixels = 0;

/* ---------------------------------------------------------------- LVGL glue */

static void flush_cb(lv_disp_drv_t *drv, const lv_area_t *area, lv_color_t *px)
{
    int32_t w = lv_area_get_width(area);
    int32_t h = lv_area_get_height(area);
    uint32_t count = (uint32_t) (w * h);
    s_flush_pixels += count;
    s_flush_calls++;
    s_frame_pixels += count;

    for (int32_t y = area->y1; y <= area->y2; y++)
    {
        memcpy(&s_fb[y * SIM_HOR + area->x1], px, (size_t) w * sizeof(uint16_t));
        px += w;
    }
    if (s_hwnd)
    {
        RECT r = {area->x1, area->y1, area->x2 + 1, area->y2 + 1};
        InvalidateRect(s_hwnd, &r, FALSE);
    }
    if (lv_disp_flush_is_last(drv))
    {
        s_frames_drawn++;
        if (s_frames_drawn == 1) s_first_frame_pixels = s_frame_pixels;
        if (s_frame_pixels > s_max_frame_pixels) s_max_frame_pixels = s_frame_pixels;
        s_frame_pixels = 0;
    }
    lv_disp_flush_ready(drv);
}

static void mouse_read_cb(lv_indev_drv_t *drv, lv_indev_data_t *data)
{
    (void) drv;
    data->point.x = s_mouse_x;
    data->point.y = s_mouse_y;
    data->state   = s_mouse_pressed ? LV_INDEV_STATE_PRESSED : LV_INDEV_STATE_RELEASED;
}

static void lvgl_setup(void)
{
    lv_init();

    static lv_disp_draw_buf_t draw_buf;
    lv_disp_draw_buf_init(&draw_buf, s_draw_buf, NULL, UI_DRAW_BUF_PIXELS);

    static lv_disp_drv_t disp_drv;
    lv_disp_drv_init(&disp_drv);
    disp_drv.hor_res  = SIM_HOR;
    disp_drv.ver_res  = SIM_VER;
    disp_drv.flush_cb = flush_cb;
    disp_drv.draw_buf = &draw_buf;
    lv_disp_drv_register(&disp_drv);

    static lv_indev_drv_t indev_drv;
    lv_indev_drv_init(&indev_drv);
    indev_drv.type    = LV_INDEV_TYPE_POINTER;
    indev_drv.read_cb = mouse_read_cb;
    lv_indev_drv_register(&indev_drv);
}

/* ---------------------------------------------------------------- Win32 window */

static void paint(HWND hwnd)
{
    PAINTSTRUCT ps;
    HDC         dc = BeginPaint(hwnd, &ps);
    struct
    {
        BITMAPINFOHEADER h;
        DWORD            masks[3];
    } info;
    memset(&info, 0, sizeof(info));
    info.h.biSize        = sizeof(BITMAPINFOHEADER);
    info.h.biWidth       = SIM_HOR;
    info.h.biHeight      = -SIM_VER; /* top-down */
    info.h.biPlanes      = 1;
    info.h.biBitCount    = 16;
    info.h.biCompression = BI_BITFIELDS;
    info.masks[0]        = 0xF800;
    info.masks[1]        = 0x07E0;
    info.masks[2]        = 0x001F;
    StretchDIBits(dc, 0, 0, SIM_HOR, SIM_VER, 0, 0, SIM_HOR, SIM_VER, s_fb, (BITMAPINFO *) &info, DIB_RGB_COLORS,
                  SRCCOPY);
    EndPaint(hwnd, &ps);
}

static LRESULT CALLBACK wnd_proc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp)
{
    switch (msg)
    {
        case WM_PAINT:
            paint(hwnd);
            return 0;
        case WM_MOUSEMOVE:
        case WM_LBUTTONDOWN:
        case WM_LBUTTONUP:
            s_mouse_x = (int16_t) LOWORD(lp);
            s_mouse_y = (int16_t) HIWORD(lp);
            if (msg == WM_LBUTTONDOWN)
            {
                s_mouse_pressed = true;
                SetCapture(hwnd);
            }
            else if (msg == WM_LBUTTONUP)
            {
                s_mouse_pressed = false;
                ReleaseCapture();
            }
            return 0;
        case WM_DESTROY:
            PostQuitMessage(0);
            return 0;
    }
    return DefWindowProc(hwnd, msg, wp, lp);
}

static HWND create_window(void)
{
    WNDCLASS wc = {0};
    wc.lpfnWndProc   = wnd_proc;
    wc.hInstance     = GetModuleHandle(NULL);
    wc.hCursor       = LoadCursor(NULL, IDC_ARROW);
    wc.lpszClassName = "bldc_hmi_sim";
    RegisterClass(&wc);

    DWORD style = WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX;
    RECT  r     = {0, 0, SIM_HOR, SIM_VER};
    AdjustWindowRect(&r, style, FALSE);
    return CreateWindow(wc.lpszClassName, "BLDC HMI Simulator (800x480)", style, CW_USEDEFAULT, CW_USEDEFAULT,
                        r.right - r.left, r.bottom - r.top, NULL, NULL, wc.hInstance, NULL);
}

/* ---------------------------------------------------------------- modes */

typedef struct
{
    const char *name;
    void (*action)(lv_event_t *);
} tab_entry_t;

static const tab_entry_t TABS[] = {
        {"dashboard", action_tab_dashboard}, {"monitor", action_tab_monitor},
        {"control", action_tab_control},     {"graphs", action_tab_graphs},
        {"diagnostics", action_tab_diagnostics}, {"settings", action_tab_settings},
};

// Apply CLI startup state after ui_init(), because ui_init() intentionally
// restores the product-safe STOP/zero-command defaults on every boot.
static void apply_start_options(void)
{
    motorCmd.bits.opMode = s_start_mode;
#if defined(UI_DEMO_SIM) && UI_DEMO_SIM
    if (s_start_demo) demo_sim_toggle();
    if (s_start_demo_run)
    {
        switch (s_start_mode)
        {
            case OP_MODE_TORQUE:
                motorCmd.bits.ctrlValRaw = UI_MAX_CURRENT_MA * 5u / 6u;
                motorCmd.bits.limitRaw = UI_CTRL_MAX_RPM;
                break;
            case OP_MODE_POSITION:
                motorCmd.bits.ctrlValRaw = UI_POSITION_MAX_RAW / 2u;
                motorCmd.bits.limitRaw = UI_MAX_CURRENT_MA;
                break;
            default:
                motorCmd.bits.ctrlValRaw = UI_CTRL_MAX_RPM * 5u / 6u;
                motorCmd.bits.limitRaw = UI_MAX_CURRENT_MA;
                break;
        }
        motorCmd.bits.cmd = 1;
    }
#endif
}

// Boot the simulator, select a tab and advance the virtual clock until the
// screen settles. Shared by --shot (framebuffer) and --layout (widget tree).
// Returns 0 on success; TABS lookup failures return 2.
static int boot_to_tab(const char *tab, uint32_t virtual_ms)
{
    lvgl_setup();
    ui_init();
    apply_start_options();

    bool found = false;
    for (size_t i = 0; i < sizeof(TABS) / sizeof(TABS[0]); i++)
    {
        if (strcmp(TABS[i].name, tab) == 0)
        {
            TABS[i].action(NULL); /* actions ignore the event pointer */
            found = true;
            break;
        }
    }
    if (!found)
    {
        fprintf(stderr, "unknown tab '%s'\n", tab);
        return 2;
    }

    /* Advance a virtual clock: fast, deterministic, no real-time waits. The
       requested tab is built lazily by ui_tick() now (coalesced switch), so the
       heap is sampled AFTER the loop to reflect the settled screen. */
    for (uint32_t t = 0; t < virtual_ms; t += 5)
    {
        lv_tick_inc(5);
        ui_tick();
        lv_timer_handler();
    }
    return 0;
}

static int run_headless_shot(const char *tab, const char *out_path, uint32_t virtual_ms)
{
    int rc = boot_to_tab(tab, virtual_ms);
    if (rc != 0) return rc;

    lv_mem_monitor_t mon;
    lv_mem_monitor(&mon);
    printf("lvgl heap: used %u%%, free %u bytes (biggest block %u), frag %u%%\n", mon.used_pct,
           (unsigned) mon.free_size, (unsigned) mon.free_biggest_size, mon.frag_pct);
    fflush(stdout);

    FILE *f = fopen(out_path, "wb");
    if (!f)
    {
        fprintf(stderr, "cannot open '%s'\n", out_path);
        return 3;
    }
    fwrite(s_fb, sizeof(s_fb), 1, f);
    fclose(f);
    printf("wrote %s (%dx%d RGB565)\n", out_path, SIM_HOR, SIM_VER);
    return 0;
}

static void sim_click(int16_t x, int16_t y)
{
    s_mouse_x = x;
    s_mouse_y = y;
    s_mouse_pressed = true;
    for (uint32_t t = 0; t < 100; t += 5)
    {
        lv_tick_inc(5);
        ui_tick();
        lv_timer_handler();
    }
    s_mouse_pressed = false;
    for (uint32_t t = 0; t < 300; t += 5)
    {
        lv_tick_inc(5);
        ui_tick();
        lv_timer_handler();
    }
}

static int run_nav_click_test(void)
{
    lvgl_setup();
    ui_init();
    apply_start_options();

    for (uint32_t t = 0; t < 3000; t += 5)
    {
        lv_tick_inc(5);
        ui_tick();
        lv_timer_handler();
    }

    static const int16_t TAB_BTN_Y[6] = {8, 85, 162, 239, 316, 392};

    for (uint8_t i = 0; i < 6; i++)
    {
        sim_click((int16_t) (UI_GAP + UI_SIDEBAR_W / 2), (int16_t) (UI_GAP + TAB_BTN_Y[i] + 32));

        if (ui_current_tab != (ui_tab_t) i)
        {
            fprintf(stderr, "nav click test: failed on tab %u, got %u\n", (unsigned) i, (unsigned) ui_current_tab);
            return 5;
        }
    }
    printf("nav click test: PASS\n");
    return 0;
}

static int run_dashboard_band_test(void)
{
    int rc = boot_to_tab("dashboard", 4000);
    if (rc != 0) return rc;
    static const struct {
        uint16_t rpm;
        uint8_t fault;
        uint8_t expected;
    } cases[] = {
        {211, 0, 0}, // just below 85% of 250 RPM
        {212, 0, 1}, // reviewed high band begins at 85% (212 RPM)
        {251, 0, 2}, // beyond the hard 250 RPM limit
        {100, 1, 2}, // any real wire fault is danger
        {100, 0, 0}, // recovery must restore normal blue
    };

    for (size_t i = 0; i < sizeof(cases) / sizeof(cases[0]); i++)
    {
        motorStatusFast.raw = 0;
        motorStatusFast.bits.actualRpm = cases[i].rpm;
        motorStatusFast.bits.fault = cases[i].fault;
        for (uint32_t t = 0; t < 250; t += 5)
        {
            lv_tick_inc(5);
            ui_tick();
            lv_timer_handler();
        }
        uint8_t actual = ui_test_dashboard_speed_band();
        if (actual != cases[i].expected)
        {
            fprintf(stderr, "dashboard band test: rpm=%u fault=%u expected=%u got=%u\n",
                    (unsigned) cases[i].rpm, (unsigned) cases[i].fault,
                    (unsigned) cases[i].expected, (unsigned) actual);
            return 6;
        }

        lv_color_t expected_color = cases[i].expected == 0 ? lv_color_hex(0x075FE8)
                                  : cases[i].expected == 1 ? COLOR_WARN : COLOR_DANGER;
        lv_color_t actual_color = ui_test_dashboard_speed_color();
        if (lv_color_to32(actual_color) != lv_color_to32(expected_color))
        {
            fprintf(stderr, "dashboard band test: rpm=%u band=%u color=%08lx expected=%08lx\n",
                    (unsigned) cases[i].rpm, (unsigned) cases[i].expected,
                    (unsigned long) lv_color_to32(actual_color),
                    (unsigned long) lv_color_to32(expected_color));
            return 7;
        }
    }
    printf("dashboard band test: PASS (signal cyan <85%%, amber >=85%%, danger on fault/over-limit)\n");
    return 0;
}

static int run_headless_profile(const char *tab, uint32_t virtual_ms)
{
    int rc = boot_to_tab(tab, 4000);
    if (rc != 0) return rc;

    s_flush_pixels = 0;
    s_flush_calls = 0;
    s_max_frame_pixels = 0;
    s_frames_drawn = 0;
    s_frame_pixels = 0;

    for (uint32_t t = 0; t < virtual_ms; t += 5)
    {
        if (s_profile_drag)
        {
            // Center of position ring is at (348, 224), radius ~106
            double angle = (double) t * 0.005;
            s_mouse_x = (int16_t) (348 + (int) (106.0 * cos(angle)));
            s_mouse_y = (int16_t) (224 + (int) (106.0 * sin(angle)));
            s_mouse_pressed = true;
        }

        lv_tick_inc(5);
        ui_tick();
        lv_timer_handler();
    }

    if (s_profile_drag) s_mouse_pressed = false;

    uint64_t px_s = virtual_ms > 0 ? (s_flush_pixels * 1000ull / virtual_ms) : 0;
    uint32_t calls_s = virtual_ms > 0 ? (uint32_t) (s_flush_calls * 1000ull / virtual_ms) : 0;
    printf("%s profile: px/s=%llu, calls/s=%u, max-frame=%u, frames=%u\n",
           tab, (unsigned long long) px_s, (unsigned) calls_s,
           (unsigned) s_max_frame_pixels, (unsigned) s_frames_drawn);
    return 0;
}

static int run_switch_profile(const char *from, const char *to)
{
    int rc = boot_to_tab(from, 4000);
    if (rc != 0) return rc;

    s_flush_pixels = 0;
    s_flush_calls = 0;
    s_max_frame_pixels = 0;
    s_frames_drawn = 0;
    s_first_frame_pixels = 0;
    s_frame_pixels = 0;

    bool found = false;
    for (size_t i = 0; i < sizeof(TABS) / sizeof(TABS[0]); i++)
    {
        if (strcmp(TABS[i].name, to) == 0)
        {
            TABS[i].action(NULL);
            found = true;
            break;
        }
    }
    if (!found)
    {
        fprintf(stderr, "unknown target tab '%s'\n", to);
        return 2;
    }

    for (uint32_t t = 0; t < 100; t += 5)
    {
        lv_tick_inc(5);
        ui_tick();
        lv_timer_handler();
        if (s_frames_drawn >= 2) break;
    }

    printf("switch profile %s->%s: pixels=%llu frames=%u chrome-frame=%u max-frame=%u calls=%u\n",
           from, to, (unsigned long long) s_flush_pixels, (unsigned) s_frames_drawn,
           (unsigned) s_first_frame_pixels, (unsigned) s_max_frame_pixels, (unsigned) s_flush_calls);
    return 0;
}

/* ---------------------------------------------------------------- layout dump */

// "Vision via text": walk the live widget tree and print each object's class,
// visibility, coordinates and semantic payload. Gives agents the ability to
// assert on layout composition, active/hidden modes, label bindings and value
// mappings without depending on computer-vision OCR over rendered pixels.
static const char *widget_class_name(const lv_obj_t *obj)
{
    if (!obj || !obj->class_p) return "obj";
    if (lv_obj_check_type(obj, &lv_label_class))    return "label";
    if (lv_obj_check_type(obj, &lv_btn_class))      return "btn";
    if (lv_obj_check_type(obj, &lv_slider_class))   return "slider";
    if (lv_obj_check_type(obj, &lv_arc_class))      return "arc";
    if (lv_obj_check_type(obj, &lv_bar_class))      return "bar";
    if (lv_obj_check_type(obj, &lv_img_class))      return "img";
    if (lv_obj_check_type(obj, &lv_dropdown_class)) return "dropdown";
    if (lv_obj_check_type(obj, &lv_chart_class))    return "chart";
    if (lv_obj_check_type(obj, &lv_switch_class))   return "switch";
    if (lv_obj_check_type(obj, &lv_table_class))    return "table";
    return "obj";
}

// Compact single-line label extraction (newlines -> spaces, trimmed).
static void dump_label_text(char *out, size_t out_sz, const char *text)
{
    if (!out || out_sz == 0) return;
    if (!text) { out[0] = '\0'; return; }
    size_t i = 0;
    for (; text[i] && i + 1 < out_sz; i++)
        out[i] = (text[i] == '\n' || text[i] == '\r') ? ' ' : text[i];
    out[i] = '\0';
}

// Effective visibility: an object may be rendered invisible by a HIDDEN flag
// on any ancestor (the Control POSITION layout, the splash overlay, ...).
static bool obj_effectively_visible(const lv_obj_t *obj)
{
    for (const lv_obj_t *o = obj; o; o = lv_obj_get_parent(o))
        if (lv_obj_has_flag(o, LV_OBJ_FLAG_HIDDEN)) return false;
    return true;
}

static void dump_widget(lv_obj_t *obj, int depth)
{
    lv_area_t a;
    lv_obj_get_coords(obj, &a);
    const char *cls = widget_class_name(obj);
    char        extra[64] = "";

    if (strcmp(cls, "label") == 0)
    {
        char t[48];
        dump_label_text(t, sizeof(t), lv_label_get_text(obj));
        snprintf(extra, sizeof(extra), " \"%s\"", t);
    }
    else if (strcmp(cls, "slider") == 0)
        snprintf(extra, sizeof(extra), " val=%d", (int) lv_slider_get_value(obj));
    else if (strcmp(cls, "arc") == 0)
        snprintf(extra, sizeof(extra), " val=%d", (int) lv_arc_get_value(obj));
    else if (strcmp(cls, "bar") == 0)
        snprintf(extra, sizeof(extra), " val=%d", (int) lv_bar_get_value(obj));
    else if (strcmp(cls, "dropdown") == 0)
        snprintf(extra, sizeof(extra), " sel=%d", (int) lv_dropdown_get_selected(obj));

    printf("%*s%-8s %s x=%4d y=%4d w=%4d h=%4d%s\n", depth * 2, "", cls,
           obj_effectively_visible(obj) ? "vis" : "HID",
           (int) a.x1, (int) a.y1, (int) lv_obj_get_width(obj), (int) lv_obj_get_height(obj),
           extra);
}

static void dump_tree(lv_obj_t *obj, int depth)
{
    if (!obj || depth > 48) return;
    dump_widget(obj, depth);
    uint32_t n = lv_obj_get_child_cnt(obj);
    for (uint32_t i = 0; i < n; i++) dump_tree(lv_obj_get_child(obj, (int32_t) i), depth + 1);
}

static int run_layout_dump(const char *tab, uint32_t virtual_ms)
{
    int rc = boot_to_tab(tab, virtual_ms);
    if (rc != 0) return rc;

    lv_disp_t *disp = lv_disp_get_default();
    printf("== layer_top ==\n");
    dump_tree(lv_disp_get_layer_top(disp), 0);
    printf("== screen ==\n");
    dump_tree(lv_disp_get_scr_act(disp), 0);
    printf("== layer_sys ==\n");
    dump_tree(lv_disp_get_layer_sys(disp), 0);
    printf("layout dump: %s done\n", tab);
    return 0;
}

/* Reproduces the field failure: switch to Demo, open Monitor, command a
 * non-zero speed, and keep rendering while the real UART-owned globals are
 * deliberately poisoned. Passing proves both source isolation and that the
 * dynamic labels do not leak/fragment the constrained LVGL heap over time. */
static int run_demo_stress(uint32_t virtual_ms)
{
    lvgl_setup();
    ui_init();

    /* First exercise the actual dropdown callback and per-mode cache with no
       telemetry connection. This is the MCU failure path that used to leave
       Control stuck on the stale echoed layout. */
    int offline_mode_bad = 0;
    int offline_stop_bad = 0;
    motorConnected = UI_LINK_DISCONNECTED;

    // A STOP pressed before any START acknowledgement must still cancel the
    // outgoing command. This is the short disconnected/no-response window in
    // which the old feedback-only guard left cmd latched at 1.
    motorCmd.bits.cmd = 0;
    action_motor_start(NULL);
    if (motorCmd.bits.cmd != 1u) offline_stop_bad = 1;
    action_motor_stop(NULL);
    if (motorCmd.bits.cmd != 0u) offline_stop_bad = 1;

    action_tab_control(NULL);
    for (uint32_t t = 0; t < 2000u; t += 5u)
    {
        if (t == 250u)
        {
            motorCmd.bits.ctrlValRaw = 123u;
            motorCmd.bits.limitRaw = 1800u;
        }
        else if (t == 500u)
        {
            if (!ui_test_control_select_mode(OP_MODE_TORQUE)) offline_mode_bad = 1;
            motorCmd.bits.ctrlValRaw = 1500u;
            motorCmd.bits.limitRaw = 200u;
            motorConnected = UI_LINK_NO_RESPONSE;
        }
        else if (t == 750u)
        {
            if (!ui_test_control_select_mode(OP_MODE_POSITION)) offline_mode_bad = 1;
            motorCmd.bits.ctrlValRaw = 1800u;
            motorCmd.bits.limitRaw = 1900u;
        }
        else if (t == 1000u)
        {
            if (!ui_test_control_select_mode(OP_MODE_SPEED) ||
                motorCmd.bits.ctrlValRaw != 123u || motorCmd.bits.limitRaw != 1800u)
                offline_mode_bad = 1;
        }
        else if (t == 1250u)
        {
            if (!ui_test_control_select_mode(OP_MODE_TORQUE) ||
                motorCmd.bits.ctrlValRaw != 1500u || motorCmd.bits.limitRaw != 200u)
                offline_mode_bad = 1;
        }
        else if (t == 1500u)
        {
            if (!ui_test_control_select_mode(OP_MODE_POSITION) ||
                motorCmd.bits.ctrlValRaw != 1800u || motorCmd.bits.limitRaw != 1900u)
                offline_mode_bad = 1;
            motorConnected = UI_LINK_DISCONNECTED;
        }

        lv_tick_inc(5);
        ui_tick();
        lv_timer_handler();
    }

    demo_sim_toggle();
    action_tab_monitor(NULL);

    uint32_t min_free = UINT32_MAX;
    uint32_t min_biggest = UINT32_MAX;
    uint8_t max_frag = 0;
    int bad_source = 0;

    for (uint32_t t = 0; t < virtual_ms; t += 5)
    {
        /* Simulate an ungated real UART receiver updating in parallel. Demo
           values must remain selected and bounded despite these extremes. */
        motorStatusFast.raw = 0;
        motorStatusFast.bits.actualRpm = 1023;
        motorStatusFast.bits.iqCurrent = 255;
        motorStatusFast.bits.motorState = MOTOR_STATE_FAULT;
        motorStatusFast.bits.phaseCurrentRms = 65535;
        motorStatusFast.bits.voltage = 65535;
        motorStatusFast.bits.fault = 0x7F;
        motorStatusSlow.raw = UINT64_MAX;
        motorConnected = (uint8_t) ((t / 5) & 1u);

        if (t == 250)
        {
            motorCmd.bits.opMode = OP_MODE_SPEED;
            motorCmd.bits.ctrlValRaw = UI_CTRL_MAX_RPM;
            motorCmd.bits.limitRaw = UI_MAX_CURRENT_MA;
            motorCmd.bits.cmd = 1;
        }

        /* Exercise all three Control layouts during teardown/rebuild. This
           deliberately changes motorCmd directly: layout selection must be
           command-owned and must not depend on a connected/status echo. */
        if (t != 0 && t % 10000u == 0)
        {
            switch ((t / 10000u) % 3u)
            {
                case 1:
                    motorCmd.bits.opMode = OP_MODE_TORQUE;
                    motorCmd.bits.ctrlValRaw = UI_MAX_CURRENT_MA;
                    motorCmd.bits.limitRaw = UI_CTRL_MAX_RPM;
                    break;
                case 2:
                    motorCmd.bits.opMode = OP_MODE_POSITION;
                    motorCmd.bits.ctrlValRaw = UI_POSITION_MAX_RAW / 2;
                    motorCmd.bits.limitRaw = UI_MAX_CURRENT_MA;
                    break;
                default:
                    motorCmd.bits.opMode = OP_MODE_SPEED;
                    motorCmd.bits.ctrlValRaw = UI_CTRL_MAX_RPM;
                    motorCmd.bits.limitRaw = UI_MAX_CURRENT_MA;
                    break;
            }
            action_tab_control(NULL);
        }
        if (t != 0 && t % 10000u == 1000u) action_tab_monitor(NULL);

        lv_tick_inc(5);
        ui_tick();
        lv_timer_handler();

        const MotorStatusFast_t *shown = ui_motor_status_fast();
        if (shown->bits.actualRpm > UI_MAX_RPM || shown->bits.voltage > 56000u ||
            shown->bits.iqCurrent > UI_MAX_CURRENT_MA / 100u ||
            shown->bits.phaseCurrentRms > UI_MAX_CURRENT_MA ||
            shown->bits.motorState == MOTOR_STATE_FAULT)
            bad_source = 1;

        if (t % 100u == 0)
        {
            lv_mem_monitor_t mon;
            lv_mem_monitor(&mon);
            if (mon.free_size < min_free) min_free = mon.free_size;
            if (mon.free_biggest_size < min_biggest) min_biggest = mon.free_biggest_size;
            if (mon.frag_pct > max_frag) max_frag = mon.frag_pct;
        }
    }

    const MotorStatusFast_t *shown = ui_motor_status_fast();
    printf("demo stress: free-min %u, biggest-min %u, frag-max %u%%, final %u rpm\n",
           (unsigned) min_free, (unsigned) min_biggest, max_frag,
           (unsigned) shown->bits.actualRpm);
    if (offline_mode_bad || offline_stop_bad || bad_source || min_free < 6000u || min_biggest < 4000u)
    {
        fprintf(stderr, "demo stress: FAIL (offline-modes=%s, offline-stop=%s, source=%s)\n",
                offline_mode_bad ? "bad" : "ok", offline_stop_bad ? "bad" : "ok",
                bad_source ? "mixed" : "isolated");
        return 4;
    }
    printf("demo stress: PASS (offline STOP/mode cache, telemetry isolation, mode rebuilds, heap stable)\n");
    return 0;
}

static int run_interactive(void)
{
    lvgl_setup();
    s_hwnd = create_window();
    ShowWindow(s_hwnd, SW_SHOW);

    ui_init();
    apply_start_options();

    uint64_t last = GetTickCount64();
    for (;;)
    {
        MSG m;
        while (PeekMessage(&m, NULL, 0, 0, PM_REMOVE))
        {
            if (m.message == WM_QUIT) return 0;
            TranslateMessage(&m);
            DispatchMessage(&m);
        }
        uint64_t now = GetTickCount64();
        lv_tick_inc((uint32_t) (now - last));
        last = now;
        ui_tick();
        lv_timer_handler();
        Sleep(5);
    }
}

static int parse_mode_name(const char *name, uint8_t *mode)
{
    if (strcmp(name, "speed") == 0) *mode = OP_MODE_SPEED;
    else if (strcmp(name, "torque") == 0) *mode = OP_MODE_TORQUE;
    else if (strcmp(name, "position") == 0) *mode = OP_MODE_POSITION;
    else return 0;
    return 1;
}

static int parse_virtual_ms(const char *text, uint32_t *value)
{
    char *end = NULL;
    if (!text || text[0] == '\0' || text[0] == '-') return 0;
    unsigned long parsed = strtoul(text, &end, 10);
    if (!end || *end != '\0' || parsed < 1ul || parsed > 600000ul) return 0;
    *value = (uint32_t) parsed;
    return 1;
}

int main(int argc, char **argv)
{
    const char *shot_tab = NULL;
    const char *out_path = NULL;
    const char *layout_tab = NULL;
    const char *profile_tab = NULL;
    const char *switch_from = NULL;
    const char *switch_to = NULL;
    uint32_t    virtual_ms = 4000;
    int         stress_demo = 0;
    int         test_nav = 0;
    int         test_dashboard_bands = 0;
    int         virtual_ms_set = 0;

    for (int i = 1; i < argc; i++)
    {
        if (strcmp(argv[i], "--shot") == 0 && i + 2 < argc)
        {
            shot_tab = argv[++i];
            out_path = argv[++i];
        }
        else if (strcmp(argv[i], "--layout") == 0 && i + 1 < argc)
        {
            layout_tab = argv[++i];
        }
        else if (strcmp(argv[i], "--profile") == 0 && i + 1 < argc)
        {
            profile_tab = argv[++i];
        }
        else if (strcmp(argv[i], "--profile-switch") == 0 && i + 2 < argc)
        {
            switch_from = argv[++i];
            switch_to = argv[++i];
        }
        else if (strcmp(argv[i], "--ms") == 0 && i + 1 < argc)
        {
            if (!parse_virtual_ms(argv[++i], &virtual_ms))
            {
                fprintf(stderr, "--ms must be an integer from 1 to 600000\n");
                return 1;
            }
            virtual_ms_set = 1;
        }
        else if (strcmp(argv[i], "--mode") == 0 && i + 1 < argc)
        {
            if (!parse_mode_name(argv[++i], &s_start_mode))
            {
                fprintf(stderr, "--mode must be speed, torque or position\n");
                return 1;
            }
        }
        else if (strcmp(argv[i], "--demo") == 0)
        {
            s_start_demo = 1;
        }
        else if (strcmp(argv[i], "--demo-run") == 0)
        {
            s_start_demo = 1;
            s_start_demo_run = 1;
        }
        else if (strcmp(argv[i], "--drag") == 0)
        {
            s_profile_drag = true;
        }
        else if (strcmp(argv[i], "--stress-demo") == 0)
        {
            stress_demo = 1;
        }
        else if (strcmp(argv[i], "--test-nav") == 0)
        {
            test_nav = 1;
        }
        else if (strcmp(argv[i], "--test-dashboard-bands") == 0)
        {
            test_dashboard_bands = 1;
        }
        else
        {
            fprintf(stderr, "usage: %s [--shot <tab> <out.raw> | --layout <tab> | --profile <tab> [--drag] | --profile-switch <from> <to> | --stress-demo | --test-nav | --test-dashboard-bands] [--ms <n>] [--demo|--demo-run] [--mode speed|torque|position]\n",
                    argv[0]);
            return 1;
        }
    }

    int run_modes = (shot_tab ? 1 : 0) + (layout_tab ? 1 : 0) +
                    (profile_tab ? 1 : 0) + (switch_from ? 1 : 0) +
                    (stress_demo ? 1 : 0) + (test_nav ? 1 : 0) +
                    (test_dashboard_bands ? 1 : 0);
    if (run_modes > 1)
    {
        fprintf(stderr, "choose only one headless/test mode\n");
        return 1;
    }
    if (s_profile_drag && !profile_tab)
    {
        fprintf(stderr, "--drag is only valid with --profile\n");
        return 1;
    }
    if (s_profile_drag && (strcmp(profile_tab, "control") != 0 || s_start_mode != OP_MODE_POSITION))
    {
        fprintf(stderr, "--drag requires --profile control --mode position\n");
        return 1;
    }

    if (stress_demo)
    {
        if (!virtual_ms_set) virtual_ms = 120000;
        return run_demo_stress(virtual_ms);
    }
    if (test_nav) return run_nav_click_test();
    if (test_dashboard_bands) return run_dashboard_band_test();
    if (switch_from) return run_switch_profile(switch_from, switch_to);
    if (profile_tab)
    {
        if (!virtual_ms_set) virtual_ms = 10000;
        return run_headless_profile(profile_tab, virtual_ms);
    }
    if (layout_tab) return run_layout_dump(layout_tab, virtual_ms);
    if (shot_tab) return run_headless_shot(shot_tab, out_path, virtual_ms);
    return run_interactive();
}
