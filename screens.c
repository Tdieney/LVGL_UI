#include "screens.h"
#include "actions.h"
#include "ui.h"
#include "images.h"
#include "ctrl_pos_math.h"
#include "demo_sim.h"
#include "fonts/ui_fonts.h"
#include <stdio.h>
#include <string.h>

MotorCmd_t        motorCmd;
MotorStatusFast_t motorStatusFast;
MotorStatusSlow_t motorStatusSlow;
uint8_t           motorConnected;

// Demo simulator runtime on/off. Defined here (screens.c is ALWAYS compiled) +
// extern in screens.h so main.c can drive it directly (set to 1 to fabricate
// telemetry, 0 to let the real UART parser own the wire structs). demo_sim.c
// (compiled only under UI_DEMO_SIM) reads/writes this instead of a file-static
// flag; demo_sim_toggle()/demo_sim_enabled() operate on it too. Boots 0 (OFF).
uint8_t           demo_sim_active;

const MotorStatusFast_t *ui_motor_status_fast(void)
{
#if defined(UI_DEMO_SIM) && UI_DEMO_SIM
    if (demo_sim_active) return demo_sim_status_fast();
#endif
    return &motorStatusFast;
}

const MotorStatusSlow_t *ui_motor_status_slow(void)
{
#if defined(UI_DEMO_SIM) && UI_DEMO_SIM
    if (demo_sim_active) return demo_sim_status_slow();
#endif
    return &motorStatusSlow;
}

uint8_t ui_motor_connected(void)
{
#if defined(UI_DEMO_SIM) && UI_DEMO_SIM
    if (demo_sim_active) return demo_sim_connected();
#endif
    return motorConnected;
}

// Semantic color system — monochrome-first with ONE vivid accent:
//   ink (COLOR_TEXT_H)  = data readouts AND interactive selection states
//   azure (COLOR_GAUGE) = the gauge hero element + RPM chart series ONLY
//   green (COLOR_OK)    = healthy/running (START green, STOP red convention)
//   amber / red         = warning / fault
// COLOR_GAUGE/OK are Apple's systemBlue / systemGreen (high-contrast tint for
// OK since it is used as small text, not just fills — see screens.h note).
// Semantic colors, saturated ~10-20% for the cheap-panel pass (see screens.h).
// Azure stays the bold hero (gauge + RPM chart only); green a touch more
// saturated but still dark enough for text; brand red untouched.
#define COLOR_GAUGE lv_color_hex(0x007AFF)     // Apple systemBlue — kept ONLY for the POSITION knob dot now
#define COLOR_GAUGE_GREEN lv_color_hex(0x14B85A) // gauge "normal" band + speed hero (user: green suits a gauge)
#define COLOR_OK lv_color_hex(0x188C34)        // green, +~12% saturation, keeps AA for bold text on white
#define COLOR_BRAND_RED lv_color_hex(0xC2272E) // Hyphen Deux logo mark red — brand color, not themed
#define COLOR_ACCENT_BG lv_color_hex(0xD5DBE6) // selected-row tint — a CLEAR cool gray (was near-white
                                                 // 0xE5E5EA) so a selection reads on white/cheap panels;
                                                 // still backed by a strong accent mark (border/radio/underline)
#define COLOR_ACCENT_DIM COLOR_TEXT_M           // dimmed text on selected items
#define COLOR_TEXT_DIM lv_color_hex(0x83878F)   // dimmest text tier (quiet captions) — darkened again for
                                                 // the cheap-panel pass so even this stays clearly legible
#define COLOR_PANEL_BG lv_color_hex(0xE7EAEF)   // light-gray chip ON the white top bar (only the link pill
                                                 // uses this now) — reads as an inset field against white.
                                                 // Controls on the gray PAGE use white COLOR_CARD_BG instead.


// --- Redraw-guard helpers ---------------------------------------------------
// Dynamic numeric labels use a small screen-local fixed-buffer pool. LVGL's
// normal lv_label_set_text() frees and allocates a new exact-sized string every
// time the number changes; Monitor used to do that for up to eight labels at
// 5 Hz, which fragmented the constrained heap during a demo. Only one screen exists
// at a time, so eight reusable buffers cover the worst case without per-tick
// allocation. Static captions point straight at const strings in Flash.
#define UI_TEXT_SLOTS 8u
#define UI_TEXT_CAP   24u
static char      s_screen_text[UI_TEXT_SLOTS][UI_TEXT_CAP];
static lv_obj_t *s_screen_text_owner[UI_TEXT_SLOTS];
static uint8_t   s_screen_text_used;

static void text_copy(char *dst, size_t cap, const char *src)
{
    size_t i = 0;
    if (cap == 0) return;
    while (src[i] != '\0' && i + 1 < cap) { dst[i] = src[i]; i++; }
    dst[i] = '\0';
}

static void screen_text_reset(void)
{
    s_screen_text_used = 0;
    memset(s_screen_text_owner, 0, sizeof(s_screen_text_owner));
}

static void label_bind_buffer(lv_obj_t *lbl, const char *initial)
{
    if (!lbl) return;
    if (s_screen_text_used >= UI_TEXT_SLOTS)
    {
        LV_LOG_WARN("screen text pool exhausted");
        lv_label_set_text(lbl, initial);
        return;
    }
    uint8_t slot = s_screen_text_used++;
    text_copy(s_screen_text[slot], UI_TEXT_CAP, initial);
    s_screen_text_owner[slot] = lbl;
    lv_label_set_text_static(lbl, s_screen_text[slot]);
}

static void label_set_if_changed(lv_obj_t *lbl, const char *text)
{
    if (!lbl || strcmp(lv_label_get_text(lbl), text) == 0) return;
    for (uint8_t i = 0; i < s_screen_text_used; i++)
    {
        if (s_screen_text_owner[i] != lbl) continue;
        text_copy(s_screen_text[i], UI_TEXT_CAP, text);
        lv_label_set_text_static(lbl, s_screen_text[i]);
        return;
    }
    // Defensive fallback for a future dynamic label that was not bound.
    lv_label_set_text_static(lbl, text);
}

static void label_set_static_if_changed(lv_obj_t *lbl, const char *text)
{
    if (lbl && strcmp(lv_label_get_text(lbl), text) != 0) lv_label_set_text_static(lbl, text);
}

static void label_color_if_changed(lv_obj_t *lbl, lv_color_t color)
{
    if (lbl == NULL) return;
    if (lv_color_to32(lv_obj_get_style_text_color(lbl, LV_PART_MAIN)) != lv_color_to32(color))
        lv_obj_set_style_text_color(lbl, color, LV_PART_MAIN);
}

// Fixed-point formatter: inserts the decimal point from integer raw units.
// No float operations and no float printf on the MCU.
static const char *fmt_scaled(char *buf, size_t n, int32_t raw, int32_t scale, uint8_t dec)
{
    static const uint16_t P[] = {1, 10, 100, 1000};
    if (dec > 3) dec = 3;
    if (scale <= 0) scale = 1;
    bool neg = raw < 0;
    uint32_t mag = neg ? (uint32_t) (-(raw + 1)) + 1u : (uint32_t) raw;
    uint32_t ip = mag / (uint32_t) scale;
    uint32_t rem = mag % (uint32_t) scale;
    uint32_t fp = (rem * P[dec] + (uint32_t) scale / 2u) / (uint32_t) scale;
    if (fp >= P[dec]) { ip++; fp = 0; }
    if (dec == 0)
        snprintf(buf, n, "%s%lu", neg ? "-" : "", (unsigned long) ip);
    else
        snprintf(buf, n, "%s%lu.%0*lu", neg ? "-" : "", (unsigned long) ip,
                 (int) dec, (unsigned long) fp);
    return buf;
}

// Thousands-grouped RPM ("1,927") — max 5 digits on this dial
static const char *fmt_rpm(char *buf, size_t n, int32_t v)
{
    if (v >= 1000)
        snprintf(buf, n, "%ld,%03ld", (long) (v / 1000), (long) (v % 1000));
    else
        snprintf(buf, n, "%ld", (long) v);
    return buf;
}


// Threshold coloring for temperatures: normal ink / warn / danger
static lv_color_t temp_color(int32_t deci_c, int32_t warn_deci_c, int32_t danger_deci_c)
{
    return deci_c > danger_deci_c ? COLOR_DANGER
                                  : (deci_c > warn_deci_c ? COLOR_WARN : COLOR_TEXT_H);
}

// --- Shared styles (hardening 'a') ------------------------------------------
// Screens are torn down + rebuilt on every tab switch (single-screen SPA to fit
// the 52 KB heap). Doing per-object `lv_obj_set_style_*` for the widgets created
// dozens of times per screen (sliders, buttons, list-row dividers) allocates a
// growable LOCAL style array on EACH object EACH rebuild — the alloc/free churn
// that fragments the heap under fast tab-flipping. These SHARED styles are
// initialised once and `lv_obj_add_style`'d instead: one small style-list entry
// per object instead of a local property array, and zero re-alloc across
// rebuilds (the styles are process-lifetime statics). Per-instance colors set
// by callers/ticks still win (local > added style). Only STATIC properties live
// here; anything a tick recolors (button bg/border/text) stays local.
static lv_style_t st_btn_geom, st_listrow, st_sld_main, st_sld_ind, st_sld_knob;
static lv_style_t st_mon_icon, st_mon_lbl, st_mon_val; // Monitor row: icon / name / value (static look)
static bool       st_inited = false;
static void styles_ensure(void)
{
    if (st_inited) return;
    st_inited = true;

    lv_style_init(&st_btn_geom); // action/dir button geometry (colors are per-state, kept local)
    lv_style_set_radius(&st_btn_geom, 10);
    lv_style_set_shadow_width(&st_btn_geom, 0);
    lv_style_set_border_width(&st_btn_geom, 1);

    lv_style_init(&st_listrow); // Monitor / Settings list-row bottom divider
    lv_style_set_border_side(&st_listrow, LV_BORDER_SIDE_BOTTOM);
    lv_style_set_border_color(&st_listrow, COLOR_BORDER);
    lv_style_set_border_width(&st_listrow, 1);

    lv_style_init(&st_sld_main); // slider track / fill / knob — fully static across the app
    lv_style_set_bg_color(&st_sld_main, COLOR_BORDER);
    lv_style_init(&st_sld_ind);
    lv_style_set_bg_color(&st_sld_ind, COLOR_ACCENT);
    lv_style_init(&st_sld_knob);
    lv_style_set_bg_color(&st_sld_knob, lv_color_white());
    lv_style_set_pad_all(&st_sld_knob, 8);
    lv_style_set_border_color(&st_sld_knob, COLOR_ACCENT);
    lv_style_set_border_width(&st_sld_knob, 3);

    // Monitor row children — every mon_row() creates icon+name+value; these carry
    // the STATIC look so each object holds one style-list entry instead of a
    // local property array (less alloc/frag per rebuild). Per-row value color set
    // by the tick (label_color_if_changed) stays local and still wins.
    lv_style_init(&st_mon_icon);
    lv_style_set_img_recolor(&st_mon_icon, COLOR_TEXT_VL);
    lv_style_set_img_recolor_opa(&st_mon_icon, LV_OPA_COVER);

    lv_style_init(&st_mon_lbl);
    lv_style_set_text_font(&st_mon_lbl, &ui_font_mono20);
    lv_style_set_text_color(&st_mon_lbl, COLOR_TEXT_L);
    lv_style_set_flex_grow(&st_mon_lbl, 1);

    lv_style_init(&st_mon_val);
    lv_style_set_text_font(&st_mon_val, &ui_font_mono22);
    lv_style_set_text_color(&st_mon_val, COLOR_TEXT_H);
    lv_style_set_text_align(&st_mon_val, LV_TEXT_ALIGN_RIGHT);
    lv_style_set_width(&st_mon_val, 130);
    lv_style_set_height(&st_mon_val, 30);
}

// Apply the shared slider look (track + accent fill + white/accent knob). The
// caller still sets width/height + range/value + the value-changed callback.
static void style_slider(lv_obj_t *s)
{
    styles_ensure();
    lv_obj_add_style(s, &st_sld_main, LV_PART_MAIN);
    lv_obj_add_style(s, &st_sld_ind, LV_PART_INDICATOR);
    lv_obj_add_style(s, &st_sld_knob, LV_PART_KNOB);
}

// --- Layout helpers ---------------------------------------------------------

// Invisible layout container: no bg, border, padding, scrolling or clicks.
static lv_obj_t *flat_cont(lv_obj_t *parent)
{
    lv_obj_t *obj = lv_obj_create(parent);
    lv_obj_remove_style_all(obj);
    lv_obj_clear_flag(obj, LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE);
    return obj;
}

