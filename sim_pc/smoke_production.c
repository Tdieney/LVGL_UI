/* =========================================================================
 * smoke_production.c
 *
 * Compiles the exported UI sources as strict C99 without UI_TEST_HOOKS and
 * runs them once against a dummy display, as the firmware would.
 * ========================================================================= */

#include "ui.h"
#include "ui_mcu_profile.h"
#include "lvgl.h"
#include <stdio.h>
#include <string.h>

#ifdef UI_TEST_HOOKS
#error The production smoke test must not enable test hooks
#endif

#define SMOKE_CHECK(cond) do { \
    if (!(cond)) { \
        fprintf(stderr, "[SMOKE ERROR] Check failed: %s (%s:%d)\n", #cond, __FILE__, __LINE__); \
        return 1; \
    } \
} while (0)

static unsigned s_events;

static void dummy_flush_cb(lv_disp_drv_t *drv, const lv_area_t *area, lv_color_t *color_p)
{
    (void)area;
    (void)color_p;
    lv_disp_flush_ready(drv);
}

static void on_action(const studio_cmd_t *cmd, void *context)
{
    (void)cmd;
    (void)context;
    s_events++;
}

int main(int argc, char **argv)
{
    static lv_color_t buf[UI_DRAW_BUF_PIXELS];
    static lv_disp_draw_buf_t disp_buf;
    static lv_disp_drv_t disp_drv;
    studio_cmd_t cmd;

    if (argc > 1 && strcmp(argv[1], "--fail-check") == 0) {
        printf("[SMOKE] Running negative test (deliberate failure detection)...\n");
        SMOKE_CHECK(1 == 2);
    }

    printf("[SMOKE] Starting production UI smoke test (UI_TEST_HOOKS disabled)...\n");
    lv_init();
    lv_disp_draw_buf_init(&disp_buf, buf, NULL, UI_DRAW_BUF_PIXELS);
    lv_disp_drv_init(&disp_drv);
    disp_drv.draw_buf = &disp_buf;
    disp_drv.hor_res = UI_DISPLAY_HOR_RES;
    disp_drv.ver_res = UI_DISPLAY_VER_RES;
    disp_drv.flush_cb = dummy_flush_cb;
    SMOKE_CHECK(lv_disp_drv_register(&disp_drv) != NULL);

    ui_init(on_action, NULL);

    memset(&cmd, 0, sizeof(cmd));
    cmd.kind = STUDIO_CMD_SCENE;
    cmd.data.scene = STUDIO_SCENE_SLEEP;
    SMOKE_CHECK(ui_apply(&cmd));
    SMOKE_CHECK(!ui_get_model()->light_power && ui_get_model()->ac_power && ui_get_model()->ac_temp == 26);

    memset(&cmd, 0, sizeof(cmd));
    cmd.kind = STUDIO_CMD_ENV;
    cmd.data.env.temp = 27;
    cmd.data.env.humid = 58;
    cmd.data.env.valid = true;
    SMOKE_CHECK(ui_apply(&cmd));
    ui_note_rx();

    memset(&cmd, 0, sizeof(cmd));
    cmd.kind = STUDIO_CMD_LIGHT;
    cmd.data.light.power = 2;
    SMOKE_CHECK(!ui_apply(&cmd));
    SMOKE_CHECK(s_events == 0);

    for (int page = 0; page < (int)UI_PAGE_COUNT; page++) {
        ui_navigate_to_page((ui_page_id_t)page);
        lv_tick_inc(25);
        (void)lv_timer_handler();
        SMOKE_CHECK(ui_get_page() == (ui_page_id_t)page);
    }
    ui_deinit();

    printf("[SMOKE] Production UI smoke test PASSED successfully.\n");
    return 0;
}
