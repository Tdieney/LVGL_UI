/* Smart Studio UI simulator (Windows, GDI).
 *
 *   sim_pc.exe                 interactive window at the panel's real size
 *   sim_pc.exe --ppi 109       same, with the monitor's pixel density given
 *   sim_pc.exe --smoke         render once and check the flush path
 *   sim_pc.exe --regression    full regression suite
 *   sim_pc.exe --shot f.raw    one 480 x 320 RGB565 frame (Light, Home scene)
 *   sim_pc.exe --shots dir     every named scenario as .raw frames
 */
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <windowsx.h>

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "lvgl.h"
#include "ui.h"
#include "ui_model.h"
#include "ui_fonts.h"
#include "ui_icons.h"
#include "ui_mcu_profile.h"
#include "ui_test_hooks.h"

#define SIM_W ((int)UI_DISPLAY_HOR_RES)
#define SIM_H ((int)UI_DISPLAY_VER_RES)

/* =========================================================================
 * Host display and pointer port (never part of the MCU export)
 * ========================================================================= */

static lv_color_t s_draw_buffer[UI_DRAW_BUF_PIXELS];
static lv_color_t s_framebuffer[SIM_W * SIM_H];
static lv_disp_draw_buf_t s_draw;
static lv_disp_drv_t s_display_driver;
static lv_indev_drv_t s_pointer_driver;
static lv_indev_t *s_pointer;
static lv_point_t s_point;
static bool s_pressed;
static uint64_t s_flush_calls;
static uint64_t s_flush_pixels;

static void flush_cb(lv_disp_drv_t *driver, const lv_area_t *area, lv_color_t *pixels)
{
    const int width = area->x2 - area->x1 + 1;
    s_flush_calls++;
    s_flush_pixels += (uint64_t)width * (uint64_t)(area->y2 - area->y1 + 1);
    for (int y = area->y1; y <= area->y2; y++) {
        memcpy(&s_framebuffer[y * SIM_W + area->x1], pixels, (size_t)width * sizeof(*pixels));
        pixels += width;
    }
    lv_disp_flush_ready(driver);
}

static void pointer_read_cb(lv_indev_drv_t *driver, lv_indev_data_t *data)
{
    (void)driver;
    data->point = s_point;
    data->state = s_pressed ? LV_INDEV_STATE_PRESSED : LV_INDEV_STATE_RELEASED;
}

static bool host_init(void)
{
    lv_init();
    lv_disp_draw_buf_init(&s_draw, s_draw_buffer, NULL, UI_DRAW_BUF_PIXELS);
    lv_disp_drv_init(&s_display_driver);
    s_display_driver.hor_res = SIM_W;
    s_display_driver.ver_res = SIM_H;
    s_display_driver.draw_buf = &s_draw;
    s_display_driver.flush_cb = flush_cb;
    if (!lv_disp_drv_register(&s_display_driver)) return false;
    lv_indev_drv_init(&s_pointer_driver);
    s_pointer_driver.type = LV_INDEV_TYPE_POINTER;
    s_pointer_driver.read_cb = pointer_read_cb;
    s_pointer = lv_indev_drv_register(&s_pointer_driver);
    return s_pointer != NULL;
}

/* Deterministic time for tests and captures: 5 ms steps. */
static void step(uint32_t ms)
{
    while (ms) {
        uint32_t d = ms > 5 ? 5 : ms;
        lv_tick_inc(d);
        (void)lv_timer_handler();
        ms -= d;
    }
}

/* Desktop time: advance the whole clock after an OS pause, but service each
 * overdue timer once instead of replaying old input samples. */
static void realtime_step(uint32_t ms)
{
    if (!ms) return;
    lv_tick_inc(ms);
    (void)lv_timer_handler();
}

static void pointer_set(int x, int y, bool pressed)
{
    s_point.x = (lv_coord_t)x;
    s_point.y = (lv_coord_t)y;
    s_pressed = pressed;
}

static void service_pointer(void)
{
    lv_indev_read_timer_cb(s_pointer_driver.read_timer);
}

/* One native sample now. Windows may report a new position only in the
 * button-up message, so deliver it as a pressed sample first. */
static void pointer_now(int x, int y, bool pressed)
{
    if (s_pressed && !pressed && (s_point.x != x || s_point.y != y)) {
        pointer_set(x, y, true);
        service_pointer();
    }
    pointer_set(x, y, pressed);
    service_pointer();
}

/* Capture or focus loss is a cancellation, not a release: the UI commits an
 * established Light drag once and does not page or toggle. */
static void release_pointer(void)
{
    if (s_pressed) {
        lv_obj_t *active = s_pointer->proc.types.pointer.act_obj;
        if (active) (void)lv_event_send(active, LV_EVENT_PRESS_LOST, s_pointer);
        lv_indev_reset(s_pointer, NULL);
        lv_indev_wait_release(s_pointer);
    }
    s_pressed = false;
    service_pointer();
}

static void reset_flush_counters(void)
{
    s_flush_calls = 0;
    s_flush_pixels = 0;
}

