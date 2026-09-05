/* Minimal Windows GDI simulator for the 800x480 Smart Hub LVGL UI. */
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <windows.h>

#include "lvgl.h"
#include "ui.h"
#include "ui_mcu_profile.h"

#define SIM_HOR ((int32_t) UI_DISPLAY_HOR_RES)
#define SIM_VER ((int32_t) UI_DISPLAY_VER_RES)

static uint16_t s_framebuffer[UI_DISPLAY_HOR_RES * UI_DISPLAY_VER_RES];
static lv_color_t s_draw_buffer[UI_DRAW_BUF_PIXELS];
static HWND s_window;
static bool s_mouse_pressed;
static int16_t s_mouse_x;
static int16_t s_mouse_y;
static uint64_t s_flushed_pixels;

static void flush_cb(lv_disp_drv_t *driver, const lv_area_t *area, lv_color_t *pixels)
{
    int32_t width = lv_area_get_width(area);
    int32_t height = lv_area_get_height(area);
    s_flushed_pixels += (uint64_t) width * (uint64_t) height;

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
        lv_tick_inc(10u);
        ui_tick();
        lv_timer_handler();
    }
    lv_refr_now(NULL);
}

static void print_heap(void)
{
    lv_mem_monitor_t memory;
    lv_mem_monitor(&memory);
    printf("lvgl heap: used=%u%% free=%u biggest=%u frag=%u%%\n",
           (unsigned) memory.used_pct, (unsigned) memory.free_size,
           (unsigned) memory.free_biggest_size, (unsigned) memory.frag_pct);
}

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

static int run_shot(const char *path)
{
    lvgl_setup();
    ui_init();
    advance_ui(500u);

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
    print_heap();
    printf("shot: wrote %s (%ux%u RGB565)\n", path,
           (unsigned) UI_DISPLAY_HOR_RES, (unsigned) UI_DISPLAY_VER_RES);
    return 0;
}

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
    (void) wparam;
    switch (message) {
        case WM_PAINT:
            paint(window);
            return 0;
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
    lvgl_setup();
    ui_init();
    s_window = create_window();
    if (s_window == NULL) {
        fputs("cannot create simulator window\n", stderr);
        return 1;
    }
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
            lv_tick_inc(delta);
            ui_tick();
            lv_timer_handler();
        }
        Sleep(1u);
    }
}

int main(int argc, char **argv)
{
    if (argc == 1) {
        return run_interactive();
    }
    if (argc == 2 && strcmp(argv[1], "--smoke") == 0) {
        return run_smoke();
    }
    if (argc == 3 && strcmp(argv[1], "--shot") == 0) {
        return run_shot(argv[2]);
    }

    fprintf(stderr, "Usage: %s [--smoke | --shot <out.raw>]\n", argv[0]);
    return 2;
}
