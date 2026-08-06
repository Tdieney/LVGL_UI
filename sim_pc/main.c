/**
 * PC simulator for the BLDC HMI (Windows GDI backend, no SDL needed).
 *
 * Modes:
 *   sim_pc.exe                            Interactive 800x480 window, mouse acts as touch.
 *   sim_pc.exe --shot <tab> <out.raw>     Headless: render <tab>, advance a virtual clock,
 *                                         dump the RGB565 framebuffer and exit.
 *                                         <tab> = dashboard|monitor|control|graphs|diagnostics|settings
 *   Extra options:
 *     --ms <n>       Virtual milliseconds to run before the dump (default 4000).
 *
 * Convert the raw dump with: python tools/raw2png.py out.raw out.png
 */
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

static uint16_t s_fb[SIM_HOR * SIM_VER];
static lv_color_t s_draw_buf[UI_DRAW_BUF_PIXELS];
static HWND s_hwnd = NULL;
static bool s_mouse_pressed = false;
static int16_t s_mouse_x = 0, s_mouse_y = 0;

/* ---------------------------------------------------------------- LVGL glue */

static void flush_cb(lv_disp_drv_t *drv, const lv_area_t *area, lv_color_t *px)
{
    int32_t w = lv_area_get_width(area);
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
    info.masks[0]        = 0xF800; /* R */
    info.masks[1]        = 0x07E0; /* G */
    info.masks[2]        = 0x001F; /* B */
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

static int run_headless_shot(const char *tab, const char *out_path, uint32_t virtual_ms)
{
    lvgl_setup();
    ui_init();
#if defined(UI_DEMO_SIM) && UI_DEMO_SIM
    if (s_start_demo) demo_sim_toggle();
#endif

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

/* Reproduces the field failure: switch to Demo, open Monitor, command a
 * non-zero speed, and keep rendering while the real UART-owned globals are
 * deliberately poisoned. Passing proves both source isolation and that the
 * dynamic labels do not leak/fragment the constrained LVGL heap over time. */
static int run_demo_stress(uint32_t virtual_ms)
{
    lvgl_setup();
    ui_init();
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
        motorStatusFast.bits.actualTorque = 255;
        motorStatusFast.bits.motorState = MOTOR_STATE_FAULT;
        motorStatusFast.bits.current = 65535;
        motorStatusFast.bits.voltage = 65535;
        motorStatusFast.bits.fault = 0x7F;
        motorStatusSlow.raw = UINT64_MAX;
        motorConnected = (uint8_t) ((t / 5) & 1u);

        if (t == 250)
        {
            motorCmd.bits.opMode = OP_MODE_SPEED;
            motorCmd.bits.ctrlValRaw = 320;
            motorCmd.bits.limitRaw = 12000;
            motorCmd.bits.cmd = 1;
        }

        /* Exercise teardown/rebuild too, but spend most time on Monitor. */
        if (t != 0 && t % 10000u == 0) action_tab_control(NULL);
        if (t != 0 && t % 10000u == 1000u) action_tab_monitor(NULL);

        lv_tick_inc(5);
        ui_tick();
        lv_timer_handler();

        const MotorStatusFast_t *shown = ui_motor_status_fast();
        if (shown->bits.actualRpm > UI_MAX_RPM || shown->bits.voltage > 56000u ||
            shown->bits.current > 20000u || shown->bits.motorState == MOTOR_STATE_FAULT)
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
    if (bad_source || min_free < 6000u || min_biggest < 4000u ||
        shown->bits.actualRpm < 250u)
    {
        fprintf(stderr, "demo stress: FAIL (source=%s)\n", bad_source ? "mixed" : "isolated");
        return 4;
    }
    printf("demo stress: PASS (telemetry isolated, heap stable)\n");
    return 0;
}

static int run_interactive(void)
{
    lvgl_setup();
    s_hwnd = create_window();
    ShowWindow(s_hwnd, SW_SHOW);

    ui_init();
#if defined(UI_DEMO_SIM) && UI_DEMO_SIM
    if (s_start_demo) demo_sim_toggle();
#endif

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

int main(int argc, char **argv)
{
    const char *shot_tab = NULL;
    const char *out_path = NULL;
    uint32_t    virtual_ms = 4000;
    int         stress_demo = 0;
    int         virtual_ms_set = 0;

    for (int i = 1; i < argc; i++)
    {
        if (strcmp(argv[i], "--shot") == 0 && i + 2 < argc)
        {
            shot_tab = argv[++i];
            out_path = argv[++i];
        }
        else if (strcmp(argv[i], "--ms") == 0 && i + 1 < argc)
        {
            virtual_ms = (uint32_t) strtoul(argv[++i], NULL, 10);
            virtual_ms_set = 1;
        }
        else if (strcmp(argv[i], "--demo") == 0)
        {
            s_start_demo = 1;
        }
        else if (strcmp(argv[i], "--stress-demo") == 0)
        {
            stress_demo = 1;
        }
        else
        {
            fprintf(stderr, "usage: %s [--shot <tab> <out.raw>] [--ms <n>] [--demo] [--stress-demo]\n", argv[0]);
            return 1;
        }
    }

    if (stress_demo)
    {
        if (!virtual_ms_set) virtual_ms = 60000;
        return run_demo_stress(virtual_ms);
    }
    if (shot_tab) return run_headless_shot(shot_tab, out_path, virtual_ms);
    return run_interactive();
}
