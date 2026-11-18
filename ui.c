/* Smart Studio UI: four swipe pages (Light, Air conditioner, Temperature,
 * Humidity), a Scenes overlay and a Connection overlay on a 480 x 320 panel.
 * Zero-text icon-and-digit design; styles in ui_theme.c, icons in ui_icons.c. */
#include "ui.h"
#include "ui_internal.h"
#include "ui_model.h"
#include "ui_theme.h"
#include "ui_fonts.h"
#include "ui_icons.h"
#include "ui_mcu_profile.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef UI_TEST_HOOKS
#include "ui_test_hooks.h"
#endif

#define UI_DISPLAY_W   ((int)UI_DISPLAY_HOR_RES)
#define UI_DISPLAY_H   ((int)UI_DISPLAY_VER_RES)
#define HEADER_BTN_X_L 14
#define HEADER_BTN_X_R 422
#define HEADER_BTN_Y   12
#define HEADER_BTN     44
#define HERO_TILE_X    18
#define HERO_TILE_Y    96
#define AC_VAL_X       140
#define AC_VAL_W       224
#define HERO_VAL_X     128
#define HERO_VAL_Y     125
#define HERO_VAL_W     248

#define TEXT_EM_DASH   "\xE2\x80\x94"
#define TEXT_DEGREE    "\xC2\xB0"

typedef struct {
    ui_model_t model;
    ui_action_cb_t callback;
    void *context;
    lv_timer_t *timer;
    bool initialized;
    ui_page_id_t page;
    int next_page;          /* -1 when no page change is waiting */
    ui_panel_t panel;
    int panel_request;      /* -1 when no panel change is waiting */
    ui_gesture_t g;
    uint32_t selected_baud;
    bool rx_seen;

    lv_obj_t *root, *btn_scenes, *btn_settings;
    lv_obj_t *pages[UI_PAGE_COUNT];

    lv_obj_t *light_sun_btn, *light_sun_core;
    lv_obj_t *light_rays[16];

    lv_obj_t *ac_toggle, *ac_icon, *ac_value;
    lv_obj_t *ac_plus, *ac_minus, *ac_plus_icon, *ac_minus_icon;

    lv_obj_t *temp_tile, *temp_icon, *temp_value;
    lv_obj_t *humid_tile, *humid_icon, *humid_value;

    lv_obj_t *scenes_panel, *scenes_back;
    lv_obj_t *scene_buttons[3];
    lv_obj_t *scene_icons[3];

    lv_obj_t *conn_panel, *conn_back, *conn_rx_dot, *baud_active_dot;
    lv_obj_t *baud_prev, *baud_next, *baud_prev_icon, *baud_next_icon;
    lv_obj_t *baud_value, *baud_apply, *baud_apply_icon;

    char ac_text[12];
    char temp_text[16];
    char humid_text[16];
    char baud_text[16];
} ui_state_t;

static ui_state_t s_ui;

static const uint32_t s_baud_rates[STUDIO_BAUD_COUNT] = STUDIO_BAUD_RATES;

static void render_all(void);
static void render_light(void);
static void render_connection(void);

/* ---- Small helpers ------------------------------------------------------- */

static const lv_style_t *font_style(const lv_font_t *font)
{
    return (font == &ui_font_digits_76) ? &ui_style_text_76 : &ui_style_text_48;
}

static lv_obj_t *add_box(lv_obj_t *parent, int x, int y, int w, int h, const lv_style_t *style)
{
    lv_obj_t *obj = lv_obj_create(parent);
    lv_obj_remove_style_all(obj);
    lv_obj_add_style(obj, &ui_style_base, 0);
    if (style) lv_obj_add_style(obj, style, 0);
    lv_obj_set_pos(obj, (lv_coord_t)x, (lv_coord_t)y);
    lv_obj_set_size(obj, (lv_coord_t)w, (lv_coord_t)h);
    lv_obj_clear_flag(obj, LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_CLICKABLE);
    return obj;
}

static lv_obj_t *add_label(lv_obj_t *parent, int x, int y, int w, const char *text,
                           const lv_font_t *font, const lv_style_t *color)
{
    lv_obj_t *label = lv_label_create(parent);
    lv_obj_remove_style_all(label);
    lv_obj_add_style(label, &ui_style_base, 0);
    lv_obj_add_style(label, font_style(font), 0);
    lv_obj_add_style(label, &ui_style_text_center, 0);
    if (color) lv_obj_add_style(label, color, 0);
    lv_obj_set_pos(label, (lv_coord_t)x, (lv_coord_t)y);
    lv_obj_set_size(label, w ? (lv_coord_t)w : LV_SIZE_CONTENT, LV_SIZE_CONTENT);
    lv_label_set_long_mode(label, LV_LABEL_LONG_CLIP);
    lv_label_set_text_static(label, text);
    lv_obj_clear_flag(label, LV_OBJ_FLAG_CLICKABLE);
    return label;
}