static bool regular_file(const char *path)
{
    DWORD attr = GetFileAttributesA(path);
    HANDLE file;
    LARGE_INTEGER size;
    bool ok;
    if (attr == INVALID_FILE_ATTRIBUTES || (attr & FILE_ATTRIBUTE_DIRECTORY)) return false;
    file = CreateFileA(path, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (file == INVALID_HANDLE_VALUE) return false;
    ok = GetFileType(file) == FILE_TYPE_DISK && GetFileSizeEx(file, &size) &&
         size.QuadPart == (LONGLONG)sizeof(s_framebuffer);
    CloseHandle(file);
    return ok;
}

static bool save_framebuffer(const char *path)
{
    FILE *f;
    bool ok;
    if (!path) return false;
    f = fopen(path, "wb");
    if (!f) return false;
    ok = fwrite(s_framebuffer, 1, sizeof(s_framebuffer), f) == sizeof(s_framebuffer);
    if (fclose(f) != 0) ok = false;
    return ok && regular_file(path);
}

static uint16_t pixel(int x, int y)
{
    return s_framebuffer[y * SIM_W + x].full;
}

/* =========================================================================
 * Allocator tracing (GNU --wrap; host only)
 * ========================================================================= */

typedef struct {
    size_t samples, min_free, largest_at_min, largest_request, failures;
} memory_trace_t;

static memory_trace_t s_memory = { 0, SIZE_MAX, 0, 0, 0 };

void *__real_lv_mem_alloc(size_t size);
void *__real_lv_mem_realloc(void *pointer, size_t size);
void *__wrap_lv_mem_alloc(size_t size);
void *__wrap_lv_mem_realloc(void *pointer, size_t size);

static void memory_sample(size_t requested, void *result)
{
    lv_mem_monitor_t monitor;
    s_memory.samples++;
    if (requested > s_memory.largest_request) s_memory.largest_request = requested;
    if (requested && !result) s_memory.failures++;
    lv_mem_monitor(&monitor);
    if (monitor.free_size < s_memory.min_free) {
        s_memory.min_free = monitor.free_size;
        s_memory.largest_at_min = monitor.free_biggest_size;
    }
}

void *__wrap_lv_mem_alloc(size_t size)
{
    void *result = __real_lv_mem_alloc(size);
    memory_sample(size, result);
    return result;
}

void *__wrap_lv_mem_realloc(void *pointer, size_t size)
{
    void *result = __real_lv_mem_realloc(pointer, size);
    memory_sample(size, result);
    return result;
}

/* =========================================================================
 * Real-size view
 *
 * The panel's active area is 73.44 x 48.96 mm. The window shows it at that
 * size using, in order: --ppi, a calibration saved for this monitor
 * resolution (keys + and -), the density Windows reports, or 96 ppi.
 * ========================================================================= */

typedef enum { VIEW_REAL = 0, VIEW_PIXEL, VIEW_DOUBLE, VIEW_COUNT } view_t;

static void actual_size(unsigned ppi, int *width, int *height)
{
    *width = (int)(((uint64_t)ppi * UI_PANEL_WIDTH_UM + 12700u) / 25400u);
    *height = (int)(((uint64_t)ppi * UI_PANEL_HEIGHT_UM + 12700u) / 25400u);
}

/* Whole number 40..600. */
static bool parse_ppi(const char *text, unsigned *ppi)
{
    unsigned value = 0, digits = 0;
    for (; *text >= '0' && *text <= '9'; text++) {
        if (++digits > 3) return false;
        value = value * 10u + (unsigned)(*text - '0');
    }
    if (!digits || *text || value < 40u || value > 600u) return false;
    *ppi = value;
    return true;
}

/* Calibration file: one "<width>x<height> <ppi>" line per monitor resolution. */
static unsigned calibration_load(const char *path, int width, int height)
{
    FILE *f = fopen(path, "r");
    char line[64];
    unsigned found = 0;
    if (!f) return 0;
    while (fgets(line, sizeof(line), f)) {
        int w, h;
        unsigned ppi;
        if (sscanf(line, "%dx%d %u", &w, &h, &ppi) == 3 && w == width && h == height && ppi >= 40u && ppi <= 600u) {
            found = ppi;
        }
    }
    fclose(f);
    return found;
}

static bool calibration_save(const char *path, int width, int height, unsigned ppi)
{
    char lines[16][64];
    unsigned count = 0;
    char line[64];
    FILE *f = fopen(path, "r");
    if (f) {
        while (count < 15 && fgets(line, sizeof(line), f)) {
            int w, h;
            unsigned p;
            if (sscanf(line, "%dx%d %u", &w, &h, &p) == 3 && !(w == width && h == height)) {
                (void)snprintf(lines[count++], sizeof(lines[0]), "%dx%d %u\n", w, h, p);
            }
        }
        fclose(f);
    }
    (void)snprintf(lines[count++], sizeof(lines[0]), "%dx%d %u\n", width, height, ppi);
    f = fopen(path, "w");
    if (!f) return false;
    for (unsigned i = 0; i < count; i++) fputs(lines[i], f);
    return fclose(f) == 0;
}

typedef HRESULT (WINAPI *monitor_dpi_fn)(HMONITOR, int, UINT *, UINT *);
typedef BOOL (WINAPI *dpi_context_fn)(HANDLE);

static view_t s_view = VIEW_REAL;
static unsigned s_ppi_override;   /* --ppi */
static unsigned s_ppi;            /* density in use for the real-size view */
static const char *s_ppi_source = "";
static int s_monitor_w, s_monitor_h;
static char s_calibration_path[MAX_PATH];

static void dpi_aware(void)
{
    /* -4 is DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2: true monitor pixels. */
    dpi_context_fn set = (dpi_context_fn)(void (*)(void))GetProcAddress(GetModuleHandleA("user32.dll"), "SetProcessDpiAwarenessContext");
    if (!set || !set((HANDLE)(INT_PTR)-4)) SetProcessDPIAware();
}

static void calibration_path_init(void)
{
    DWORD n = GetModuleFileNameA(NULL, s_calibration_path, MAX_PATH);
    char *slash;
    if (!n || n >= MAX_PATH) {
        s_calibration_path[0] = '\0';
        return;
    }
    slash = strrchr(s_calibration_path, '\\');
    if (slash && (size_t)(slash - s_calibration_path) + 16u < MAX_PATH) strcpy(slash + 1, "sim_view.txt");
}

static void resolve_ppi(HWND window)
{
    static monitor_dpi_fn raw;
    static bool loaded;
    HMONITOR monitor = MonitorFromWindow(window, MONITOR_DEFAULTTONEAREST);
    MONITORINFO info;
    UINT x = 0, y = 0;
    unsigned saved;
    info.cbSize = sizeof(info);
    s_monitor_w = s_monitor_h = 0;
    if (GetMonitorInfoA(monitor, &info)) {
        s_monitor_w = info.rcMonitor.right - info.rcMonitor.left;
        s_monitor_h = info.rcMonitor.bottom - info.rcMonitor.top;
    }
    if (s_ppi_override) {
        s_ppi = s_ppi_override;
        s_ppi_source = "--ppi";
        return;
    }
    saved = s_calibration_path[0] ? calibration_load(s_calibration_path, s_monitor_w, s_monitor_h) : 0;
    if (saved) {
        s_ppi = saved;
        s_ppi_source = "your calibration";
        return;
    }
    if (!loaded) {
        HMODULE shcore = LoadLibraryA("shcore.dll");
        loaded = true;
        if (shcore) raw = (monitor_dpi_fn)(void (*)(void))GetProcAddress(shcore, "GetDpiForMonitor");
    }
    /* 2 is MDT_RAW_DPI: from the physical size the monitor reports. */
    if (raw && raw(monitor, 2, &x, &y) == S_OK && x >= 40u && x <= 600u) {
        s_ppi = x;
        s_ppi_source = "reported by Windows";
        return;
    }
    s_ppi = 96;
    s_ppi_source = "assumed";
}

static void view_size(int *width, int *height)
{
    if (s_view == VIEW_REAL) actual_size(s_ppi, width, height);
    else if (s_view == VIEW_DOUBLE) { *width = 2 * SIM_W; *height = 2 * SIM_H; }
    else { *width = SIM_W; *height = SIM_H; }
}

static void apply_view(HWND window, bool announce)
{
    static const char *const titles[VIEW_COUNT] = {
        "Smart Studio 3.5\" - real size", "Smart Studio 3.5\" - pixel 1:1", "Smart Studio 3.5\" - pixel 2:1"
    };
    RECT outer, client;
    int width, height;
    if (IsIconic(window)) return;
    if (s_view == VIEW_REAL) resolve_ppi(window);
    view_size(&width, &height);
    if (!GetWindowRect(window, &outer) || !GetClientRect(window, &client)) return;
    if (client.right != width || client.bottom != height) {
        SetWindowPos(window, NULL, 0, 0, (outer.right - outer.left) - client.right + width,
                     (outer.bottom - outer.top) - client.bottom + height,
                     SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE);
    }
    SetWindowTextA(window, titles[s_view]);
    InvalidateRect(window, NULL, FALSE);
    if (!announce) return;
    if (s_view == VIEW_REAL) {
        printf("View: real size, 73.4 x 49.0 mm = %d x %d px at %u ppi (%s), monitor %dx%d.\n",
               width, height, s_ppi, s_ppi_source, s_monitor_w, s_monitor_h);
        printf("      Hold the LCD or a ruler to the screen: the picture must be 73 mm wide.\n"
               "      Press + or - to adjust; the value is saved for this monitor resolution.\n");
    } else {
        printf("View: %d x %d px, one panel pixel = %d screen pixel(s); larger than the real panel.\n",
               width, height, width / SIM_W);
    }
    fflush(stdout);
}

static void adjust_ppi(HWND window, int delta)
{
    unsigned next = (unsigned)((int)s_ppi + delta);
    if (s_view != VIEW_REAL || next < 40u || next > 600u) return;
    s_ppi_override = 0;
    if (!s_calibration_path[0] || !calibration_save(s_calibration_path, s_monitor_w, s_monitor_h, next)) {
        fprintf(stderr, "Could not save the calibration\n");
        s_ppi_override = next;
    }
    apply_view(window, true);
}

/* Native mouse input mapped to panel coordinates. Returns true if handled. */
static bool window_input(HWND window, UINT message, WPARAM wparam, LPARAM lparam)
{
    RECT r;
    int x, y;
    switch (message) {
    case WM_LBUTTONDOWN:
    case WM_MOUSEMOVE:
    case WM_LBUTTONUP:
        if (message == WM_MOUSEMOVE && GetCapture() != window) return true;
        if (!GetClientRect(window, &r) || r.right <= 0 || r.bottom <= 0) {
            release_pointer();
            if (GetCapture() == window) ReleaseCapture();
            return true;
        }
        if (message == WM_LBUTTONDOWN) SetCapture(window);
        x = (int)((int64_t)GET_X_LPARAM(lparam) * SIM_W / r.right);
        y = (int)((int64_t)GET_Y_LPARAM(lparam) * SIM_H / r.bottom);
        if (x < 0) x = 0;
        if (x > SIM_W - 1) x = SIM_W - 1;
        if (y < 0) y = 0;
        if (y > SIM_H - 1) y = SIM_H - 1;
        pointer_now(x, y, message != WM_LBUTTONUP);
        if (message == WM_LBUTTONUP && GetCapture() == window) ReleaseCapture();
        return true;
    case WM_CAPTURECHANGED:
    case WM_CANCELMODE:
    case WM_KILLFOCUS:
        release_pointer();
        if (GetCapture() == window) ReleaseCapture();
        return true;
    case WM_SIZE:
        if (wparam == SIZE_MINIMIZED) {
            release_pointer();
            if (GetCapture() == window) ReleaseCapture();
        }
        return false;
    default:
        return false;
    }
}

/* =========================================================================
 * Demo PC: prints the JSON the firmware would send, answers baud requests
 * ========================================================================= */

static bool s_fail_next_baud;
static uint32_t s_baud_request;
static uint32_t s_baud_due;
static bool s_env_available = true;

static const char *scene_wire_name(studio_scene_t scene)
{
    return scene == STUDIO_SCENE_HOME ? "WELCOME" : scene == STUDIO_SCENE_SLEEP ? "SLEEP" : "LEAVE";
}

static void demo_action(const studio_cmd_t *cmd, void *context)
{
    (void)context;
    switch (cmd->kind) {
    case STUDIO_CMD_LIGHT:
        printf("{\"event\":\"LIGHT\",\"power\":%d,\"bright\":%d}\n", cmd->data.light.power, cmd->data.light.bright);
        break;
    case STUDIO_CMD_AC:
        printf("{\"event\":\"AC\",\"power\":%d,\"temp\":%d}\n", cmd->data.ac.power, cmd->data.ac.temp);
        break;
    case STUDIO_CMD_SCENE:
        printf("{\"event\":\"SCENE\",\"mode\":\"%s\"}\n", scene_wire_name(cmd->data.scene));
        break;
    case STUDIO_CMD_BAUD:
        printf("{\"event\":\"BAUD\",\"val\":%lu}\n", (unsigned long)cmd->data.baud);
        s_baud_request = cmd->data.baud;
        s_baud_due = lv_tick_get() + 1200u;
        break;
    default:
        break;
    }
    fflush(stdout);
}

static void demo_env(void)
{
    studio_cmd_t cmd = { .kind = STUDIO_CMD_ENV };
    cmd.data.env.temp = 27;
    cmd.data.env.humid = 58;
    cmd.data.env.valid = s_env_available;
    (void)ui_apply(&cmd);
    ui_note_rx();
}

static void demo_scene(studio_scene_t scene)
{
    studio_cmd_t cmd = { .kind = STUDIO_CMD_SCENE };
    cmd.data.scene = scene;
    (void)ui_apply(&cmd);
    ui_note_rx();
}

/* =========================================================================
 * Regression helpers
 * ========================================================================= */

static unsigned s_checks;
#define CHECK(cond) do { \
    s_checks++; \
    if (!(cond)) { \
        fprintf(stderr, "FAIL %s:%d: %s (tick %lu)\n", __FILE__, __LINE__, #cond, (unsigned long)lv_tick_get()); \
        exit(1); \
    } \
} while (0)

static unsigned s_actions;
static studio_cmd_t s_last;
static uint32_t s_action_times[4096];
static size_t s_min_settled_free = SIZE_MAX;

static void record_action(const studio_cmd_t *cmd, void *context)
{
    CHECK(context == &s_actions);
    if (s_actions < sizeof(s_action_times) / sizeof(s_action_times[0])) s_action_times[s_actions] = lv_tick_get();
    s_actions++;
    s_last = *cmd;
}

static const ui_test_hooks_t *hooks(void)
{
    const ui_test_hooks_t *h = ui_get_test_hooks();
    CHECK(h != NULL);
    return h;
}

static void settle(void)
{
    lv_mem_monitor_t mem;
    step(200);
    lv_mem_monitor(&mem);
    if (mem.free_size < s_min_settled_free) s_min_settled_free = mem.free_size;
}

static void point(int x, int y, bool down)
{
    pointer_set(x, y, down);
    step(40);
}

static void tap(int x, int y)
{
    point(x, y, true);
    point(x, y, false);
    settle();
}

static void center_of(lv_obj_t *obj, int *x, int *y)
{
    lv_area_t a;
    CHECK(obj != NULL);
    lv_obj_get_coords(obj, &a);
    *x = (a.x1 + a.x2) / 2;
    *y = (a.y1 + a.y2) / 2;
}

static void click(lv_obj_t *obj)
{
    int x, y;
    center_of(obj, &x, &y);
    tap(x, y);
}

/* Horizontal swipe across the lower screen area (y = 290..292, exercising event
 * bubbling where a lower control overlaps). */
static void swipe(int direction)
{
    point(direction > 0 ? 280 : 60, 290, true);
    point(direction > 0 ? 40 : 300, 292, true);
    point(direction > 0 ? 40 : 300, 292, false);
    settle();
}

static void drag(int x1, int y1, int x2, int y2)
{
    point(x1, y1, true);
    point(x2, y2, true);
    point(x2, y2, false);
    settle();
}

static void go(ui_page_id_t page)
{
    for (unsigned attempt = 0; ui_get_page() != page && attempt < 4; attempt++) swipe(ui_get_page() < page ? 1 : -1);
    CHECK(ui_get_page() == page);
    CHECK(!lv_obj_has_flag(hooks()->pages[page], LV_OBJ_FLAG_HIDDEN));
}

static void apply_ok(studio_cmd_t cmd)
{
    CHECK(ui_apply(&cmd));
    settle();
}

static studio_cmd_t light_cmd(int power, int bright)
{
    studio_cmd_t c = { .kind = STUDIO_CMD_LIGHT };
    c.data.light.power = power;
    c.data.light.bright = bright;
    return c;
}

static studio_cmd_t ac_cmd(int power, int temp)
{
    studio_cmd_t c = { .kind = STUDIO_CMD_AC };
    c.data.ac.power = power;
    c.data.ac.temp = temp;
    return c;
}

static studio_cmd_t env_cmd(int temp, int humid, bool valid)
{
    studio_cmd_t c = { .kind = STUDIO_CMD_ENV };
    c.data.env.temp = temp;
    c.data.env.humid = humid;
    c.data.env.valid = valid;
    return c;
}

static studio_cmd_t scene_cmd(studio_scene_t scene)
{
    studio_cmd_t c = { .kind = STUDIO_CMD_SCENE };
    c.data.scene = scene;
    return c;
}

static void text_is(lv_obj_t *label, const char *want)
{
    CHECK(label != NULL);
    if (strcmp(lv_label_get_text(label), want) != 0) {
        fprintf(stderr, "label '%s', expected '%s'\n", lv_label_get_text(label), want);
    }
    CHECK(strcmp(lv_label_get_text(label), want) == 0);
}

static bool visible(lv_obj_t *obj)
{
    for (; obj; obj = lv_obj_get_parent(obj)) {
        if (lv_obj_has_flag(obj, LV_OBJ_FLAG_HIDDEN)) return false;
    }
    return true;
}

/* Every visible label: approved font, numeric-only codepoints (zero stray
 * English text), text inside its box and its parent, and no glyphs under an
 * unrelated control's touch area. */
static void audit_labels(lv_obj_t *obj, lv_obj_t *const *controls, unsigned count)
{
    if (lv_obj_has_flag(obj, LV_OBJ_FLAG_HIDDEN)) return;
    if (lv_obj_check_type(obj, &lv_label_class)) {
        lv_point_t size;
        lv_area_t a, parent;
        const char *txt = lv_label_get_text(obj);
        uint32_t ofs = 0;
        const lv_font_t *font = lv_obj_get_style_text_font(obj, LV_PART_MAIN);
        CHECK(font == &ui_font_48 || font == &ui_font_digits_76);
        CHECK(txt != NULL && txt[0] != '\0');
        while (txt[ofs] != '\0') {
            uint32_t cp = _lv_txt_encoded_next(txt, &ofs);
            if (!((cp >= '0' && cp <= '9') || cp == '-' ||
                  cp == '%' || cp == 0x00B0u || cp == 0x2014u)) {
                fprintf(stderr, "NON-INTEGER CODEPOINT U+%04lX in '%s'\n", (unsigned long)cp, txt);
            }
            CHECK((cp >= '0' && cp <= '9') || cp == '-' ||
                  cp == '%' || cp == 0x00B0u || cp == 0x2014u);
        }
        lv_txt_get_size(&size, txt, font, 0, 0, LV_COORD_MAX, LV_TEXT_FLAG_NONE);
        if (size.x > lv_obj_get_width(obj) || size.y > lv_obj_get_height(obj)) {
            fprintf(stderr, "GLYPH FIT '%s' %dx%d in %dx%d\n", txt, size.x, size.y,
                    lv_obj_get_width(obj), lv_obj_get_height(obj));
        }
        CHECK(size.x <= lv_obj_get_width(obj) && size.y <= lv_obj_get_height(obj));
        lv_obj_get_coords(obj, &a);
        lv_obj_get_content_coords(lv_obj_get_parent(obj), &parent);
        CHECK(a.x1 >= parent.x1 && a.x2 <= parent.x2 && a.y1 >= parent.y1 && a.y2 <= parent.y2);
        if (size.x > 0) {
            lv_text_align_t align = lv_obj_get_style_text_align(obj, LV_PART_MAIN);
            lv_coord_t w = lv_obj_get_width(obj);
            if (align == LV_TEXT_ALIGN_CENTER) a.x1 = (lv_coord_t)(a.x1 + (w - size.x) / 2);
            else if (align == LV_TEXT_ALIGN_RIGHT) a.x1 = (lv_coord_t)(a.x1 + w - size.x);
            a.x2 = (lv_coord_t)(a.x1 + size.x - 1);
            a.y2 = (lv_coord_t)(a.y1 + size.y - 1);
        }
        for (unsigned c = 0; c < count && size.x; c++) {
            lv_obj_t *ancestor = lv_obj_get_parent(obj);
            lv_area_t hit, overlap;
            while (ancestor && ancestor != controls[c]) ancestor = lv_obj_get_parent(ancestor);
            if (ancestor || !visible(controls[c])) continue;
            lv_obj_get_click_area(controls[c], &hit);
            if (_lv_area_intersect(&overlap, &a, &hit)) {
                fprintf(stderr, "label '%s' under a control\n", txt);
            }
            CHECK(!_lv_area_intersect(&overlap, &a, &hit));
        }
    }
    for (uint32_t i = 0; i < lv_obj_get_child_cnt(obj); i++) audit_labels(lv_obj_get_child(obj, (int32_t)i), controls, count);
}

/* Touch targets: at least 44 x 44, on screen, not overlapping each other. */
static void audit_targets(lv_obj_t *const *controls, unsigned count)
{
    for (unsigned i = 0; i < count; i++) CHECK(controls[i] != NULL);
    for (unsigned i = 0; i < count; i++) {
        lv_area_t a;
        lv_obj_get_click_area(controls[i], &a);
        CHECK(a.x2 - a.x1 + 1 >= 44 && a.y2 - a.y1 + 1 >= 44);
        CHECK(a.x1 >= 0 && a.y1 >= 0 && a.x2 < SIM_W && a.y2 < SIM_H);
        for (unsigned j = i + 1; j < count; j++) {
            lv_area_t b, overlap;
            lv_obj_get_click_area(controls[j], &b);
            CHECK(!_lv_area_intersect(&overlap, &a, &b));
        }
    }
}

static void audit_page(ui_page_id_t page)
{
    const ui_test_hooks_t *h;
    lv_obj_t *controls[5];
    unsigned n = 0;
    go(page);
    h = hooks();
    controls[n++] = h->btn_scenes;
    controls[n++] = h->btn_settings;
    if (page == UI_PAGE_LIGHT) controls[n++] = h->light_sun_btn;
    if (page == UI_PAGE_AC) {
        controls[n++] = h->ac_toggle;
        controls[n++] = h->ac_minus;
        controls[n++] = h->ac_plus;
    }
    audit_targets(controls, n);
    audit_labels(h->root, controls, n);
}

/* =========================================================================
 * Regression: model
 * ========================================================================= */

static void regression_model(void)
{
    static const studio_cmd_t bad[] = {
        { .kind = (studio_cmd_kind_t)-1 },
        { .kind = (studio_cmd_kind_t)99 },
        { .kind = STUDIO_CMD_LIGHT, .data.light = { -1, 80 } },
        { .kind = STUDIO_CMD_LIGHT, .data.light = { 2, 80 } },
        { .kind = STUDIO_CMD_LIGHT, .data.light = { 0, -1 } },
        { .kind = STUDIO_CMD_LIGHT, .data.light = { 0, 101 } },
        { .kind = STUDIO_CMD_LIGHT, .data.light = { 1, INT32_MAX } },
        { .kind = STUDIO_CMD_AC, .data.ac = { -1, 25 } },
        { .kind = STUDIO_CMD_AC, .data.ac = { 2, 25 } },
        { .kind = STUDIO_CMD_AC, .data.ac = { 0, 15 } },
        { .kind = STUDIO_CMD_AC, .data.ac = { 0, 31 } },
        { .kind = STUDIO_CMD_AC, .data.ac = { 1, INT32_MIN } },
        { .kind = STUDIO_CMD_ENV, .data.env = { -32769, 50, true } },
        { .kind = STUDIO_CMD_ENV, .data.env = { 32768, 50, true } },
        { .kind = STUDIO_CMD_ENV, .data.env = { 27, -1, true } },
        { .kind = STUDIO_CMD_ENV, .data.env = { 27, 101, true } },
        { .kind = STUDIO_CMD_SCENE, .data.scene = STUDIO_SCENE_NONE },
        { .kind = STUDIO_CMD_SCENE, .data.scene = (studio_scene_t)99 },
        { .kind = STUDIO_CMD_BAUD, .data.baud = 0 },
        { .kind = STUDIO_CMD_BAUD, .data.baud = 115201 },
        { .kind = STUDIO_CMD_BAUD, .data.baud = UINT32_MAX }
    };
    static const uint32_t rates[] = STUDIO_BAUD_RATES;
    ui_model_t m, before;
    studio_cmd_t c;

    memset(&m, 0xa5, sizeof(m));
    ui_model_init(&m);
    CHECK(m.light_power == 0 && m.light_bright == 0 && m.light_last_nonzero_bright == 80);
    CHECK(m.ac_power == 0 && m.ac_temp == 25 && !m.env_valid);
    CHECK(m.applied_baud == 115200 && m.pending_baud == 0 && m.baud_state == UI_BAUD_IDLE);
    CHECK(m.selected_scene == STUDIO_SCENE_NONE);
    CHECK(ui_model_light_toggle(&m) && m.light_power == 1 && m.light_bright == 80);

    /* Scenes, including the assignment's Sleep (light off, not the Web 10 %). */
    ui_model_init(&m);
    c = scene_cmd(STUDIO_SCENE_HOME);
    CHECK(ui_model_apply(&m, &c));
    CHECK(m.light_power == 1 && m.light_bright == 80 && m.ac_power == 1 && m.ac_temp == 24);
    CHECK(m.selected_scene == STUDIO_SCENE_HOME && ui_model_apply(&m, &c));
    c = scene_cmd(STUDIO_SCENE_SLEEP);
    CHECK(ui_model_apply(&m, &c));
    CHECK(m.light_power == 0 && m.light_bright == 80 && m.ac_power == 1 && m.ac_temp == 26);
    CHECK(ui_model_light_toggle(&m) && m.light_power == 1 && m.light_bright == 80);
    CHECK(m.selected_scene == STUDIO_SCENE_NONE);
    c = scene_cmd(STUDIO_SCENE_AWAY);
    CHECK(ui_model_apply(&m, &c));
    CHECK(m.light_power == 0 && m.ac_power == 0 && m.ac_temp == 26 && m.selected_scene == STUDIO_SCENE_AWAY);

    /* Remote values are kept as sent; dimmer memory; scene divergence. */
    ui_model_init(&m);
    c = light_cmd(0, 37);
    CHECK(ui_model_apply(&m, &c) && m.light_power == 0 && m.light_bright == 37 && m.light_last_nonzero_bright == 37);
    c = light_cmd(1, 0);
    CHECK(ui_model_apply(&m, &c) && m.light_power == 1 && m.light_bright == 0 && m.light_last_nonzero_bright == 37);
    c = scene_cmd(STUDIO_SCENE_HOME);
    CHECK(ui_model_apply(&m, &c));
    c = light_cmd(1, 80);
    CHECK(ui_model_apply(&m, &c) && m.selected_scene == STUDIO_SCENE_HOME);
    c = light_cmd(1, 79);
    CHECK(ui_model_apply(&m, &c) && m.selected_scene == STUDIO_SCENE_NONE);
    c = ac_cmd(0, 25);
    CHECK(ui_model_apply(&m, &c) && m.ac_power == 0 && m.ac_temp == 25);

    /* Inclusive bounds. */
    c = light_cmd(1, 100);
    CHECK(ui_model_apply(&m, &c));
    c = ac_cmd(1, 16);
    CHECK(ui_model_apply(&m, &c));
    c = ac_cmd(1, 30);
    CHECK(ui_model_apply(&m, &c));
    c = env_cmd(-32768, 0, true);
    CHECK(ui_model_apply(&m, &c) && m.env_temp == -32768);
    c = env_cmd(32767, 100, true);
    CHECK(ui_model_apply(&m, &c) && m.env_humid == 100);

    /* Rejected input changes nothing at all. */
    m.pending_baud = 230400;
    m.baud_state = UI_BAUD_PENDING;
    memcpy(&before, &m, sizeof(m));
    for (size_t i = 0; i < sizeof(bad) / sizeof(bad[0]); i++) {
        CHECK(!ui_model_apply(&m, &bad[i]));
        CHECK(memcmp(&m, &before, sizeof(m)) == 0);
    }
    CHECK(!ui_model_apply(&m, NULL) && !ui_model_apply(NULL, &c) && !ui_model_light_toggle(NULL));
    ui_model_init(NULL);

    /* Baud: every supported rate; a remote BAUD does not touch a pending request. */
    c.kind = STUDIO_CMD_BAUD;
    for (size_t i = 0; i < sizeof(rates) / sizeof(rates[0]); i++) {
        CHECK(ui_model_baud_supported(rates[i]));
        c.data.baud = rates[i];
        CHECK(ui_model_apply(&m, &c) && m.applied_baud == rates[i]);
        CHECK(m.pending_baud == 230400 && m.baud_state == UI_BAUD_PENDING);
    }
    puts("PASS model: defaults, scenes, remote values, bounds, atomic rejection, baud");
}

/* =========================================================================
 * Regression: host timing
 * ========================================================================= */

static unsigned s_timer_hits;
static void count_timer(lv_timer_t *t) { (void)t; s_timer_hits++; }

static void regression_host_time(void)
{
    lv_timer_t *timer = lv_timer_create(count_timer, 5, NULL);
    uint32_t before;
    CHECK(timer != NULL);
    step(20);
    CHECK(s_timer_hits == 4);
    s_timer_hits = 0;
    before = lv_tick_get();
    realtime_step(3600000u);
    CHECK((uint32_t)(lv_tick_get() - before) == 3600000u && s_timer_hits == 1);
    lv_timer_del(timer);
    puts("PASS host time: one-hour pause serviced once, no replay");
}

/* =========================================================================
 * Regression: UI
 * ========================================================================= */

static int expected_ray_units(int level)
{
    int units;
    if (level <= 0) return 0;
    if (level >= 100) return 32;
    units = (level * 32 + 50) / 100;
    if (units < 1) units = 1;
    if (units > 31) units = 31;
    return units;
}

static void check_rays(const ui_test_hooks_t *h, int level)
{
    int units = expected_ray_units(level);
    for (int k = 0; k < 16; k++) {
        bool full = units >= (k + 1) * 2;
        bool half = !full && (units == k * 2 + 1);
        CHECK(h->light_rays[k] != NULL);
        CHECK(lv_obj_has_state(h->light_rays[k], LV_STATE_CHECKED) == full);
        CHECK(lv_obj_has_state(h->light_rays[k], LV_STATE_USER_1) == half);
    }
}

static void regression_pages(void)
{
    const ui_test_hooks_t *h = hooks();
    unsigned count = s_actions;
    CHECK(ui_get_page() == UI_PAGE_LIGHT);
    CHECK(!lv_obj_has_flag(h->pages[UI_PAGE_LIGHT], LV_OBJ_FLAG_HIDDEN));
    tap(100, 150); /* blank canvas area */
    CHECK(s_actions == count && ui_get_page() == UI_PAGE_LIGHT);
    swipe(-1);
    CHECK(ui_get_page() == UI_PAGE_LIGHT);
    swipe(1);
    CHECK(ui_get_page() == UI_PAGE_AC && !lv_obj_has_flag(hooks()->pages[UI_PAGE_AC], LV_OBJ_FLAG_HIDDEN));
    swipe(1);
    CHECK(ui_get_page() == UI_PAGE_TEMPERATURE && !lv_obj_has_flag(hooks()->pages[UI_PAGE_TEMPERATURE], LV_OBJ_FLAG_HIDDEN));
    swipe(1);
    CHECK(ui_get_page() == UI_PAGE_HUMIDITY && !lv_obj_has_flag(hooks()->pages[UI_PAGE_HUMIDITY], LV_OBJ_FLAG_HIDDEN));
    swipe(1);
    CHECK(ui_get_page() == UI_PAGE_HUMIDITY && s_actions == count);
    go(UI_PAGE_LIGHT);
    /* A swipe that starts on a header button pages and does not open a panel. */
    drag(36, 34, 36 + 200, 36);
    CHECK(ui_get_page() == UI_PAGE_LIGHT && hooks()->panel == UI_PANEL_NONE);
    drag(444, 34, 444 - 200, 36);
    CHECK(ui_get_page() == UI_PAGE_AC && hooks()->panel == UI_PANEL_NONE);
    go(UI_PAGE_LIGHT);
    for (unsigned p = 0; p < UI_PAGE_COUNT; p++) audit_page((ui_page_id_t)p);
    go(UI_PAGE_LIGHT);
    puts("PASS pages: swipe-only navigation, clamped, swipes from buttons, zero-text numeric labels, fit, targets");
}

static void regression_light(void)
{
    const ui_test_hooks_t *h = hooks();
    int sx, sy;
    unsigned count;
    uint16_t px_off = 0, px_half = 0, px_full = 0, core_off = 0;
    uint64_t seen_units = 0;
    go(UI_PAGE_LIGHT);
    apply_ok(light_cmd(0, 0));
    CHECK(h->light_sun_btn != NULL && h->light_sun_core != NULL);
    CHECK(!lv_obj_has_state(h->light_sun_btn, LV_STATE_CHECKED));
    CHECK(!lv_obj_has_state(h->light_sun_core, LV_STATE_CHECKED));
    check_rays(h, 0);
    px_off = pixel(240, 91);
    core_off = pixel(240, 168);

    count = s_actions;
    click(h->light_sun_btn);
    CHECK(s_actions == count + 1 && s_last.kind == STUDIO_CMD_LIGHT && s_last.data.light.power == 1 && s_last.data.light.bright == 80);
    CHECK(lv_obj_has_state(h->light_sun_btn, LV_STATE_CHECKED) && lv_obj_has_state(h->light_sun_core, LV_STATE_CHECKED));
    {
        lv_area_t ab, ac;
        lv_obj_get_coords(h->light_sun_btn, &ab);
        lv_obj_get_coords(h->light_sun_core, &ac);
        CHECK(ac.x1 - ab.x1 == ab.x2 - ac.x2 && ac.y1 - ab.y1 == ab.y2 - ac.y2);
    }
    CHECK(pixel(240, 168) != core_off);
    check_rays(h, 80);
    click(h->light_sun_btn);
    CHECK(s_last.data.light.power == 0 && s_last.data.light.bright == 80 && !ui_get_model()->light_power);
    CHECK(!lv_obj_has_state(h->light_sun_btn, LV_STATE_CHECKED) && !lv_obj_has_state(h->light_sun_core, LV_STATE_CHECKED));
    {
        lv_area_t ab, ac;
        lv_obj_get_coords(h->light_sun_btn, &ab);
        lv_obj_get_coords(h->light_sun_core, &ac);
        CHECK(ac.x1 - ab.x1 == ab.x2 - ac.x2 && ac.y1 - ab.y1 == ab.y2 - ac.y2);
    }
    CHECK(pixel(240, 168) == core_off);
    check_rays(h, 0);

    /* Sweep 0..100%: all 33 radial brightness states (0..32 half-ray units) and quadrant landmarks. */
    seen_units |= UINT64_C(1) << 0;
    for (int lvl = 1; lvl <= 100; lvl++) {
        int u = expected_ray_units(lvl);
        apply_ok(light_cmd(1, lvl));
        CHECK(lv_obj_has_state(h->light_sun_btn, LV_STATE_CHECKED) && lv_obj_has_state(h->light_sun_core, LV_STATE_CHECKED));
        check_rays(h, lvl);
        seen_units |= UINT64_C(1) << (unsigned)u;
        if (lvl == 1) {
            CHECK(u == 1);
            px_half = pixel(240, 91);
        } else if (lvl == 5) {
            CHECK(u == 2);
            px_full = pixel(240, 91);
        } else if (lvl == 25) {
            CHECK(u == 8);
        } else if (lvl == 27) {
            CHECK(u == 9);
        } else if (lvl == 50) {
            CHECK(u == 16);
        } else if (lvl == 75) {
            CHECK(u == 24);
        } else if (lvl == 99) {
            CHECK(u == 31);
        } else if (lvl == 100) {
            CHECK(u == 32);
        }
    }
    CHECK(seen_units == ((UINT64_C(1) << 33) - 1u));
    CHECK(px_off != px_half && px_half != px_full && px_off != px_full);

    /* Edge case: remote power=1, bright=0 lights the sun core with 0 rays. */
    apply_ok(light_cmd(1, 0));
    CHECK(lv_obj_has_state(h->light_sun_btn, LV_STATE_CHECKED) && lv_obj_has_state(h->light_sun_core, LV_STATE_CHECKED));
    CHECK(pixel(240, 168) != core_off);
    check_rays(h, 0);
    apply_ok(light_cmd(0, 0));

    /* Vertical drag anywhere on the screen: up 100 px from off -> 50 % on. */
    point(100, 260, true);
    point(101, 160, true);
    CHECK(ui_get_model()->light_power == 1 && ui_get_model()->light_bright == 50);
    check_rays(h, 50);
    point(101, 160, false);
    settle();
    CHECK(s_last.data.light.power == 1 && s_last.data.light.bright == 50 && ui_get_page() == UI_PAGE_LIGHT);
    check_rays(h, 50);
    /* Vertical lock survives later sideways motion. */
    count = s_actions;
    point(100, 180, true);
    point(102, 160, true);
    point(60, 140, true);
    point(60, 140, false);
    settle();
    CHECK(ui_get_page() == UI_PAGE_LIGHT && s_last.data.light.bright == 70 && s_actions > count);
    /* Horizontal lock never dims. */
    count = s_actions;
    point(330, 160, true);
    point(310, 158, true);
    point(100, 80, true);
    point(100, 80, false);
    settle();
    CHECK(ui_get_page() == UI_PAGE_AC && s_actions == count);
    go(UI_PAGE_LIGHT);
    /* Clamp both ways; dragging to zero turns the light off. */
    drag(100, 300, 100, 70);
    CHECK(s_last.data.light.bright == 100 && s_last.data.light.power == 1);
    check_rays(h, 100);
    drag(100, 75, 100, 310);
    CHECK(s_last.data.light.bright == 0 && s_last.data.light.power == 0);
    CHECK(!lv_obj_has_state(h->light_sun_btn, LV_STATE_CHECKED) && !lv_obj_has_state(h->light_sun_core, LV_STATE_CHECKED));
    check_rays(h, 0);
    /* A drag that starts on the sun core button dims and does not toggle. */
    center_of(h->light_sun_btn, &sx, &sy);
    count = s_actions;
    drag(sx, sy, sx + 2, sy - 60);
    CHECK(s_last.kind == STUDIO_CMD_LIGHT && s_last.data.light.bright == 30 && s_last.data.light.power == 1);
    CHECK(s_actions > count && lv_obj_has_state(h->light_sun_core, LV_STATE_CHECKED));
    /* A swipe that starts on the sun core button pages and does not toggle. */
    count = s_actions;
    drag(sx, sy, sx + 180, sy + 2);
    CHECK(ui_get_page() == UI_PAGE_LIGHT && s_actions == count);
    drag(sx, sy, sx - 180, sy + 2);
    CHECK(ui_get_page() == UI_PAGE_AC && s_actions == count);
    go(UI_PAGE_LIGHT);
    /* Press lost commits an established drag once, then nothing else. */
    apply_ok(light_cmd(0, 0));
    point(100, 260, true);
    point(100, 160, true);
    count = s_actions;
    release_pointer();
    settle();
    CHECK(s_actions == count + 1 && s_last.data.light.bright == 50 && ui_get_page() == UI_PAGE_LIGHT);
    count = s_actions;
    point(sx, sy, true);
    release_pointer();
    settle();
    CHECK(s_actions == count);
    point(330, 160, true);
    point(100, 160, true);
    release_pointer();
    settle();
    CHECK(ui_get_page() == UI_PAGE_LIGHT && s_actions == count);
    puts("PASS light: sun tap, 32 radial brightness steps, power=1/bright=0, relative drag, axis lock, swipes from sun, press lost");
}

static void regression_drag_timing(void)
{
    const ui_test_hooks_t *h = hooks();
    unsigned start, release, count;
    uint16_t off_pixel, draft_pixel;
    go(UI_PAGE_LIGHT);
    apply_ok(light_cmd(0, 0));
    /* Intermediate events at least 100 ms apart, across the tick wrap. */
    lv_tick_inc(UINT32_MAX - lv_tick_get() - 100u);
    start = s_actions;
    point(100, 290, true);
    for (int i = 1; i <= 8; i++) point(100, 290 - i * 20, true);
    release = s_actions;
    point(100, 130, false);
    settle();
    CHECK(s_actions == release + 1 && s_last.data.light.bright == 80);
    CHECK(s_action_times[start] > UINT32_MAX - 1000u && s_action_times[release - 1] < 1000u);
    for (unsigned i = start + 1; i < release; i++) CHECK((uint32_t)(s_action_times[i] - s_action_times[i - 1]) >= 100u);

    /* While a finger holds a drag, a remote LIGHT does not overwrite the draft sun/rays. */
    apply_ok(light_cmd(0, 0));
    off_pixel = pixel(240, 91);
    point(100, 260, true);
    point(100, 240, true);
    point(100, 220, true);
    count = s_actions;
    draft_pixel = pixel(240, 91);
    CHECK(draft_pixel != off_pixel);
    CHECK(ui_apply(&(studio_cmd_t){ .kind = STUDIO_CMD_LIGHT, .data.light = { 0, 21 } }));
    CHECK(s_actions == count);
    step(100);
    CHECK(pixel(240, 91) == draft_pixel && lv_obj_has_state(h->light_sun_core, LV_STATE_CHECKED));
    CHECK(s_actions == count + 1 && s_last.data.light.bright == 20);
    release = s_actions;
    point(100, 220, false);
    settle();
    CHECK(s_actions == release + 1 && s_last.data.light.bright == 20 && ui_get_model()->light_bright == 20);
    CHECK((uint32_t)(s_action_times[release] - s_action_times[release - 1]) < 100u);
    /* After release the PC is authoritative again. */
    apply_ok(light_cmd(0, 37));
    CHECK(ui_get_model()->light_bright == 37 && !lv_obj_has_state(h->light_sun_core, LV_STATE_CHECKED));
    CHECK(pixel(240, 91) == off_pixel);
    count = s_actions;
    step(200);
    CHECK(s_actions == count);
    puts("PASS drag timing: >=100 ms between intermediate events across tick wrap, final at once, held drag wins, remote after release wins");
}

static void regression_ac(void)
{
    const ui_test_hooks_t *h = hooks();
    unsigned count;
    int px, py;
    uint16_t tile_off_px;
    lv_area_t a_ac, a_temp, a_val, a_tval, a_plus, a_minus;
    go(UI_PAGE_AC);
    lv_obj_get_coords(h->ac_toggle, &a_ac);
    lv_obj_get_coords(h->temp_tile, &a_temp);
    lv_obj_get_coords(h->ac_value, &a_val);
    lv_obj_get_coords(h->temp_value, &a_tval);
    lv_obj_get_coords(h->ac_plus, &a_plus);
    lv_obj_get_coords(h->ac_minus, &a_minus);
    CHECK(a_ac.x1 == 18 && a_ac.y1 == 96 && a_ac.x2 == a_temp.x2 && a_ac.y2 == a_temp.y2 && (a_ac.y1 + a_ac.y2 + 1) / 2 == SIM_H / 2);
    CHECK(a_val.x1 == 140 && a_val.y1 == 125 && a_val.y1 == a_tval.y1 && lv_obj_get_width(h->ac_value) == 224 &&
          (a_val.x1 + a_val.x2 + 1) / 2 == (a_tval.x1 + a_tval.x2 + 1) / 2);
    CHECK(a_plus.x1 == 366 && a_plus.y1 == 66 && lv_obj_get_width(h->ac_plus) == 96 && lv_obj_get_height(h->ac_plus) == 86);
    CHECK(a_minus.x1 == 366 && a_minus.y1 == 168 && lv_obj_get_width(h->ac_minus) == 96 && lv_obj_get_height(h->ac_minus) == 86 && (a_plus.y1 + a_minus.y2 + 1) / 2 == SIM_H / 2);
    apply_ok(ac_cmd(0, 25));
    CHECK(!lv_obj_has_state(h->ac_toggle, LV_STATE_CHECKED) && !lv_obj_has_state(h->ac_value, LV_STATE_CHECKED));
    tile_off_px = pixel(28, 108);
    text_is(h->ac_value, "25\xC2\xB0");
    click(h->ac_toggle);
    CHECK(s_last.kind == STUDIO_CMD_AC && s_last.data.ac.power == 1 && s_last.data.ac.temp == 25);
    CHECK(lv_obj_has_state(h->ac_toggle, LV_STATE_CHECKED) && lv_obj_has_state(h->ac_value, LV_STATE_CHECKED));
    CHECK(pixel(28, 108) != tile_off_px);
    for (unsigned i = 0; i < 12; i++) click(h->ac_minus);
    CHECK(ui_get_model()->ac_temp == 16);
    text_is(h->ac_value, "16\xC2\xB0");
    CHECK(lv_obj_has_state(h->ac_minus, LV_STATE_DISABLED));
    count = s_actions;
    click(h->ac_minus);
    CHECK(s_actions == count);
    for (unsigned i = 0; i < 16; i++) click(h->ac_plus);
    CHECK(ui_get_model()->ac_temp == 30 && lv_obj_has_state(h->ac_plus, LV_STATE_DISABLED));
    text_is(h->ac_value, "30\xC2\xB0");
    count = s_actions;
    click(h->ac_plus);
    CHECK(s_actions == count);
    click(h->ac_toggle);
    CHECK(!ui_get_model()->ac_power && ui_get_model()->ac_temp == 30);
    CHECK(!lv_obj_has_state(h->ac_toggle, LV_STATE_CHECKED) && !lv_obj_has_state(h->ac_value, LV_STATE_CHECKED));
    CHECK(pixel(28, 108) == tile_off_px);
    /* A swipe that starts on the snowflake tile or a stepper pages and does not act. */
    center_of(h->ac_toggle, &px, &py);
    count = s_actions;
    drag(px, py, px + 200, py + 2);
    CHECK(ui_get_page() == UI_PAGE_LIGHT && s_actions == count && !ui_get_model()->ac_power);
    go(UI_PAGE_AC);
    center_of(h->ac_minus, &px, &py);
    count = s_actions;
    drag(px, py, px - 200, py + 3);
    CHECK(ui_get_page() == UI_PAGE_TEMPERATURE && s_actions == count && ui_get_model()->ac_temp == 30);
    go(UI_PAGE_AC);
    puts("PASS AC: unified 96x128 snowflake tile, 16..30 with disabled ends, swipes from tile and stepper");
}

static void regression_scenes(void)
{
    const ui_test_hooks_t *h;
    lv_obj_t *controls[4];
    unsigned count;
    go(UI_PAGE_AC);
    click(hooks()->btn_scenes);
    h = hooks();
    CHECK(h->panel == UI_PANEL_SCENES && !lv_obj_has_flag(h->scenes_panel, LV_OBJ_FLAG_HIDDEN));
    CHECK(lv_obj_has_flag(h->root, LV_OBJ_FLAG_HIDDEN));
    controls[0] = h->scenes_back;
    for (unsigned i = 0; i < 3; i++) controls[i + 1] = h->scene_buttons[i];
    audit_targets(controls, 4);
    audit_labels(h->scenes_panel, controls, 4);
    count = s_actions;
    swipe(1);
    CHECK(ui_get_page() == UI_PAGE_AC && s_actions == count);
    click(h->scene_buttons[0]);
    CHECK(s_last.kind == STUDIO_CMD_SCENE && s_last.data.scene == STUDIO_SCENE_HOME);
    CHECK(ui_get_model()->light_power && ui_get_model()->light_bright == 80 && ui_get_model()->ac_power && ui_get_model()->ac_temp == 24);
    CHECK(lv_obj_has_state(h->scene_buttons[0], LV_STATE_CHECKED) && lv_obj_has_state(h->scene_icons[0], LV_STATE_CHECKED));
    CHECK(!lv_obj_has_state(h->scene_buttons[1], LV_STATE_CHECKED) && !lv_obj_has_state(h->scene_buttons[2], LV_STATE_CHECKED));
    click(h->scene_buttons[1]);
    CHECK(!ui_get_model()->light_power && ui_get_model()->ac_power && ui_get_model()->ac_temp == 26);
    CHECK(!lv_obj_has_state(h->scene_buttons[0], LV_STATE_CHECKED) && !lv_obj_has_state(h->scene_icons[0], LV_STATE_CHECKED));
    CHECK(lv_obj_has_state(h->scene_buttons[1], LV_STATE_CHECKED) && lv_obj_has_state(h->scene_icons[1], LV_STATE_CHECKED));
    click(h->scene_buttons[2]);
    CHECK(!ui_get_model()->light_power && !ui_get_model()->ac_power && s_last.data.scene == STUDIO_SCENE_AWAY);
    CHECK(lv_obj_has_state(h->scene_buttons[2], LV_STATE_CHECKED) && lv_obj_has_state(h->scene_icons[2], LV_STATE_CHECKED));
    /* A device change that no longer matches clears the selection. */
    CHECK(ui_apply(&(studio_cmd_t){ .kind = STUDIO_CMD_AC, .data.ac = { 1, 26 } }));
    settle();
    CHECK(!lv_obj_has_state(h->scene_buttons[2], LV_STATE_CHECKED) && !lv_obj_has_state(h->scene_icons[2], LV_STATE_CHECKED));
    click(h->scenes_back);
    CHECK(hooks()->panel == UI_PANEL_NONE && ui_get_page() == UI_PAGE_AC && !lv_obj_has_flag(h->root, LV_OBJ_FLAG_HIDDEN));
    puts("PASS scenes: panel owns touches, Home/Sleep/Away circles, selection, return to the same page");
}

static void regression_environment(void)
{
    static const int temps[] = { -32768, 32767, -27, -1, 0, 27 };
    static const int humid[] = { 100, 95, 20, 0, 5, 58 };
    static const char *const t[] = {
        "-32768\xC2\xB0", "32767\xC2\xB0", "-27\xC2\xB0", "-1\xC2\xB0", "0\xC2\xB0", "27\xC2\xB0"
    };
    static const char *const rh[] = { "100%", "95%", "20%", "0%", "5%", "58%" };
    const ui_test_hooks_t *h = hooks();
    lv_area_t at, ah, atv, ahv;
    go(UI_PAGE_TEMPERATURE);
    lv_obj_get_coords(h->temp_tile, &at);
    lv_obj_get_coords(h->humid_tile, &ah);
    lv_obj_get_coords(h->temp_value, &atv);
    lv_obj_get_coords(h->humid_value, &ahv);
    CHECK(at.x1 == 18 && at.y1 == 96 && ah.x1 == 18 && ah.y1 == 96);
    CHECK(atv.x1 == 128 && atv.y1 == 125 && lv_obj_get_width(h->temp_value) == 248 && (atv.x1 + atv.x2 + 1) / 2 == 252);
    CHECK(ahv.x1 == 128 && ahv.y1 == 125 && lv_obj_get_width(h->humid_value) == 248 && (ahv.x1 + ahv.x2 + 1) / 2 == 252);
    apply_ok(env_cmd(0, 0, false));
    text_is(h->temp_value, "\xE2\x80\x94");
    text_is(h->humid_value, "\xE2\x80\x94");
    CHECK(!lv_obj_has_state(h->temp_icon, LV_STATE_CHECKED) && !lv_obj_has_state(h->temp_value, LV_STATE_CHECKED));
    CHECK(!lv_obj_has_state(h->humid_icon, LV_STATE_CHECKED) && !lv_obj_has_state(h->humid_value, LV_STATE_CHECKED));
    audit_page(UI_PAGE_TEMPERATURE);
    audit_page(UI_PAGE_HUMIDITY);
    for (unsigned i = 0; i < 6; i++) {
        apply_ok(env_cmd(temps[i], humid[i], true));
        text_is(h->temp_value, t[i]);
        text_is(h->humid_value, rh[i]);
        CHECK(lv_obj_has_state(h->temp_icon, LV_STATE_CHECKED) && lv_obj_has_state(h->temp_value, LV_STATE_CHECKED));
        CHECK(lv_obj_has_state(h->humid_icon, LV_STATE_CHECKED) && lv_obj_has_state(h->humid_value, LV_STATE_CHECKED));
        audit_page(UI_PAGE_TEMPERATURE);
        audit_page(UI_PAGE_HUMIDITY);
    }
    go(UI_PAGE_TEMPERATURE);
    puts("PASS environment: screen-centered whole-integer readings, wide fallback, unavailable dash");
}

static void regression_redraw(void)
{
    unsigned count = s_actions;
    ui_model_t unchanged;
    apply_ok(light_cmd(1, 80));
    apply_ok(ac_cmd(0, 30));
    apply_ok(env_cmd(27, 58, true));
    for (unsigned p = 0; p < UI_PAGE_COUNT; p++) {
        go((ui_page_id_t)p);
        settle();
        reset_flush_counters();
        for (unsigned i = 0; i < 10; i++) {
            CHECK(ui_apply(&(studio_cmd_t){ .kind = STUDIO_CMD_LIGHT, .data.light = { 1, 80 } }));
            CHECK(ui_apply(&(studio_cmd_t){ .kind = STUDIO_CMD_AC, .data.ac = { 0, 30 } }));
            CHECK(ui_apply(&(studio_cmd_t){ .kind = STUDIO_CMD_ENV, .data.env = { 27, 58, true } }));
            step(100);
        }
        CHECK(s_flush_calls == 0 && s_flush_pixels == 0);
    }
    count = s_actions;
    unchanged = *ui_get_model();
    reset_flush_counters();
    CHECK(!ui_apply(NULL));
    CHECK(!ui_apply(&(studio_cmd_t){ .kind = STUDIO_CMD_LIGHT, .data.light = { 1, 101 } }));
    settle();
    CHECK(memcmp(&unchanged, ui_get_model(), sizeof(unchanged)) == 0 && s_flush_calls == 0 && s_actions == count);

    go(UI_PAGE_TEMPERATURE);
    reset_flush_counters();
    apply_ok(env_cmd(28, 58, true));
    CHECK(s_flush_calls > 0 && s_flush_pixels < 480u * 120u);
    printf("      temperature change: %llu flushes / %llu pixels\n", (unsigned long long)s_flush_calls, (unsigned long long)s_flush_pixels);
    go(UI_PAGE_LIGHT);
    reset_flush_counters();
    apply_ok(light_cmd(1, 90));
    CHECK(s_flush_calls > 0 && s_flush_pixels < 480u * 160u);
    printf("      light change: %llu flushes / %llu pixels\n", (unsigned long long)s_flush_calls, (unsigned long long)s_flush_pixels);
    puts("PASS redraw: identical LIGHT/AC/ENV x10 on every page = 0 flushes; changes stay local");
}

static void regression_connection(void)
{
    static const uint32_t rates[] = STUDIO_BAUD_RATES;
    const ui_test_hooks_t *h;
    lv_obj_t *controls[4];
    unsigned before;
    ui_model_t saved;
    char text[16];
    go(UI_PAGE_HUMIDITY);
    click(hooks()->btn_settings);
    h = hooks();
    CHECK(h->panel == UI_PANEL_CONNECTION);
    controls[0] = h->conn_back;
    controls[1] = h->baud_prev;
    controls[2] = h->baud_next;
    controls[3] = h->baud_apply;
    audit_targets(controls, 4);
    audit_labels(h->conn_panel, controls, 4);
    text_is(h->baud_value, "115200");
    CHECK(!lv_obj_has_flag(h->baud_active_dot, LV_OBJ_FLAG_HIDDEN));
    CHECK(!lv_obj_has_state(h->baud_prev, LV_STATE_DISABLED) && !lv_obj_has_state(h->baud_next, LV_STATE_DISABLED));
    CHECK(lv_img_get_src(h->baud_apply_icon) == &ui_icon_check_28);
    CHECK(!lv_obj_has_state(h->conn_rx_dot, LV_STATE_CHECKED));

    /* Step down from default 115200 (index 4) to 9600 (index 0) and verify clamp. */
    before = s_actions;
    for (unsigned step_dn = 0; step_dn < 4; step_dn++) {
        click(h->baud_prev);
        CHECK(lv_obj_has_flag(h->baud_active_dot, LV_OBJ_FLAG_HIDDEN));
    }
    text_is(h->baud_value, "9600");
    CHECK(lv_obj_has_state(h->baud_prev, LV_STATE_DISABLED) && !lv_obj_has_state(h->baud_next, LV_STATE_DISABLED));
    click(h->baud_prev);
    text_is(h->baud_value, "9600");
    CHECK(s_actions == before);

    for (unsigned i = 0; i < STUDIO_BAUD_COUNT; i++) {
        (void)snprintf(text, sizeof(text), "%lu", (unsigned long)rates[i]);
        text_is(h->baud_value, text);
        CHECK(lv_obj_has_state(h->baud_prev, LV_STATE_DISABLED) == (i == 0));
        CHECK(lv_obj_has_state(h->baud_next, LV_STATE_DISABLED) == (i == STUDIO_BAUD_COUNT - 1));
        click(h->baud_apply);
        CHECK(s_actions == before + 1 && s_last.kind == STUDIO_CMD_BAUD && s_last.data.baud == rates[i]);
        CHECK(ui_get_model()->baud_state == UI_BAUD_PENDING && ui_get_model()->pending_baud == rates[i]);
        CHECK(lv_obj_has_state(h->baud_apply, LV_STATE_DISABLED) && lv_obj_has_state(h->baud_apply, LV_STATE_USER_1));
        CHECK(lv_obj_has_state(h->baud_prev, LV_STATE_DISABLED) && lv_obj_has_state(h->baud_next, LV_STATE_DISABLED));
        CHECK(lv_img_get_src(h->baud_apply_icon) == &ui_icon_dots_28);
        audit_labels(h->conn_panel, controls, 4);
        /* Locked while pending. */
        click(h->baud_apply);
        click(h->baud_prev);
        click(h->baud_next);
        CHECK(s_actions == before + 1);
        text_is(h->baud_value, text);
        saved = *ui_get_model();
        CHECK(!ui_baud_complete(12345, true));
        CHECK(!ui_baud_complete(rates[(i + 1) % STUDIO_BAUD_COUNT], false));
        CHECK(memcmp(&saved, ui_get_model(), sizeof(saved)) == 0);
        click(h->conn_back);
        click(hooks()->btn_settings);
        text_is(h->baud_value, text);
        CHECK(lv_img_get_src(h->baud_apply_icon) == &ui_icon_dots_28);
        if (i == 0) {
            /* Failure keeps previous rate; stepping clears failed state; tap retries. */
            CHECK(ui_baud_complete(rates[0], false));
            settle();
            CHECK(ui_get_model()->applied_baud == 115200 && ui_get_model()->baud_state == UI_BAUD_FAILED);
            CHECK(!lv_obj_has_state(h->baud_apply, LV_STATE_DISABLED) && lv_obj_has_state(h->baud_apply, LV_STATE_USER_2));
            CHECK(lv_img_get_src(h->baud_apply_icon) == &ui_icon_cross_24);
            click(h->baud_next);
            text_is(h->baud_value, "19200");
            CHECK(ui_get_model()->baud_state == UI_BAUD_IDLE);
            CHECK(!lv_obj_has_state(h->baud_apply, LV_STATE_USER_2) && lv_img_get_src(h->baud_apply_icon) == &ui_icon_check_28);
            click(h->baud_prev);
            text_is(h->baud_value, "9600");
            click(h->baud_apply);
            CHECK(s_actions == before + 2);
            CHECK(ui_baud_complete(rates[0], false));
            settle();
            CHECK(ui_get_model()->baud_state == UI_BAUD_FAILED && lv_img_get_src(h->baud_apply_icon) == &ui_icon_cross_24);
            click(h->baud_apply);
            CHECK(s_actions == before + 3);
            before += 2;
        }
        CHECK(ui_baud_complete(rates[i], true));
        settle();
        CHECK(ui_get_model()->applied_baud == rates[i] && ui_get_model()->baud_state == UI_BAUD_IDLE && ui_get_model()->pending_baud == 0);
        CHECK(!ui_baud_complete(rates[i], true));
        CHECK(!lv_obj_has_state(h->baud_apply, LV_STATE_DISABLED));
        CHECK(!lv_obj_has_state(h->baud_apply, LV_STATE_USER_1) && !lv_obj_has_state(h->baud_apply, LV_STATE_USER_2));
        CHECK(lv_img_get_src(h->baud_apply_icon) == &ui_icon_check_28);
        CHECK(!lv_obj_has_flag(h->baud_active_dot, LV_OBJ_FLAG_HIDDEN));
        before++;
        if (i + 1 < STUDIO_BAUD_COUNT) {
            click(h->baud_next);
            CHECK(lv_obj_has_flag(h->baud_active_dot, LV_OBJ_FLAG_HIDDEN));
        }
    }
    CHECK(lv_obj_has_state(h->baud_next, LV_STATE_DISABLED) && !lv_obj_has_state(h->baud_prev, LV_STATE_DISABLED));
    click(h->baud_next);
    text_is(h->baud_value, "921600");
    CHECK(s_actions == before);

    /* Remote BAUD syncs stepper selection and clears a prior failed pill state. */
    click(h->baud_prev);
    click(h->baud_apply);
    CHECK(ui_baud_complete(460800, false));
    settle();
    CHECK(ui_get_model()->baud_state == UI_BAUD_FAILED && lv_obj_has_state(h->baud_apply, LV_STATE_USER_2));
    CHECK(lv_img_get_src(h->baud_apply_icon) == &ui_icon_cross_24);
    apply_ok((studio_cmd_t){ .kind = STUDIO_CMD_BAUD, .data.baud = 115200 });
    text_is(h->baud_value, "115200");
    CHECK(!lv_obj_has_flag(h->baud_active_dot, LV_OBJ_FLAG_HIDDEN));
    CHECK(ui_get_model()->baud_state == UI_BAUD_IDLE);
    CHECK(!lv_obj_has_state(h->baud_apply, LV_STATE_USER_2) && lv_img_get_src(h->baud_apply_icon) == &ui_icon_check_28);

    /* RX dot lights on ui_note_rx() and causes 0 periodic timer flushes. */
    CHECK(!lv_obj_has_state(h->conn_rx_dot, LV_STATE_CHECKED));
    ui_note_rx();
    settle();
    CHECK(lv_obj_has_state(h->conn_rx_dot, LV_STATE_CHECKED));
    reset_flush_counters();
    step(2000);
    CHECK(s_flush_calls == 0 && s_flush_pixels == 0);
    click(h->conn_back);
    CHECK(hooks()->panel == UI_PANEL_NONE && ui_get_page() == UI_PAGE_HUMIDITY);
    puts("PASS connection: 8 stepper rates, active dot, pending lock, failure/retry/step-clear, RX dot, 0 idle flushes");
}

static void regression_lifecycle(void)
{
    lv_mem_monitor_t before, after;
    const ui_test_hooks_t *h;
    go(UI_PAGE_LIGHT);
    click(hooks()->btn_settings);
    click(hooks()->baud_next);
    click(hooks()->baud_prev);
    click(hooks()->conn_back);
    settle();
    lv_mem_monitor(&before);
    for (unsigned i = 0; i < 100; i++) {
        go(UI_PAGE_HUMIDITY);
        go(UI_PAGE_LIGHT);
        h = hooks();
        click(h->btn_scenes);
        click(hooks()->scene_buttons[i % 3]);
        click(hooks()->scenes_back);
        click(hooks()->btn_settings);
        click(hooks()->baud_next);
        click(hooks()->baud_prev);
        click(hooks()->conn_back);
    }
    lv_mem_monitor(&after);
    CHECK(before.free_size == after.free_size);
    printf("      100 page/scene/connection/stepper cycles: free %lu -> %lu, largest %lu bytes\n",
           (unsigned long)before.free_size, (unsigned long)after.free_size, (unsigned long)after.free_biggest_size);
    puts("PASS lifecycle: no heap drift over 100 full cycles");
}

static void regression_no_handler(void)
{
    unsigned before = s_actions;
    ui_deinit();
    CHECK(!ui_baud_complete(115200, true));
    CHECK(!ui_apply(&(studio_cmd_t){ .kind = STUDIO_CMD_LIGHT, .data.light = { 1, 80 } }));
    ui_note_rx();
    ui_init(NULL, NULL);
    settle();
    click(hooks()->btn_settings);
    click(hooks()->baud_prev);
    click(hooks()->baud_apply);
    CHECK(ui_get_model()->baud_state == UI_BAUD_FAILED && ui_get_model()->applied_baud == 115200);
    CHECK(lv_obj_has_state(hooks()->baud_apply, LV_STATE_USER_2) && lv_img_get_src(hooks()->baud_apply_icon) == &ui_icon_cross_24);
    click(hooks()->conn_back);
    click(hooks()->light_sun_btn);
    CHECK(ui_get_model()->light_power == 1 && s_actions == before);
    ui_deinit();
    puts("PASS no handler: baud request fails at once, local controls still work");
}

static LRESULT CALLBACK hidden_proc(HWND window, UINT message, WPARAM wparam, LPARAM lparam)
{
    if (window_input(window, message, wparam, lparam)) return 0;
    return DefWindowProcA(window, message, wparam, lparam);
}

static void mouse(HWND window, UINT kind, int x, int y)
{
    SendMessageA(window, kind, kind == WM_LBUTTONUP ? 0 : MK_LBUTTON, MAKELPARAM(x * 2, y * 2));
}

/* Native Windows input through an invisible 960 x 640 window. */
static void regression_window(void)
{
    WNDCLASSA type;
    HWND window;
    RECT client;
    unsigned count;
    const UINT losses[] = { WM_CAPTURECHANGED, WM_CANCELMODE, WM_KILLFOCUS, WM_SIZE };
    ui_init(record_action, &s_actions);
    settle();
    memset(&type, 0, sizeof(type));
    type.lpfnWndProc = hidden_proc;
    type.hInstance = GetModuleHandleA(NULL);
    type.lpszClassName = "SmartStudioHiddenInput";
    CHECK(RegisterClassA(&type));
    window = CreateWindowA(type.lpszClassName, "hidden", WS_POPUP, 0, 0, 2 * SIM_W, 2 * SIM_H, NULL, NULL, type.hInstance, NULL);
    CHECK(window && !IsWindowVisible(window));
    CHECK(GetClientRect(window, &client) && client.right == 2 * SIM_W && client.bottom == 2 * SIM_H);
    /* Scaled swipe pages once. */
    mouse(window, WM_LBUTTONDOWN, 280, 290);
    mouse(window, WM_MOUSEMOVE, 40, 292);
    mouse(window, WM_LBUTTONUP, 40, 292);
    settle();
    CHECK(ui_get_page() == UI_PAGE_AC && GetCapture() != window);
    ui_navigate_to_page(UI_PAGE_LIGHT);
    settle();
    /* Each kind of input loss commits an established drag once and nothing else. */
    for (unsigned i = 0; i < sizeof(losses) / sizeof(losses[0]); i++) {
        CHECK(ui_apply(&(studio_cmd_t){ .kind = STUDIO_CMD_LIGHT, .data.light = { 0, 0 } }));
        settle();
        mouse(window, WM_LBUTTONDOWN, 200, 260);
        step(25);
        mouse(window, WM_MOUSEMOVE, 200, 160);
        step(25);
        count = s_actions;
        SendMessageA(window, losses[i], losses[i] == WM_SIZE ? SIZE_MINIMIZED : 0, 0);
        settle();
        CHECK(s_actions == count + 1 && s_last.data.light.bright == 50 && GetCapture() != window);
        mouse(window, WM_LBUTTONUP, 200, 160);
        settle();
        CHECK(s_actions == count + 1 && ui_get_page() == UI_PAGE_LIGHT);
    }
    DestroyWindow(window);
    UnregisterClassA(type.lpszClassName, type.hInstance);
    ui_deinit();
    puts("PASS window input: 2x scaling, capture/cancel/focus/minimize commit a drag once");
}

static void regression_real_size(void)
{
    char path[MAX_PATH + 64], dir[MAX_PATH];
    unsigned ppi = 0;
    int w, h;
    actual_size(166, &w, &h);
    CHECK(w == 480 && h == 320);
    actual_size(82, &w, &h);
    CHECK(w == 237 && h == 158);
    actual_size(109, &w, &h);
    CHECK(w == 315 && h == 210);
    CHECK(parse_ppi("109", &ppi) && ppi == 109);
    CHECK(parse_ppi("40", &ppi) && parse_ppi("600", &ppi));
    CHECK(!parse_ppi("", &ppi) && !parse_ppi("39", &ppi) && !parse_ppi("601", &ppi));
    CHECK(!parse_ppi("109.5", &ppi) && !parse_ppi("-96", &ppi) && !parse_ppi("1090", &ppi));
    CHECK(GetTempPathA(MAX_PATH, dir) > 0);
    (void)snprintf(path, sizeof(path), "%ssmart_studio_view_%lu.txt", dir, (unsigned long)GetCurrentProcessId());
    (void)DeleteFileA(path);
    CHECK(calibration_load(path, 1920, 1080) == 0);
    CHECK(calibration_save(path, 1920, 1080, 82));
    CHECK(calibration_save(path, 2560, 1440, 109));
    CHECK(calibration_save(path, 1920, 1080, 84));
    CHECK(calibration_load(path, 1920, 1080) == 84 && calibration_load(path, 2560, 1440) == 109);
    CHECK(calibration_load(path, 1280, 720) == 0);
    (void)DeleteFileA(path);
    puts("PASS real size: 73.44 x 48.96 mm at any density, --ppi parser, per-monitor calibration");
}

static int run_regression(void)
{
    lv_mem_monitor_t mem;
    if (!host_init()) return 1;
    regression_model();
    regression_host_time();
    regression_real_size();
    ui_init(record_action, &s_actions);
    settle();
    regression_pages();
    regression_light();
    regression_drag_timing();
    regression_ac();
    regression_scenes();
    regression_environment();
    regression_redraw();
    regression_connection();
    regression_lifecycle();
    lv_mem_monitor(&mem);
    CHECK(mem.free_size >= 4096 && s_min_settled_free >= 4096);
    regression_no_handler();
    regression_window();
    CHECK(s_memory.samples > 0 && s_memory.min_free >= 4096 && s_memory.failures == 0);
    printf("Heap: settled minimum free %lu bytes of %u; allocator-return minimum %llu, largest request %llu, failures %llu\n",
           (unsigned long)s_min_settled_free, (unsigned)UI_LVGL_HEAP_BYTES, (unsigned long long)s_memory.min_free,
           (unsigned long long)s_memory.largest_request, (unsigned long long)s_memory.failures);
    printf("PASS Smart Studio regression: %u checks\n", s_checks);
    return 0;
}

/* =========================================================================
 * Smoke, shots
 * ========================================================================= */

static int run_smoke(void)
{
    if (!host_init()) return 1;
    reset_flush_counters();
    ui_init(NULL, NULL);
    step(100);
    if (s_flush_calls == 0 || s_flush_pixels < (uint64_t)SIM_W * SIM_H) {
        fprintf(stderr, "Smoke: first frame not fully flushed (%llu pixels)\n", (unsigned long long)s_flush_pixels);
        return 1;
    }
    printf("Smoke: first frame %llu flushes, %llu pixels through a %u-line buffer\n",
           (unsigned long long)s_flush_calls, (unsigned long long)s_flush_pixels, (unsigned)UI_DRAW_BUF_LINES);
    ui_deinit();
    return 0;
}

static void home_fixture(void)
{
    ui_init(record_action, &s_actions);
    (void)ui_apply(&(studio_cmd_t){ .kind = STUDIO_CMD_ENV, .data.env = { 27, 58, true } });
    (void)ui_apply(&(studio_cmd_t){ .kind = STUDIO_CMD_SCENE, .data.scene = STUDIO_SCENE_HOME });
    settle();
}

static int run_single_shot(const char *path)
{
    if (!host_init()) return 1;
    home_fixture();
    if (!save_framebuffer(path)) {
        fprintf(stderr, "FATAL: could not write '%s'\n", path);
        return 1;
    }
    return 0;
}

static const char *s_shot_dir;
static unsigned s_shot_count;

static void shot(const char *name)
{
    char path[MAX_PATH];
    int n;
    settle();
    n = snprintf(path, sizeof(path), "%s\\%s.raw", s_shot_dir, name);
    CHECK(n > 0 && n < (int)sizeof(path));
    if (!save_framebuffer(path)) {
        fprintf(stderr, "FATAL: could not write '%s'\n", path);
        exit(1);
    }
    s_shot_count++;
    printf("Captured %s\n", name);
}

static int run_scenario_shots(const char *dir)
{
    DWORD attr;
    const ui_test_hooks_t *h;
    if (!CreateDirectoryA(dir, NULL) && GetLastError() != ERROR_ALREADY_EXISTS) attr = INVALID_FILE_ATTRIBUTES;
    else attr = GetFileAttributesA(dir);
    if (attr == INVALID_FILE_ATTRIBUTES || !(attr & FILE_ATTRIBUTE_DIRECTORY)) {
        fprintf(stderr, "FATAL: Output directory '%s' is not a valid directory or could not be created\n", dir);
        return 1;
    }
    s_shot_dir = dir;
    if (!host_init()) return 1;
    home_fixture();
    h = hooks();
    shot("light_on");
    point(200, 260, true);
    point(200, 200, true);
    shot("light_drag");
    point(200, 200, false);
    apply_ok(light_cmd(1, 27));
    shot("light_dim");
    apply_ok(light_cmd(0, 80));
    shot("light_off");
    swipe(1);
    shot("ac_on");
    apply_ok(ac_cmd(1, 16));
    shot("ac_min");
    apply_ok(ac_cmd(1, 30));
    shot("ac_max");
    apply_ok(ac_cmd(0, 30));
    shot("ac_off");
    swipe(1);
    shot("temperature");
    apply_ok(env_cmd(-32768, 100, true));
    shot("temperature_extreme");
    apply_ok(env_cmd(0, 0, false));
    shot("temperature_unavailable");
    apply_ok(env_cmd(27, 58, true));
    swipe(1);
    shot("humidity");
    apply_ok(env_cmd(27, 0, false));
    shot("humidity_unavailable");
    apply_ok(scene_cmd(STUDIO_SCENE_HOME));
    click(h->btn_scenes);
    shot("scenes_home");
    click(hooks()->scene_buttons[1]);
    shot("scenes_sleep");
    click(hooks()->scenes_back);
    click(hooks()->btn_settings);
    shot("connection_idle");
    click(hooks()->baud_next);
    shot("connection_step");
    click(hooks()->baud_apply);
    shot("connection_pending");
    CHECK(ui_baud_complete(230400, false));
    shot("connection_failed");
    printf("Generated %u scenario shots\n", s_shot_count);
    ui_deinit();
    return 0;
}

/* =========================================================================
 * Interactive window
 * ========================================================================= */

static uint64_t s_painted = UINT64_MAX;

static void paint(HWND window)
{
    PAINTSTRUCT ps;
    RECT r;
    struct { BITMAPINFOHEADER header; DWORD masks[3]; } bitmap;
    HDC dc = BeginPaint(window, &ps);
    GetClientRect(window, &r);
    memset(&bitmap, 0, sizeof(bitmap));
    bitmap.header.biSize = sizeof(BITMAPINFOHEADER);
    bitmap.header.biWidth = SIM_W;
    bitmap.header.biHeight = -SIM_H;
    bitmap.header.biPlanes = 1;
    bitmap.header.biBitCount = 16;
    bitmap.header.biCompression = BI_BITFIELDS;
    bitmap.masks[0] = 0xF800;
    bitmap.masks[1] = 0x07E0;
    bitmap.masks[2] = 0x001F;
    /* Whole multiples stay pixel exact; other sizes are averaged. */
    if (r.right % SIM_W || r.bottom % SIM_H) {
        SetStretchBltMode(dc, HALFTONE);
        SetBrushOrgEx(dc, 0, 0, NULL);
    } else {
        SetStretchBltMode(dc, COLORONCOLOR);
    }
    if (r.right > 0 && r.bottom > 0) {
        StretchDIBits(dc, 0, 0, r.right, r.bottom, 0, 0, SIM_W, SIM_H, s_framebuffer,
                      (const BITMAPINFO *)&bitmap, DIB_RGB_COLORS, SRCCOPY);
    }
    EndPaint(window, &ps);
}

static LRESULT CALLBACK window_proc(HWND window, UINT message, WPARAM wparam, LPARAM lparam)
{
    if (window_input(window, message, wparam, lparam)) return 0;
    switch (message) {
    case WM_PAINT:
        paint(window);
        return 0;
    case WM_EXITSIZEMOVE:
    case WM_DPICHANGED:
        apply_view(window, true);
        return 0;
    case WM_KEYDOWN:
        if (wparam == VK_ESCAPE) DestroyWindow(window);
        else if (wparam >= '1' && wparam <= '4') ui_navigate_to_page((ui_page_id_t)(wparam - '1'));
        else if (wparam == 'V') { s_view = (view_t)((s_view + 1) % VIEW_COUNT); apply_view(window, true); }
        else if (wparam == VK_OEM_PLUS || wparam == VK_ADD) adjust_ppi(window, 1);
        else if (wparam == VK_OEM_MINUS || wparam == VK_SUBTRACT) adjust_ppi(window, -1);
        else if (wparam == 'H') demo_scene(STUDIO_SCENE_HOME);
        else if (wparam == 'S') demo_scene(STUDIO_SCENE_SLEEP);
        else if (wparam == 'A') demo_scene(STUDIO_SCENE_AWAY);
        else if (wparam == 'E') { s_env_available = !s_env_available; demo_env(); }
        else if (wparam == 'F') { s_fail_next_baud = true; puts("Next baud request will fail"); }
        return 0;
    case WM_CLOSE:
        DestroyWindow(window);
        return 0;
    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;
    default:
        return DefWindowProcA(window, message, wparam, lparam);
    }
}

static int run_interactive(void)
{
    const DWORD style = WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX;
    WNDCLASSA type;
    HWND window;
    MSG message;
    LARGE_INTEGER frequency, previous, current;
    int64_t remainder = 0;
    RECT bounds = { 0, 0, SIM_W, SIM_H };

    dpi_aware();
    calibration_path_init();
    if (!host_init()) return 1;
    ui_init(demo_action, NULL);
    demo_env();
    demo_scene(STUDIO_SCENE_HOME);
    step(100);

    memset(&type, 0, sizeof(type));
    type.lpfnWndProc = window_proc;
    type.hInstance = GetModuleHandleA(NULL);
    type.lpszClassName = "SmartStudioSimulator";
    type.hCursor = LoadCursor(NULL, IDC_ARROW);
    if (!RegisterClassA(&type)) return 1;
    AdjustWindowRect(&bounds, style, FALSE);
    window = CreateWindowA(type.lpszClassName, "Smart Studio 3.5\"", style, CW_USEDEFAULT, CW_USEDEFAULT,
                           bounds.right - bounds.left, bounds.bottom - bounds.top, NULL, NULL, type.hInstance, NULL);
    if (!window) return 1;
    apply_view(window, true);
    ShowWindow(window, SW_SHOW);
    puts("Keys: 1-4 page, V view (real size / 1:1 / 2:1), + - calibrate real size,");
    puts("      H S A scene from the PC, E room data on/off, F fail next baud, Esc quit.");
    puts("Touch actions are printed as the JSON events the firmware would send.");
    fflush(stdout);

    QueryPerformanceFrequency(&frequency);
    QueryPerformanceCounter(&previous);
    for (;;) {
        while (PeekMessageA(&message, NULL, 0, 0, PM_REMOVE)) {
            if (message.message == WM_QUIT) {
                ui_deinit();
                return 0;
            }
            TranslateMessage(&message);
            DispatchMessageA(&message);
        }
        QueryPerformanceCounter(&current);
        remainder += (current.QuadPart - previous.QuadPart) * 1000;
        previous = current;
        if (remainder >= frequency.QuadPart) {
            uint32_t elapsed = (uint32_t)(remainder / frequency.QuadPart);
            remainder %= frequency.QuadPart;
            realtime_step(elapsed);
        }
        if (s_baud_request && (int32_t)(lv_tick_get() - s_baud_due) >= 0) {
            (void)ui_baud_complete(s_baud_request, !s_fail_next_baud);
            s_baud_request = 0;
            s_fail_next_baud = false;
        }
        if (s_painted != s_flush_calls) {
            s_painted = s_flush_calls;
            InvalidateRect(window, NULL, FALSE);
        }
        MsgWaitForMultipleObjects(0, NULL, FALSE, 5, QS_ALLINPUT);
    }
}

int main(int argc, char **argv)
{
    if (argc == 1) return run_interactive();
    if (argc == 3 && strcmp(argv[1], "--ppi") == 0) {
        if (!parse_ppi(argv[2], &s_ppi_override)) {
            fprintf(stderr, "--ppi takes a whole number from 40 to 600\n");
            return 2;
        }
        return run_interactive();
    }
    if (argc == 2 && strcmp(argv[1], "--smoke") == 0) return run_smoke();
    if (argc == 2 && strcmp(argv[1], "--regression") == 0) return run_regression();
    if (argc == 3 && strcmp(argv[1], "--shot") == 0) return run_single_shot(argv[2]);
    if (argc == 3 && strcmp(argv[1], "--shots") == 0) return run_scenario_shots(argv[2]);
    fprintf(stderr, "Usage: %s [--ppi N | --smoke | --regression | --shot <out.raw> | --shots <dir>]\n", argv[0]);
    return 2;
}