static lv_obj_t *flex_row(lv_obj_t *parent)
{
    lv_obj_t *row = flat_cont(parent);
    lv_obj_set_layout(row, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(row, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    return row;
}

// Section header: small tracked label + hairline running to the right edge
static lv_obj_t *create_section_label(lv_obj_t *parent, const char *text)
{
    lv_obj_t *row = flex_row(parent);
    lv_obj_set_width(row, LV_PCT(100));
    lv_obj_set_height(row, LV_SIZE_CONTENT);
    lv_obj_set_style_pad_column(row, 8, LV_PART_MAIN);

    lv_obj_t *lbl = lv_label_create(row);
    lv_label_set_text_static(lbl, text);
    lv_obj_set_style_text_font(lbl, &ui_font_mono20, LV_PART_MAIN);
    lv_obj_set_style_text_color(lbl, COLOR_TEXT_L, LV_PART_MAIN);
    lv_obj_set_style_text_letter_space(lbl, 2, LV_PART_MAIN);

    lv_obj_t *line = flat_cont(row);
    lv_obj_set_height(line, 1);
    lv_obj_set_flex_grow(line, 1);
    lv_obj_set_style_bg_color(line, COLOR_BORDER, LV_PART_MAIN);
    lv_obj_set_style_bg_opa(line, LV_OPA_COVER, LV_PART_MAIN);
    return row;
}

// Metric card: small tracked title + mono value + unit. Returns the card;
// the dynamic value label comes back through out_val.
static lv_obj_t *create_mcard(lv_obj_t *parent, const char *label_text, const char *unit_text, lv_color_t val_color,
                              lv_obj_t **out_val)
{
    lv_obj_t *card = lv_obj_create(parent);
    lv_obj_set_size(card, LV_PCT(100), LV_SIZE_CONTENT);
    lv_obj_set_style_bg_color(card, COLOR_CARD_BG, LV_PART_MAIN);
    lv_obj_set_style_border_color(card, COLOR_BORDER, LV_PART_MAIN);
    lv_obj_set_style_border_width(card, 1, LV_PART_MAIN);
    lv_obj_set_style_radius(card, 10, LV_PART_MAIN);
    lv_obj_set_style_pad_hor(card, 14, LV_PART_MAIN);
    lv_obj_set_style_pad_ver(card, 12, LV_PART_MAIN);
    lv_obj_set_style_pad_row(card, 4, LV_PART_MAIN);
    lv_obj_clear_flag(card, LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_layout(card, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(card, LV_FLEX_FLOW_COLUMN);

    lv_obj_t *lbl = lv_label_create(card);
    lv_label_set_text_static(lbl, label_text);
    lv_obj_set_style_text_font(lbl, &ui_font_mono20, LV_PART_MAIN);
    lv_obj_set_style_text_color(lbl, COLOR_TEXT_L, LV_PART_MAIN);
    lv_obj_set_style_text_letter_space(lbl, 1, LV_PART_MAIN);

    // Value + unit on one line (value 30px ink, unit 20px dim just after it —
    // matches the mockup's inline card value). value grows to text width; a
    // text change re-lays out only this 2-item row, negligible for the 2
    // Dashboard cards on the 5Hz lane.
    lv_obj_t *valrow = flex_row(card);
    lv_obj_set_size(valrow, LV_PCT(100), LV_SIZE_CONTENT);
    lv_obj_set_style_pad_column(valrow, 6, LV_PART_MAIN);
    lv_obj_set_flex_align(valrow, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_END, LV_FLEX_ALIGN_END);

    lv_obj_t *val = lv_label_create(valrow);
    label_bind_buffer(val, "0");
    lv_obj_set_style_text_font(val, &ui_font_mono30, LV_PART_MAIN);
    lv_obj_set_style_text_color(val, val_color, LV_PART_MAIN);

    if (unit_text && unit_text[0] != '\0')
    {
        lv_obj_t *unit = lv_label_create(valrow);
        lv_label_set_text_static(unit, unit_text);
        lv_obj_set_style_text_font(unit, &ui_font_mono20, LV_PART_MAIN);
        lv_obj_set_style_text_color(unit, COLOR_TEXT_VL, LV_PART_MAIN);
        lv_obj_set_style_pad_bottom(unit, 3, LV_PART_MAIN); // sit on the value's baseline
    }

    if (out_val) *out_val = val;
    return card;
}


// --- Common UI: top bar + tab bar (created once on the top layer) -----------

static lv_obj_t *top_lbl_state;
static lv_obj_t *top_link_dot;
static char      top_pill_text[24];
static lv_obj_t *top_pill_lbl; // "RS-485 | <baud>" — reflects ui_rs485_baud

static lv_obj_t *tab_btns[6];
static lv_obj_t *tab_icons[6];
static lv_obj_t *tab_lbls[6];

static void create_tab_btn(int idx, lv_obj_t *parent, const lv_img_dsc_t *icon, const char *text,
                           lv_event_cb_t event_cb)
{
    lv_obj_t *btn = lv_btn_create(parent);
    lv_obj_set_flex_grow(btn, 1);
    lv_obj_set_height(btn, LV_PCT(100));
    lv_obj_set_style_bg_color(btn, COLOR_TOP_BG, LV_PART_MAIN);
    lv_obj_set_style_bg_opa(btn, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_radius(btn, 0, LV_PART_MAIN);
    lv_obj_set_style_shadow_width(btn, 0, LV_PART_MAIN);
    lv_obj_set_style_border_side(btn, LV_BORDER_SIDE_BOTTOM, LV_PART_MAIN);
    lv_obj_set_style_border_width(btn, 2, LV_PART_MAIN);
    lv_obj_set_style_border_color(btn, COLOR_ACCENT, LV_PART_MAIN);
    lv_obj_set_style_border_opa(btn, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_add_event_cb(btn, event_cb, LV_EVENT_CLICKED, NULL);

    lv_obj_t *row = flex_row(btn);
    lv_obj_set_size(row, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
    lv_obj_center(row);
    lv_obj_set_style_pad_column(row, 8, LV_PART_MAIN);

    // Custom alpha-mask icon (mockup SVG shapes), recolored per state. An
    // ALPHA_4BIT image draws in the img_recolor color at the mask's coverage,
    // so recolor + full recolor_opa tints it exactly like the old font glyph.
    lv_obj_t *ic = lv_img_create(row);
    lv_img_set_src(ic, icon);
    lv_obj_set_style_img_recolor(ic, COLOR_TEXT_L, LV_PART_MAIN);
    lv_obj_set_style_img_recolor_opa(ic, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_clear_flag(ic, LV_OBJ_FLAG_CLICKABLE);

    lv_obj_t *lbl = lv_label_create(row);
    lv_label_set_text_static(lbl, text);
    lv_obj_set_style_text_font(lbl, &ui_font_mono20, LV_PART_MAIN);
    lv_obj_set_style_text_color(lbl, COLOR_TEXT_L, LV_PART_MAIN);
    lv_obj_set_style_text_letter_space(lbl, 1, LV_PART_MAIN);

    tab_btns[idx]  = btn;
    tab_icons[idx] = ic;
    tab_lbls[idx]  = lbl;
}

// When the motor last SETTLED into STOPPED (0 = not currently stopped). Updated
// by tick_common_ui every SLOW tick; read by the demo→normal switch gate below.
// Declared outside the UI_DEMO_SIM guard because tick_common_ui (always compiled)
// maintains it.
static uint32_t s_motor_stopped_since;

#if defined(UI_DEMO_SIM) && UI_DEMO_SIM
// Hold the "MOTOR CONTROL HMI" brand ~800ms → flip the runtime demo simulator
// on/off (demo-build only; real firmware has no simulator to toggle).
//
// WHY A HOLD, NOT A TAP: this switches which private telemetry source is shown.
// The real UART path can continue receiving into its own structs without racing
// the Demo timer. A stray single tap on the brand — easy to hit while pointing at
// the screen during a live demo — used to flip modes instantly; the handoff
// mid-demo left the panel in a broken-looking state (operator report: whole UI
// appeared corrupt, needed an MCU reset). Requiring a deliberate ~800ms hold
// means an accidental brush can no longer trigger the switch. A debounce blocks
// an immediate re-flip and PRESS_LOST cancels if the finger slides off. The next
// scheduled 5 Hz lane repaints the selected source; no heavy redraw runs from
// inside the input event callback.
#define DEMO_HOLD_MS     800u  // deliberate hold required to flip demo mode
#define DEMO_DEBOUNCE_MS 1200u // ignore a second flip landing right after one

static uint32_t s_demo_press_tick;
static uint32_t s_demo_last_toggle;
static uint8_t  s_demo_pressing;

static void topbar_demo_toggle_cb(lv_event_t *e)
{
    switch (lv_event_get_code(e))
    {
        case LV_EVENT_PRESSED:
            s_demo_press_tick = lv_tick_get();
            s_demo_pressing   = 1;
            break;
        case LV_EVENT_RELEASED:
        {
            if (!s_demo_pressing) break;
            s_demo_pressing  = 0;
            uint32_t now     = lv_tick_get();
            if (now - s_demo_press_tick < DEMO_HOLD_MS) break; // released too soon — not a deliberate hold
            if (s_demo_last_toggle != 0 && now - s_demo_last_toggle < DEMO_DEBOUNCE_MS) break; // debounce
            // Switching demo -> normal hands the wire structs back to the real UART
            // path — only allow it once the operator has STOPped the motor and it
            // has settled for >=1s (user request: no live handoff while spinning).
            if (demo_sim_active)
            {
                if (ui_motor_status_fast()->bits.motorState != MOTOR_STATE_STOPPED) break;
                if (s_motor_stopped_since == 0 || now - s_motor_stopped_since < 1000u) break; // not settled 1s yet
            }
            s_demo_last_toggle = now;
            demo_sim_toggle();
            break;
        }
        case LV_EVENT_PRESS_LOST:
            s_demo_pressing = 0; // finger slid off the brand — cancel, no flip
            break;
        default:
            break;
    }
}
#endif

void create_common_ui(lv_obj_t *parent)
{
    // --- Top bar ---
    lv_obj_t *top_bar = lv_obj_create(parent);
    lv_obj_set_size(top_bar, 800, 60);
    lv_obj_set_pos(top_bar, 0, 0);
    lv_obj_set_style_bg_color(top_bar, COLOR_TOP_BG, LV_PART_MAIN);
    lv_obj_set_style_border_side(top_bar, LV_BORDER_SIDE_BOTTOM, LV_PART_MAIN);
    lv_obj_set_style_border_color(top_bar, COLOR_BORDER, LV_PART_MAIN);
    lv_obj_set_style_border_width(top_bar, 1, LV_PART_MAIN);
    lv_obj_set_style_radius(top_bar, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_all(top_bar, 0, LV_PART_MAIN);
    lv_obj_clear_flag(top_bar, LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *left = flex_row(top_bar);
    lv_obj_set_size(left, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
    lv_obj_align(left, LV_ALIGN_LEFT_MID, 16, 0);
    lv_obj_set_style_pad_column(left, 12, LV_PART_MAIN);
#if defined(UI_DEMO_SIM) && UI_DEMO_SIM
    // Hidden demo gesture: HOLD the brand ~800ms to toggle the demo simulator
    // on/off (see topbar_demo_toggle_cb — a deliberate hold, not a tap, so an
    // accidental brush can't flip modes mid-demo). No ext_click_area: the target
    // shouldn't be any easier to hit than the brand itself.
    lv_obj_add_flag(left, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(left, topbar_demo_toggle_cb, LV_EVENT_PRESSED, NULL);
    lv_obj_add_event_cb(left, topbar_demo_toggle_cb, LV_EVENT_RELEASED, NULL);
    lv_obj_add_event_cb(left, topbar_demo_toggle_cb, LV_EVENT_PRESS_LOST, NULL);
#endif

    // Logo mark: two tiny bars in the Hyphen Deux brand colors (ink + red)
    lv_obj_t *logo = flat_cont(left);
    lv_obj_set_size(logo, 6, 30);
    lv_obj_t *bar1 = flat_cont(logo);
    lv_obj_set_size(bar1, 6, 13);
    lv_obj_set_pos(bar1, 0, 0);
    lv_obj_set_style_bg_color(bar1, COLOR_TEXT_H, LV_PART_MAIN);
    lv_obj_set_style_bg_opa(bar1, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_radius(bar1, 2, LV_PART_MAIN);
    lv_obj_t *bar2 = flat_cont(logo);
    lv_obj_set_size(bar2, 6, 13);
    lv_obj_set_pos(bar2, 0, 17);
    lv_obj_set_style_bg_color(bar2, COLOR_BRAND_RED, LV_PART_MAIN);
    lv_obj_set_style_bg_opa(bar2, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_radius(bar2, 2, LV_PART_MAIN);

    lv_obj_t *title = lv_label_create(left);
    lv_label_set_text_static(title, "MOTOR CONTROL HMI");
    lv_obj_set_style_text_font(title, &ui_font_mono20, LV_PART_MAIN);
    lv_obj_set_style_text_color(title, COLOR_TEXT_L, LV_PART_MAIN);
    lv_obj_set_style_text_letter_space(title, 2, LV_PART_MAIN);
    // The page-name-in-topbar + divider that used to sit right of the title
    // were dropped: at the new 20px+ type scale there wasn't room for both
    // that AND a readable MOTOR state + link pill on the right, and the
    // active tab is already highlighted one bar down — showing its name
    // twice was the more dispensable of the two redundant reads.

    lv_obj_t *right = flex_row(top_bar);
    lv_obj_set_size(right, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
    lv_obj_align(right, LV_ALIGN_RIGHT_MID, -16, 0);
    lv_obj_set_style_pad_column(right, 12, LV_PART_MAIN);

    lv_obj_t *state_tag = lv_label_create(right);
    lv_label_set_text_static(state_tag, "MOTOR");
    lv_obj_set_style_text_font(state_tag, &ui_font_mono20, LV_PART_MAIN);
    lv_obj_set_style_text_color(state_tag, COLOR_TEXT_DIM, LV_PART_MAIN);
    lv_obj_set_style_text_letter_space(state_tag, 1, LV_PART_MAIN);

    top_lbl_state = lv_label_create(right);
    lv_label_set_text_static(top_lbl_state, "STOPPED");
    lv_obj_set_style_text_font(top_lbl_state, &ui_font_mono22, LV_PART_MAIN);
    lv_obj_set_style_text_color(top_lbl_state, COLOR_TEXT_L, LV_PART_MAIN);

    lv_obj_t *pill = flex_row(right);
    lv_obj_set_size(pill, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
    lv_obj_set_style_bg_color(pill, COLOR_PANEL_BG, LV_PART_MAIN);
    lv_obj_set_style_bg_opa(pill, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_border_color(pill, COLOR_BORDER, LV_PART_MAIN);
    lv_obj_set_style_border_width(pill, 1, LV_PART_MAIN);
    lv_obj_set_style_radius(pill, LV_RADIUS_CIRCLE, LV_PART_MAIN);
    lv_obj_set_style_pad_hor(pill, 10, LV_PART_MAIN);
    lv_obj_set_style_pad_ver(pill, 5, LV_PART_MAIN);
    lv_obj_set_style_pad_column(pill, 7, LV_PART_MAIN);

    top_link_dot = flat_cont(pill);
    lv_obj_set_size(top_link_dot, 8, 8);
    lv_obj_set_style_radius(top_link_dot, LV_RADIUS_CIRCLE, LV_PART_MAIN);
    lv_obj_set_style_bg_color(top_link_dot, COLOR_OK, LV_PART_MAIN);
    lv_obj_set_style_bg_opa(top_link_dot, LV_OPA_COVER, LV_PART_MAIN);

    top_pill_lbl = lv_label_create(pill);
    snprintf(top_pill_text, sizeof(top_pill_text), "RS-485 | %lu", (unsigned long) ui_rs485_baud);
    lv_label_set_text_static(top_pill_lbl, top_pill_text);
    lv_obj_set_style_text_font(top_pill_lbl, &ui_font_mono20, LV_PART_MAIN);
    lv_obj_set_style_text_color(top_pill_lbl, COLOR_TEXT_L, LV_PART_MAIN);

    // --- Tab bar ---
    lv_obj_t *tab_bar = lv_obj_create(parent);
    lv_obj_set_size(tab_bar, 800, 56);
    lv_obj_set_pos(tab_bar, 0, 60);
    lv_obj_set_style_bg_color(tab_bar, COLOR_TOP_BG, LV_PART_MAIN);
    lv_obj_set_style_border_side(tab_bar, LV_BORDER_SIDE_BOTTOM, LV_PART_MAIN);
    lv_obj_set_style_border_color(tab_bar, COLOR_BORDER, LV_PART_MAIN);
    lv_obj_set_style_border_width(tab_bar, 1, LV_PART_MAIN);
    lv_obj_set_style_radius(tab_bar, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_all(tab_bar, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_column(tab_bar, 0, LV_PART_MAIN);
    lv_obj_set_flex_flow(tab_bar, LV_FLEX_FLOW_ROW);
    lv_obj_clear_flag(tab_bar, LV_OBJ_FLAG_SCROLLABLE);

    create_tab_btn(0, tab_bar, &ui_icon_dash, "DASH", action_tab_dashboard);
    create_tab_btn(1, tab_bar, &ui_icon_mon, "MON", action_tab_monitor);
    create_tab_btn(2, tab_bar, &ui_icon_ctrl, "CTRL", action_tab_control);
    create_tab_btn(3, tab_bar, &ui_icon_graph, "GRAPH", action_tab_graphs);
    create_tab_btn(4, tab_bar, &ui_icon_diag, "DIAG", action_tab_diagnostics);
    create_tab_btn(5, tab_bar, &ui_icon_set, "SET", action_tab_settings);

    ui_tabbar_set_active(TAB_DASHBOARD);
}

void ui_tabbar_set_active(ui_tab_t tab)
{
    for (int i = 0; i < 6; i++)
    {
        bool       active = ((ui_tab_t) i == tab);
        lv_color_t c      = active ? COLOR_ACCENT : COLOR_TEXT_L;
        lv_obj_set_style_img_recolor(tab_icons[i], c, LV_PART_MAIN); // icon is an lv_img now
        lv_obj_set_style_text_color(tab_lbls[i], c, LV_PART_MAIN);
        lv_obj_set_style_border_opa(tab_btns[i], active ? LV_OPA_COVER : LV_OPA_TRANSP, LV_PART_MAIN);
        lv_obj_set_style_bg_color(tab_btns[i], active ? COLOR_ACCENT_BG : COLOR_TOP_BG, LV_PART_MAIN);
    }
}

void tick_common_ui(void)
{
    static const char * const STATE_NAMES[5] = {"STOPPED", "STARTING", "RUNNING", "STOPPING", "FAULT"};
    static uint8_t     last_state     = 0xFF;
    static uint8_t     last_conn      = 0xFF;

    uint8_t motorState = (uint8_t) ui_motor_status_fast()->bits.motorState;
    // Track how long the motor has been settled in STOPPED — the demo→normal
    // switch gate (topbar_demo_toggle_cb) needs a >=1s settled stop.
    if (motorState == MOTOR_STATE_STOPPED)
    {
        if (s_motor_stopped_since == 0) s_motor_stopped_since = lv_tick_get();
    }
    else
    {
        s_motor_stopped_since = 0;
    }
    if (top_lbl_state && motorState != last_state)
    {
        last_state = motorState;
        lv_label_set_text_static(top_lbl_state, STATE_NAMES[motorState <= 4 ? motorState : 4]);
        lv_color_t c = COLOR_TEXT_L;
        if (motorState == 2) c = COLOR_OK;
        else if (motorState == 4) c = COLOR_DANGER;
        else if (motorState == 1 || motorState == 3) c = COLOR_WARN;
        lv_obj_set_style_text_color(top_lbl_state, c, LV_PART_MAIN);
    }
    uint8_t connected = ui_motor_connected();
    if (top_link_dot && connected != last_conn)
    {
        last_conn = connected;
        lv_obj_set_style_bg_color(top_link_dot, connected ? COLOR_OK : COLOR_DANGER, LV_PART_MAIN);
    }
    // Reflect the RS-485 baud picked in Settings (or seeded by main.c).
    static uint32_t last_baud = 0;
    if (top_pill_lbl && ui_rs485_baud != last_baud)
    {
        last_baud = ui_rs485_baud;
        snprintf(top_pill_text, sizeof(top_pill_text), "RS-485 | %lu", (unsigned long) ui_rs485_baud);
        lv_label_set_text_static(top_pill_lbl, top_pill_text);
    }
}

// --- Boot splash -------------------------------------------------------------

static void splash_fade_anim(void *obj, int32_t opa)
{
    lv_obj_set_style_opa((lv_obj_t *) obj, (lv_opa_t) opa, LV_PART_MAIN);
}

// Logo entrance: fades in while sliding up ~14px (blank white beat first,
// then a light fade + slide — nothing snaps in).
#define SPLASH_LOGO_SLIDE_PX 14

static void splash_logo_anim(void *obj, int32_t v)
{
    lv_obj_t *logo = (lv_obj_t *) obj;
    lv_obj_set_style_opa(logo, (lv_opa_t) v, LV_PART_MAIN);
    int32_t offset = SPLASH_LOGO_SLIDE_PX - (SPLASH_LOGO_SLIDE_PX * v) / LV_OPA_COVER;
    lv_obj_align(logo, LV_ALIGN_CENTER, 0, offset);
}

static void splash_done(lv_anim_t *a)
{
    lv_obj_del((lv_obj_t *) a->var);
}

void create_splash(void)
{
    lv_obj_t *splash = lv_obj_create(lv_layer_top());
    lv_obj_set_size(splash, 800, 480);
    lv_obj_set_pos(splash, 0, 0);
    // Same tone as the top/tab bars (COLOR_BG) — a full-screen pure white
    // splash reads as glaring, and matching the chrome makes the transition
    // into the running UI feel like one continuous surface, not two.
    lv_obj_set_style_bg_color(splash, COLOR_BG, LV_PART_MAIN);
    lv_obj_set_style_border_width(splash, 0, LV_PART_MAIN);
    lv_obj_set_style_radius(splash, 0, LV_PART_MAIN);
    lv_obj_clear_flag(splash, LV_OBJ_FLAG_SCROLLABLE);

    // Blank white beat before the logo appears (matches user request: bg
    // shows first, logo animates in after).
    lv_obj_t *logo = lv_img_create(splash);
    lv_img_set_src(logo, &ui_img_logo);
    lv_obj_clear_flag(logo, LV_OBJ_FLAG_CLICKABLE);
    splash_logo_anim(logo, 0); // start fully transparent, offset down

    lv_anim_t a;
    lv_anim_init(&a);
    lv_anim_set_var(&a, logo);
    lv_anim_set_exec_cb(&a, splash_logo_anim);
    lv_anim_set_values(&a, 0, LV_OPA_COVER);
    lv_anim_set_delay(&a, 250);
    lv_anim_set_time(&a, 350); // snappier: fully vivid (black + brand red) by ~600ms, not a lingering pale fade
    lv_anim_set_path_cb(&a, lv_anim_path_ease_out);
    lv_anim_start(&a);

    lv_anim_init(&a);
    lv_anim_set_var(&a, splash);
    lv_anim_set_exec_cb(&a, splash_fade_anim);
    lv_anim_set_values(&a, LV_OPA_COVER, LV_OPA_TRANSP);
    lv_anim_set_delay(&a, 2100);
    lv_anim_set_time(&a, 400);
    lv_anim_set_ready_cb(&a, splash_done);
    lv_anim_start(&a);
}

// --- Screen background -------------------------------------------------------

void init_screen_bg(lv_obj_t *screen)
{
    // The previous screen has already been cleaned. Reuse its fixed numeric
    // text buffers for this screen instead of carrying per-screen RAM.
    screen_text_reset();
    lv_obj_set_style_bg_color(screen, COLOR_BG, LV_PART_MAIN);
    // Add top padding so content is not hidden by top bar (60) + tab bar (56)
    lv_obj_set_style_pad_top(screen, 116, LV_PART_MAIN);
}

// =============================================================================
// DASHBOARD
// =============================================================================

// Clean ring gauge (no baked scale image — the finalized mockup's gauge is a
// plain track ring + value arc + ink needle + azure hub, no tick marks or
// 0..400 scale numbers). Geometry, in the gauge_wrap's local coords:
#define GAUGE_SZ 260                 // arc widget = wrap size
#define GAUGE_C (GAUGE_SZ / 2)       // center (130,130)
#define GAUGE_ARC_W 18               // track/value stroke
#define GAUGE_NEEDLE_LEN 104         // needle reaches ~86% of the track radius

static lv_obj_t *dash_lbl_rpm; // big speed number (Style E2 tile — no arc/needle/hub)
static lv_obj_t *dash_e2_chip; // in-tile state chip (bg = state color, white text)
static lv_obj_t *dash_e2_dir;  // in-tile direction label (FWD/REV, read-only)
static lv_obj_t *dash_segs[12]; // E2 speed level bar (12 segments)
static lv_obj_t *dash_val_power;
static lv_obj_t *dash_val_temp;
static lv_obj_t *dash_btn_start;
static lv_obj_t *dash_lbl_start;
static lv_obj_t *dash_btn_stop;
static lv_obj_t *dash_lbl_stop;
static lv_obj_t *dash_btn_fwd;
static lv_obj_t *dash_lbl_fwd;
static lv_obj_t *dash_btn_rev;
static lv_obj_t *dash_lbl_rev;
static lv_obj_t *dash_lbl_setpoint;
static lv_obj_t *dash_slider;

// Style caches so the slow tick only restyles on state transitions
static uint8_t dash_last_state;
static uint8_t dash_last_dir;
static uint8_t dash_last_running; // START/STOP emphasis flip cache (like Control)
static uint8_t dash_last_segn;    // E2 level-bar lit-segment cache

// Big primary button (START/STOP/CALIBRATE-style). No icon glyph, so it can
// use the bold mono font — the mockup buttons carry no LV_SYMBOL either.
static lv_obj_t *create_action_btn(lv_obj_t *parent, const char *text, lv_obj_t **out_lbl)
{
    lv_obj_t *btn = lv_btn_create(parent);
    styles_ensure();
    lv_obj_add_style(btn, &st_btn_geom, LV_PART_MAIN); // radius/shadow/border-width (colors set by caller/tick)
    lv_obj_set_flex_grow(btn, 1);
    lv_obj_set_height(btn, 52);
    lv_obj_set_ext_click_area(btn, 6);

    lv_obj_t *lbl = lv_label_create(btn);
    lv_label_set_text_static(lbl, text);
    lv_obj_set_style_text_font(lbl, &ui_font_mono22, LV_PART_MAIN);
    lv_obj_set_style_text_letter_space(lbl, 2, LV_PART_MAIN);
    lv_obj_center(lbl);
    if (out_lbl) *out_lbl = lbl;
    return btn;
}

// Direction toggle button (FWD/REV). Same height/weight as the action buttons
// in the mockup; active state is solid accent + white text (set in the tick).
static lv_obj_t *create_dir_btn(lv_obj_t *parent, const char *text, lv_obj_t **out_lbl)
{
    lv_obj_t *btn = lv_btn_create(parent);
    styles_ensure();
    lv_obj_add_style(btn, &st_btn_geom, LV_PART_MAIN); // shared geometry; active/idle colors set in the tick
    lv_obj_set_flex_grow(btn, 1);
    lv_obj_set_height(btn, 52);
    lv_obj_set_ext_click_area(btn, 6);

    lv_obj_t *lbl = lv_label_create(btn);
    lv_label_set_text_static(lbl, text);
    lv_obj_set_style_text_font(lbl, &ui_font_mono22, LV_PART_MAIN);
    lv_obj_set_style_text_letter_space(lbl, 1, LV_PART_MAIN);
    lv_obj_center(lbl);
    if (out_lbl) *out_lbl = lbl;
    return btn;
}

// Direction toggle button carrying a ROTATION ICON instead of a text label
// (FWD/REV read oddly, FORWARD/BACKWARD too long, CW/CCW too cryptic for
// non-engineers — a curved rotate arrow is compact AND universally legible).
// Same geometry as create_dir_btn; the tick recolors the alpha icon per state
// (white on the active accent fill / ink on the idle white face). img_recolor_opa
// is set COVER once here so the tick only has to change the recolor color.
static lv_obj_t *create_dir_icon_btn(lv_obj_t *parent, const lv_img_dsc_t *icon, lv_obj_t **out_img)
{
    lv_obj_t *btn = lv_btn_create(parent);
    styles_ensure();
    lv_obj_add_style(btn, &st_btn_geom, LV_PART_MAIN);
    lv_obj_set_flex_grow(btn, 1);
    lv_obj_set_height(btn, 52);
    lv_obj_set_ext_click_area(btn, 6);

    lv_obj_t *img = lv_img_create(btn);
    lv_img_set_src(img, icon);
    lv_obj_set_style_img_recolor_opa(img, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_clear_flag(img, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_center(img);
    if (out_img) *out_img = img;
    return btn;
}

// Filled status pill (bg tint + colored text, radius 6) — the mockup's
// state-row pills, not the old bordered white capsule.
static lv_obj_t *dash_fill_pill(lv_obj_t *parent, const char *text, lv_color_t bg, lv_color_t fg)
{
    lv_obj_t *lbl = lv_label_create(parent);
    lv_label_set_text_static(lbl, text);
    lv_obj_set_style_text_font(lbl, &ui_font_mono20, LV_PART_MAIN);
    lv_obj_set_style_text_color(lbl, fg, LV_PART_MAIN);
    lv_obj_set_style_bg_color(lbl, bg, LV_PART_MAIN);
    lv_obj_set_style_bg_opa(lbl, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_radius(lbl, 6, LV_PART_MAIN);
    lv_obj_set_style_pad_hor(lbl, 14, LV_PART_MAIN);
    lv_obj_set_style_pad_ver(lbl, 7, LV_PART_MAIN);
    return lbl;
}

void create_screen_dashboard(lv_obj_t *parent)
{
    init_screen_bg(parent);
    dash_last_state      = 0xFF;
    dash_last_dir        = 0xFF;
    dash_last_running    = 0xFF;
    dash_last_segn       = 0xFF;

    // ---- Left column: gauge (divider at x=320, matching the mockup) ----
    lv_obj_t *col_left = flat_cont(parent);
    lv_obj_set_size(col_left, 320, 364);
    lv_obj_set_pos(col_left, 0, 0);
    lv_obj_set_style_border_side(col_left, LV_BORDER_SIDE_RIGHT, LV_PART_MAIN);
    lv_obj_set_style_border_color(col_left, COLOR_BORDER, LV_PART_MAIN);
    lv_obj_set_style_border_width(col_left, 1, LV_PART_MAIN);
    lv_obj_set_layout(col_left, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(col_left, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(col_left, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_row(col_left, 14, LV_PART_MAIN);

    // Speed tile — Style E2 (chosen mockup), sized to FILL the whole left column
    // (300x344 white card): a big faint BLDC-motor cross-section watermark centered
    // behind the readout, SPEED + state chip on top, a LEFT-aligned big RPM number,
    // a full-width 12-segment level bar, and direction at the bottom.
    lv_obj_t *gwrap = flat_cont(col_left);
    lv_obj_set_size(gwrap, 300, 344);
    lv_obj_set_style_bg_color(gwrap, COLOR_CARD_BG, LV_PART_MAIN);
    lv_obj_set_style_bg_opa(gwrap, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_border_color(gwrap, COLOR_BORDER, LV_PART_MAIN);
    lv_obj_set_style_border_width(gwrap, 1, LV_PART_MAIN);
    lv_obj_set_style_radius(gwrap, 14, LV_PART_MAIN);

    lv_obj_t *wm = lv_img_create(gwrap); // 175px native, right-anchored (x kept), lifted UP into the
    lv_img_set_src(wm, &ui_img_motor);   // empty upper-right space (user: change y, keep x)
    lv_obj_align(wm, LV_ALIGN_BOTTOM_RIGHT, 26, -96);
    lv_obj_set_style_img_recolor(wm, COLOR_TEXT_H, LV_PART_MAIN);
    lv_obj_set_style_img_recolor_opa(wm, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_img_opa(wm, 34, LV_PART_MAIN);
    lv_obj_clear_flag(wm, LV_OBJ_FLAG_CLICKABLE);

    lv_obj_t *sph = lv_label_create(gwrap);
    lv_label_set_text_static(sph, "SPEED");
    lv_obj_set_pos(sph, 26, 30);
    lv_obj_set_style_text_font(sph, &ui_font_mono20, LV_PART_MAIN);
    lv_obj_set_style_text_color(sph, COLOR_TEXT_VL, LV_PART_MAIN);
    lv_obj_set_style_text_letter_space(sph, 4, LV_PART_MAIN);

    dash_e2_chip = lv_label_create(gwrap); // state chip (colored in the tick)
    lv_label_set_text_static(dash_e2_chip, "STOPPED");
    lv_obj_align(dash_e2_chip, LV_ALIGN_TOP_RIGHT, -18, 26);
    lv_obj_set_style_text_font(dash_e2_chip, &ui_font_mono20, LV_PART_MAIN);
    lv_obj_set_style_text_color(dash_e2_chip, lv_color_white(), LV_PART_MAIN);
    lv_obj_set_style_bg_color(dash_e2_chip, COLOR_TEXT_L, LV_PART_MAIN);
    lv_obj_set_style_bg_opa(dash_e2_chip, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_radius(dash_e2_chip, LV_RADIUS_CIRCLE, LV_PART_MAIN);
    lv_obj_set_style_pad_hor(dash_e2_chip, 11, LV_PART_MAIN);
    lv_obj_set_style_pad_ver(dash_e2_chip, 4, LV_PART_MAIN);

    // Number + RPM unit as ONE left-aligned group, vertically centered in the
    // tile (user). Tight pad_row keeps the unit just under the hero number. The
    // number's left edge stays pinned (LEFT_MID + LEFT-aligned content) so a
    // width change on update doesn't shift it.
    lv_obj_t *numgrp = flat_cont(gwrap);
    lv_obj_set_size(numgrp, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
    lv_obj_align(numgrp, LV_ALIGN_LEFT_MID, 26, 0);
    lv_obj_set_layout(numgrp, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(numgrp, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(numgrp, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);
    lv_obj_set_style_pad_row(numgrp, 2, LV_PART_MAIN); // small gap number <-> RPM

    dash_lbl_rpm = lv_label_create(numgrp);
    label_bind_buffer(dash_lbl_rpm, "0");
    lv_obj_set_style_text_font(dash_lbl_rpm, &ui_font_mono66, LV_PART_MAIN); // hero RPM, ~50% bigger
    lv_obj_set_style_text_color(dash_lbl_rpm, COLOR_TEXT_H, LV_PART_MAIN);

    lv_obj_t *rpm_unit = lv_label_create(numgrp);
    lv_label_set_text_static(rpm_unit, "RPM");
    lv_obj_set_style_text_font(rpm_unit, &ui_font_mono20, LV_PART_MAIN);
    lv_obj_set_style_text_color(rpm_unit, COLOR_TEXT_VL, LV_PART_MAIN);
    lv_obj_set_style_text_letter_space(rpm_unit, 4, LV_PART_MAIN);

    lv_obj_t *segbar = flex_row(gwrap); // 12-segment level bar (full width)
    lv_obj_set_size(segbar, 248, 10);
    lv_obj_set_pos(segbar, 26, 278); // nudged down (user)
    lv_obj_set_style_pad_column(segbar, 4, LV_PART_MAIN);
    for (int i = 0; i < 12; i++)
    {
        lv_obj_t *s = flat_cont(segbar);
        lv_obj_set_flex_grow(s, 1);
        lv_obj_set_height(s, 10);
        lv_obj_set_style_radius(s, 2, LV_PART_MAIN);
        lv_obj_set_style_bg_color(s, lv_color_hex(0xE3E6EC), LV_PART_MAIN);
        lv_obj_set_style_bg_opa(s, LV_OPA_COVER, LV_PART_MAIN);
        dash_segs[i] = s;
    }

    dash_e2_dir = lv_img_create(gwrap); // current rotation direction (CW/CCW icon), read-only, bottom-left
    lv_img_set_src(dash_e2_dir, &ui_icon_cw);
    lv_obj_set_pos(dash_e2_dir, 26, 300);
    lv_obj_set_style_img_recolor(dash_e2_dir, COLOR_TEXT_L, LV_PART_MAIN);
    lv_obj_set_style_img_recolor_opa(dash_e2_dir, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_clear_flag(dash_e2_dir, LV_OBJ_FLAG_CLICKABLE);

    // ---- Right column: controls (wider than left, matching the mockup) ----
    lv_obj_t *col_right = flat_cont(parent);
    lv_obj_set_size(col_right, 480, 364);
    lv_obj_set_pos(col_right, 320, 0);
    lv_obj_set_style_pad_left(col_right, 16, LV_PART_MAIN);
    lv_obj_set_style_pad_right(col_right, 20, LV_PART_MAIN);
    lv_obj_set_style_pad_ver(col_right, 16, LV_PART_MAIN);
    lv_obj_set_style_pad_row(col_right, 14, LV_PART_MAIN);
    lv_obj_set_layout(col_right, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(col_right, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(col_right, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER);

    // Health at a glance: power + motor temp (speed is already the big gauge,
    // target is already the setpoint readout — no duplicated numbers)
    lv_obj_t *row_cards = flex_row(col_right);
    lv_obj_set_size(row_cards, LV_PCT(100), LV_SIZE_CONTENT);
    lv_obj_set_style_pad_column(row_cards, 12, LV_PART_MAIN);
    lv_obj_t *card_a = create_mcard(row_cards, "POWER", "W", COLOR_TEXT_H, &dash_val_power);
    lv_obj_set_width(card_a, 50); // flex_grow overrides; equal halves
    lv_obj_set_flex_grow(card_a, 1);
    lv_obj_t *card_t = create_mcard(row_cards, "MOTOR TEMP", "\xC2\xB0" "C", COLOR_TEXT_M, &dash_val_temp);
    lv_obj_set_width(card_t, 50);
    lv_obj_set_flex_grow(card_t, 1);

    // Resting (stopped) look: START solid green, STOP red-outline. The SLOW
    // tick flips the emphasis on the run-state transition (STOP → solid red,
    // START → green-outline), same as Control.
    lv_obj_t *row_btns = flex_row(col_right);
    lv_obj_set_size(row_btns, LV_PCT(100), LV_SIZE_CONTENT);
    lv_obj_set_style_pad_column(row_btns, 10, LV_PART_MAIN);
    dash_btn_start = create_action_btn(row_btns, "START", &dash_lbl_start);
    lv_obj_set_style_bg_color(dash_btn_start, COLOR_OK, LV_PART_MAIN);
    lv_obj_set_style_border_color(dash_btn_start, COLOR_OK, LV_PART_MAIN);
    lv_obj_set_style_text_color(dash_lbl_start, lv_color_white(), LV_PART_MAIN);
    lv_obj_add_event_cb(dash_btn_start, action_motor_start, LV_EVENT_CLICKED, NULL);
    dash_btn_stop = create_action_btn(row_btns, "STOP", &dash_lbl_stop);
    lv_obj_set_style_bg_color(dash_btn_stop, COLOR_CARD_BG, LV_PART_MAIN);
    lv_obj_set_style_border_color(dash_btn_stop, COLOR_DANGER, LV_PART_MAIN);
    lv_obj_set_style_text_color(dash_lbl_stop, COLOR_DANGER, LV_PART_MAIN);
    lv_obj_add_event_cb(dash_btn_stop, action_motor_stop, LV_EVENT_CLICKED, NULL);

    lv_obj_t *row_dir = flex_row(col_right);
    lv_obj_set_size(row_dir, LV_PCT(100), LV_SIZE_CONTENT);
    lv_obj_set_style_pad_column(row_dir, 10, LV_PART_MAIN);
    dash_btn_fwd = create_dir_icon_btn(row_dir, &ui_icon_cw, &dash_lbl_fwd);
    lv_obj_add_event_cb(dash_btn_fwd, action_motor_dir_fwd, LV_EVENT_CLICKED, NULL);
    dash_btn_rev = create_dir_icon_btn(row_dir, &ui_icon_ccw, &dash_lbl_rev);
    lv_obj_add_event_cb(dash_btn_rev, action_motor_dir_rev, LV_EVENT_CLICKED, NULL);

    // Speed setpoint — label + live value on one line, slider below. No scale
    // ticks (the mockup dropped them; the value readout already shows target).
    lv_obj_t *sp_block = flat_cont(col_right);
    lv_obj_set_size(sp_block, LV_PCT(100), LV_SIZE_CONTENT);
    lv_obj_set_style_pad_row(sp_block, 10, LV_PART_MAIN);
    lv_obj_set_layout(sp_block, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(sp_block, LV_FLEX_FLOW_COLUMN);
    lv_obj_add_flag(sp_block, LV_OBJ_FLAG_OVERFLOW_VISIBLE);

    lv_obj_t *sp_head = flex_row(sp_block);
    lv_obj_set_size(sp_head, LV_PCT(100), LV_SIZE_CONTENT);
    lv_obj_set_flex_align(sp_head, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_t *sp_title = lv_label_create(sp_head);
    lv_label_set_text_static(sp_title, "SPEED SETPOINT");
    lv_obj_set_style_text_font(sp_title, &ui_font_mono20, LV_PART_MAIN);
    lv_obj_set_style_text_color(sp_title, COLOR_TEXT_M, LV_PART_MAIN);
    lv_obj_set_style_text_letter_space(sp_title, 1, LV_PART_MAIN);
    dash_lbl_setpoint = lv_label_create(sp_head);
    label_bind_buffer(dash_lbl_setpoint, "0 RPM");
    lv_obj_set_style_text_font(dash_lbl_setpoint, &ui_font_mono22, LV_PART_MAIN);
    lv_obj_set_style_text_color(dash_lbl_setpoint, COLOR_TEXT_H, LV_PART_MAIN);

    dash_slider = lv_slider_create(sp_block);
    lv_obj_set_width(dash_slider, LV_PCT(100));
    lv_obj_set_height(dash_slider, 12);
    lv_slider_set_range(dash_slider, 0, UI_MAX_RPM);
    lv_slider_set_value(dash_slider, 0, LV_ANIM_OFF);
    style_slider(dash_slider); // shared track/fill/knob look
    lv_obj_add_event_cb(dash_slider, action_motor_speed_change, LV_EVENT_VALUE_CHANGED, NULL);
}

void tick_screen_dashboard(ui_lane_t lane)
{
    if (ui_current_tab != TAB_DASHBOARD)
        return;
    char buf[24], num[16];

    if (lane == UI_LANE_FAST)
    {
        // Speed tile is plain text now — nothing fast-moving (no arc/needle).
        return;
    }
    else if (lane == UI_LANE_SLOW)
    {
        int32_t rpm       = (int32_t) ui_motor_status_fast()->bits.actualRpm;
        int32_t motorTemp = MotorStatusSlow_GetMtTemp(ui_motor_status_slow());

        label_set_if_changed(dash_lbl_rpm, fmt_rpm(buf, sizeof(buf), rpm));
        label_set_if_changed(dash_val_power,
                             fmt_scaled(num, sizeof(num), (int32_t) motor_power_deciwatt(), 10, 1));
        label_set_if_changed(dash_val_temp, fmt_scaled(num, sizeof(num), motorTemp, 10, 1));
        label_color_if_changed(dash_val_temp, temp_color(motorTemp, 600, 800));

        // E2 state chip (solid state color, white text) + 12-segment level bar
        // (lit up to the speed fraction, colored by state). STOPPED gray /
        // RUNNING green / STARTING·STOPPING amber / FAULT red.
        uint8_t motorState = (uint8_t) ui_motor_status_fast()->bits.motorState;
        lv_color_t sc = COLOR_TEXT_L;
        if (motorState == 2) sc = COLOR_OK;
        else if (motorState == 4) sc = COLOR_DANGER;
        else if (motorState == 1 || motorState == 3) sc = COLOR_WARN;
        bool stateChanged = (motorState != dash_last_state);
        if (stateChanged && dash_e2_chip)
        {
            static const char * const SN[5] = {"STOPPED", "STARTING", "RUNNING", "STOPPING", "FAULT"};
            label_set_static_if_changed(dash_e2_chip, SN[motorState <= 4 ? motorState : 4]);
            lv_obj_set_style_bg_color(dash_e2_chip, sc, LV_PART_MAIN);
        }
        int segn = (rpm * 12 + UI_MAX_RPM / 2) / UI_MAX_RPM;
        if (segn > 12) segn = 12;
        if (segn < 0) segn = 0;
        if ((segn != dash_last_segn || stateChanged) && dash_segs[0])
        {
            dash_last_segn = (uint8_t) segn;
            for (int i = 0; i < 12; i++)
                lv_obj_set_style_bg_color(dash_segs[i], i < segn ? sc : lv_color_hex(0xE3E6EC), LV_PART_MAIN);
        }
        dash_last_state = motorState;

        // START/STOP emphasis flips with the confirmed run-state — same as
        // Control: stopped → START solid green + STOP red-outline; running →
        // STOP solid red + START green-outline. Transition-guarded.
        uint8_t running = motor_is_running() ? 1 : 0;
        if (running != dash_last_running && dash_btn_start)
        {
            dash_last_running = running;
            lv_obj_set_style_bg_color(dash_btn_start, running ? COLOR_CARD_BG : COLOR_OK, LV_PART_MAIN);
            lv_obj_set_style_border_color(dash_btn_start, COLOR_OK, LV_PART_MAIN);
            lv_obj_set_style_text_color(dash_lbl_start, running ? COLOR_OK : lv_color_white(), LV_PART_MAIN);
            lv_obj_set_style_bg_color(dash_btn_stop, running ? COLOR_DANGER : COLOR_CARD_BG, LV_PART_MAIN);
            lv_obj_set_style_border_color(dash_btn_stop, COLOR_DANGER, LV_PART_MAIN);
            lv_obj_set_style_text_color(dash_lbl_stop, running ? lv_color_white() : COLOR_DANGER, LV_PART_MAIN);
        }

        // Direction pill + toggle buttons — read the CONFIRMED direction from
        // telemetry, not motorCmd (the button only flips once the motor board
        // echoes it back, see UART_PROTOCOL.md sec 3.1). Active button is solid
        // accent + white text (the mockup's btn-dir.active look).
        uint8_t direction = (uint8_t) ui_motor_status_fast()->bits.dir;
        if (direction != dash_last_dir)
        {
            dash_last_dir = direction;
            if (dash_e2_dir) lv_img_set_src(dash_e2_dir, direction ? &ui_icon_ccw : &ui_icon_cw);
            lv_obj_t *on      = direction ? dash_btn_rev : dash_btn_fwd;
            lv_obj_t *off     = direction ? dash_btn_fwd : dash_btn_rev;
            lv_obj_t *on_ic   = direction ? dash_lbl_rev : dash_lbl_fwd;  // icon img now
            lv_obj_t *off_ic  = direction ? dash_lbl_fwd : dash_lbl_rev;
            lv_obj_set_style_bg_color(on, COLOR_ACCENT, LV_PART_MAIN);
            lv_obj_set_style_border_color(on, COLOR_ACCENT, LV_PART_MAIN);
            lv_obj_set_style_img_recolor(on_ic, lv_color_white(), LV_PART_MAIN);
            lv_obj_set_style_bg_color(off, COLOR_CARD_BG, LV_PART_MAIN);
            lv_obj_set_style_border_color(off, COLOR_BORDER, LV_PART_MAIN);
            lv_obj_set_style_img_recolor(off_ic, COLOR_TEXT_M, LV_PART_MAIN);
        }

        // Setpoint readout + slider follow (skip while the user is dragging).
        // Reads motorCmd (UI-owned, set immediately on drag — see actions.c),
        // not telemetry: this is what WE last commanded, not an RX echo.
        // ctrlValRaw only means "target RPM" while opMode is actually SPEED —
        // otherwise it's a leftover torque/duty/position value in different
        // units, so show 0 rather than a misleadingly-labeled number.
        int32_t targetRpm =
                (motorCmd.bits.opMode == OP_MODE_SPEED) ? (int32_t) motorCmd.bits.ctrlValRaw : 0;
        snprintf(buf, sizeof(buf), "%d RPM", (int) targetRpm);
        label_set_if_changed(dash_lbl_setpoint, buf);
        if (dash_slider && !lv_obj_has_state(dash_slider, LV_STATE_PRESSED) &&
            lv_slider_get_value(dash_slider) != targetRpm)
            lv_slider_set_value(dash_slider, targetRpm, LV_ANIM_OFF);
        // START/STOP are static (green solid / red outline, set in create) —
        // the mockup shows both colored at all times; action_* are guarded.
        // Link status lives only in the top bar now.
    }
}

// =============================================================================
// MONITOR — operator-focused: 8 essential values in two rows. The original
// 17-value layout saturated the MCU (heap near the ceiling + a burst of dirty
// regions over xSPI every tick) and corrupted the UI on hardware.
// Dropped: Id (FOC-internal, ~0), per-phase A/B/C (|Iph| RMS covers magnitude;
// imbalance diagnosis belongs on Diagnostics), MCU temp, footer strip.
// =============================================================================

static lv_obj_t *mon_val_current, *mon_val_dc, *mon_val_power, *mon_val_eff;
static lv_obj_t *mon_val_motortemp, *mon_val_invtemp, *mon_val_speed, *mon_val_torque;
static uint8_t   mon_update_phase;
static uint8_t   mon_force_all;

// Redesigned from a 7-card grid to a row list (icon + label + value) — the
// card grid didn't have room for the new 20px+ type scale without
// overflowing, and a flat list of numbers reads faster than a grid of boxes
// anyway. No level bar: the finalized mockup dropped the inline bars as visual
// clutter; threshold feedback lives on the value's text color instead.
static lv_obj_t *mon_row(lv_obj_t *parent, const lv_img_dsc_t *icon, const char *label_text, lv_obj_t **out_val)
{
    lv_obj_t *row = flex_row(parent);
    styles_ensure();
    lv_obj_add_style(row, &st_listrow, LV_PART_MAIN); // shared bottom divider
    lv_obj_set_size(row, LV_PCT(100), LV_SIZE_CONTENT);
    lv_obj_set_style_pad_ver(row, 11, LV_PART_MAIN);
    lv_obj_set_style_pad_column(row, 10, LV_PART_MAIN);

    // Custom alpha-mask icon (mockup shapes), recolored neutral via shared style.
    lv_obj_t *ic = lv_img_create(row);
    lv_img_set_src(ic, icon);
    lv_obj_add_style(ic, &st_mon_icon, LV_PART_MAIN);
    lv_obj_clear_flag(ic, LV_OBJ_FLAG_CLICKABLE);

    lv_obj_t *lbl = lv_label_create(row);
    lv_label_set_text_static(lbl, label_text);
    lv_obj_add_style(lbl, &st_mon_lbl, LV_PART_MAIN);

    lv_obj_t *val = lv_label_create(row);
    label_bind_buffer(val, "0");
    lv_label_set_long_mode(val, LV_LABEL_LONG_CLIP);
    lv_obj_add_style(val, &st_mon_val, LV_PART_MAIN);
    if (out_val) *out_val = val;
    return row;
}

// Card wrapper: accent-tick + tracked title, then a stack of mon_row()s.
static lv_obj_t *mon_card(lv_obj_t *parent, const char *title, lv_color_t tick_color)
{
    lv_obj_t *card = lv_obj_create(parent);
    lv_obj_set_width(card, 50); // flex_grow overrides; equal halves
    lv_obj_set_flex_grow(card, 1);
    lv_obj_set_height(card, LV_SIZE_CONTENT);
    lv_obj_set_style_bg_color(card, COLOR_CARD_BG, LV_PART_MAIN);
    lv_obj_set_style_border_color(card, COLOR_BORDER, LV_PART_MAIN);
    lv_obj_set_style_border_width(card, 1, LV_PART_MAIN);
    lv_obj_set_style_radius(card, 10, LV_PART_MAIN);
    lv_obj_set_style_pad_hor(card, 16, LV_PART_MAIN);
    lv_obj_set_style_pad_ver(card, 12, LV_PART_MAIN);
    lv_obj_clear_flag(card, LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_layout(card, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(card, LV_FLEX_FLOW_COLUMN);

    lv_obj_t *head = flex_row(card);
    lv_obj_set_size(head, LV_PCT(100), LV_SIZE_CONTENT);
    lv_obj_set_style_pad_column(head, 9, LV_PART_MAIN);
    lv_obj_set_style_pad_bottom(head, 6, LV_PART_MAIN);
    lv_obj_t *tick = flat_cont(head);
    lv_obj_set_size(tick, 4, 16);
    lv_obj_set_style_radius(tick, 2, LV_PART_MAIN);
    lv_obj_set_style_bg_color(tick, tick_color, LV_PART_MAIN);
    lv_obj_set_style_bg_opa(tick, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_t *lbl = lv_label_create(head);
    lv_label_set_text_static(lbl, title);
    lv_obj_set_style_text_font(lbl, &ui_font_mono20, LV_PART_MAIN);
    lv_obj_set_style_text_color(lbl, COLOR_TEXT_VL, LV_PART_MAIN);
    lv_obj_set_style_text_letter_space(lbl, 1, LV_PART_MAIN);
    return card;
}

void create_screen_monitor(lv_obj_t *parent)
{
    init_screen_bg(parent);
    mon_force_all = 1;

    lv_obj_t *main_cont = flat_cont(parent);
    lv_obj_set_size(main_cont, 800, 364);
    lv_obj_set_pos(main_cont, 0, 0);
    lv_obj_set_style_pad_all(main_cont, 20, LV_PART_MAIN);
    lv_obj_set_style_pad_column(main_cont, 16, LV_PART_MAIN);
    lv_obj_set_layout(main_cont, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(main_cont, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(main_cont, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);

    // Data in neutral ink — color is reserved for state (thresholds, health).
    // Only one current row: the wire protocol exposes a single stator current
    // magnitude, not separate Id/Iq/per-phase values.
    lv_obj_t *card_elec = mon_card(main_cont, "ELECTRICAL", COLOR_GAUGE);
    mon_row(card_elec, &ui_icon_bolt, "CURRENT", &mon_val_current);
    mon_row(card_elec, &ui_icon_battery, "DC BUS", &mon_val_dc);
    mon_row(card_elec, &ui_icon_power, "POWER", &mon_val_power);
    mon_row(card_elec, &ui_icon_eff, "EFFICIENCY", &mon_val_eff);

    lv_obj_t *card_therm = mon_card(main_cont, "THERMAL & MOTION", COLOR_WARN);
    mon_row(card_therm, &ui_icon_thermo, "MOTOR TEMP", &mon_val_motortemp);
    mon_row(card_therm, &ui_icon_thermo, "INVERTER", &mon_val_invtemp);
    mon_row(card_therm, &ui_icon_dash, "SPEED", &mon_val_speed);
    mon_row(card_therm, &ui_icon_dash, "TORQUE", &mon_val_torque); // shares the SPEED icon (user request)
}

// =============================================================================
// CONTROL
// =============================================================================

// Control's actualRpm gauge — same construction as the Dashboard gauge, just
// sized for the card (270° arc from 135, ink needle from the hub, azure hub).
#define CG_SZ 260
#define CG_C (CG_SZ / 2)
#define CG_ARC_W 17
#define CG_NEEDLE 98
// Shared width of the right-hand column so MODE (dropdown), TARGET row, slider,
// and FWD/REV all share one width with aligned left edges.
#define CTRL_RCOL_W 280

static lv_obj_t *ctrl_mode_dd;        // compact MODE dropdown, top-right of the card (persists across modes)
static lv_obj_t *ctrl_level_layout;   // SPEED/TORQUE/OPEN LOOP: SPEED+TORQUE readout (left) + control column (right)
static lv_obj_t *ctrl_pos_layout;     // POSITION: one big gray ring + an azure dot inside it
// LEVEL readout (design "Divided", no gauge): two big numbers SPEED | TORQUE +
// a state pill + direction. Torque reads the real actualTorque wire field.
static lv_obj_t *ctrl_spd_num, *ctrl_tq_num, *ctrl_spd_bar, *ctrl_tq_bar; // LEVEL readout: SPEED | TORQUE cols + level bars
static uint8_t   ctrl_spd_band, ctrl_tq_band; // recolor each bar's fill only on a band transition
static lv_obj_t *ctrl_lvl_title, *ctrl_lvl_num, *ctrl_lvl_unit, *ctrl_lvl_slider; // reused per level mode
// Secondary LIMIT control in the level column (current limit for SPEED/OPEN
// LOOP, speed limit for TORQUE) — one reused row + slider, reconfigured by the
// tick like the primary one. Writes MotorCmd_t.limitRaw.
static lv_obj_t *ctrl_lim_title, *ctrl_lim_num, *ctrl_lim_unit, *ctrl_lim_slider;
static lv_obj_t *ctrl_btn_fwd, *ctrl_lbl_fwd, *ctrl_btn_rev, *ctrl_lbl_rev;
static lv_obj_t *ctrl_pos_ring, *ctrl_pos_dot; // POSITION: draggable arc (knob hidden) + a manual dot inside
static lv_obj_t *ctrl_pos_area;                // POSITION: the ring's touch target (drag), for the tick's drag guard
static lv_obj_t *ctrl_pos_num;                 // POSITION: target angle readout (degrees) in the ring centre
static lv_obj_t *ctrl_pos_lim_num, *ctrl_pos_lim_slider; // POSITION: speed-limit row (right column)
static lv_obj_t *ctrl_btn_start, *ctrl_lbl_start, *ctrl_btn_stop, *ctrl_lbl_stop, *ctrl_btn_cal;
static uint8_t   ctrl_last_mode, ctrl_last_dir, ctrl_last_running, ctrl_lvl_mode;

// The MODE dropdown's visual order is not the OpMode_e value order.
static const uint8_t ctrl_mode_order[4] = {OP_MODE_SPEED, OP_MODE_TORQUE, OP_MODE_OPEN_LOOP, OP_MODE_POSITION};

// The single reused level slider drives whichever level mode is showing, so
// dispatch to that mode's action_*_change (each reads the slider + writes
// motorCmd). ctrl_lvl_mode is set by the tick when it reconfigures the panel.
// (SPEED's handler is also used directly by the Dashboard slider — shared.)
static void ctrl_setpoint_cb(lv_event_t *e)
{
    switch (ctrl_lvl_mode)
    {
        case OP_MODE_TORQUE:    action_torque_change(e);      break;
        case OP_MODE_OPEN_LOOP: action_openloop_change(e);    break;
        default:                action_motor_speed_change(e); break; // SPEED
    }
}

// MODE dropdown → opMode (+ zero BOTH ctrlValRaw and limitRaw, exactly like
// action_mode_select). Both are single fields shared across every opMode, so a
// leftover value from the previous mode must not carry into the new one's units.
static void ctrl_mode_dd_cb(lv_event_t *e)
{
    uint16_t sel = lv_dropdown_get_selected(lv_event_get_target(e));
    if (sel > 3) return;
    motorCmd.bits.opMode     = ctrl_mode_order[sel];
    motorCmd.bits.ctrlValRaw = 0;
    motorCmd.bits.limitRaw   = 0;
}

// The reused LEVEL limit slider (SPEED/OPEN LOOP → current mA, TORQUE → speed
// RPM). Value goes straight onto limitRaw. Pin opMode to the panel's mode so
// the shared limitRaw isn't misread under another mode (same guard as the
// primary handlers).
static void ctrl_limit_cb(lv_event_t *e)
{
    int32_t v = lv_slider_get_value(lv_event_get_target(e));
    motorCmd.bits.opMode   = ctrl_lvl_mode;
    motorCmd.bits.limitRaw = (uint32_t) v;
}

// POSITION's speed-limit slider (RPM → limitRaw). Pins opMode to POSITION.
static void ctrl_pos_limit_cb(lv_event_t *e)
{
    int32_t v = lv_slider_get_value(lv_event_get_target(e));
    motorCmd.bits.opMode   = OP_MODE_POSITION;
    motorCmd.bits.limitRaw = (uint32_t) v;
}

// POSITION: place the azure dot INSIDE the ring, near its inner rim, at the
// angle of the current arc value (the arc's own knob is hidden). rotation 270
// puts value 0 at the top; screen trig (0°=3 o'clock, clockwise, y down).
#define CTRL_POS_DOT_R 82 // dot-center radius from ring center (inside the rim)
static void ctrl_pos_update_dot(void)
{
    if (!ctrl_pos_ring || !ctrl_pos_dot) return;
    int32_t v   = lv_arc_get_value(ctrl_pos_ring);
    int16_t deg = (int16_t) (270 + (v * 360) / UI_POSITION_MAX_RAW);
    lv_coord_t x = (lv_coord_t) (((int32_t) CTRL_POS_DOT_R * lv_trigo_cos(deg)) >> LV_TRIGO_SHIFT);
    lv_coord_t y = (lv_coord_t) (((int32_t) CTRL_POS_DOT_R * lv_trigo_sin(deg)) >> LV_TRIGO_SHIFT);
    lv_obj_align(ctrl_pos_dot, LV_ALIGN_CENTER, x, y);
}

// POSITION: refresh the target-angle number in the ring centre (0.1°/LSB →
// degrees, 1 decimal, e.g. "180.0"). The "°" sits next to it as a static unit.
static void ctrl_pos_update_readout(void)
{
    if (!ctrl_pos_num) return;
    char b[12];
    fmt_scaled(b, sizeof(b), lv_arc_get_value(ctrl_pos_ring), 10, 1);
    label_set_if_changed(ctrl_pos_num, b);
}

// POSITION drag → compute the target angle STRAIGHT from the touch point (see
// ctrl_pos_math.h: continuous, no lv_arc full-circle seam snap = fixes the
// "jump" bug), then move the ring value + the azure dot + write ctrlValRaw.
// Bound to ctrl_pos_layout (the ring itself is display-only) for PRESSED +
// PRESSING so a press or a drag both track the finger.
static void ctrl_pos_drag_cb(lv_event_t *e)
{
    (void) e;
    lv_indev_t *indev = lv_indev_get_act();
    if (!indev || !ctrl_pos_ring) return;
    lv_point_t p;
    lv_indev_get_point(indev, &p);
    lv_area_t a;
    lv_obj_get_coords(ctrl_pos_ring, &a);
    int32_t v = ctrl_pos_angle_to_raw((int) (p.x - (a.x1 + a.x2) / 2), (int) (p.y - (a.y1 + a.y2) / 2),
                                      UI_POSITION_MAX_RAW);
    lv_arc_set_value(ctrl_pos_ring, v);
    ctrl_pos_update_dot();
    ctrl_pos_update_readout();
    motorCmd.bits.opMode     = OP_MODE_POSITION;
    motorCmd.bits.ctrlValRaw = (uint32_t) v;
}

// Layout: the mode selector is a compact DROPDOWN pinned to the card's
// top-right corner (freeing the whole central space); a big white card fills
// the middle; START/STOP/CALIBRATE (equal width) sit below. The card holds two
// overlapping full-size layouts, only one un-HIDDEN by the tick:
//   • LEVEL (SPEED/TORQUE/OPEN LOOP): a full dashboard-style actualRpm gauge on
//     the LEFT + a compact right column (title/value, slider, FWD/REV).
//   • POSITION: one big gray ring with an azure dot INSIDE it near the rim.
void create_screen_control(lv_obj_t *parent)
{
    init_screen_bg(parent);
    ctrl_last_mode       = 0xFF;
    ctrl_last_dir        = 0xFF;
    ctrl_last_running    = 0xFF;
    ctrl_spd_band        = 0xFF;
    ctrl_tq_band         = 0xFF;
    ctrl_lvl_mode        = OP_MODE_SPEED; // boot default; tick reconfigures on mode echo

    lv_obj_t *root = flat_cont(parent);
    lv_obj_set_size(root, 800, 364);
    lv_obj_set_pos(root, 0, 0);
    lv_obj_set_style_pad_hor(root, 12, LV_PART_MAIN);
    lv_obj_set_style_pad_ver(root, 12, LV_PART_MAIN);
    lv_obj_set_style_pad_row(root, 12, LV_PART_MAIN);
    lv_obj_set_layout(root, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(root, LV_FLEX_FLOW_COLUMN);

    // ---- Central white card (fills the space; mode dropdown floats top-right) ----
    lv_obj_t *card = flat_cont(root);
    lv_obj_set_width(card, LV_PCT(100));
    lv_obj_set_flex_grow(card, 1);
    lv_obj_set_style_bg_color(card, COLOR_CARD_BG, LV_PART_MAIN);
    lv_obj_set_style_bg_opa(card, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_border_color(card, COLOR_BORDER, LV_PART_MAIN);
    lv_obj_set_style_border_width(card, 1, LV_PART_MAIN);
    lv_obj_set_style_radius(card, 10, LV_PART_MAIN);
    lv_obj_set_style_pad_hor(card, 18, LV_PART_MAIN);
    lv_obj_set_style_pad_ver(card, 12, LV_PART_MAIN); // trimmed to give the bigger gauge room
    lv_obj_add_flag(card, LV_OBJ_FLAG_OVERFLOW_VISIBLE); // slider knob mustn't clip

    // ---- LEVEL layout: dashboard-style actualRpm gauge (left) + control (right) ----
    ctrl_level_layout = flat_cont(card);
    lv_obj_set_size(ctrl_level_layout, LV_PCT(100), LV_PCT(100));
    lv_obj_set_layout(ctrl_level_layout, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(ctrl_level_layout, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(ctrl_level_layout, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_column(ctrl_level_layout, 20, LV_PART_MAIN);
    lv_obj_add_flag(ctrl_level_layout, LV_OBJ_FLAG_OVERFLOW_VISIBLE);

    // ---- LEVEL left: SPEED | TORQUE — two EQUAL columns; all text LEFT-aligned,
    // labels pinned to the TOP, generous spacing, and a level bar at the BOTTOM of
    // each (user). Hairline divider between them + a hairline to the control column.
    lv_obj_t *garea = flat_cont(ctrl_level_layout);
    lv_obj_set_flex_grow(garea, 1);
    lv_obj_set_height(garea, LV_PCT(100));
    lv_obj_set_layout(garea, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(garea, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(garea, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);
    lv_obj_set_style_border_side(garea, LV_BORDER_SIDE_RIGHT, LV_PART_MAIN);
    lv_obj_set_style_border_color(garea, COLOR_BORDER, LV_PART_MAIN);
    lv_obj_set_style_border_width(garea, 1, LV_PART_MAIN);

    // SPEED column (flex_grow 1 = exactly half). top + left aligned, spaced.
    lv_obj_t *scol = flat_cont(garea);
    lv_obj_set_flex_grow(scol, 1);
    lv_obj_set_height(scol, LV_PCT(100));
    lv_obj_set_layout(scol, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(scol, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(scol, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);
    lv_obj_set_style_pad_all(scol, 22, LV_PART_MAIN);
    lv_obj_set_style_pad_row(scol, 10, LV_PART_MAIN);
    lv_obj_t *slab = lv_label_create(scol);
    lv_label_set_text_static(slab, "SPEED");
    lv_obj_set_style_text_font(slab, &ui_font_mono20, LV_PART_MAIN);
    lv_obj_set_style_text_color(slab, COLOR_TEXT_L, LV_PART_MAIN);
    lv_obj_set_style_text_letter_space(slab, 3, LV_PART_MAIN);
    ctrl_spd_num = lv_label_create(scol);
    label_bind_buffer(ctrl_spd_num, "0");
    lv_obj_set_style_text_font(ctrl_spd_num, &ui_font_mono66, LV_PART_MAIN); // ~50% bigger hero
    lv_obj_set_style_text_color(ctrl_spd_num, COLOR_TEXT_H, LV_PART_MAIN);
    lv_obj_set_style_pad_top(ctrl_spd_num, 30, LV_PART_MAIN); // drop number+unit below the SPEED label
    lv_obj_t *sunit = lv_label_create(scol);
    lv_label_set_text_static(sunit, "RPM");
    lv_obj_set_style_text_font(sunit, &ui_font_mono20, LV_PART_MAIN);
    lv_obj_set_style_text_color(sunit, COLOR_TEXT_VL, LV_PART_MAIN);
    lv_obj_set_style_text_letter_space(sunit, 3, LV_PART_MAIN);
    lv_obj_t *sspacer = flat_cont(scol); // pushes the bar to the bottom
    lv_obj_set_flex_grow(sspacer, 1);
    lv_obj_set_width(sspacer, LV_PCT(100));
    ctrl_spd_bar = lv_bar_create(scol);
    lv_obj_set_size(ctrl_spd_bar, LV_PCT(100), 10);
    lv_bar_set_range(ctrl_spd_bar, 0, 100);
    lv_bar_set_value(ctrl_spd_bar, 0, LV_ANIM_OFF);
    lv_obj_set_style_bg_color(ctrl_spd_bar, lv_color_hex(0xE3E6EC), LV_PART_MAIN);
    lv_obj_set_style_radius(ctrl_spd_bar, 5, LV_PART_MAIN);
    lv_obj_set_style_bg_color(ctrl_spd_bar, COLOR_GAUGE_GREEN, LV_PART_INDICATOR);
    lv_obj_set_style_radius(ctrl_spd_bar, 5, LV_PART_INDICATOR);

    // TORQUE column (hairline divider on its left edge)
    lv_obj_t *tcol = flat_cont(garea);
    lv_obj_set_flex_grow(tcol, 1);
    lv_obj_set_height(tcol, LV_PCT(100));
    lv_obj_set_layout(tcol, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(tcol, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(tcol, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);
    lv_obj_set_style_pad_all(tcol, 22, LV_PART_MAIN);
    lv_obj_set_style_pad_row(tcol, 10, LV_PART_MAIN);
    lv_obj_set_style_border_side(tcol, LV_BORDER_SIDE_LEFT, LV_PART_MAIN);
    lv_obj_set_style_border_color(tcol, COLOR_BORDER, LV_PART_MAIN);
    lv_obj_set_style_border_width(tcol, 1, LV_PART_MAIN);
    lv_obj_t *tlab = lv_label_create(tcol);
    lv_label_set_text_static(tlab, "TORQUE");
    lv_obj_set_style_text_font(tlab, &ui_font_mono20, LV_PART_MAIN);
    lv_obj_set_style_text_color(tlab, COLOR_TEXT_L, LV_PART_MAIN);
    lv_obj_set_style_text_letter_space(tlab, 3, LV_PART_MAIN);
    ctrl_tq_num = lv_label_create(tcol);
    label_bind_buffer(ctrl_tq_num, "0.0");
    lv_obj_set_style_text_font(ctrl_tq_num, &ui_font_mono66, LV_PART_MAIN); // ~50% bigger hero
    lv_obj_set_style_text_color(ctrl_tq_num, COLOR_TEXT_H, LV_PART_MAIN);
    lv_obj_set_style_pad_top(ctrl_tq_num, 30, LV_PART_MAIN); // drop number+unit below the TORQUE label
    lv_obj_t *tunit = lv_label_create(tcol);
    lv_label_set_text_static(tunit, "N.m");
    lv_obj_set_style_text_font(tunit, &ui_font_mono20, LV_PART_MAIN);
    lv_obj_set_style_text_color(tunit, COLOR_TEXT_VL, LV_PART_MAIN);
    lv_obj_set_style_text_letter_space(tunit, 3, LV_PART_MAIN);
    lv_obj_t *tspacer = flat_cont(tcol);
    lv_obj_set_flex_grow(tspacer, 1);
    lv_obj_set_width(tspacer, LV_PCT(100));
    ctrl_tq_bar = lv_bar_create(tcol);
    lv_obj_set_size(ctrl_tq_bar, LV_PCT(100), 10);
    lv_bar_set_range(ctrl_tq_bar, 0, 100);
    lv_bar_set_value(ctrl_tq_bar, 0, LV_ANIM_OFF);
    lv_obj_set_style_bg_color(ctrl_tq_bar, lv_color_hex(0xE3E6EC), LV_PART_MAIN);
    lv_obj_set_style_radius(ctrl_tq_bar, 5, LV_PART_MAIN);
    lv_obj_set_style_bg_color(ctrl_tq_bar, COLOR_GAUGE_GREEN, LV_PART_INDICATOR);
    lv_obj_set_style_radius(ctrl_tq_bar, 5, LV_PART_INDICATOR);

    // Right control column: MODE dropdown clearance at top (pad_top), then THREE
    // groups distributed evenly by SPACE_BETWEEN — TARGET (title+slider), LIMIT
    // (title+slider), direction buttons. Grouping keeps each title just above its
    // own slider while the even inter-group gaps remove the dead space that a
    // single bottom spacer left above the buttons (user: spacing wasn't even and
    // the title sat too close to its slider).
    lv_obj_t *rcol = flat_cont(ctrl_level_layout);
    lv_obj_set_size(rcol, CTRL_RCOL_W, LV_PCT(100));
    lv_obj_set_layout(rcol, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(rcol, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(rcol, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);
    lv_obj_set_style_pad_top(rcol, 50, LV_PART_MAIN);
    lv_obj_add_flag(rcol, LV_OBJ_FLAG_OVERFLOW_VISIBLE);

    // --- TARGET group (title row + its slider, kept together) ---
    lv_obj_t *g_target = flat_cont(rcol);
    lv_obj_set_width(g_target, LV_PCT(100));
    lv_obj_set_height(g_target, LV_SIZE_CONTENT);
    lv_obj_set_layout(g_target, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(g_target, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_row(g_target, 14, LV_PART_MAIN); // breathing room title -> slider
    lv_obj_add_flag(g_target, LV_OBJ_FLAG_OVERFLOW_VISIBLE);

    lv_obj_t *trow = flex_row(g_target);
    lv_obj_set_size(trow, LV_PCT(100), LV_SIZE_CONTENT);
    lv_obj_set_flex_align(trow, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_END, LV_FLEX_ALIGN_END);
    ctrl_lvl_title = lv_label_create(trow); // left-aligned title above the slider
    lv_label_set_text_static(ctrl_lvl_title, "TARGET");
    lv_obj_set_style_text_font(ctrl_lvl_title, &ui_font_mono20, LV_PART_MAIN);
    lv_obj_set_style_text_color(ctrl_lvl_title, COLOR_TEXT_L, LV_PART_MAIN);
    lv_obj_set_style_text_letter_space(ctrl_lvl_title, 2, LV_PART_MAIN);
    lv_obj_t *vrow = flex_row(trow); // right-aligned value + unit above the slider
    lv_obj_set_size(vrow, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
    lv_obj_set_style_pad_column(vrow, 6, LV_PART_MAIN);
    lv_obj_set_flex_align(vrow, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_END, LV_FLEX_ALIGN_END);
    ctrl_lvl_num = lv_label_create(vrow);
    label_bind_buffer(ctrl_lvl_num, "0");
    lv_obj_set_style_text_font(ctrl_lvl_num, &ui_font_mono22, LV_PART_MAIN);
    lv_obj_set_style_text_color(ctrl_lvl_num, COLOR_TEXT_H, LV_PART_MAIN);
    ctrl_lvl_unit = lv_label_create(vrow);
    lv_label_set_text_static(ctrl_lvl_unit, "RPM");
    lv_obj_set_style_text_font(ctrl_lvl_unit, &ui_font_mono20, LV_PART_MAIN);
    lv_obj_set_style_text_color(ctrl_lvl_unit, COLOR_TEXT_VL, LV_PART_MAIN);

    ctrl_lvl_slider = lv_slider_create(g_target);
    lv_obj_set_width(ctrl_lvl_slider, LV_PCT(100));
    lv_obj_set_height(ctrl_lvl_slider, 12);
    lv_slider_set_range(ctrl_lvl_slider, 0, UI_MAX_RPM);
    lv_slider_set_value(ctrl_lvl_slider, 0, LV_ANIM_OFF);
    style_slider(ctrl_lvl_slider);
    lv_obj_add_event_cb(ctrl_lvl_slider, ctrl_setpoint_cb, LV_EVENT_VALUE_CHANGED, NULL);

    // --- LIMIT group (title left + value/unit right, then its slider) — the
    // secondary safety limit. Title/unit/range reconfigured per mode by the tick
    // (SPEED/OPEN LOOP → current "A", TORQUE → speed "RPM"). ---
    lv_obj_t *g_limit = flat_cont(rcol);
    lv_obj_set_width(g_limit, LV_PCT(100));
    lv_obj_set_height(g_limit, LV_SIZE_CONTENT);
    lv_obj_set_layout(g_limit, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(g_limit, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_row(g_limit, 14, LV_PART_MAIN);
    lv_obj_add_flag(g_limit, LV_OBJ_FLAG_OVERFLOW_VISIBLE);

    lv_obj_t *lrow = flex_row(g_limit);
    lv_obj_set_size(lrow, LV_PCT(100), LV_SIZE_CONTENT);
    lv_obj_set_flex_align(lrow, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_END, LV_FLEX_ALIGN_END);
    ctrl_lim_title = lv_label_create(lrow);
    lv_label_set_text_static(ctrl_lim_title, "CURRENT LIM");
    lv_obj_set_style_text_font(ctrl_lim_title, &ui_font_mono20, LV_PART_MAIN);
    lv_obj_set_style_text_color(ctrl_lim_title, COLOR_TEXT_L, LV_PART_MAIN);
    lv_obj_set_style_text_letter_space(ctrl_lim_title, 2, LV_PART_MAIN);
    lv_obj_t *lvrow = flex_row(lrow);
    lv_obj_set_size(lvrow, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
    lv_obj_set_style_pad_column(lvrow, 6, LV_PART_MAIN);
    lv_obj_set_flex_align(lvrow, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_END, LV_FLEX_ALIGN_END);
    ctrl_lim_num = lv_label_create(lvrow);
    label_bind_buffer(ctrl_lim_num, "0");
    lv_obj_set_style_text_font(ctrl_lim_num, &ui_font_mono22, LV_PART_MAIN);
    lv_obj_set_style_text_color(ctrl_lim_num, COLOR_TEXT_H, LV_PART_MAIN);
    ctrl_lim_unit = lv_label_create(lvrow);
    lv_label_set_text_static(ctrl_lim_unit, "A");
    lv_obj_set_style_text_font(ctrl_lim_unit, &ui_font_mono20, LV_PART_MAIN);
    lv_obj_set_style_text_color(ctrl_lim_unit, COLOR_TEXT_VL, LV_PART_MAIN);

    ctrl_lim_slider = lv_slider_create(g_limit);
    lv_obj_set_width(ctrl_lim_slider, LV_PCT(100));
    lv_obj_set_height(ctrl_lim_slider, 12);
    lv_slider_set_range(ctrl_lim_slider, 0, UI_MAX_CURRENT_MA);
    lv_slider_set_value(ctrl_lim_slider, 0, LV_ANIM_OFF);
    style_slider(ctrl_lim_slider);
    lv_obj_add_event_cb(ctrl_lim_slider, ctrl_limit_cb, LV_EVENT_VALUE_CHANGED, NULL);

    // --- Direction buttons: SPACE_BETWEEN drops this group to the column bottom. ---
    lv_obj_t *drow = flex_row(rcol);
    lv_obj_set_size(drow, LV_PCT(100), LV_SIZE_CONTENT);
    lv_obj_set_style_pad_column(drow, 10, LV_PART_MAIN);
    ctrl_btn_fwd = create_dir_icon_btn(drow, &ui_icon_cw, &ctrl_lbl_fwd);
    lv_obj_add_event_cb(ctrl_btn_fwd, action_motor_dir_fwd, LV_EVENT_CLICKED, NULL);
    ctrl_btn_rev = create_dir_icon_btn(drow, &ui_icon_ccw, &ctrl_lbl_rev);
    lv_obj_add_event_cb(ctrl_btn_rev, action_motor_dir_rev, LV_EVENT_CLICKED, NULL);

    // ---- POSITION layout: big gray ring + dot (LEFT) + speed-limit column
    // (RIGHT). Mirrors the LEVEL layout's gauge-left / controls-right split so
    // the two modes feel consistent — which also shifts the ring left of centre
    // (user request) and makes room for the new limit control. ----
    ctrl_pos_layout = flat_cont(card);
    lv_obj_set_size(ctrl_pos_layout, LV_PCT(100), LV_PCT(100));
    lv_obj_set_layout(ctrl_pos_layout, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(ctrl_pos_layout, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(ctrl_pos_layout, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_column(ctrl_pos_layout, 20, LV_PART_MAIN);
    lv_obj_add_flag(ctrl_pos_layout, LV_OBJ_FLAG_OVERFLOW_VISIBLE);
    lv_obj_add_flag(ctrl_pos_layout, LV_OBJ_FLAG_HIDDEN); // POSITION not the boot default

    // Left: ring area (takes the freed space, centres the ring).
    lv_obj_t *parea = flat_cont(ctrl_pos_layout);
    ctrl_pos_area = parea;
    lv_obj_set_flex_grow(parea, 1);
    lv_obj_set_height(parea, LV_PCT(100));
    lv_obj_add_flag(parea, LV_OBJ_FLAG_OVERFLOW_VISIBLE);

    ctrl_pos_ring = lv_arc_create(parea);
    lv_obj_set_size(ctrl_pos_ring, 236, 236);
    lv_obj_align(ctrl_pos_ring, LV_ALIGN_CENTER, 0, 0);
    lv_arc_set_rotation(ctrl_pos_ring, 270);        // value 0 at the top
    lv_arc_set_bg_angles(ctrl_pos_ring, 0, 360);    // full gray ring
    lv_arc_set_range(ctrl_pos_ring, 0, UI_POSITION_MAX_RAW);
    lv_arc_set_value(ctrl_pos_ring, 0);
    lv_obj_set_style_arc_color(ctrl_pos_ring, COLOR_BORDER, LV_PART_MAIN); // gray ring
    lv_obj_set_style_arc_width(ctrl_pos_ring, 18, LV_PART_MAIN);
    lv_obj_set_style_arc_opa(ctrl_pos_ring, LV_OPA_TRANSP, LV_PART_INDICATOR); // no value fill
    lv_obj_remove_style(ctrl_pos_ring, NULL, LV_PART_KNOB);                    // no on-stroke knob
    // DISPLAY-ONLY: we do NOT use lv_arc's built-in drag — for a gapless
    // full-circle arc its seam heuristic snaps the value to min/max near the
    // top (the erratic jump). Touch is handled on parea via
    // ctrl_pos_angle_to_raw() (ctrl_pos_math.h), which is continuous.
    lv_obj_clear_flag(ctrl_pos_ring, LV_OBJ_FLAG_CLICKABLE);

    // Dot INSIDE the ring near the inner rim (created after the ring → on top;
    // non-clickable → drags pass through to parea). Charcoal accent = an
    // interactive handle (matches the slider knobs), not an azure data value.
    ctrl_pos_dot = flat_cont(parea);
    lv_obj_set_size(ctrl_pos_dot, 26, 26);
    lv_obj_set_style_radius(ctrl_pos_dot, LV_RADIUS_CIRCLE, LV_PART_MAIN);
    lv_obj_set_style_bg_color(ctrl_pos_dot, COLOR_ACCENT, LV_PART_MAIN);
    lv_obj_set_style_bg_opa(ctrl_pos_dot, LV_OPA_COVER, LV_PART_MAIN);

    // Target-angle readout in the ring centre: big number + "°" unit. Fixed-
    // width, centre-aligned row so the digits stay centred as the value changes.
    lv_obj_t *pnumrow = flex_row(parea);
    lv_obj_set_size(pnumrow, 200, LV_SIZE_CONTENT);
    lv_obj_align(pnumrow, LV_ALIGN_CENTER, 0, 0);
    lv_obj_set_style_pad_column(pnumrow, 2, LV_PART_MAIN);
    lv_obj_set_flex_align(pnumrow, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_END, LV_FLEX_ALIGN_END);
    lv_obj_clear_flag(pnumrow, LV_OBJ_FLAG_CLICKABLE);
    ctrl_pos_num = lv_label_create(pnumrow);
    label_bind_buffer(ctrl_pos_num, "0");
    lv_obj_set_style_text_font(ctrl_pos_num, &ui_font_mono44, LV_PART_MAIN);
    lv_obj_set_style_text_color(ctrl_pos_num, COLOR_TEXT_H, LV_PART_MAIN);
    lv_obj_t *pdeg = lv_label_create(pnumrow);
    lv_label_set_text_static(pdeg, "\xC2\xB0");
    lv_obj_set_style_text_font(pdeg, &ui_font_mono20, LV_PART_MAIN);
    lv_obj_set_style_text_color(pdeg, COLOR_TEXT_VL, LV_PART_MAIN);
    ctrl_pos_update_dot();
    ctrl_pos_update_readout();

    // The ring area is the touch target (parea, not the whole layout — so the
    // limit slider in the right column isn't treated as a ring drag). Press or
    // drag anywhere in it sets the angle.
    lv_obj_add_flag(parea, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(parea, ctrl_pos_drag_cb, LV_EVENT_PRESSED, NULL);
    lv_obj_add_event_cb(parea, ctrl_pos_drag_cb, LV_EVENT_PRESSING, NULL);

    // Right: SPEED LIM column (same width/pad_top as the LEVEL right column).
    lv_obj_t *pcol = flat_cont(ctrl_pos_layout);
    lv_obj_set_size(pcol, CTRL_RCOL_W, LV_PCT(100));
    lv_obj_set_layout(pcol, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(pcol, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(pcol, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);
    lv_obj_set_style_pad_row(pcol, 11, LV_PART_MAIN);
    lv_obj_set_style_pad_top(pcol, 50, LV_PART_MAIN);
    lv_obj_add_flag(pcol, LV_OBJ_FLAG_OVERFLOW_VISIBLE);

    lv_obj_t *plrow = flex_row(pcol);
    lv_obj_set_size(plrow, LV_PCT(100), LV_SIZE_CONTENT);
    lv_obj_set_flex_align(plrow, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_END, LV_FLEX_ALIGN_END);
    lv_obj_t *plt = lv_label_create(plrow);
    lv_label_set_text_static(plt, "SPEED LIM");
    lv_obj_set_style_text_font(plt, &ui_font_mono20, LV_PART_MAIN);
    lv_obj_set_style_text_color(plt, COLOR_TEXT_L, LV_PART_MAIN);
    lv_obj_set_style_text_letter_space(plt, 2, LV_PART_MAIN);
    lv_obj_t *plvrow = flex_row(plrow);
    lv_obj_set_size(plvrow, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
    lv_obj_set_style_pad_column(plvrow, 6, LV_PART_MAIN);
    lv_obj_set_flex_align(plvrow, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_END, LV_FLEX_ALIGN_END);
    ctrl_pos_lim_num = lv_label_create(plvrow);
    label_bind_buffer(ctrl_pos_lim_num, "0");
    lv_obj_set_style_text_font(ctrl_pos_lim_num, &ui_font_mono22, LV_PART_MAIN);
    lv_obj_set_style_text_color(ctrl_pos_lim_num, COLOR_TEXT_H, LV_PART_MAIN);
    lv_obj_t *plu = lv_label_create(plvrow);
    lv_label_set_text_static(plu, "RPM");
    lv_obj_set_style_text_font(plu, &ui_font_mono20, LV_PART_MAIN);
    lv_obj_set_style_text_color(plu, COLOR_TEXT_VL, LV_PART_MAIN);

    ctrl_pos_lim_slider = lv_slider_create(pcol);
    lv_obj_set_width(ctrl_pos_lim_slider, LV_PCT(100));
    lv_obj_set_height(ctrl_pos_lim_slider, 12);
    lv_slider_set_range(ctrl_pos_lim_slider, 0, UI_MAX_RPM);
    lv_slider_set_value(ctrl_pos_lim_slider, 0, LV_ANIM_OFF);
    style_slider(ctrl_pos_lim_slider);
    lv_obj_add_event_cb(ctrl_pos_lim_slider, ctrl_pos_limit_cb, LV_EVENT_VALUE_CHANGED, NULL);

    // ---- Mode dropdown, pinned to the card's top-right corner. dd_wrap spans
    // CTRL_RCOL_W (same as rcol) with the label at its left edge → "MODE" lines
    // up with TARGET / slider / FWD-REV below, and the dropdown sits at the
    // right edge like the TARGET value does. ----
    lv_obj_t *dd_wrap = flex_row(card);
    lv_obj_set_size(dd_wrap, CTRL_RCOL_W, LV_SIZE_CONTENT);
    lv_obj_set_flex_align(dd_wrap, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_align(dd_wrap, LV_ALIGN_TOP_RIGHT, 0, 0);
    lv_obj_t *ddlab = lv_label_create(dd_wrap);
    lv_label_set_text_static(ddlab, "MODE");
    lv_obj_set_style_text_font(ddlab, &ui_font_mono20, LV_PART_MAIN);
    lv_obj_set_style_text_color(ddlab, COLOR_TEXT_L, LV_PART_MAIN);
    lv_obj_set_style_text_letter_space(ddlab, 2, LV_PART_MAIN);

    ctrl_mode_dd = lv_dropdown_create(dd_wrap);
    lv_obj_set_width(ctrl_mode_dd, 200);
    lv_obj_set_height(ctrl_mode_dd, 44);
    lv_dropdown_set_options_static(ctrl_mode_dd, "SPEED\nTORQUE\nOPEN LOOP\nPOSITION");
    lv_dropdown_set_selected(ctrl_mode_dd, 0); // SPEED (visual index 0)
    lv_obj_set_style_bg_color(ctrl_mode_dd, COLOR_CARD_BG, LV_PART_MAIN);
    lv_obj_set_style_border_color(ctrl_mode_dd, COLOR_BORDER, LV_PART_MAIN);
    lv_obj_set_style_border_width(ctrl_mode_dd, 1, LV_PART_MAIN);
    lv_obj_set_style_radius(ctrl_mode_dd, 10, LV_PART_MAIN);
    lv_dropdown_set_symbol(ctrl_mode_dd, &ui_icon_chevron); // avoid the Montserrat LV_SYMBOL_DOWN tofu trap
    lv_obj_set_style_img_recolor(ctrl_mode_dd, COLOR_TEXT_M, LV_PART_INDICATOR);
    lv_obj_set_style_img_recolor_opa(ctrl_mode_dd, LV_OPA_COVER, LV_PART_INDICATOR);
    lv_obj_set_style_text_font(ctrl_mode_dd, &ui_font_mono20, LV_PART_MAIN);
    lv_obj_set_style_text_color(ctrl_mode_dd, COLOR_TEXT_M, LV_PART_MAIN);
    lv_obj_set_style_pad_ver(ctrl_mode_dd, 8, LV_PART_MAIN);
    lv_obj_add_event_cb(ctrl_mode_dd, ctrl_mode_dd_cb, LV_EVENT_VALUE_CHANGED, NULL);
    lv_obj_t *ddlist = lv_dropdown_get_list(ctrl_mode_dd);
    lv_obj_set_style_bg_color(ddlist, COLOR_CARD_BG, LV_PART_MAIN);
    lv_obj_set_style_border_color(ddlist, COLOR_BORDER, LV_PART_MAIN);
    lv_obj_set_style_text_font(ddlist, &ui_font_mono20, LV_PART_MAIN);
    lv_obj_set_style_text_color(ddlist, COLOR_TEXT_M, LV_PART_MAIN);
    lv_obj_set_style_bg_color(ddlist, COLOR_ACCENT_BG, LV_PART_SELECTED | LV_STATE_CHECKED);
    lv_obj_set_style_text_color(ddlist, COLOR_ACCENT, LV_PART_SELECTED | LV_STATE_CHECKED);

    // ---- Action bar — START / STOP / CALIBRATE, equal width ----
    lv_obj_t *actions = flex_row(root);
    lv_obj_set_size(actions, LV_PCT(100), 54);
    lv_obj_set_style_pad_column(actions, 10, LV_PART_MAIN);

    // Resting look = stopped (START armed green, STOP idle red-outline); the
    // tick flips emphasis on the running-state transition. All three are
    // create_action_btn (flex_grow 1) → equal width.
    ctrl_btn_start = create_action_btn(actions, "START", &ctrl_lbl_start);
    lv_obj_set_style_bg_color(ctrl_btn_start, COLOR_OK, LV_PART_MAIN);
    lv_obj_set_style_border_color(ctrl_btn_start, COLOR_OK, LV_PART_MAIN);
    lv_obj_set_style_text_color(ctrl_lbl_start, lv_color_white(), LV_PART_MAIN);
    lv_obj_add_event_cb(ctrl_btn_start, action_motor_start, LV_EVENT_CLICKED, NULL);
    ctrl_btn_stop = create_action_btn(actions, "STOP", &ctrl_lbl_stop);
    lv_obj_set_style_bg_color(ctrl_btn_stop, COLOR_CARD_BG, LV_PART_MAIN);
    lv_obj_set_style_border_color(ctrl_btn_stop, COLOR_DANGER, LV_PART_MAIN);
    lv_obj_set_style_text_color(ctrl_lbl_stop, COLOR_DANGER, LV_PART_MAIN);
    lv_obj_add_event_cb(ctrl_btn_stop, action_motor_stop, LV_EVENT_CLICKED, NULL);

    lv_obj_t *lbl_cal;
    ctrl_btn_cal = create_action_btn(actions, "CALIBRATE", &lbl_cal);
    lv_obj_set_style_bg_color(ctrl_btn_cal, COLOR_CARD_BG, LV_PART_MAIN);
    lv_obj_set_style_border_color(ctrl_btn_cal, COLOR_ACCENT, LV_PART_MAIN);
    lv_obj_set_style_text_color(lbl_cal, COLOR_ACCENT, LV_PART_MAIN);
    lv_obj_add_event_cb(ctrl_btn_cal, action_calibrate, LV_EVENT_CLICKED, NULL);
}

// =============================================================================
// GRAPHS
// =============================================================================

static lv_obj_t *chart_rpm, *chart_current, *chart_voltage, *chart_temp;
static lv_obj_t *graph_val_rpm, *graph_val_current, *graph_val_voltage, *graph_val_temp;

// Rolling history for the 4 series (0=rpm,1=current A,2=voltage V,3=temp degC),
// collected EVERY chart tick regardless of the active tab so the Graphs screen
// is pre-drawn on entry instead of starting empty (user request). Values are
// the integer the chart plots (same as lv_chart_set_next_value gets). Size ==
// the chart point count (create_chart_widget: 50). New sample appended; when
// full, the oldest is dropped (shift). ~50*4*2 = 400 B.
#define GRAPH_PTS 50
static int16_t g_hist[4][GRAPH_PTS];
static uint8_t g_hist_n[4]; // valid samples per series (0..GRAPH_PTS)

static void graph_hist_push(uint8_t s, int16_t v)
{
    if (g_hist_n[s] < GRAPH_PTS)
        g_hist[s][g_hist_n[s]++] = v;
    else
    {
        memmove(&g_hist[s][0], &g_hist[s][1], (GRAPH_PTS - 1) * sizeof(int16_t));
        g_hist[s][GRAPH_PTS - 1] = v;
    }
}

// Replay a series' history into its (freshly built) chart so it shows on entry.
static void graph_prefill(lv_obj_t *chart, uint8_t s)
{
    if (!chart) return;
    lv_chart_series_t *ser = lv_chart_get_series_next(chart, NULL);
    for (uint8_t i = 0; i < g_hist_n[s]; i++) lv_chart_set_next_value(chart, ser, g_hist[s][i]);
}

// Redesigned from 4 stacked charts to a 2x2 grid (fits the new 20px+ type
// scale without shrinking each chart to a sliver), each with a Y-axis
// min/max readout hugging the card's left edge — user request: show the
// scale at a glance, and keep the axis column narrow so the line itself
// gets the width.
static void create_chart_widget(lv_obj_t *parent, const char *title_text, lv_color_t line_color, int32_t min,
                                int32_t max, const char *min_text, const char *max_text,
                                lv_obj_t **out_chart, lv_obj_t **out_val)
{
    lv_obj_t *cont = lv_obj_create(parent);
    lv_obj_set_size(cont, LV_PCT(100), LV_PCT(100)); // cross-axis fill in either a row or column parent
    lv_obj_set_flex_grow(cont, 1);                   // main-axis grow in either a row or column parent
    lv_obj_set_style_bg_color(cont, COLOR_CARD_BG, LV_PART_MAIN); // white chart card on the gray page
    lv_obj_set_style_border_color(cont, COLOR_BORDER, LV_PART_MAIN);
    lv_obj_set_style_border_width(cont, 1, LV_PART_MAIN);
    lv_obj_set_style_radius(cont, 10, LV_PART_MAIN);
    lv_obj_set_style_pad_hor(cont, 10, LV_PART_MAIN);
    lv_obj_set_style_pad_ver(cont, 6, LV_PART_MAIN);
    lv_obj_set_style_pad_row(cont, 2, LV_PART_MAIN);
    lv_obj_clear_flag(cont, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_layout(cont, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(cont, LV_FLEX_FLOW_COLUMN);

    lv_obj_t *head = flex_row(cont);
    lv_obj_set_size(head, LV_PCT(100), LV_SIZE_CONTENT);
    lv_obj_set_flex_align(head, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    lv_obj_t *title = lv_label_create(head);
    lv_label_set_text_static(title, title_text);
    lv_obj_set_style_text_font(title, &ui_font_mono20, LV_PART_MAIN);
    lv_obj_set_style_text_color(title, COLOR_TEXT_L, LV_PART_MAIN);
    lv_obj_set_style_text_letter_space(title, 2, LV_PART_MAIN);

    lv_obj_t *val = lv_label_create(head);
    label_bind_buffer(val, "-- --");
    lv_obj_set_style_text_font(val, &ui_font_mono22, LV_PART_MAIN);
    lv_obj_set_style_text_color(val, line_color, LV_PART_MAIN);

    // Body: a narrow min/max label column hugging the left edge, then the
    // chart taking the rest of the width — lv_chart_set_axis_tick() was
    // tried first, but its "draw_size" only grows the widget's EXTERNAL
    // draw area, not its layout box, so the tick labels rendered past the
    // card's edge and were clipped by the parent's default overflow:hidden.
    // Plain sibling labels avoid that entirely.
    lv_obj_t *body = flex_row(cont);
    lv_obj_set_size(body, LV_PCT(100), LV_SIZE_CONTENT);
    lv_obj_set_flex_grow(body, 1);
    lv_obj_set_style_pad_column(body, 6, LV_PART_MAIN);

    lv_obj_t *axis = flat_cont(body);
    lv_obj_set_size(axis, 40, LV_PCT(100));
    lv_obj_set_layout(axis, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(axis, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(axis, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);
    lv_obj_t *axis_max = lv_label_create(axis);
    lv_label_set_text_static(axis_max, max_text);
    lv_obj_set_style_text_font(axis_max, &ui_font_mono20, LV_PART_MAIN);
    lv_obj_set_style_text_color(axis_max, COLOR_TEXT_VL, LV_PART_MAIN);
    lv_obj_t *axis_min = lv_label_create(axis);
    lv_label_set_text_static(axis_min, min_text);
    lv_obj_set_style_text_font(axis_min, &ui_font_mono20, LV_PART_MAIN);
    lv_obj_set_style_text_color(axis_min, COLOR_TEXT_VL, LV_PART_MAIN);

    lv_obj_t *chart = lv_chart_create(body);
    lv_obj_set_height(chart, LV_PCT(100));
    lv_obj_set_flex_grow(chart, 1);
    lv_obj_set_style_bg_opa(chart, 0, LV_PART_MAIN);
    lv_obj_set_style_border_width(chart, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_all(chart, 2, LV_PART_MAIN);
    lv_obj_set_style_line_color(chart, COLOR_BORDER, LV_PART_MAIN); // division lines
    lv_obj_set_style_line_width(chart, 1, LV_PART_MAIN);
    lv_obj_set_style_line_width(chart, 2, LV_PART_ITEMS); // series line
    lv_chart_set_div_line_count(chart, 3, 7);
    lv_chart_set_type(chart, LV_CHART_TYPE_LINE);
    lv_chart_set_range(chart, LV_CHART_AXIS_PRIMARY_Y, min, max);
    lv_chart_set_point_count(chart, GRAPH_PTS);
    lv_chart_set_update_mode(chart, LV_CHART_UPDATE_MODE_SHIFT);
    lv_obj_set_style_size(chart, 0, LV_PART_INDICATOR); // hide points
    lv_chart_add_series(chart, line_color, LV_CHART_AXIS_PRIMARY_Y);

    if (out_chart) *out_chart = chart;
    if (out_val) *out_val = val;
}

void create_screen_graphs(lv_obj_t *parent)
{
    init_screen_bg(parent);

    lv_obj_t *main_cont = flat_cont(parent);
    lv_obj_set_size(main_cont, 800, 364);
    lv_obj_set_pos(main_cont, 0, 0);
    lv_obj_set_style_pad_all(main_cont, 16, LV_PART_MAIN);
    lv_obj_set_style_pad_row(main_cont, 14, LV_PART_MAIN);
    lv_obj_set_layout(main_cont, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(main_cont, LV_FLEX_FLOW_COLUMN);

    lv_obj_t *row1 = flex_row(main_cont);
    lv_obj_set_size(row1, LV_PCT(100), 50);
    lv_obj_set_flex_grow(row1, 1);
    lv_obj_set_style_pad_column(row1, 14, LV_PART_MAIN);
    // Peak scales match the real motor ceilings: speed = UI_MAX_RPM (400),
    // current = 30 A, voltage = 12-56 V, temp = 0-100 °C (user-specified).
    // Each trend gets its own hue so the four read apart at a glance (user
    // request): RPM azure (the hero series), CURRENT gold/yellow, VOLTAGE green,
    // TEMP warm orange.
    create_chart_widget(row1, "SPEED", COLOR_GAUGE_GREEN, 0, UI_MAX_RPM, "0", "400",
                        &chart_rpm, &graph_val_rpm);
    create_chart_widget(row1, "CURRENT", lv_color_hex(0xA87A00), 0, 30, "0", "30",
                        &chart_current, &graph_val_current);

    lv_obj_t *row2 = flex_row(main_cont);
    lv_obj_set_size(row2, LV_PCT(100), 50);
    lv_obj_set_flex_grow(row2, 1);
    lv_obj_set_style_pad_column(row2, 14, LV_PART_MAIN);
    create_chart_widget(row2, "VOLTAGE", COLOR_GAUGE, 0, 80, "0", "80",
                        &chart_voltage, &graph_val_voltage);
    create_chart_widget(row2, "MOTOR TEMP", COLOR_WARN, 0, 100, "0", "100",
                        &chart_temp, &graph_val_temp);

    // Pre-draw from the rolling history so the charts aren't empty on entry.
    graph_prefill(chart_rpm, 0);
    graph_prefill(chart_current, 1);
    graph_prefill(chart_voltage, 2);
    graph_prefill(chart_temp, 3);

    // Seed the value labels too so they don't flash "-- --" until the round-robin catches up.
    char vb[24], vn[16];
    snprintf(vb, sizeof(vb), "%d rpm", (int) ui_motor_status_fast()->bits.actualRpm);
    label_set_if_changed(graph_val_rpm, vb);
    snprintf(vb, sizeof(vb), "%s A",
             fmt_scaled(vn, sizeof(vn), (int32_t) ui_motor_status_fast()->bits.current, 1000, 1));
    label_set_if_changed(graph_val_current, vb);
    snprintf(vb, sizeof(vb), "%s V",
             fmt_scaled(vn, sizeof(vn), (int32_t) ui_motor_status_fast()->bits.voltage, 1000, 2));
    label_set_if_changed(graph_val_voltage, vb);
    snprintf(vb, sizeof(vb), "%s \xC2\xB0" "C",
             fmt_scaled(vn, sizeof(vn), MotorStatusSlow_GetMtTemp(ui_motor_status_slow()), 10, 1));
    label_set_if_changed(graph_val_temp, vb);
}

// =============================================================================
// DIAGNOSTICS
// =============================================================================

#define FAULT_COUNT 7
static lv_obj_t *diag_fault_marks[FAULT_COUNT]; // right-side status glyph (check / x), per fault bit
static lv_obj_t *diag_fault_names[FAULT_COUNT];
static lv_obj_t *diag_banner;      // summary rollup strip at the top
static lv_obj_t *diag_banner_icon; // check / x
static lv_obj_t *diag_banner_text; // "ALL SYSTEMS NORMAL" / "FAULT DETECTED"
static lv_obj_t *diag_banner_ratio; // "N / 7 OK"
static uint8_t   diag_last_faults;  // low 7 bits mirror MotorStatusFast_t.fault

// Left type-icon per fault bit (0..6), custom alpha images matching the mockup:
// electrical faults share the bolt, over-temp the thermometer, then the two
// sensors + the link each get their own shape.
static const lv_img_dsc_t * const FAULT_ICONS[FAULT_COUNT] = {
        &ui_icon_bolt, &ui_icon_bolt,       &ui_icon_bolt, &ui_icon_thermo,
        &ui_icon_crosshairs, &ui_icon_encoder, &ui_icon_link,
};
static const char * const FAULT_NAMES[FAULT_COUNT] = {
        "OVERCURRENT", "OVERVOLTAGE", "UNDERVOLTAGE", "OVER TEMP", "HALL SENSOR", "ENCODER", "RS-485 LINK",
};

static void blink_anim_cb(void *obj, int32_t v)
{
    lv_obj_set_style_opa((lv_obj_t *) obj, (lv_opa_t) v, LV_PART_MAIN);
}

static void start_blink(lv_obj_t *obj)
{
    lv_anim_t a;
    lv_anim_init(&a);
    lv_anim_set_var(&a, obj);
    lv_anim_set_exec_cb(&a, blink_anim_cb);
    lv_anim_set_values(&a, LV_OPA_COVER, LV_OPA_30);
    lv_anim_set_time(&a, 450);
    lv_anim_set_playback_time(&a, 450);
    lv_anim_set_repeat_count(&a, LV_ANIM_REPEAT_INFINITE);
    lv_anim_start(&a);
}

static void stop_blink(lv_obj_t *obj)
{
    lv_anim_del(obj, blink_anim_cb);
    lv_obj_set_style_opa(obj, LV_OPA_COVER, LV_PART_MAIN);
}

// One fault cell: [type icon] [name, grows] [status mark]. Laid out 3-across
// in a wrapping row (see create_screen_diagnostics), row-major like the mockup.
// 3-column CSS-grid-style layout descriptors (row-major, like the mockup).
// GRID_CONTENT rows keep each row exactly as tall as its cells — flex
// ROW_WRAP was tried first but distributed the wrap tracks with big vertical
// gaps, pushing the last row + Clear button off the 480px frame.
static lv_coord_t diag_col_dsc[] = {LV_GRID_FR(1), LV_GRID_FR(1), LV_GRID_FR(1), LV_GRID_TEMPLATE_LAST};
static lv_coord_t diag_row_dsc[] = {LV_GRID_CONTENT, LV_GRID_CONTENT, LV_GRID_CONTENT, LV_GRID_TEMPLATE_LAST};

static void diag_cell(lv_obj_t *parent, int idx)
{
    lv_obj_t *cell = flex_row(parent);
    lv_obj_set_height(cell, LV_SIZE_CONTENT); // else it keeps lv_obj's default height and the row balloons
    lv_obj_set_grid_cell(cell, LV_GRID_ALIGN_STRETCH, idx % 3, 1, LV_GRID_ALIGN_CENTER, idx / 3, 1);
    lv_obj_set_style_pad_ver(cell, 9, LV_PART_MAIN);
    lv_obj_set_style_pad_column(cell, 8, LV_PART_MAIN);
    lv_obj_set_style_border_side(cell, LV_BORDER_SIDE_BOTTOM, LV_PART_MAIN);
    lv_obj_set_style_border_color(cell, COLOR_BORDER, LV_PART_MAIN);
    lv_obj_set_style_border_width(cell, 1, LV_PART_MAIN);

    lv_obj_t *ic = lv_img_create(cell);
    lv_img_set_src(ic, FAULT_ICONS[idx]);
    lv_obj_set_style_img_recolor(ic, COLOR_TEXT_VL, LV_PART_MAIN);
    lv_obj_set_style_img_recolor_opa(ic, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_clear_flag(ic, LV_OBJ_FLAG_CLICKABLE);

    lv_obj_t *nm = lv_label_create(cell);
    lv_label_set_text_static(nm, FAULT_NAMES[idx]);
    lv_obj_set_style_text_font(nm, &ui_font_mono20, LV_PART_MAIN);
    lv_obj_set_style_text_color(nm, COLOR_TEXT_L, LV_PART_MAIN);
    lv_obj_set_flex_grow(nm, 1);

    // Status mark: check (OK, green) / x (fault, red) — swapped + recolored in
    // the tick. Alpha image so one recolor tints it, like the mockup's bare ✓.
    lv_obj_t *mk = lv_img_create(cell);
    lv_img_set_src(mk, &ui_icon_check);
    lv_obj_set_style_img_recolor(mk, COLOR_OK, LV_PART_MAIN);
    lv_obj_set_style_img_recolor_opa(mk, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_clear_flag(mk, LV_OBJ_FLAG_CLICKABLE);

    diag_fault_names[idx] = nm;
    diag_fault_marks[idx] = mk;
}

void create_screen_diagnostics(lv_obj_t *parent)
{
    init_screen_bg(parent);
    diag_last_faults = 0xFF;

    lv_obj_t *root = flat_cont(parent);
    lv_obj_set_size(root, 800, 364);
    lv_obj_set_pos(root, 0, 0);
    lv_obj_set_style_pad_hor(root, 20, LV_PART_MAIN);
    lv_obj_set_style_pad_ver(root, 16, LV_PART_MAIN);
    lv_obj_set_style_pad_row(root, 14, LV_PART_MAIN);
    lv_obj_set_layout(root, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(root, LV_FLEX_FLOW_COLUMN);

    // ---- Summary banner (health rollup, read before the per-bit detail) ----
    diag_banner = flex_row(root);
    lv_obj_set_size(diag_banner, LV_PCT(100), LV_SIZE_CONTENT);
    lv_obj_set_style_bg_opa(diag_banner, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_border_width(diag_banner, 1, LV_PART_MAIN);
    lv_obj_set_style_radius(diag_banner, 10, LV_PART_MAIN);
    lv_obj_set_style_pad_hor(diag_banner, 18, LV_PART_MAIN);
    lv_obj_set_style_pad_ver(diag_banner, 11, LV_PART_MAIN);
    lv_obj_set_style_pad_column(diag_banner, 12, LV_PART_MAIN);

    diag_banner_icon = lv_img_create(diag_banner);
    lv_img_set_src(diag_banner_icon, &ui_icon_check);
    lv_obj_set_style_img_recolor(diag_banner_icon, COLOR_OK, LV_PART_MAIN);
    lv_obj_set_style_img_recolor_opa(diag_banner_icon, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_clear_flag(diag_banner_icon, LV_OBJ_FLAG_CLICKABLE);

    diag_banner_text = lv_label_create(diag_banner);
    lv_label_set_text_static(diag_banner_text, "ALL SYSTEMS NORMAL");
    lv_obj_set_style_text_font(diag_banner_text, &ui_font_mono22, LV_PART_MAIN);
    lv_obj_set_style_text_letter_space(diag_banner_text, 1, LV_PART_MAIN);

    lv_obj_t *bspacer = flat_cont(diag_banner);
    lv_obj_set_height(bspacer, 1);
    lv_obj_set_flex_grow(bspacer, 1);

    diag_banner_ratio = lv_label_create(diag_banner);
    label_bind_buffer(diag_banner_ratio, "7 / 7 OK");
    lv_obj_set_style_text_font(diag_banner_ratio, &ui_font_mono20, LV_PART_MAIN);

    // ---- Fault register card: title + 3-column wrapping grid + clear btn ----
    lv_obj_t *card = lv_obj_create(root);
    lv_obj_set_width(card, LV_PCT(100));
    lv_obj_set_flex_grow(card, 1);
    lv_obj_set_style_bg_color(card, COLOR_CARD_BG, LV_PART_MAIN);
    lv_obj_set_style_border_color(card, COLOR_BORDER, LV_PART_MAIN);
    lv_obj_set_style_border_width(card, 1, LV_PART_MAIN);
    lv_obj_set_style_radius(card, 10, LV_PART_MAIN);
    lv_obj_set_style_pad_hor(card, 18, LV_PART_MAIN);
    lv_obj_set_style_pad_ver(card, 12, LV_PART_MAIN);
    lv_obj_set_style_pad_row(card, 8, LV_PART_MAIN);
    lv_obj_clear_flag(card, LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_layout(card, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(card, LV_FLEX_FLOW_COLUMN);

    lv_obj_t *title = lv_label_create(card);
    lv_label_set_text_static(title, "FAULT REGISTER");
    lv_obj_set_style_text_font(title, &ui_font_mono20, LV_PART_MAIN);
    lv_obj_set_style_text_color(title, COLOR_TEXT_VL, LV_PART_MAIN);
    lv_obj_set_style_text_letter_space(title, 1, LV_PART_MAIN);

    lv_obj_t *grid = flat_cont(card);
    lv_obj_set_width(grid, LV_PCT(100));
    lv_obj_set_height(grid, LV_SIZE_CONTENT);
    lv_obj_set_style_pad_column(grid, 20, LV_PART_MAIN);
    lv_obj_set_style_pad_row(grid, 0, LV_PART_MAIN);
    lv_obj_set_grid_dsc_array(grid, diag_col_dsc, diag_row_dsc);
    lv_obj_set_layout(grid, LV_LAYOUT_GRID);
    for (int i = 0; i < FAULT_COUNT; i++) diag_cell(grid, i);

    lv_obj_t *cspacer = flat_cont(card);
    lv_obj_set_width(cspacer, LV_PCT(100));
    lv_obj_set_flex_grow(cspacer, 1);

    // Secondary outlined button (mockup .btn): white bg, hairline border, ink
    // text — reads as a distinct clickable, not the flat gray panel it was.
    lv_obj_t *btn_clear = lv_btn_create(card);
    lv_obj_set_size(btn_clear, LV_PCT(100), 46);
    lv_obj_set_style_bg_color(btn_clear, COLOR_CARD_BG, LV_PART_MAIN);
    lv_obj_set_style_border_color(btn_clear, COLOR_BORDER, LV_PART_MAIN);
    lv_obj_set_style_border_width(btn_clear, 1, LV_PART_MAIN);
    lv_obj_set_style_radius(btn_clear, 10, LV_PART_MAIN);
    lv_obj_set_style_shadow_width(btn_clear, 0, LV_PART_MAIN);
    lv_obj_add_event_cb(btn_clear, action_clear_faults, LV_EVENT_CLICKED, NULL);
    lv_obj_t *lbl_clear = lv_label_create(btn_clear);
    lv_label_set_text_static(lbl_clear, "CLEAR FAULTS");
    lv_obj_set_style_text_font(lbl_clear, &ui_font_mono22, LV_PART_MAIN);
    lv_obj_set_style_text_color(lbl_clear, COLOR_TEXT_M, LV_PART_MAIN);
    lv_obj_set_style_text_letter_space(lbl_clear, 2, LV_PART_MAIN);
    lv_obj_center(lbl_clear);
}

// =============================================================================
// SETTINGS
// =============================================================================

// One settings row: label on the left, a control/value on the right, with a
// hairline border-bottom — the mockup's .frow list style. Returns the row; the
// caller adds the right-hand widget.
static lv_obj_t *set_frow(lv_obj_t *parent, const char *name)
{
    lv_obj_t *row = flex_row(parent);
    styles_ensure();
    lv_obj_add_style(row, &st_listrow, LV_PART_MAIN); // shared bottom divider
    lv_obj_set_size(row, LV_PCT(100), LV_SIZE_CONTENT);
    lv_obj_set_flex_align(row, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_ver(row, 10, LV_PART_MAIN);

    lv_obj_t *lbl = lv_label_create(row);
    lv_label_set_text_static(lbl, name);
    lv_obj_set_style_text_font(lbl, &ui_font_mono20, LV_PART_MAIN);
    lv_obj_set_style_text_color(lbl, COLOR_TEXT_L, LV_PART_MAIN);
    return row;
}

static lv_obj_t *create_dd_row(lv_obj_t *parent, const char *name, const char *options, uint16_t selected)
{
    lv_obj_t *row = set_frow(parent, name);

    lv_obj_t *dd = lv_dropdown_create(row);
    lv_obj_set_width(dd, 180); // fixed width, right-aligned (mockup field-select)
    lv_obj_set_height(dd, 44);
    lv_dropdown_set_options_static(dd, options);
    lv_dropdown_set_selected(dd, selected);
    lv_obj_set_style_bg_color(dd, COLOR_CARD_BG, LV_PART_MAIN); // white field on the gray page
    lv_obj_set_style_border_color(dd, COLOR_BORDER, LV_PART_MAIN);
    lv_obj_set_style_border_width(dd, 1, LV_PART_MAIN);
    lv_obj_set_style_radius(dd, 10, LV_PART_MAIN);
    // Mono font for the value text. The default down-arrow glyph is
    // LV_SYMBOL_DOWN, which only exists in Montserrat — so instead of keeping
    // the widget on Montserrat, swap the symbol for our own chevron IMAGE
    // (drawn in the INDICATOR part, recolored like any icon). That frees the
    // widget font to be mono without the arrow turning into a tofu box.
    lv_dropdown_set_symbol(dd, &ui_icon_chevron);
    lv_obj_set_style_img_recolor(dd, COLOR_TEXT_M, LV_PART_INDICATOR);
    lv_obj_set_style_img_recolor_opa(dd, LV_OPA_COVER, LV_PART_INDICATOR);
    lv_obj_set_style_text_font(dd, &ui_font_mono20, LV_PART_MAIN);
    lv_obj_set_style_text_color(dd, COLOR_TEXT_M, LV_PART_MAIN);
    lv_obj_set_style_pad_ver(dd, 8, LV_PART_MAIN);

    lv_obj_t *list = lv_dropdown_get_list(dd);
    lv_obj_set_style_bg_color(list, COLOR_CARD_BG, LV_PART_MAIN);
    lv_obj_set_style_border_color(list, COLOR_BORDER, LV_PART_MAIN);
    lv_obj_set_style_text_font(list, &ui_font_mono20, LV_PART_MAIN);
    lv_obj_set_style_text_color(list, COLOR_TEXT_M, LV_PART_MAIN);
    // Tinted selection (not solid fill+white text) — matches every other
    // selection state in the app (mode select, direction, tab active) and
    // Apple's own list-selection convention (a light accent tint, not an
    // inverted fill, which Apple reserves for primary/destructive buttons).
    lv_obj_set_style_bg_color(list, COLOR_ACCENT_BG, LV_PART_SELECTED | LV_STATE_CHECKED);
    lv_obj_set_style_text_color(list, COLOR_ACCENT, LV_PART_SELECTED | LV_STATE_CHECKED);
    return dd;
}

// Read-only motor-parameter row: value (bold ink), right-aligned,
// plain text (no input box) — the mockup's .fstatic look.
static void settings_param_row(lv_obj_t *parent, const char *name, const char *value)
{
    lv_obj_t *row = set_frow(parent, name);

    lv_obj_t *vr = flex_row(row);
    lv_obj_set_size(vr, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
    lv_obj_set_style_pad_column(vr, 6, LV_PART_MAIN);
    lv_obj_set_flex_align(vr, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_END, LV_FLEX_ALIGN_END);

    lv_obj_t *val = lv_label_create(vr);
    lv_label_set_text_static(val, value);
    lv_obj_set_style_text_font(val, &ui_font_mono22, LV_PART_MAIN);
    lv_obj_set_style_text_color(val, COLOR_TEXT_H, LV_PART_MAIN);
}

// RS-485 link config — the ACTUAL values (not widget indices), exposed via
// screens.h so main.c can configure the UART peripheral. Defaults: 921600 8N1.
uint32_t ui_rs485_baud     = 921600;
uint8_t  ui_rs485_parity   = 0; // None
uint8_t  ui_rs485_stopbits = 0; // 1 bit

// Baud dropdown index → actual baud. Order MUST match the BAUD RATE options
// string below. main.c reads ui_rs485_baud, never the index.
static const uint32_t RS485_BAUDS[] = {9600, 19200, 38400, 57600, 115200, 230400, 460800, 921600};
#define RS485_BAUD_COUNT ((int) (sizeof(RS485_BAUDS) / sizeof(RS485_BAUDS[0])))

static uint16_t rs485_baud_to_index(uint32_t baud)
{
    for (int i = 0; i < RS485_BAUD_COUNT; i++)
        if (RS485_BAUDS[i] == baud) return (uint16_t) i;
    return RS485_BAUD_COUNT - 1; // default to the top (921600) if unrecognised
}

static void rs485_baud_cb(lv_event_t *e)
{
    uint16_t sel = lv_dropdown_get_selected(lv_event_get_target(e));
    if (sel < RS485_BAUD_COUNT) ui_rs485_baud = RS485_BAUDS[sel];
}
static void rs485_parity_cb(lv_event_t *e)
{
    ui_rs485_parity = (uint8_t) lv_dropdown_get_selected(lv_event_get_target(e));
}
static void rs485_stopbits_cb(lv_event_t *e)
{
    ui_rs485_stopbits = (uint8_t) lv_dropdown_get_selected(lv_event_get_target(e));
}

void create_screen_settings(lv_obj_t *parent)
{
    init_screen_bg(parent);

    // ---- Left: RS-485 configuration ----
    lv_obj_t *col_left = flat_cont(parent);
    lv_obj_set_size(col_left, 360, 364);
    lv_obj_set_pos(col_left, 0, 0);
    lv_obj_set_style_border_side(col_left, LV_BORDER_SIDE_RIGHT, LV_PART_MAIN);
    lv_obj_set_style_border_color(col_left, COLOR_BORDER, LV_PART_MAIN);
    lv_obj_set_style_border_width(col_left, 1, LV_PART_MAIN);
    lv_obj_set_style_pad_hor(col_left, 16, LV_PART_MAIN);
    lv_obj_set_style_pad_ver(col_left, 14, LV_PART_MAIN);
    lv_obj_set_style_pad_row(col_left, 2, LV_PART_MAIN); // frows carry their own divider; keep them near-contiguous
    lv_obj_set_layout(col_left, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(col_left, LV_FLEX_FLOW_COLUMN);

    create_section_label(col_left, "RS-485 CONFIGURATION");

    // INTERFACE: fixed hardware (not editable) — plain text value in a frow.
    lv_obj_t *iface_row = set_frow(col_left, "INTERFACE");
    lv_obj_t *iface_val = lv_label_create(iface_row);
    lv_label_set_text_static(iface_val, "RS-485");
    lv_obj_set_style_text_font(iface_val, &ui_font_mono22, LV_PART_MAIN);
    lv_obj_set_style_text_color(iface_val, COLOR_TEXT_H, LV_PART_MAIN);

    // Baud ceiling 921600. Each dropdown is initialised FROM the ui_rs485_*
    // globals (so a boot default set by main.c shows up) and writes back to
    // them on change — see screens.h. Order must match RS485_BAUDS / the codes.
    lv_obj_t *dd_baud = create_dd_row(col_left, "BAUD RATE",
                                      "9600\n19200\n38400\n57600\n115200\n230400\n460800\n921600",
                                      rs485_baud_to_index(ui_rs485_baud));
    lv_obj_add_event_cb(dd_baud, rs485_baud_cb, LV_EVENT_VALUE_CHANGED, NULL);
    lv_obj_t *dd_parity = create_dd_row(col_left, "PARITY", "None\nEven\nOdd", ui_rs485_parity);
    lv_obj_add_event_cb(dd_parity, rs485_parity_cb, LV_EVENT_VALUE_CHANGED, NULL);
    lv_obj_t *dd_stop = create_dd_row(col_left, "STOP BITS", "1\n1.5\n2", ui_rs485_stopbits);
    lv_obj_add_event_cb(dd_stop, rs485_stopbits_cb, LV_EVENT_VALUE_CHANGED, NULL);

    // LINK: a frow with a dot + CONNECTED (green), like the mockup — not a
    // separate boxed status strip. (The old "MODBUS / PROTOCOL" card of made-up
    // Protocol/Frame/Timeout/Retries text stays removed; the real framing is
    // motor_comm_protocol.h's SOF/ID/LEN/PAYLOAD/EOF, and none of it was live.)
    lv_obj_t *link_row = set_frow(col_left, "LINK");
    lv_obj_t *link_val = flex_row(link_row);
    lv_obj_set_size(link_val, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
    lv_obj_set_style_pad_column(link_val, 8, LV_PART_MAIN);
    lv_obj_t *sdot = flat_cont(link_val);
    lv_obj_set_size(sdot, 9, 9);
    lv_obj_set_style_radius(sdot, LV_RADIUS_CIRCLE, LV_PART_MAIN);
    lv_obj_set_style_bg_color(sdot, COLOR_OK, LV_PART_MAIN);
    lv_obj_set_style_bg_opa(sdot, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_t *stext = lv_label_create(link_val);
    lv_label_set_text_static(stext, "CONNECTED");
    lv_obj_set_style_text_font(stext, &ui_font_mono22, LV_PART_MAIN);
    lv_obj_set_style_text_color(stext, COLOR_OK, LV_PART_MAIN);

    // ---- Right: motor parameters ----
    lv_obj_t *col_right = flat_cont(parent);
    lv_obj_set_size(col_right, 440, 364);
    lv_obj_set_pos(col_right, 360, 0);
    lv_obj_set_style_pad_hor(col_right, 16, LV_PART_MAIN);
    lv_obj_set_style_pad_ver(col_right, 14, LV_PART_MAIN);
    lv_obj_set_style_pad_row(col_right, 2, LV_PART_MAIN);
    lv_obj_set_layout(col_right, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(col_right, LV_FLEX_FLOW_COLUMN);

    // Operating limits only — commissioning constants (Rs, Ld/Lq) are set from
    // the host tool, not by the machine operator. These 3 values are still
    // static placeholders (no config read/write message exists yet, see
    // UART_PROTOCOL.md); they mirror the finalized mockup. "MAX SPEED" was
    // dropped here to match that mockup — UI_MAX_RPM (400) is still the live
    // gauge/slider ceiling, this row was just a duplicate static readout.
    create_section_label(col_right, "MOTOR PARAMETERS");
    settings_param_row(col_right, "POLE PAIRS", "7");
    settings_param_row(col_right, "KV RATING", "85 RPM/V");
    settings_param_row(col_right, "MAX CURRENT", "12.0 A");

    lv_obj_t *rspacer = flat_cont(col_right);
    lv_obj_set_width(rspacer, LV_PCT(100));
    lv_obj_set_flex_grow(rspacer, 1);

    lv_obj_t *btn_row = flex_row(col_right);
    lv_obj_set_size(btn_row, LV_PCT(100), LV_SIZE_CONTENT);
    lv_obj_set_style_pad_column(btn_row, 8, LV_PART_MAIN);

    // Primary action: solid ink fill (vs. DEFAULTS' neutral outline below) —
    // gives the pair real hierarchy instead of two same-weight azure buttons.
    lv_obj_t *btn_save = lv_btn_create(btn_row);
    lv_obj_set_flex_grow(btn_save, 1);
    lv_obj_set_height(btn_save, 48);
    lv_obj_set_style_bg_color(btn_save, COLOR_ACCENT, LV_PART_MAIN);
    lv_obj_set_style_border_width(btn_save, 0, LV_PART_MAIN);
    lv_obj_set_style_radius(btn_save, 10, LV_PART_MAIN);
    lv_obj_set_style_shadow_width(btn_save, 0, LV_PART_MAIN);
    lv_obj_add_event_cb(btn_save, action_save_config, LV_EVENT_CLICKED, NULL);
    lv_obj_t *lbl_save = lv_label_create(btn_save);
    lv_label_set_text_static(lbl_save, "SAVE");
    lv_obj_set_style_text_font(lbl_save, &ui_font_mono22, LV_PART_MAIN);
    lv_obj_set_style_text_color(lbl_save, lv_color_white(), LV_PART_MAIN);
    lv_obj_set_style_text_letter_space(lbl_save, 1, LV_PART_MAIN);
    lv_obj_center(lbl_save);

    lv_obj_t *btn_def = lv_btn_create(btn_row);
    lv_obj_set_flex_grow(btn_def, 1);
    lv_obj_set_height(btn_def, 48);
    lv_obj_set_style_bg_color(btn_def, COLOR_CARD_BG, LV_PART_MAIN); // white outlined button on gray page
    lv_obj_set_style_border_color(btn_def, COLOR_BORDER, LV_PART_MAIN);
    lv_obj_set_style_border_width(btn_def, 1, LV_PART_MAIN);
    lv_obj_set_style_radius(btn_def, 10, LV_PART_MAIN);
    lv_obj_set_style_shadow_width(btn_def, 0, LV_PART_MAIN);
    lv_obj_add_event_cb(btn_def, action_load_defaults, LV_EVENT_CLICKED, NULL);
    lv_obj_t *lbl_def = lv_label_create(btn_def);
    lv_label_set_text_static(lbl_def, "RESET");
    lv_obj_set_style_text_font(lbl_def, &ui_font_mono22, LV_PART_MAIN);
    lv_obj_set_style_text_color(lbl_def, COLOR_TEXT_L, LV_PART_MAIN);
    lv_obj_set_style_text_letter_space(lbl_def, 1, LV_PART_MAIN);
    lv_obj_center(lbl_def);
}

// =============================================================================
// TICKS (monitor / control / graphs / diagnostics / settings)
// =============================================================================

// Join "<number> <unit>" into one label without the outer snprintf that used to
// run 8x/Monitor-tick. fmt_scaled() already produced `num` with an integer
// core (%ld, no float printf); this is just a bounded byte copy + a space, so
// the hot tick no longer re-enters snprintf's format machinery per value.
static void mon_set(lv_obj_t *lbl, const char *num, const char *unit)
{
    char   buf[24];
    size_t i = 0;
    while (*num && i < sizeof(buf) - 1) buf[i++] = *num++;
    if (i < sizeof(buf) - 1) buf[i++] = ' ';
    while (*unit && i < sizeof(buf) - 1) buf[i++] = *unit++;
    buf[i] = '\0';
    label_set_if_changed(lbl, buf);
}

void tick_screen_monitor(ui_lane_t lane)
{
    if (ui_current_tab != TAB_MONITOR || lane != UI_LANE_SLOW)
        return;
    char num[16];

    const MotorStatusFast_t *fast = ui_motor_status_fast();
    const MotorStatusSlow_t *slow = ui_motor_status_slow();
    int32_t current      = (int32_t) fast->bits.current;
    int32_t dcBusVoltage = (int32_t) fast->bits.voltage;
    int32_t efficiency   = (int32_t) slow->bits.efficiency;
    int32_t motorTemp    = MotorStatusSlow_GetMtTemp(slow);
    int32_t inverterTemp = MotorStatusSlow_GetInvTemp(slow);
    bool all = mon_force_all != 0;
    uint8_t phase = mon_update_phase++ & 1u;
    mon_force_all = 0;

    // Unit lives in the same label as the value (one row = one widget pair —
    // see mon_row()). Threshold feedback is the value's text color (temp_color
    // / efficiency bands) — no level bar. mon_set() concatenates without snprintf.
    // After the first full frame, cap each Monitor service to four labels. This
    // halves the xSPI dirty-region burst during START without changing the
    // underlying 10 Hz telemetry producer.
    if (all || phase == 0)
    {
        mon_set(mon_val_current, fmt_scaled(num, sizeof(num), current, 1000, 2), "A");
        mon_set(mon_val_dc, fmt_scaled(num, sizeof(num), dcBusVoltage, 1000, 1), "V");
        label_color_if_changed(mon_val_dc, dcBusVoltage < 12000 ? COLOR_DANGER : COLOR_TEXT_H);
        mon_set(mon_val_power, fmt_scaled(num, sizeof(num), (int32_t) motor_power_deciwatt(), 10, 1), "W");
        mon_set(mon_val_eff, fmt_scaled(num, sizeof(num), efficiency, 10, 1), "%");
        label_color_if_changed(mon_val_eff, efficiency > 850 ? COLOR_OK
                                            : efficiency > 700 ? COLOR_WARN
                                                               : COLOR_DANGER);
    }

    if (all || phase == 1)
    {
        mon_set(mon_val_motortemp, fmt_scaled(num, sizeof(num), motorTemp, 10, 1), "\xC2\xB0" "C");
        label_color_if_changed(mon_val_motortemp, temp_color(motorTemp, 600, 800));
        mon_set(mon_val_invtemp, fmt_scaled(num, sizeof(num), inverterTemp, 10, 1), "\xC2\xB0" "C");
        label_color_if_changed(mon_val_invtemp, temp_color(inverterTemp, 650, 850));
        mon_set(mon_val_speed, fmt_scaled(num, sizeof(num), (int32_t) fast->bits.actualRpm, 1, 0), "RPM");
        mon_set(mon_val_torque, fmt_scaled(num, sizeof(num), (int32_t) fast->bits.actualTorque, 10, 1), "N.m");
    }
}

void tick_screen_control(ui_lane_t lane)
{
    if (ui_current_tab != TAB_CONTROL)
        return;

    // FAST lane: nothing to smooth — the SPEED+TORQUE readout is plain text,
    // updated on the SLOW lane below (no arc/needle to glide anymore).
    if (lane == UI_LANE_FAST)
        return;
    if (lane != UI_LANE_SLOW)
        return;
    char buf[16];

    // Mode change: sync the MODE dropdown selection, swap the visible layout
    // (LEVEL vs POSITION), and reconfigure the ONE reused level panel (title /
    // unit / slider range / ctrl_lvl_mode). Reads the CONFIRMED opMode from
    // telemetry (waits for the echo), same as direction below / the Dashboard.
    uint8_t opMode = (uint8_t) ui_motor_status_fast()->bits.opMode;
    if (opMode != ctrl_last_mode && ctrl_mode_dd)
    {
        ctrl_last_mode = opMode;
        // Reflect the confirmed mode in the dropdown (visual order ≠ enum order).
        for (int i = 0; i < 4; i++)
            if (ctrl_mode_order[i] == opMode)
            {
                if (lv_dropdown_get_selected(ctrl_mode_dd) != i) lv_dropdown_set_selected(ctrl_mode_dd, i);
                break;
            }
        bool isPos = (opMode == OP_MODE_POSITION);
        if (isPos)
        {
            lv_obj_add_flag(ctrl_level_layout, LV_OBJ_FLAG_HIDDEN);
            lv_obj_clear_flag(ctrl_pos_layout, LV_OBJ_FLAG_HIDDEN);
        }
        else
        {
            lv_obj_clear_flag(ctrl_level_layout, LV_OBJ_FLAG_HIDDEN);
            lv_obj_add_flag(ctrl_pos_layout, LV_OBJ_FLAG_HIDDEN);
            // Reconfigure the single reused level panel for the active mode:
            // the TARGET row (title stays generic; unit + range change) AND the
            // LIMIT row (current limit for SPEED/OPEN LOOP, speed limit for TORQUE).
            ctrl_lvl_mode = opMode;
            const char *unit, *lim_title, *lim_unit;
            int32_t max, lim_max;
            switch (opMode)
            {
                case OP_MODE_TORQUE:
                    unit = "N.m";  max = UI_MAX_TORQUE_MNM; // raw mN.m; formatter inserts decimal point
                    lim_title = "SPEED LIM";   lim_unit = "RPM"; lim_max = UI_MAX_RPM;
                    break;
                case OP_MODE_OPEN_LOOP:
                    unit = "%";    max = 100;
                    lim_title = "CURRENT LIM"; lim_unit = "A";   lim_max = UI_MAX_CURRENT_MA;
                    break;
                default: // SPEED
                    unit = "RPM";  max = UI_MAX_RPM;
                    lim_title = "CURRENT LIM"; lim_unit = "A";   lim_max = UI_MAX_CURRENT_MA;
                    break;
            }
            label_set_static_if_changed(ctrl_lvl_unit, unit);
            lv_slider_set_range(ctrl_lvl_slider, 0, max);
            label_set_static_if_changed(ctrl_lim_title, lim_title);
            label_set_static_if_changed(ctrl_lim_unit, lim_unit);
            lv_slider_set_range(ctrl_lim_slider, 0, lim_max);
        }
    }

    // Active level value (NUMBER only; unit is the static sibling). The reused
    // panel represents ctrl_lvl_mode, so trust ctrlValRaw only when
    // motorCmd.opMode matches it — else show 0. This is the shared-field guard
    // (UART_PROTOCOL.md sec 2.1): all four opModes share ctrlValRaw, so a stale
    // value from another mode must never render here in this mode's unit.
    if (opMode != OP_MODE_POSITION && ctrl_lvl_num)
    {
        int32_t raw    = (motorCmd.bits.opMode == ctrl_lvl_mode) ? (int32_t) motorCmd.bits.ctrlValRaw : 0;
        // Slider works in the raw unit (RPM / mN.m) except OPEN_LOOP where the
        // slider is 0-100 % and the wire is 0.1 %/LSB (raw = %×10).
        int32_t sldval = (ctrl_lvl_mode == OP_MODE_OPEN_LOOP) ? raw / 10 : raw;
        if (ctrl_lvl_mode == OP_MODE_TORQUE)
            fmt_scaled(buf, sizeof(buf), raw, 1000, 2);
        else
            snprintf(buf, sizeof(buf), "%d", (int) sldval);
        label_set_if_changed(ctrl_lvl_num, buf);
        if (ctrl_lvl_slider && !lv_obj_has_state(ctrl_lvl_slider, LV_STATE_PRESSED) &&
            lv_slider_get_value(ctrl_lvl_slider) != sldval)
            lv_slider_set_value(ctrl_lvl_slider, sldval, LV_ANIM_OFF);

        // LIMIT value: TORQUE → speed limit (RPM, integer); SPEED/OPEN LOOP →
        // current limit (mA → A, 1 decimal). limitRaw stores it 1:1 with the slider.
        if (ctrl_lim_num)
        {
            int32_t lraw = (motorCmd.bits.opMode == ctrl_lvl_mode) ? (int32_t) motorCmd.bits.limitRaw : 0;
            if (ctrl_lvl_mode == OP_MODE_TORQUE)
                snprintf(buf, sizeof(buf), "%d", (int) lraw);
            else
                fmt_scaled(buf, sizeof(buf), lraw, 1000, 1);
            label_set_if_changed(ctrl_lim_num, buf);
            if (ctrl_lim_slider && !lv_obj_has_state(ctrl_lim_slider, LV_STATE_PRESSED) &&
                lv_slider_get_value(ctrl_lim_slider) != lraw)
                lv_slider_set_value(ctrl_lim_slider, lraw, LV_ANIM_OFF);
        }
    }

    // POSITION ring: sync to the target when not being dragged, then move the
    // azure dot to match (no number/label — just the ring + dot).
    if (opMode == OP_MODE_POSITION && ctrl_pos_ring)
    {
        int32_t deciDeg = (motorCmd.bits.opMode == OP_MODE_POSITION) ? (int32_t) motorCmd.bits.ctrlValRaw : 0;
        if ((!ctrl_pos_area || !lv_obj_has_state(ctrl_pos_area, LV_STATE_PRESSED)) &&
            lv_arc_get_value(ctrl_pos_ring) != deciDeg)
        {
            lv_arc_set_value(ctrl_pos_ring, deciDeg);
            ctrl_pos_update_dot();
            ctrl_pos_update_readout();
        }
        // Speed-limit value + slider (RPM, 1:1 with limitRaw).
        if (ctrl_pos_lim_num)
        {
            int32_t lraw = (motorCmd.bits.opMode == OP_MODE_POSITION) ? (int32_t) motorCmd.bits.limitRaw : 0;
            snprintf(buf, sizeof(buf), "%d", (int) lraw);
            label_set_if_changed(ctrl_pos_lim_num, buf);
            if (ctrl_pos_lim_slider && !lv_obj_has_state(ctrl_pos_lim_slider, LV_STATE_PRESSED) &&
                lv_slider_get_value(ctrl_pos_lim_slider) != lraw)
                lv_slider_set_value(ctrl_pos_lim_slider, lraw, LV_ANIM_OFF);
        }
    }

    // LEVEL readout: actual SPEED + TORQUE (two columns; no state/dir here —
    // run-state is on the START/STOP buttons, direction on the FWD/REV toggle).
    // Each column has a level bar: SPEED = %/UI_MAX_RPM, TORQUE = %/peak 11 N.m,
    // fill recolored by band (green / amber / red) only on a transition.
    if (ctrl_spd_num)
    {
        int32_t rpm = (int32_t) ui_motor_status_fast()->bits.actualRpm;
        snprintf(buf, sizeof(buf), "%d", (int) rpm);
        label_set_if_changed(ctrl_spd_num, buf);
        int32_t tq = (int32_t) motor_torque_decinm();
        label_set_if_changed(ctrl_tq_num, fmt_scaled(buf, sizeof(buf), tq, 10, 1));

        int spct = rpm * 100 / UI_MAX_RPM;
        if (spct > 100) spct = 100;
        lv_bar_set_value(ctrl_spd_bar, spct, LV_ANIM_OFF);
        uint8_t sb = (rpm < UI_MAX_RPM * 6 / 10) ? 0 : (rpm < UI_MAX_RPM * 85 / 100) ? 1 : 2;
        if (sb != ctrl_spd_band)
        {
            ctrl_spd_band = sb;
            lv_obj_set_style_bg_color(ctrl_spd_bar,
                sb == 0 ? COLOR_GAUGE_GREEN : sb == 1 ? COLOR_WARN : COLOR_DANGER, LV_PART_INDICATOR);
        }
        int tpct = (int) ((tq * 100 + 55) / 110);
        if (tpct > 100) tpct = 100;
        if (tpct < 0) tpct = 0;
        lv_bar_set_value(ctrl_tq_bar, tpct, LV_ANIM_OFF);
        uint8_t tb = (tq < 50) ? 0 : (tq < 94) ? 1 : 2;
        if (tb != ctrl_tq_band)
        {
            ctrl_tq_band = tb;
            lv_obj_set_style_bg_color(ctrl_tq_bar,
                tb == 0 ? COLOR_GAUGE_GREEN : tb == 1 ? COLOR_WARN : COLOR_DANGER, LV_PART_INDICATOR);
        }
    }

    // START / STOP emphasis flips with the confirmed run-state; CALIBRATE is
    // disabled while running (can't calibrate a spinning motor). Transition-
    // guarded so it only restyles on an actual start/stop, not every tick.
    uint8_t running = motor_is_running() ? 1 : 0;
    if (running != ctrl_last_running && ctrl_btn_start)
    {
        ctrl_last_running = running;
        // Armed button = solid fill; the other = outline. START green / STOP red.
        lv_obj_set_style_bg_color(ctrl_btn_start, running ? COLOR_CARD_BG : COLOR_OK, LV_PART_MAIN);
        lv_obj_set_style_border_color(ctrl_btn_start, COLOR_OK, LV_PART_MAIN);
        lv_obj_set_style_text_color(ctrl_lbl_start, running ? COLOR_OK : lv_color_white(), LV_PART_MAIN);
        lv_obj_set_style_bg_color(ctrl_btn_stop, running ? COLOR_DANGER : COLOR_CARD_BG, LV_PART_MAIN);
        lv_obj_set_style_border_color(ctrl_btn_stop, COLOR_DANGER, LV_PART_MAIN);
        lv_obj_set_style_text_color(ctrl_lbl_stop, running ? lv_color_white() : COLOR_DANGER, LV_PART_MAIN);
        if (ctrl_btn_cal)
        {
            if (running) lv_obj_add_state(ctrl_btn_cal, LV_STATE_DISABLED);
            else lv_obj_clear_state(ctrl_btn_cal, LV_STATE_DISABLED);
            lv_obj_set_style_opa(ctrl_btn_cal, running ? LV_OPA_50 : LV_OPA_COVER, LV_PART_MAIN);
        }
    }

    uint8_t direction = (uint8_t) ui_motor_status_fast()->bits.dir;
    if (direction != ctrl_last_dir && ctrl_btn_fwd)
    {
        ctrl_last_dir     = direction;
        lv_obj_t *on      = direction ? ctrl_btn_rev : ctrl_btn_fwd;
        lv_obj_t *off     = direction ? ctrl_btn_fwd : ctrl_btn_rev;
        lv_obj_t *on_ic   = direction ? ctrl_lbl_rev : ctrl_lbl_fwd;  // icon img now
        lv_obj_t *off_ic  = direction ? ctrl_lbl_fwd : ctrl_lbl_rev;
        // Active = solid accent + white icon (mockup btn-dir.active), same as Dashboard.
        lv_obj_set_style_bg_color(on, COLOR_ACCENT, LV_PART_MAIN);
        lv_obj_set_style_border_color(on, COLOR_ACCENT, LV_PART_MAIN);
        lv_obj_set_style_img_recolor(on_ic, lv_color_white(), LV_PART_MAIN);
        lv_obj_set_style_bg_color(off, COLOR_CARD_BG, LV_PART_MAIN);
        lv_obj_set_style_border_color(off, COLOR_BORDER, LV_PART_MAIN);
        lv_obj_set_style_img_recolor(off_ic, COLOR_TEXT_M, LV_PART_MAIN);
    }
}

void tick_screen_graphs(ui_lane_t lane)
{
    if (lane != UI_LANE_CHART) return; // collect history EVERY chart tick, even off-tab
    bool on = (ui_current_tab == TAB_GRAPHS);
    char buf[24], num[16];

    // Round-robin one series per CHART tick (125ms) => each sampled at 2Hz,
    // staggered so the xSPI bus never redraws two charts in one frame. The
    // sample is pushed to the rolling history ALWAYS (so Graphs is pre-drawn on
    // entry); the chart + value label are touched only when the Graphs screen
    // is actually built (`on`) — its chart_* pointers are dangling otherwise
    // (freed by the last lv_obj_clean).
    static uint8_t idx = 0;
    switch (idx++ & 3)
    {
        case 0:
        {
            int32_t rpm = (int32_t) ui_motor_status_fast()->bits.actualRpm;
            graph_hist_push(0, (int16_t) rpm);
            if (on && chart_rpm)
            {
                lv_chart_set_next_value(chart_rpm, lv_chart_get_series_next(chart_rpm, NULL), rpm);
                snprintf(buf, sizeof(buf), "%d rpm", (int) rpm);
                label_set_if_changed(graph_val_rpm, buf);
            }
            break;
        }
        case 1:
        {
            int32_t current_ma = (int32_t) ui_motor_status_fast()->bits.current;
            int32_t current_a = current_ma / 1000;
            graph_hist_push(1, (int16_t) current_a);
            if (on && chart_current)
            {
                lv_chart_set_next_value(chart_current, lv_chart_get_series_next(chart_current, NULL),
                                        current_a);
                snprintf(buf, sizeof(buf), "%s A", fmt_scaled(num, sizeof(num), current_ma, 1000, 1));
                label_set_if_changed(graph_val_current, buf);
            }
            break;
        }
        case 2:
        {
            int32_t voltage_mv = (int32_t) ui_motor_status_fast()->bits.voltage;
            int32_t voltage_v = voltage_mv / 1000;
            graph_hist_push(2, (int16_t) voltage_v);
            if (on && chart_voltage)
            {
                lv_chart_set_next_value(chart_voltage, lv_chart_get_series_next(chart_voltage, NULL),
                                        voltage_v);
                snprintf(buf, sizeof(buf), "%s V", fmt_scaled(num, sizeof(num), voltage_mv, 1000, 2));
                label_set_if_changed(graph_val_voltage, buf);
            }
            break;
        }
        case 3:
        {
            int32_t motor_temp_deci = MotorStatusSlow_GetMtTemp(ui_motor_status_slow());
            int32_t motor_temp_c = motor_temp_deci / 10;
            graph_hist_push(3, (int16_t) motor_temp_c);
            if (on && chart_temp)
            {
                lv_chart_set_next_value(chart_temp, lv_chart_get_series_next(chart_temp, NULL),
                                        motor_temp_c);
                snprintf(buf, sizeof(buf), "%s \xC2\xB0" "C",
                         fmt_scaled(num, sizeof(num), motor_temp_deci, 10, 1));
                label_set_if_changed(graph_val_temp, buf);
            }
            break;
        }
    }
}

void tick_screen_diagnostics(ui_lane_t lane)
{
    if (ui_current_tab != TAB_DIAGNOSTICS || lane != UI_LANE_SLOW)
        return;
    if (!diag_fault_marks[0]) return;

    uint8_t faults = (uint8_t) ui_motor_status_fast()->bits.fault & 0x7F;
    if (faults == diag_last_faults) return;

    uint8_t changed = faults ^ diag_last_faults;
    for (int i = 0; i < FAULT_COUNT; i++)
    {
        if (!(changed >> i & 1)) continue;
        bool active = (faults >> i) & 1;
        // Right-side status mark swaps check(green) <-> x(red) image + blinks on
        // fault; the name text goes to ink on fault so an active row reads heavier.
        lv_img_set_src(diag_fault_marks[i], active ? &ui_icon_xmark : &ui_icon_check);
        lv_obj_set_style_img_recolor(diag_fault_marks[i], active ? COLOR_DANGER : COLOR_OK, LV_PART_MAIN);
        lv_obj_set_style_text_color(diag_fault_names[i], active ? COLOR_TEXT_H : COLOR_TEXT_L, LV_PART_MAIN);
        if (active) start_blink(diag_fault_marks[i]);
        else stop_blink(diag_fault_marks[i]);
    }
    diag_last_faults = faults;

    // Summary banner rollup: green "ALL SYSTEMS NORMAL / 7 / 7 OK" when clean,
    // red "FAULT DETECTED / (7-N) / 7 OK" when any bit is set.
    uint8_t nfault = 0;
    for (int i = 0; i < FAULT_COUNT; i++) nfault += (faults >> i) & 1;
    bool       ok      = (nfault == 0);
    lv_color_t fg      = ok ? COLOR_OK : COLOR_DANGER;
    lv_color_t bg      = ok ? lv_color_hex(0xD6EEDD) : lv_color_hex(0xF6D8D8);
    lv_color_t bd      = ok ? lv_color_hex(0xB6DCC3) : lv_color_hex(0xE2B4B4);
    lv_obj_set_style_bg_color(diag_banner, bg, LV_PART_MAIN);
    lv_obj_set_style_border_color(diag_banner, bd, LV_PART_MAIN);
    lv_img_set_src(diag_banner_icon, ok ? &ui_icon_check : &ui_icon_xmark);
    lv_obj_set_style_img_recolor(diag_banner_icon, fg, LV_PART_MAIN);
    label_set_static_if_changed(diag_banner_text, ok ? "ALL SYSTEMS NORMAL" : "FAULT DETECTED");
    lv_obj_set_style_text_color(diag_banner_text, fg, LV_PART_MAIN);
    char ratio[16];
    snprintf(ratio, sizeof(ratio), "%d / 7 OK", (int) (FAULT_COUNT - nfault));
    label_set_if_changed(diag_banner_ratio, ratio);
    lv_obj_set_style_text_color(diag_banner_ratio, fg, LV_PART_MAIN);
}

void tick_screen_settings(ui_lane_t lane)
{
    if (ui_current_tab != TAB_SETTINGS || lane != UI_LANE_SLOW)
        return;
}