static lv_obj_t *add_icon(lv_obj_t *parent, const lv_img_dsc_t *src, const lv_style_t *color)
{
    lv_obj_t *img = lv_img_create(parent);
    lv_obj_add_style(img, color, 0);
    lv_img_set_src(img, src);
    lv_obj_clear_flag(img, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_center(img);
    return img;
}

static lv_obj_t *add_icon_at(lv_obj_t *parent, int x, int y, const lv_img_dsc_t *src, const lv_style_t *color)
{
    lv_obj_t *img = lv_img_create(parent);
    lv_obj_add_style(img, color, 0);
    lv_img_set_src(img, src);
    lv_obj_set_pos(img, (lv_coord_t)x, (lv_coord_t)y);
    lv_obj_clear_flag(img, LV_OBJ_FLAG_CLICKABLE);
    return img;
}

/* Controls report every press to their page root or panel (event bubbling)
 * and keep their press while the finger slides off, so a swipe that crosses
 * another control never presses it. */
static void make_page_control(lv_obj_t *obj)
{
    lv_obj_add_flag(obj, LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_PRESS_LOCK | LV_OBJ_FLAG_EVENT_BUBBLE);
}

static void set_state(lv_obj_t *obj, lv_state_t state, bool on)
{
    if (lv_obj_has_state(obj, state) == on) return;
    if (on) lv_obj_add_state(obj, state);
    else lv_obj_clear_state(obj, state);
}

static void set_hidden(lv_obj_t *obj, bool hidden)
{
    if (lv_obj_has_flag(obj, LV_OBJ_FLAG_HIDDEN) == hidden) return;
    if (hidden) lv_obj_add_flag(obj, LV_OBJ_FLAG_HIDDEN);
    else lv_obj_clear_flag(obj, LV_OBJ_FLAG_HIDDEN);
}

static void set_text(lv_obj_t *label, char *owned, size_t capacity, const char *text)
{
    size_t len = strlen(text);
    if (lv_label_get_text(label) == owned && strcmp(owned, text) == 0) return;
    if (len >= capacity) return;
    memcpy(owned, text, len + 1);
    lv_label_set_text_static(label, owned);
}

static unsigned baud_index(uint32_t baud)
{
    for (unsigned i = 0; i < STUDIO_BAUD_COUNT; i++) {
        if (s_baud_rates[i] == baud) return i;
    }
    return 4u;
}

/* A hidden or re-shown view must not keep acting on a press that began
 * before the change: cancel the pointer's press and wait for its release. */
static void cancel_gesture(void);
static void cancel_transition_input(void)
{
    lv_indev_t *input = lv_indev_get_next(NULL);
    cancel_gesture();
    for (; input; input = lv_indev_get_next(input)) {
        lv_indev_type_t type = lv_indev_get_type(input);
        if (type != LV_INDEV_TYPE_POINTER && type != LV_INDEV_TYPE_BUTTON) continue;
        /* LVGL 8.4 has no public getter for the pressed state outside input
         * callbacks; read its runtime state and change it through the API. */
        if (input->proc.state != LV_INDEV_STATE_PRESSED) continue;
        if (input->proc.types.pointer.act_obj) {
            (void)lv_event_send(input->proc.types.pointer.act_obj, LV_EVENT_PRESS_LOST, input);
        }
        lv_indev_reset(input, NULL);
        lv_indev_wait_release(input);
    }
}

static void set_view_hidden(lv_obj_t *obj, bool hidden)
{
    if (lv_obj_has_flag(obj, LV_OBJ_FLAG_HIDDEN) == hidden) return;
    if (s_ui.initialized) cancel_transition_input();
    set_hidden(obj, hidden);
}

/* ---- Commands ------------------------------------------------------------ */

static void notify(const studio_cmd_t *cmd)
{
    if (s_ui.callback) s_ui.callback(cmd, s_ui.context);
}

static void dispatch(const studio_cmd_t *cmd)
{
    if (ui_model_apply(&s_ui.model, cmd)) {
        render_all();
        notify(cmd);
    }
}

static int effective_level(void)
{
    return s_ui.model.light_power ? s_ui.model.light_bright : 0;
}

static studio_cmd_t draft_command(void)
{
    studio_cmd_t cmd = { .kind = STUDIO_CMD_LIGHT };
    cmd.data.light.bright = s_ui.g.draft;
    cmd.data.light.power = s_ui.g.draft > 0;
    return cmd;
}

/* Intermediate drag values go out at most once per 100 ms; the final value
 * on release always goes out. */
static void apply_draft(bool final)
{
    ui_gesture_t *g = &s_ui.g;
    studio_cmd_t cmd = draft_command();
    uint32_t now = lv_tick_get();
    (void)ui_model_apply(&s_ui.model, &cmd);
    if (final || !g->emitted || (uint32_t)(now - g->last_emit) >= UI_DRAG_EMIT_MS) {
        g->pending = false;
        g->emitted = true;
        g->last_emit = now;
        notify(&cmd);
    } else {
        g->pending = true;
    }
    render_all();
}

static void cancel_gesture(void)
{
    ui_gesture_t *g = &s_ui.g;
    if (!g->active) return;
    g->canceled = true;
    g->tap = false;
    if (g->dragging) apply_draft(true);
    g->active = false;
    g->dragging = false;
    g->pending = false;
    render_light();
}

/* ---- Touch: taps, swipes and drags --------------------------------------- *
 * The page root and both panels receive every press of their controls by
 * event bubbling. On the root a horizontal swipe changes the page and a
 * vertical drag on Light dims; on a panel motion only cancels the tap.
 * Controls act on LV_EVENT_CLICKED only when the release was a tap. */

static void touch_event(lv_event_t *e)
{
    lv_event_code_t code = lv_event_get_code(e);
    ui_gesture_t *g = &s_ui.g;
    bool on_root = lv_event_get_current_target(e) == s_ui.root;
    lv_indev_t *input;
    lv_point_t p;
    int dx, dy;

    if (code == LV_EVENT_PRESS_LOST) {
        cancel_gesture();
        return;
    }
    if (code != LV_EVENT_PRESSED && code != LV_EVENT_PRESSING && code != LV_EVENT_RELEASED) return;
    input = lv_indev_get_act();
    if (!input) return;
    lv_indev_get_point(input, &p);

    if (code == LV_EVENT_PRESSED) {
        g->active = true;
        g->canceled = false;
        g->dragging = false;
        g->tap = false;
        g->emitted = false;
        g->pending = false;
        g->axis = UI_AXIS_NONE;
        g->start = p;
        g->start_level = effective_level();
        g->draft = g->start_level;
        return;
    }
    if (!g->active) return;

    dx = p.x - g->start.x;
    dy = p.y - g->start.y;
    if (g->axis == UI_AXIS_NONE && (abs(dx) >= UI_AXIS_LOCK_PX || abs(dy) >= UI_AXIS_LOCK_PX)) {
        lv_obj_t *pressed = lv_indev_get_obj_act();
        g->axis = abs(dx) >= abs(dy) ? UI_AXIS_HORIZONTAL : UI_AXIS_VERTICAL;
        /* A swipe that starts on a button must not look like a press. */
        if (pressed && pressed != lv_event_get_current_target(e)) lv_obj_clear_state(pressed, LV_STATE_PRESSED);
    }
    if (on_root && g->axis == UI_AXIS_VERTICAL && s_ui.page == UI_PAGE_LIGHT) {
        int level = g->start_level - dy / UI_DIM_PX_PER_STEP;
        if (level < 0) level = 0;
        if (level > 100) level = 100;
        if (!g->dragging) {
            g->dragging = true;
            render_light();
        }
        if (level != g->draft) {
            g->draft = level;
            apply_draft(false);
        }
    }
    if (code == LV_EVENT_RELEASED) {
        if (g->dragging) {
            apply_draft(true);
        } else if (on_root && g->axis == UI_AXIS_HORIZONTAL && abs(dx) >= UI_PAGE_SWIPE_PX) {
            int next = (int)s_ui.page + (dx < 0 ? 1 : -1);
            if (next >= 0 && next < (int)UI_PAGE_COUNT) s_ui.next_page = next;
        }
        g->tap = g->axis == UI_AXIS_NONE;
        g->active = false;
        g->dragging = false;
        g->pending = false;
        render_light();
    }
}

/* ---- Control callbacks --------------------------------------------------- */

static void on_light_sun(lv_event_t *e)
{
    const ui_model_t *m = &s_ui.model;
    studio_cmd_t cmd = { .kind = STUDIO_CMD_LIGHT };
    (void)e;
    if (!s_ui.g.tap) return;
    cmd.data.light.power = !m->light_power;
    cmd.data.light.bright = cmd.data.light.power ? m->light_last_nonzero_bright : m->light_bright;
    dispatch(&cmd);
}

static void on_ac(lv_event_t *e)
{
    lv_obj_t *target = lv_event_get_current_target(e);
    const ui_model_t *m = &s_ui.model;
    studio_cmd_t cmd = { .kind = STUDIO_CMD_AC };
    if (!s_ui.g.tap) return;
    cmd.data.ac.power = m->ac_power;
    cmd.data.ac.temp = m->ac_temp;
    if (target == s_ui.ac_toggle) cmd.data.ac.power = !m->ac_power;
    else if (target == s_ui.ac_minus && m->ac_temp > 16) cmd.data.ac.temp--;
    else if (target == s_ui.ac_plus && m->ac_temp < 30) cmd.data.ac.temp++;
    else return;
    dispatch(&cmd);
}

static void on_header(lv_event_t *e)
{
    lv_obj_t *target = lv_event_get_current_target(e);
    if (!s_ui.g.tap) return;
    s_ui.panel_request = target == s_ui.btn_settings ? UI_PANEL_CONNECTION : UI_PANEL_SCENES;
}

static void on_panel_back(lv_event_t *e)
{
    (void)e;
    if (!s_ui.g.tap) return;
    s_ui.panel_request = UI_PANEL_NONE;
}

static void on_scene(lv_event_t *e)
{
    studio_cmd_t cmd = { .kind = STUDIO_CMD_SCENE };
    if (!s_ui.g.tap) return;
    cmd.data.scene = (studio_scene_t)(uintptr_t)lv_event_get_user_data(e);
    dispatch(&cmd);
}

static void on_baud_step(lv_event_t *e)
{
    ui_model_t *m = &s_ui.model;
    lv_obj_t *target = lv_event_get_current_target(e);
    unsigned idx;
    if (!s_ui.g.tap || m->baud_state == UI_BAUD_PENDING) return;
    idx = baud_index(s_ui.selected_baud);
    if (target == s_ui.baud_prev && idx > 0u) idx--;
    else if (target == s_ui.baud_next && idx + 1u < STUDIO_BAUD_COUNT) idx++;
    else return;
    s_ui.selected_baud = s_baud_rates[idx];
    if (m->baud_state == UI_BAUD_FAILED) m->baud_state = UI_BAUD_IDLE;
    render_connection();
}

static void on_baud_apply(lv_event_t *e)
{
    ui_model_t *m = &s_ui.model;
    studio_cmd_t cmd = { .kind = STUDIO_CMD_BAUD };
    (void)e;
    if (!s_ui.g.tap || m->baud_state == UI_BAUD_PENDING) return;
    cmd.data.baud = s_ui.selected_baud;
    if (s_ui.callback == NULL) {
        m->pending_baud = 0;
        m->baud_state = UI_BAUD_FAILED;
    } else {
        m->pending_baud = cmd.data.baud;
        m->baud_state = UI_BAUD_PENDING;
    }
    render_connection();
    /* The callback queues the request and returns; the firmware reports the
     * outcome later through ui_baud_complete(). */
    notify(&cmd);
}

/* ---- Rendering ----------------------------------------------------------- */

static int light_ray_units(int level)
{
    int units;
    if (level <= 0) return 0;
    if (level >= 100) return 32;
    units = (level * 32 + 50) / 100;
    if (units < 1) units = 1;
    if (units > 31) units = 31;
    return units;
}

static void render_light(void)
{
    const ui_gesture_t *g = &s_ui.g;
    int level = g->dragging ? g->draft : effective_level();
    int units = light_ray_units(level);
    bool sun_lit = g->dragging ? (g->draft > 0) : (s_ui.model.light_power != 0);

    set_state(s_ui.light_sun_btn, LV_STATE_CHECKED, sun_lit);
    set_state(s_ui.light_sun_core, LV_STATE_CHECKED, sun_lit);

    for (int k = 0; k < 16; k++) {
        bool full = units >= (k + 1) * 2;
        bool half = !full && (units == k * 2 + 1);
        set_state(s_ui.light_rays[k], LV_STATE_CHECKED, full);
        set_state(s_ui.light_rays[k], LV_STATE_USER_1, half);
    }
}

static void render_ac(void)
{
    const ui_model_t *m = &s_ui.model;
    char text[12];
    bool on = m->ac_power != 0;

    (void)snprintf(text, sizeof(text), "%d" TEXT_DEGREE, m->ac_temp);
    set_text(s_ui.ac_value, s_ui.ac_text, sizeof(s_ui.ac_text), text);
    set_state(s_ui.ac_value, LV_STATE_CHECKED, on);
    set_state(s_ui.ac_toggle, LV_STATE_CHECKED, on);
    set_state(s_ui.ac_icon, LV_STATE_CHECKED, on);
    set_state(s_ui.ac_minus, LV_STATE_DISABLED, m->ac_temp <= 16);
    set_state(s_ui.ac_minus_icon, LV_STATE_DISABLED, m->ac_temp <= 16);
    set_state(s_ui.ac_plus, LV_STATE_DISABLED, m->ac_temp >= 30);
    set_state(s_ui.ac_plus_icon, LV_STATE_DISABLED, m->ac_temp >= 30);
}

static void set_hero_text(lv_obj_t *label, char *owned, size_t capacity, const char *text)
{
    lv_point_t sz;
    size_t len = strlen(text);
    if (lv_label_get_text(label) == owned && strcmp(owned, text) == 0) return;
    if (len >= capacity) return;
    lv_txt_get_size(&sz, text, &ui_font_digits_76, 0, 0, LV_COORD_MAX, LV_TEXT_FLAG_NONE);
    set_state(label, LV_STATE_USER_1, sz.x > HERO_VAL_W);
    memcpy(owned, text, len + 1);
    lv_label_set_text_static(label, owned);
}

static void render_env(void)
{
    const ui_model_t *m = &s_ui.model;
    char text[16];

    if (m->env_valid) {
        (void)snprintf(text, sizeof(text), "%d" TEXT_DEGREE, (int)m->env_temp);
    } else {
        (void)snprintf(text, sizeof(text), "%s", TEXT_EM_DASH);
    }
    set_hero_text(s_ui.temp_value, s_ui.temp_text, sizeof(s_ui.temp_text), text);
    set_state(s_ui.temp_icon, LV_STATE_CHECKED, m->env_valid);
    set_state(s_ui.temp_value, LV_STATE_CHECKED, m->env_valid);

    if (m->env_valid) {
        (void)snprintf(text, sizeof(text), "%u%%", (unsigned)m->env_humid);
    } else {
        (void)snprintf(text, sizeof(text), "%s", TEXT_EM_DASH);
    }
    set_hero_text(s_ui.humid_value, s_ui.humid_text, sizeof(s_ui.humid_text), text);
    set_state(s_ui.humid_icon, LV_STATE_CHECKED, m->env_valid);
    set_state(s_ui.humid_value, LV_STATE_CHECKED, m->env_valid);
}

static void render_scenes(void)
{
    for (unsigned i = 0; i < 3; i++) {
        bool selected = s_ui.model.selected_scene == (studio_scene_t)(i + 1);
        set_state(s_ui.scene_buttons[i], LV_STATE_CHECKED, selected);
        set_state(s_ui.scene_icons[i], LV_STATE_CHECKED, selected);
    }
}

static void render_connection(void)
{
    const ui_model_t *m = &s_ui.model;
    char text[16];
    unsigned idx = baud_index(s_ui.selected_baud);
    bool pending = m->baud_state == UI_BAUD_PENDING;
    bool failed = m->baud_state == UI_BAUD_FAILED;
    bool prev_dis = pending || idx == 0u;
    bool next_dis = pending || idx + 1u >= STUDIO_BAUD_COUNT;
    const lv_img_dsc_t *apply_src = pending ? &ui_icon_dots_28 :
                                    failed  ? &ui_icon_cross_24 :
                                              &ui_icon_check_28;

    (void)snprintf(text, sizeof(text), "%lu", (unsigned long)s_ui.selected_baud);
    set_text(s_ui.baud_value, s_ui.baud_text, sizeof(s_ui.baud_text), text);
    set_hidden(s_ui.baud_active_dot, s_ui.selected_baud != m->applied_baud);
    set_state(s_ui.conn_rx_dot, LV_STATE_CHECKED, s_ui.rx_seen);

    set_state(s_ui.baud_prev, LV_STATE_DISABLED, prev_dis);
    set_state(s_ui.baud_prev_icon, LV_STATE_DISABLED, prev_dis);
    set_state(s_ui.baud_next, LV_STATE_DISABLED, next_dis);
    set_state(s_ui.baud_next_icon, LV_STATE_DISABLED, next_dis);

    set_state(s_ui.baud_apply, LV_STATE_DISABLED, pending);
    set_state(s_ui.baud_apply, LV_STATE_USER_1, pending);
    set_state(s_ui.baud_apply, LV_STATE_USER_2, failed);
    set_state(s_ui.baud_apply_icon, LV_STATE_USER_1, pending);
    set_state(s_ui.baud_apply_icon, LV_STATE_USER_2, failed);
    if (lv_img_get_src(s_ui.baud_apply_icon) != apply_src) {
        lv_img_set_src(s_ui.baud_apply_icon, apply_src);
    }
}

static void render_all(void)
{
    render_light();
    render_ac();
    render_env();
    render_scenes();
    render_connection();
}

/* ---- Views ---------------------------------------------------------------- */

static void show_page(ui_page_id_t page)
{
    if (page >= UI_PAGE_COUNT || page == s_ui.page) return;
    set_view_hidden(s_ui.pages[s_ui.page], true);
    s_ui.page = page;
    set_view_hidden(s_ui.pages[s_ui.page], false);
}

static void show_panel(ui_panel_t panel)
{
    if (panel == s_ui.panel) return;
    if (panel == UI_PANEL_CONNECTION) render_connection();
    s_ui.panel = panel;
    set_view_hidden(s_ui.scenes_panel, panel != UI_PANEL_SCENES);
    set_view_hidden(s_ui.conn_panel, panel != UI_PANEL_CONNECTION);
    set_view_hidden(s_ui.root, panel != UI_PANEL_NONE);
}

static void timer_cb(lv_timer_t *timer)
{
    ui_gesture_t *g = &s_ui.g;
    (void)timer;
    if (s_ui.next_page >= 0) {
        ui_page_id_t page = (ui_page_id_t)s_ui.next_page;
        s_ui.next_page = -1;
        show_page(page);
    }
    if (s_ui.panel_request >= 0) {
        ui_panel_t panel = (ui_panel_t)s_ui.panel_request;
        s_ui.panel_request = -1;
        show_panel(panel);
    }
    if (g->dragging && g->pending && (uint32_t)(lv_tick_get() - g->last_emit) >= UI_DRAG_EMIT_MS) {
        studio_cmd_t cmd = draft_command();
        g->pending = false;
        g->last_emit = lv_tick_get();
        notify(&cmd);
    }
}

/* ---- Build ---------------------------------------------------------------- */

static lv_obj_t *add_header_button(lv_obj_t *parent, int x, const lv_img_dsc_t *icon, lv_event_cb_t cb)
{
    lv_obj_t *btn = add_box(parent, x, HEADER_BTN_Y, HEADER_BTN, HEADER_BTN, &ui_style_btn_header);
    lv_obj_add_style(btn, &ui_style_btn_pressed, LV_STATE_PRESSED);
    (void)add_icon(btn, icon, &ui_style_icon_idle);
    make_page_control(btn);
    lv_obj_add_event_cb(btn, cb, LV_EVENT_CLICKED, NULL);
    return btn;
}

static lv_obj_t *add_page(ui_page_id_t page)
{
    lv_obj_t *box = add_box(s_ui.root, 0, 0, UI_DISPLAY_W, UI_DISPLAY_H, NULL);
    lv_obj_add_flag(box, LV_OBJ_FLAG_EVENT_BUBBLE);
    s_ui.pages[page] = box;
    return box;
}

static void build_light(lv_obj_t *page)
{
    static const struct { const lv_img_dsc_t *src; int x; int y; } rays[16] = {
        { &ui_icon_ray_0,  234,  68 },
        { &ui_icon_ray_1,  256,  74 },
        { &ui_icon_ray_2,  276,  94 },
        { &ui_icon_ray_3,  288, 126 },
        { &ui_icon_ray_4,  294, 162 },
        { &ui_icon_ray_5,  288, 184 },
        { &ui_icon_ray_6,  276, 204 },
        { &ui_icon_ray_7,  256, 216 },
        { &ui_icon_ray_8,  234, 222 },
        { &ui_icon_ray_9,  198, 216 },
        { &ui_icon_ray_10, 166, 204 },
        { &ui_icon_ray_11, 146, 184 },
        { &ui_icon_ray_12, 140, 162 },
        { &ui_icon_ray_13, 146, 126 },
        { &ui_icon_ray_14, 166,  94 },
        { &ui_icon_ray_15, 198,  74 }
    };
    for (int k = 0; k < 16; k++) {
        lv_obj_t *r = add_icon_at(page, rays[k].x, rays[k].y, rays[k].src, &ui_style_icon_ray_off);
        lv_obj_add_style(r, &ui_style_icon_gold_half, LV_STATE_USER_1);
        lv_obj_add_style(r, &ui_style_icon_gold, LV_STATE_CHECKED);
        s_ui.light_rays[k] = r;
    }
    s_ui.light_sun_btn = add_box(page, 198, 126, 84, 84, &ui_style_circle_btn);
    lv_obj_add_style(s_ui.light_sun_btn, &ui_style_btn_pressed, LV_STATE_PRESSED);
    lv_obj_add_style(s_ui.light_sun_btn, &ui_style_sun_ring_on, LV_STATE_CHECKED);
    make_page_control(s_ui.light_sun_btn);
    lv_obj_add_event_cb(s_ui.light_sun_btn, on_light_sun, LV_EVENT_CLICKED, NULL);

    s_ui.light_sun_core = add_box(page, 218, 146, 44, 44, &ui_style_sun_core);
    lv_obj_add_style(s_ui.light_sun_core, &ui_style_sun_core_on, LV_STATE_CHECKED);
}

static lv_obj_t *add_ac_stepper(lv_obj_t *page, int y, const lv_img_dsc_t *icon, lv_obj_t **icon_out)
{
    lv_obj_t *btn = add_box(page, 366, y, 96, 86, &ui_style_ac_stepper);
    lv_obj_t *img;
    lv_obj_add_style(btn, &ui_style_btn_pressed, LV_STATE_PRESSED);
    lv_obj_add_style(btn, &ui_style_btn_disabled, LV_STATE_DISABLED);
    img = add_icon(btn, icon, &ui_style_icon_ink);
    lv_obj_add_style(img, &ui_style_icon_muted, LV_STATE_DISABLED);
    make_page_control(btn);
    lv_obj_add_event_cb(btn, on_ac, LV_EVENT_CLICKED, NULL);
    *icon_out = img;
    return btn;
}

static void build_ac(lv_obj_t *page)
{
    s_ui.ac_toggle = add_box(page, HERO_TILE_X, HERO_TILE_Y, 96, 128, &ui_style_tile);
    lv_obj_add_style(s_ui.ac_toggle, &ui_style_btn_pressed, LV_STATE_PRESSED);
    lv_obj_add_style(s_ui.ac_toggle, &ui_style_ac_toggle_on, LV_STATE_CHECKED);
    make_page_control(s_ui.ac_toggle);
    lv_obj_add_event_cb(s_ui.ac_toggle, on_ac, LV_EVENT_CLICKED, NULL);
    s_ui.ac_icon = add_icon(s_ui.ac_toggle, &ui_icon_snowflake_56, &ui_style_icon_muted);
    lv_obj_add_style(s_ui.ac_icon, &ui_style_icon_cyan, LV_STATE_CHECKED);

    s_ui.ac_value = add_label(page, AC_VAL_X, HERO_VAL_Y, AC_VAL_W, "", &ui_font_digits_76, &ui_style_color_muted);
    lv_obj_add_style(s_ui.ac_value, &ui_style_color_ink, LV_STATE_CHECKED);

    s_ui.ac_plus = add_ac_stepper(page, 66, &ui_icon_plus_28, &s_ui.ac_plus_icon);
    s_ui.ac_minus = add_ac_stepper(page, 168, &ui_icon_minus_28, &s_ui.ac_minus_icon);
}

static void build_reading(lv_obj_t *page, const lv_img_dsc_t *icon, const lv_style_t *icon_on,
                          lv_obj_t **tile_out, lv_obj_t **icon_out, lv_obj_t **value_out)
{
    lv_obj_t *tile = add_box(page, HERO_TILE_X, HERO_TILE_Y, 96, 128, &ui_style_tile);
    lv_obj_t *img = add_icon(tile, icon, &ui_style_icon_muted);
    lv_obj_t *val = add_label(page, HERO_VAL_X, HERO_VAL_Y, HERO_VAL_W, "", &ui_font_digits_76, &ui_style_color_muted);
    lv_obj_add_style(img, icon_on, LV_STATE_CHECKED);
    lv_obj_add_style(val, &ui_style_color_ink, LV_STATE_CHECKED);
    lv_obj_add_style(val, &ui_style_text_48_env, LV_STATE_USER_1);
    *tile_out = tile;
    *icon_out = img;
    *value_out = val;
}

static void build_root(void)
{
    s_ui.root = add_box(lv_scr_act(), 0, 0, UI_DISPLAY_W, UI_DISPLAY_H, &ui_style_canvas);
    lv_obj_add_flag(s_ui.root, LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_PRESS_LOCK);
    lv_obj_add_event_cb(s_ui.root, touch_event, LV_EVENT_ALL, NULL);

    build_light(add_page(UI_PAGE_LIGHT));
    build_ac(add_page(UI_PAGE_AC));
    build_reading(add_page(UI_PAGE_TEMPERATURE), &ui_icon_thermometer_64, &ui_style_icon_coral,
                  &s_ui.temp_tile, &s_ui.temp_icon, &s_ui.temp_value);
    build_reading(add_page(UI_PAGE_HUMIDITY), &ui_icon_drop_64, &ui_style_icon_cyan,
                  &s_ui.humid_tile, &s_ui.humid_icon, &s_ui.humid_value);

    for (unsigned i = 1; i < UI_PAGE_COUNT; i++) set_hidden(s_ui.pages[i], true);

    /* Header buttons sit above page containers in z-order. */
    s_ui.btn_scenes = add_header_button(s_ui.root, HEADER_BTN_X_L, &ui_icon_scenes_24, on_header);
    s_ui.btn_settings = add_header_button(s_ui.root, HEADER_BTN_X_R, &ui_icon_sliders_24, on_header);
}

static lv_obj_t *add_panel(lv_obj_t **back)
{
    lv_obj_t *panel = add_box(lv_scr_act(), 0, 0, UI_DISPLAY_W, UI_DISPLAY_H, &ui_style_canvas);
    /* An open panel owns every touch until it is closed. */
    lv_obj_add_flag(panel, LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_PRESS_LOCK);
    lv_obj_add_event_cb(panel, touch_event, LV_EVENT_ALL, NULL);
    *back = add_header_button(panel, HEADER_BTN_X_L, &ui_icon_chevron_left_24, on_panel_back);
    set_hidden(panel, true);
    return panel;
}

static void build_scenes(void)
{
    static const lv_img_dsc_t *const icons[3] = { &ui_icon_home_48, &ui_icon_moon_48, &ui_icon_away_48 };
    s_ui.scenes_panel = add_panel(&s_ui.scenes_back);
    for (unsigned i = 0; i < 3; i++) {
        lv_obj_t *btn = add_box(s_ui.scenes_panel, 40 + (int)i * 144, 122, 112, 112, &ui_style_circle_btn);
        lv_obj_t *img;
        lv_obj_add_style(btn, &ui_style_btn_pressed, LV_STATE_PRESSED);
        lv_obj_add_style(btn, &ui_style_scene_selected, LV_STATE_CHECKED);
        lv_obj_add_style(btn, &ui_style_scene_selected_pressed, LV_STATE_CHECKED | LV_STATE_PRESSED);
        make_page_control(btn);
        lv_obj_add_event_cb(btn, on_scene, LV_EVENT_CLICKED, (void *)(uintptr_t)(i + 1));
        img = add_icon(btn, icons[i], &ui_style_icon_idle);
        lv_obj_add_style(img, &ui_style_icon_dark, LV_STATE_CHECKED);
        s_ui.scene_buttons[i] = btn;
        s_ui.scene_icons[i] = img;
    }
}

static lv_obj_t *add_baud_stepper(int x, const lv_img_dsc_t *icon, lv_obj_t **icon_out)
{
    lv_obj_t *btn = add_box(s_ui.conn_panel, x, 126, 60, 60, &ui_style_circle_btn);
    lv_obj_t *img;
    lv_obj_add_style(btn, &ui_style_btn_pressed, LV_STATE_PRESSED);
    lv_obj_add_style(btn, &ui_style_btn_disabled, LV_STATE_DISABLED);
    img = add_icon(btn, icon, &ui_style_icon_ink);
    lv_obj_add_style(img, &ui_style_icon_muted, LV_STATE_DISABLED);
    make_page_control(btn);
    lv_obj_add_event_cb(btn, on_baud_step, LV_EVENT_CLICKED, NULL);
    *icon_out = img;
    return btn;
}

static void build_connection(void)
{
    s_ui.conn_panel = add_panel(&s_ui.conn_back);

    s_ui.conn_rx_dot = add_box(s_ui.conn_panel, 452, 30, 8, 8, &ui_style_dot_idle);
    lv_obj_add_style(s_ui.conn_rx_dot, &ui_style_dot_cyan, LV_STATE_CHECKED);

    s_ui.baud_active_dot = add_box(s_ui.conn_panel, 236, 98, 8, 8, &ui_style_dot_cyan);

    s_ui.baud_prev = add_baud_stepper(44, &ui_icon_chevron_left_24, &s_ui.baud_prev_icon);
    s_ui.baud_next = add_baud_stepper(376, &ui_icon_chevron_right_24, &s_ui.baud_next_icon);

    s_ui.baud_value = add_label(s_ui.conn_panel, 116, 126, 248, "", &ui_font_48, &ui_style_color_ink);

    s_ui.baud_apply = add_box(s_ui.conn_panel, 182, 232, 116, 52, &ui_style_pill_cyan);
    lv_obj_add_style(s_ui.baud_apply, &ui_style_pill_cyan_pressed, LV_STATE_PRESSED);
    lv_obj_add_style(s_ui.baud_apply, &ui_style_pill_pending, LV_STATE_USER_1);
    lv_obj_add_style(s_ui.baud_apply, &ui_style_pill_failed, LV_STATE_USER_2);
    make_page_control(s_ui.baud_apply);
    lv_obj_add_event_cb(s_ui.baud_apply, on_baud_apply, LV_EVENT_CLICKED, NULL);

    s_ui.baud_apply_icon = add_icon(s_ui.baud_apply, &ui_icon_check_28, &ui_style_icon_dark);
    lv_obj_add_style(s_ui.baud_apply_icon, &ui_style_icon_gold, LV_STATE_USER_1);
    lv_obj_add_style(s_ui.baud_apply_icon, &ui_style_icon_error, LV_STATE_USER_2);
}

/* ---- Public API ------------------------------------------------------------ */

void ui_init(ui_action_cb_t cb, void *context)
{
    if (s_ui.initialized) ui_deinit();
    memset(&s_ui, 0, sizeof(s_ui));
    ui_model_init(&s_ui.model);
    s_ui.callback = cb;
    s_ui.context = context;
    s_ui.next_page = -1;
    s_ui.panel_request = -1;
    s_ui.selected_baud = s_ui.model.applied_baud;
    ui_theme_init();
    build_root();
    build_scenes();
    build_connection();
    s_ui.timer = lv_timer_create(timer_cb, UI_TIMER_PERIOD_MS, NULL);
    s_ui.initialized = true;
    render_all();
}

bool ui_apply(const studio_cmd_t *cmd)
{
    if (!s_ui.initialized || !ui_model_apply(&s_ui.model, cmd)) return false;
    if (cmd->kind == STUDIO_CMD_BAUD && s_ui.model.baud_state != UI_BAUD_PENDING) {
        s_ui.selected_baud = s_ui.model.applied_baud;
        s_ui.model.baud_state = UI_BAUD_IDLE;
    }
    render_all();
    return true;
}

bool ui_baud_complete(uint32_t requested_baud, bool success)
{
    ui_model_t *m = &s_ui.model;
    if (!s_ui.initialized || m->baud_state != UI_BAUD_PENDING || m->pending_baud != requested_baud) return false;
    if (success) m->applied_baud = requested_baud;
    m->pending_baud = 0;
    m->baud_state = success ? UI_BAUD_IDLE : UI_BAUD_FAILED;
    render_connection();
    return true;
}

void ui_note_rx(void)
{
    if (!s_ui.initialized) return;
    s_ui.rx_seen = true;
    set_state(s_ui.conn_rx_dot, LV_STATE_CHECKED, true);
}

const ui_model_t *ui_get_model(void)
{
    return &s_ui.model;
}

void ui_navigate_to_page(ui_page_id_t page)
{
    if (!s_ui.initialized) return;
    s_ui.next_page = -1;
    show_page(page);
}

ui_page_id_t ui_get_page(void)
{
    return s_ui.page;
}

void ui_deinit(void)
{
    if (!s_ui.initialized) return;
    lv_timer_del(s_ui.timer);
    lv_obj_del(s_ui.root);
    lv_obj_del(s_ui.scenes_panel);
    lv_obj_del(s_ui.conn_panel);
    memset(&s_ui, 0, sizeof(s_ui));
}

#ifdef UI_TEST_HOOKS
const ui_test_hooks_t *ui_get_test_hooks(void)
{
    static ui_test_hooks_t h;
    h.gesture = &s_ui.g;
    h.page = s_ui.page;
    h.panel = s_ui.panel;
    h.root = s_ui.root;
    h.btn_scenes = s_ui.btn_scenes;
    h.btn_settings = s_ui.btn_settings;
    for (unsigned i = 0; i < UI_PAGE_COUNT; i++) h.pages[i] = s_ui.pages[i];
    h.light_sun_btn = s_ui.light_sun_btn;
    h.light_sun_core = s_ui.light_sun_core;
    for (unsigned i = 0; i < 16; i++) h.light_rays[i] = s_ui.light_rays[i];
    h.ac_toggle = s_ui.ac_toggle;
    h.ac_value = s_ui.ac_value;
    h.ac_plus = s_ui.ac_plus;
    h.ac_minus = s_ui.ac_minus;
    h.temp_tile = s_ui.temp_tile;
    h.temp_icon = s_ui.temp_icon;
    h.temp_value = s_ui.temp_value;
    h.humid_tile = s_ui.humid_tile;
    h.humid_icon = s_ui.humid_icon;
    h.humid_value = s_ui.humid_value;
    h.scenes_panel = s_ui.scenes_panel;
    h.scenes_back = s_ui.scenes_back;
    for (unsigned i = 0; i < 3; i++) {
        h.scene_buttons[i] = s_ui.scene_buttons[i];
        h.scene_icons[i] = s_ui.scene_icons[i];
    }
    h.conn_panel = s_ui.conn_panel;
    h.conn_back = s_ui.conn_back;
    h.conn_rx_dot = s_ui.conn_rx_dot;
    h.baud_active_dot = s_ui.baud_active_dot;
    h.baud_prev = s_ui.baud_prev;
    h.baud_next = s_ui.baud_next;
    h.baud_value = s_ui.baud_value;
    h.baud_apply = s_ui.baud_apply;
    h.baud_apply_icon = s_ui.baud_apply_icon;
    return &h;
}
#endif
