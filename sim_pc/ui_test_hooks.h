#ifndef UI_TEST_HOOKS_H
#define UI_TEST_HOOKS_H

/* Simulator-only view of the UI's widgets and gesture state. ui.c fills it
 * when built with UI_TEST_HOOKS; the MCU export never defines that. */
#include "ui.h"
#include "ui_internal.h"
#include "lvgl.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    const ui_gesture_t *gesture;
    ui_page_id_t page;
    ui_panel_t panel;

    lv_obj_t *root;
    lv_obj_t *btn_scenes;
    lv_obj_t *btn_settings;
    lv_obj_t *pages[UI_PAGE_COUNT];

    lv_obj_t *light_sun_btn;
    lv_obj_t *light_sun_core;
    lv_obj_t *light_rays[16];

    lv_obj_t *ac_toggle;
    lv_obj_t *ac_value;
    lv_obj_t *ac_plus;
    lv_obj_t *ac_minus;

    lv_obj_t *temp_tile;
    lv_obj_t *temp_icon;
    lv_obj_t *temp_value;
    lv_obj_t *humid_tile;
    lv_obj_t *humid_icon;
    lv_obj_t *humid_value;

    lv_obj_t *scenes_panel;
    lv_obj_t *scenes_back;
    lv_obj_t *scene_buttons[3];
    lv_obj_t *scene_icons[3];

    lv_obj_t *conn_panel;
    lv_obj_t *conn_back;
    lv_obj_t *conn_rx_dot;
    lv_obj_t *baud_active_dot;
    lv_obj_t *baud_prev;
    lv_obj_t *baud_next;
    lv_obj_t *baud_value;
    lv_obj_t *baud_apply;
    lv_obj_t *baud_apply_icon;
} ui_test_hooks_t;

const ui_test_hooks_t *ui_get_test_hooks(void);

#ifdef __cplusplus
}
#endif

#endif /* UI_TEST_HOOKS_H */
