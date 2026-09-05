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

// Session runtime is intentionally UI-local: it starts at the latest accepted
// START command and is naturally cleared by a device reboot. The 32-bit LVGL
// tick subtraction is wrap-safe for a single run shorter than the tick period
// (~49 days); the displayed hours are capped below to keep the label bounded.
static uint32_t s_run_start_tick;
static uint32_t s_run_frozen_ms;
static uint8_t  s_run_active;

void ui_runtime_on_start(void)
{
    s_run_start_tick = lv_tick_get();
    s_run_frozen_ms  = 0;
    s_run_active     = 1;
}

void ui_runtime_on_stop(void)
{
    if (!s_run_active) return;
    s_run_frozen_ms = lv_tick_get() - s_run_start_tick;
    s_run_active    = 0;
}

uint32_t ui_runtime_seconds(void)
{
    uint32_t elapsed_ms = s_run_active ? (lv_tick_get() - s_run_start_tick)
                                       : s_run_frozen_ms;
    return elapsed_ms / 1000u;
}

// Keep the timer correct if the motor starts/stops outside these UI buttons,
// and freeze it on a confirmed FAULT. A freshly issued START is not cancelled
// while telemetry still echoes STOPPED because motorCmd.cmd is already RUN.
static void ui_runtime_sync_state(uint8_t motor_state)
{
    static uint8_t previous_state = 0xFF;
    bool now_running = motor_state == MOTOR_STATE_STARTING ||
                       motor_state == MOTOR_STATE_RUNNING;
    bool was_running = previous_state == MOTOR_STATE_STARTING ||
                       previous_state == MOTOR_STATE_RUNNING;

    if (now_running && !was_running && !s_run_active) ui_runtime_on_start();
    if (s_run_active && (motor_state == MOTOR_STATE_FAULT ||
                         (motor_state == MOTOR_STATE_STOPPED && motorCmd.bits.cmd == 0)))
        ui_runtime_on_stop();
    previous_state = motor_state;
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
#define COLOR_GAUGE_GREEN lv_color_hex(0x0B8F45) // gauge "normal" band + speed hero — darkened from 0x14B85A
                                                 // after review: ~4.2:1 on white, survives RGB565 + TN panels
#define COLOR_OK lv_color_hex(0x188C34)        // green, +~12% saturation, keeps AA for bold text on white
#define COLOR_BRAND_RED lv_color_hex(0xC2272E) // Hyphen Deux logo mark red — brand color, not themed
#define COLOR_ACCENT_BG lv_color_hex(0xD5DBE6) // selected-row tint — a CLEAR cool gray (was near-white
                                                 // 0xE5E5EA) so a selection reads on white/cheap panels;
                                                 // still backed by a strong accent mark (border/radio/underline)
#define COLOR_ACCENT_DIM COLOR_TEXT_M           // dimmed text on selected items
#define COLOR_TEXT_DIM lv_color_hex(0x83878F)   // dimmest text tier (quiet captions) — darkened again for
                                                 // the cheap-panel pass so even this stays clearly legible
#define COLOR_PANEL_BG     lv_color_hex(0xDFE5EC) // top-bar neutral chip, darkened ~10-15 RGB levels
#define COLOR_PANEL_BORDER lv_color_hex(0xBEC8D4) // crisp separation from the white header band

// "QUANTUM SIGNAL — Embedded World Showcase" palette (Monitor + Control).
// Three tokens per metric: INK (title/icon), SIGNAL (underline/bar/dot),
// TINT (band/tile background, separate Monitor vs Control strength).
//
// WHY AN ENUM + TABLE (root-cause fix): lv_color_hex() quantizes to RGB565, so
// exact-comparing lv_color_to32() against RGB888 literals (e.g. 0x0057A8) never
// matches — every metric silently fell into a neutral fallback. Indexing a const
// table by metric id cannot quantize and cannot drift.
typedef enum {
    METRIC_SPEED,
    METRIC_IQ,
    METRIC_IRMS,
    METRIC_ENERGY,     // Voltage/Power — Plasma Magenta
    METRIC_DIRECTION,  // Ion Indigo
    METRIC_HEALTH,     // Faults/Healthy — Neon Diagnostic Green
    METRIC_RUNTIME     // Technical Steel
} metric_id_t;

typedef struct {
    lv_color_t ink;
    lv_color_t signal;
    lv_color_t monitor_tint;
    lv_color_t control_tint;
} metric_palette_t;

static const metric_palette_t metric_palette[] = {
    /* SPEED     */ { LV_COLOR_MAKE(0x07, 0x5F, 0xE8), LV_COLOR_MAKE(0x07, 0x5F, 0xE8),
                      LV_COLOR_MAKE(0xC9, 0xE8, 0xFF), LV_COLOR_MAKE(0x0D, 0x29, 0x46) },
    /* IQ        */ { LV_COLOR_MAKE(0x00, 0x70, 0x5E), LV_COLOR_MAKE(0x00, 0x70, 0x5E),
                      LV_COLOR_MAKE(0xC5, 0xEE, 0xE7), LV_COLOR_MAKE(0x0C, 0x30, 0x31) },
    /* IRMS      */ { LV_COLOR_MAKE(0x59, 0x1A, 0x8F), LV_COLOR_MAKE(0x59, 0x1A, 0x8F),
                      LV_COLOR_MAKE(0xE1, 0xD6, 0xFA), LV_COLOR_MAKE(0xE1, 0xD6, 0xFA) },
    /* ENERGY    */ { LV_COLOR_MAKE(0xB8, 0x45, 0x00), LV_COLOR_MAKE(0xB8, 0x45, 0x00),
                      LV_COLOR_MAKE(0xF5, 0xD2, 0xE5), LV_COLOR_MAKE(0xF5, 0xD2, 0xE5) },
    /* DIRECTION */ { LV_COLOR_MAKE(0x28, 0x35, 0x93), LV_COLOR_MAKE(0x28, 0x35, 0x93),
                      LV_COLOR_MAKE(0xD7, 0xDE, 0xFA), LV_COLOR_MAKE(0xD7, 0xDE, 0xFA) },
    /* HEALTH    */ { LV_COLOR_MAKE(0x06, 0x68, 0x39), LV_COLOR_MAKE(0x06, 0x68, 0x39),
                      LV_COLOR_MAKE(0xCC, 0xEB, 0xD7), LV_COLOR_MAKE(0xCC, 0xEB, 0xD7) },
    /* RUNTIME   */ { LV_COLOR_MAKE(0x3E, 0x4C, 0x59), LV_COLOR_MAKE(0x3E, 0x4C, 0x59),
                      LV_COLOR_MAKE(0xD9, 0xE3, 0xED), LV_COLOR_MAKE(0xD9, 0xE3, 0xED) },
};

#define COLOR_DECK_NAVY   lv_color_hex(0x132238) // Monitor header + deck instrument background
#define COLOR_DECK_VALUE  lv_color_hex(0xF7FAFF) // numeric values on dark deck tiles
#define COLOR_DECK_UNIT   lv_color_hex(0xB8C5D6) // units on dark deck tiles

#define COLOR_SPEED_SIGNAL lv_color_hex(0x075FE8) // Dashboard hero; unified with SPEED across all tabs
#define COLOR_CMD_PANEL    lv_color_hex(0xEDF2F7) // Control command surface restored from reviewed layout
#define COLOR_PLOT_BG      lv_color_hex(0xF8FAFC) // Graphs plot area background
#define COLOR_UNIT         lv_color_hex(0x59677A) // unit/helper text (Monitor + Control)
#define COLOR_BORDER_MUTED lv_color_hex(0xB6C3D2) // inactive control border
#define COLOR_NAVY_BTN     lv_color_hex(0x17324F) // CW/CCW active + slider knob dark

static uint8_t link_state_normalize(uint8_t state)
{
    return state <= UI_LINK_NO_RESPONSE ? state : UI_LINK_DISCONNECTED;
}

static lv_color_t link_state_color(uint8_t state)
{
    switch (link_state_normalize(state))
    {
        case UI_LINK_CONNECTED:   return COLOR_OK;
        case UI_LINK_NO_RESPONSE: return COLOR_WARN;
        default:                  return COLOR_DANGER;
    }
}

static const char *link_state_text(uint8_t state)
{
    switch (link_state_normalize(state))
    {
        case UI_LINK_CONNECTED:   return "CONNECTED";
        case UI_LINK_NO_RESPONSE: return "NO RESPONSE";
        default:                  return "DISCONNECTED";
    }
}


// --- Redraw-guard helpers ---------------------------------------------------
// Dynamic numeric labels use a small screen-local fixed-buffer pool. LVGL's
// normal lv_label_set_text() frees and allocates a new exact-sized string every
// time the number changes; Monitor used to do that for up to eight labels at
// 5 Hz, which fragmented the constrained heap during a demo. Only one screen exists
// at a time, so eight reusable buffers cover the worst case without per-tick
// allocation. Static captions point straight at const strings in Flash.
#define UI_TEXT_SLOTS 12u
#define UI_TEXT_CAP   24u
static char      s_screen_text[UI_TEXT_SLOTS][UI_TEXT_CAP];
static lv_obj_t *s_screen_text_owner[UI_TEXT_SLOTS];
static uint8_t   s_screen_text_used;
// Small overflow ring for rare exhaustion cases: keeps static lifetime
#define UI_TEXT_OVERFLOW 2u
static char      s_screen_text_overflow[UI_TEXT_OVERFLOW][UI_TEXT_CAP];
static lv_obj_t *s_screen_text_overflow_owner[UI_TEXT_OVERFLOW];
static uint8_t   s_screen_text_overflow_idx;

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
        // Pool exhausted: use small overflow ring with process-lifetime
        // buffers to avoid per-tick heap allocations. This is a rare fallback.
        LV_LOG_WARN("screen text pool exhausted, using overflow buffer");
        uint8_t o = s_screen_text_overflow_idx++ % UI_TEXT_OVERFLOW;
        text_copy(s_screen_text_overflow[o], UI_TEXT_CAP, initial);
        s_screen_text_overflow_owner[o] = lbl;
        lv_label_set_text_static(lbl, s_screen_text_overflow[o]);
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
    // Check overflow ring owners too
    for (uint8_t i = 0; i < UI_TEXT_OVERFLOW; i++)
    {
        if (s_screen_text_overflow_owner[i] != lbl) continue;
        text_copy(s_screen_text_overflow[i], UI_TEXT_CAP, text);
        lv_label_set_text_static(lbl, s_screen_text_overflow[i]);
        return;
    }
    // The incoming text is commonly a stack buffer, so set_text_static() here
    // would leave LVGL holding a dangling pointer after this function returns.
    // Do not allocate every tick either: report the programming error and keep
    // the label's last safe value.
    LV_LOG_WARN("dynamic label is not bound to the fixed text pool");
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


// --- Shared styles (hardening 'a') ------------------------------------------
// Screens are torn down + rebuilt on every tab switch (single-screen SPA to fit
// the 42 KB heap). Doing per-object `lv_obj_set_style_*` for the widgets created
// dozens of times per screen (sliders, buttons, list-row dividers) allocates a
// growable LOCAL style array on EACH object EACH rebuild — the alloc/free churn
// that fragments the heap under fast tab-flipping. These SHARED styles are
// initialised once and `lv_obj_add_style`'d instead: one small style-list entry
// per object instead of a local property array, and zero re-alloc across
// rebuilds (the styles are process-lifetime statics). Per-instance colors set
// by callers/ticks still win (local > added style). Only STATIC properties live
// here; anything a tick recolors (button bg/border/text) stays local.
static lv_style_t st_btn_geom, st_listrow, st_sld_main, st_sld_ind, st_sld_knob, st_card_shadow;
static lv_style_t st_instrument_frame;
static lv_style_t st_top_bar, st_pill, st_tab_bar;
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
    lv_style_set_bg_color(&st_sld_main, lv_color_hex(0xCED8E5)); // inactive track (slate)
    lv_style_init(&st_sld_ind);
    lv_style_set_bg_color(&st_sld_ind, COLOR_ACCENT);
    lv_style_init(&st_sld_knob);
    lv_style_set_bg_color(&st_sld_knob, COLOR_NAVY_BTN); // dark navy thumb
                                                         // white+ring knob read like a radio dot next
                                                         // to a line, not a draggable handle (review)
    lv_style_set_pad_all(&st_sld_knob, 8);
    lv_style_set_border_color(&st_sld_knob, lv_color_white());
    lv_style_set_border_width(&st_sld_knob, 2);

    // Floating-card shadow (operator request: soft flat shadow on surfaces).
    // LVGL shadows are a SOLID outline at one opacity — no gradient, so RGB565
    // cheap panels do not band. Shared style = ONE style-list entry per object
    // instead of 4 local style props allocated on every rebuild.
    lv_style_init(&st_card_shadow);
    lv_style_set_shadow_width(&st_card_shadow, 10);
    lv_style_set_shadow_ofs_y(&st_card_shadow, 3);
    lv_style_set_shadow_color(&st_card_shadow, lv_color_hex(0x0D0F14));
    lv_style_set_shadow_opa(&st_card_shadow, LV_OPA_20);

    // Nested white-body instruments need a visible grouping edge against the
    // white Control card. Border-only keeps the shared parent as the sole body
    // fill; border_post prevents the full-width header/bar children from
    // painting over the 1 px outline. One shared style avoids local-array churn.
    lv_style_init(&st_instrument_frame);
    lv_style_set_radius(&st_instrument_frame, 12);
    lv_style_set_clip_corner(&st_instrument_frame, true);
    lv_style_set_border_width(&st_instrument_frame, 1);
    lv_style_set_border_color(&st_instrument_frame, COLOR_BORDER_MUTED);
    lv_style_set_border_opa(&st_instrument_frame, LV_OPA_COVER);
    lv_style_set_border_post(&st_instrument_frame, true);

    // Top bar surface (or pre-rendered img recolor). Includes img_recolor
    // properties so the same style works for both lv_obj and lv_img paths.
    lv_style_init(&st_top_bar);
    lv_style_set_img_recolor(&st_top_bar, COLOR_CARD_BG);
    lv_style_set_img_recolor_opa(&st_top_bar, LV_OPA_COVER);
    lv_style_set_bg_color(&st_top_bar, COLOR_CARD_BG);
    lv_style_set_bg_opa(&st_top_bar, LV_OPA_COVER);
    lv_style_set_border_width(&st_top_bar, 0);
    lv_style_set_pad_all(&st_top_bar, 0);

    // Small pill/chip style used in the top-right link/status pill.
    lv_style_init(&st_pill);
    lv_style_set_bg_color(&st_pill, COLOR_PANEL_BG);
    lv_style_set_bg_opa(&st_pill, LV_OPA_COVER);
    lv_style_set_border_color(&st_pill, COLOR_PANEL_BORDER);
    lv_style_set_border_width(&st_pill, 1);
    lv_style_set_radius(&st_pill, LV_RADIUS_CIRCLE);
    lv_style_set_pad_hor(&st_pill, 10);
    lv_style_set_pad_ver(&st_pill, 5);
    lv_style_set_pad_column(&st_pill, 7);

    // Sidebar rail/tab bar style.
    lv_style_init(&st_tab_bar);
    lv_style_set_bg_color(&st_tab_bar, COLOR_CARD_BG);
    lv_style_set_bg_opa(&st_tab_bar, LV_OPA_COVER);
    lv_style_set_border_width(&st_tab_bar, 0);
    lv_style_set_radius(&st_tab_bar, 14);
    lv_style_set_pad_all(&st_tab_bar, 0);

    // Monitor row children — every mon_row() creates icon+name+value; these carry
    // the STATIC look so each object holds one style-list entry instead of a
    // local property array (less alloc/frag per rebuild). Per-row value color set
    // by the tick (label_color_if_changed) stays local and still wins.
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

// Floating-card look (operator request: no borders on surfaces + a soft shadow).
// Shadow properties live in the SHARED st_card_shadow style — one style-list
// entry per object, zero local style-array growth per rebuild (P0 refactor).
static void style_card_shadow(lv_obj_t *o)
{
    styles_ensure();
    lv_obj_add_style(o, &st_card_shadow, LV_PART_MAIN);
}

static void style_instrument_frame(lv_obj_t *o)
{
    styles_ensure();
    lv_obj_add_style(o, &st_instrument_frame, LV_PART_MAIN);
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
    lv_obj_set_style_text_font(lbl, &ui_font_sans20, LV_PART_MAIN);
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
    lv_obj_set_style_border_width(card, 0, LV_PART_MAIN);
    lv_obj_set_style_radius(card, 10, LV_PART_MAIN);
    style_card_shadow(card);
    lv_obj_set_style_pad_hor(card, 14, LV_PART_MAIN);
    lv_obj_set_style_pad_ver(card, 12, LV_PART_MAIN);
    lv_obj_set_style_pad_row(card, 12, LV_PART_MAIN); // values pushed down for balance
    lv_obj_clear_flag(card, LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_layout(card, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(card, LV_FLEX_FLOW_COLUMN);

    lv_obj_t *lbl = lv_label_create(card);
    lv_label_set_text_static(lbl, label_text);
    lv_obj_set_style_text_font(lbl, &ui_font_sans20, LV_PART_MAIN);
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
        lv_obj_set_style_text_color(unit, COLOR_UNIT, LV_PART_MAIN);
        lv_obj_set_style_pad_bottom(unit, 3, LV_PART_MAIN); // sit on the value's baseline
    }

    if (out_val) *out_val = val;
    return card;
}


// --- Common UI: top bar + left navigation (created once on the top layer) --

// The content screen is padded into this frame. Keeping the dimensions in one
// place prevents individual tabs from drifting back to the old top-tab layout.
// The left navigation rail spans the FULL screen height (UI_SCREEN_H) and the
// top bar only covers the content area right of it.
//
// SPACING SYSTEM (operator request: ONE width for every gap):
//   UI_GAP            = 12 px — every outer gap/margin: page margins, header
//                      band inset (top/left/right), rail-to-card, card gaps,
//                      row gaps and bottom margins.
//   UI_HEADER_BAND_H  = 64 px — the white header band, floating at UI_GAP from
//                      the screen top.
//   UI_HEADER_H       = band bottom (12+64) + one UI_GAP, because the cards
//                      add their own UI_GAP margin inside the content area —
//                      net visible gap below the band is exactly UI_GAP.
//   Borders stay uniformly 1 px (COLOR_BORDER).
// Shell geometry is declared in screens.h: ui.c also needs the exact content
// rectangle so tab teardown can invalidate only that region.

static lv_obj_t *top_lbl_state;
static lv_obj_t *top_state_chip; // tinted chip wrapping the MOTOR state text
static lv_obj_t *top_link_dot;
static lv_obj_t *top_pill;       // "RS-485" pill — the DOT carries the link state
static lv_obj_t *top_pill_lbl;   // "RS-485" (text fixed; state is the dot, operator)

// Current page name shown in the header band (the sidebar is icon-only, so this
// is the "where am I" read). Updated by ui_tabbar_set_active on every switch.
static lv_obj_t *top_page_lbl;
static const char * const PAGE_NAMES[6] = {
    "DASHBOARD", "MONITOR", "CONTROL", "GRAPHS", "DIAGNOSTICS", "SETTINGS"
};

static lv_obj_t *tab_bar;
static uint8_t   tab_active_idx = 0xFF;

// MOTOR state chip tints (bg + border) indexed by MotorState_e — same tint
// family language as the Diagnostics banner. Functions (not const arrays):
// lv_color_hex() is not a constant expression in LVGL v8, so the tables would
// not be compile-time initializable. Called only on state transitions.
static lv_color_t chip_bg(uint8_t state)
{
    switch (state)
    {
        case MOTOR_STATE_RUNNING:  return lv_color_hex(0xC5E5CF);
        case MOTOR_STATE_FAULT:    return lv_color_hex(0xF0C7C7);
        case MOTOR_STATE_STARTING:
        case MOTOR_STATE_STOPPING: return lv_color_hex(0xF4D9C5);
        default:                   return lv_color_hex(0xD8DEE6); // STOPPED
    }
}
static lv_color_t chip_bd(uint8_t state)
{
    switch (state)
    {
        case MOTOR_STATE_RUNNING:  return lv_color_hex(0x91C3A2);
        case MOTOR_STATE_FAULT:    return lv_color_hex(0xD99B9B);
        case MOTOR_STATE_STARTING:
        case MOTOR_STATE_STOPPING: return lv_color_hex(0xE2AE87);
        default:                   return lv_color_hex(0xB6C0CC);
    }
}

// Rail-local Y of the six 64px buttons: first pill top = UI_GAP, last pill
// bottom = UI_GAP; gaps alternate 13/12 px so the 464px rail divides exactly
// (6*64 + 5*~12.8 + 2*8). Flex SPACE_BETWEEN rounded the remainder onto the
// last gap, leaving an asymmetric 12px bottom margin — explicit positions fix it.
static const int16_t TAB_BTN_Y[6] = {8, 85, 162, 239, 316, 392};

static const lv_img_dsc_t * const TAB_ICONS[6] = {
    &ui_icon_dash, &ui_icon_mon, &ui_icon_ctrl,
    &ui_icon_graph, &ui_icon_diag, &ui_icon_set
};

// Draw six tab cells in one resident object. The old tree used one button, one
// pill and one image object per tab (18 children total). The geometry and alpha
// icon maps are already const Flash data, so a draw callback keeps the exact
// visual and hit regions without spending LVGL heap on those children. A dirty
// old/new cell still invokes this callback, therefore reject the other five
// cells against draw_ctx->clip_area before preparing/drawing their icons.
static void tabbar_draw_cb(lv_event_t *e)
{
    lv_obj_t *obj = lv_event_get_target(e);
    lv_draw_ctx_t *draw_ctx = lv_event_get_draw_ctx(e);
    lv_area_t rail;
    lv_obj_get_coords(obj, &rail);

    lv_draw_rect_dsc_t pill_dsc;
    lv_draw_rect_dsc_init(&pill_dsc);
    pill_dsc.bg_color = COLOR_ACCENT;
    pill_dsc.bg_opa = LV_OPA_COVER;
    pill_dsc.radius = 16;

    for (uint8_t i = 0; i < 6; i++)
    {
        lv_area_t pill = {
            .x1 = (lv_coord_t) (rail.x1 + UI_GAP),
            .y1 = (lv_coord_t) (rail.y1 + TAB_BTN_Y[i]),
            .x2 = (lv_coord_t) (rail.x1 + UI_GAP + 63),
            .y2 = (lv_coord_t) (rail.y1 + TAB_BTN_Y[i] + 63)
        };
        const lv_area_t *clip = draw_ctx->clip_area;
        if (pill.x2 < clip->x1 || pill.x1 > clip->x2 ||
            pill.y2 < clip->y1 || pill.y1 > clip->y2)
            continue;

        bool active = i == tab_active_idx;
        if (active) lv_draw_rect(draw_ctx, &pill_dsc, &pill);

        lv_area_t icon = {
            .x1 = (lv_coord_t) (pill.x1 + 10),
            .y1 = (lv_coord_t) (pill.y1 + 10),
            .x2 = (lv_coord_t) (pill.x1 + 53),
            .y2 = (lv_coord_t) (pill.y1 + 53)
        };
        lv_draw_img_dsc_t icon_dsc;
        lv_draw_img_dsc_init(&icon_dsc);
        icon_dsc.recolor = active ? lv_color_white() : COLOR_TEXT_L;
        icon_dsc.recolor_opa = LV_OPA_COVER;
        icon_dsc.opa = LV_OPA_COVER;
        lv_draw_img(draw_ctx, &icon_dsc, &icon, TAB_ICONS[i]);
    }
}

static void tabbar_click_cb(lv_event_t *e)
{
    lv_indev_t *indev = lv_indev_get_act();
    if (!indev) return;
    lv_point_t point;
    lv_indev_get_point(indev, &point);
    lv_area_t rail;
    lv_obj_get_coords(lv_event_get_target(e), &rail);
    lv_coord_t local_y = (lv_coord_t) (point.y - rail.y1);
    for (uint8_t i = 0; i < 6; i++)
    {
        if (local_y < TAB_BTN_Y[i] || local_y >= TAB_BTN_Y[i] + 64) continue;
        ui_request_tab((ui_tab_t) i);
        return;
    }
}

static void tabbar_invalidate_cell(uint8_t idx)
{
    if (!tab_bar || idx > TAB_SETTINGS) return;
    lv_area_t rail;
    lv_obj_get_coords(tab_bar, &rail);
    lv_area_t cell = {
        .x1 = (lv_coord_t) (rail.x1 + UI_GAP),
        .y1 = (lv_coord_t) (rail.y1 + TAB_BTN_Y[idx]),
        .x2 = (lv_coord_t) (rail.x1 + UI_GAP + 63),
        .y2 = (lv_coord_t) (rail.y1 + TAB_BTN_Y[idx] + 63)
    };
    lv_obj_invalidate_area(tab_bar, &cell);
}

// When the motor last SETTLED into STOPPED (0 = not currently stopped). Updated
// by tick_common_ui every SLOW tick; read by the demo→normal switch gate below.
// Declared outside the UI_DEMO_SIM guard because tick_common_ui (always compiled)
// maintains it.
static uint32_t s_motor_stopped_since;

#if defined(UI_DEMO_SIM) && UI_DEMO_SIM
// Hold the "MOTOR CONTROL HMI" brand ~800me → flip the runtime demo simulator
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
    // --- Header band: dark navy frameless surface, inset UI_GAP from the rail,
    // the screen edges and the screen top so the chrome floats with the same gap
    // on every side.
    //
    // POC pre-render switch (UI_TOPBAR_IMG_POC): when enabled the band itself is
    // a pre-rendered ALPHA_4BIT mask (ui_img_topbar, recolored to COLOR_CHROME)
    // instead of a plain lv_obj background. Dynamic children (page title, state
    // chip, RS-485 pill) remain LVGL objects on top of the image. Default OFF —
    // the placeholder mask is a full rect and both paths render identically.
    // TODO: replace the placeholder with real pre-rendered band art, then flip
    // the define.
#ifndef UI_TOPBAR_IMG_POC
#define UI_TOPBAR_IMG_POC 0
#endif

#if UI_TOPBAR_IMG_POC
    lv_obj_t *top_bar = lv_img_create(parent);
    lv_img_set_src(top_bar, &ui_img_topbar);
    styles_ensure();
    lv_obj_add_style(top_bar, &st_top_bar, LV_PART_MAIN);
#else
    lv_obj_t *top_bar = lv_obj_create(parent);
    styles_ensure();
    lv_obj_add_style(top_bar, &st_top_bar, LV_PART_MAIN);
#endif
    lv_obj_set_size(top_bar, UI_CONTENT_W - 2 * UI_GAP, UI_HEADER_BAND_H);
    lv_obj_set_pos(top_bar, UI_SIDEBAR_W + 2 * UI_GAP, UI_GAP);
    style_card_shadow(top_bar);
    lv_obj_clear_flag(top_bar, LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *left = flex_row(top_bar);
    lv_obj_set_size(left, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
    lv_obj_align(left, LV_ALIGN_LEFT_MID, UI_GAP, 0);
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
    lv_label_set_text_static(title, PAGE_NAMES[TAB_DASHBOARD]);
    top_page_lbl = title;
    lv_obj_set_style_text_font(title, &ui_font_sans22, LV_PART_MAIN);
    lv_obj_set_style_text_color(title, COLOR_TEXT_H, LV_PART_MAIN);
    lv_obj_set_style_text_letter_space(title, 2, LV_PART_MAIN);
    // The static "MOTOR CONTROL HMI" brand caption used to sit here. It said
    // the same thing on every page, so the space now carries the CURRENT PAGE
    // NAME instead — the sidebar is icon-only, and this is the orientation
    // read the operator needs. The logo bars left of it keep the brand.

    lv_obj_t *right = flex_row(top_bar);
    lv_obj_set_size(right, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
    lv_obj_align(right, LV_ALIGN_RIGHT_MID, -UI_GAP, 0);
    lv_obj_set_style_pad_column(right, 12, LV_PART_MAIN);

    // Motor state as a tinted chip (bg + border follow the state in
    // tick_common_ui). The old static "MOTOR" label was redundant — the chip
    // carries the state on its own.
    top_state_chip = flex_row(right);
    lv_obj_set_size(top_state_chip, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
    lv_obj_set_style_bg_color(top_state_chip, chip_bg(MOTOR_STATE_STOPPED), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(top_state_chip, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_border_color(top_state_chip, chip_bd(MOTOR_STATE_STOPPED), LV_PART_MAIN);
    lv_obj_set_style_border_width(top_state_chip, 1, LV_PART_MAIN);
    lv_obj_set_style_radius(top_state_chip, LV_RADIUS_CIRCLE, LV_PART_MAIN);
    lv_obj_set_style_pad_hor(top_state_chip, 12, LV_PART_MAIN);
    lv_obj_set_style_pad_ver(top_state_chip, 5, LV_PART_MAIN); // match RS-485 pill height exactly

    top_lbl_state = lv_label_create(top_state_chip);
    lv_label_set_text_static(top_lbl_state, "STOPPED");
    lv_obj_set_style_text_font(top_lbl_state, &ui_font_mono22, LV_PART_MAIN);
    lv_obj_set_style_text_color(top_lbl_state, COLOR_TEXT_L, LV_PART_MAIN);

    lv_obj_t *pill = flex_row(right);
    top_pill = pill;
    lv_obj_set_size(pill, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
    styles_ensure();
    lv_obj_add_style(pill, &st_pill, LV_PART_MAIN);

    top_link_dot = flat_cont(pill);
    lv_obj_set_size(top_link_dot, 8, 8);
    lv_obj_set_style_radius(top_link_dot, LV_RADIUS_CIRCLE, LV_PART_MAIN);
    lv_obj_set_style_bg_color(top_link_dot, COLOR_DANGER, LV_PART_MAIN);
    lv_obj_set_style_bg_opa(top_link_dot, LV_OPA_COVER, LV_PART_MAIN);

    top_pill_lbl = lv_label_create(pill);
    lv_label_set_text_static(top_pill_lbl, "RS-485");
    lv_obj_set_style_text_font(top_pill_lbl, &ui_font_mono22, LV_PART_MAIN);
    lv_obj_set_style_text_color(top_pill_lbl, COLOR_TEXT_L, LV_PART_MAIN);

    // --- Left navigation rail: WHITE floating panel — rounded on all FOUR
    // corners, inset UI_GAP from the top/left/bottom screen edges, with the
    // same flat shadow as the cards (operator: same style as the content). ---
    tab_bar = lv_obj_create(parent);
    lv_obj_set_size(tab_bar, UI_SIDEBAR_W, UI_SCREEN_H - 2 * UI_GAP);
    lv_obj_set_pos(tab_bar, UI_GAP, UI_GAP);
    styles_ensure();
    lv_obj_add_style(tab_bar, &st_tab_bar, LV_PART_MAIN);
    style_card_shadow(tab_bar);
    // One object owns drawing and hit mapping for all cells. First/last pills
    // remain exactly UI_GAP from the rail edges (see TAB_BTN_Y).
    lv_obj_clear_flag(tab_bar, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(tab_bar, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(tab_bar, tabbar_draw_cb, LV_EVENT_DRAW_MAIN, NULL);
    lv_obj_add_event_cb(tab_bar, tabbar_click_cb, LV_EVENT_CLICKED, NULL);

    tab_active_idx = 0xFF;
    ui_tabbar_set_active(TAB_DASHBOARD);
}

void ui_tabbar_set_active(ui_tab_t tab)
{
    uint8_t idx = (uint8_t) tab <= TAB_SETTINGS ? (uint8_t) tab : (uint8_t) TAB_DASHBOARD;
    if (idx == tab_active_idx) return;

    // Only the old and new pills change. The previous implementation rewrote
    // all three style properties on all six persistent nav items, invalidating
    // ~25k px of unchanged chrome on every content switch.
    if (tab_active_idx <= TAB_SETTINGS) tabbar_invalidate_cell(tab_active_idx);
    tab_active_idx = idx;
    tabbar_invalidate_cell(tab_active_idx);

    if (top_page_lbl)
        lv_label_set_text_static(top_page_lbl, PAGE_NAMES[idx]);
}

void tick_common_ui(void)
{
    static const char * const STATE_NAMES[5] = {"STOPPED", "STARTING", "RUNNING", "STOPPING", "FAULT"};
    static uint8_t     last_state     = 0xFF;
    static uint8_t     last_conn      = 0xFF;

    uint8_t motorState = (uint8_t) ui_motor_status_fast()->bits.motorState;
    ui_runtime_sync_state(motorState);
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
        if (top_state_chip)
        {
            uint8_t idx = motorState <= 4 ? motorState : 0;
            lv_obj_set_style_bg_color(top_state_chip, chip_bg(idx), LV_PART_MAIN);
            lv_obj_set_style_border_color(top_state_chip, chip_bd(idx), LV_PART_MAIN);
        }
    }
    uint8_t link_state = link_state_normalize(ui_motor_connected());
    if (top_link_dot && link_state != last_conn)
    {
        last_conn = link_state;
        lv_color_t c = link_state_color(link_state);
        lv_obj_set_style_bg_color(top_link_dot, c, LV_PART_MAIN);
        // The dot alone carries the link state — the pill caption is fixed
        // "RS-485" (operator: ONLINE/OFFLINE text made the header noisy).
    }
}

// --- Boot splash -------------------------------------------------------------

static void splash_fade_anim(void *obj, int32_t opa)
{
    lv_obj_set_style_opa((lv_obj_t *) obj, (lv_opa_t) opa, LV_PART_MAIN);
}

// Logo entrance: fades in while sliding up ~14px (blank white beat first,
// then a light fade + slide — nothing snap in).
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

// --- Per-screen reset --------------------------------------------------------

static void init_screen_content(void)
{
    // The previous content host has already been cleaned. Reuse its fixed
    // numeric text buffers instead of carrying per-screen RAM. Background and
    // shell geometry are persistent properties of ui_MainScreen/ui_ContentHost;
    // touching them here would invalidate the full 800x480 screen on every tab.
    screen_text_reset();
}

// =============================================================================
// DASHBOARD
// =============================================================================

// Dashboard owns four dynamic labels, one display-only arc and one
// source-swapped direction icon.
static lv_obj_t *dash_lbl_rpm;
static lv_obj_t *dash_gauge;
static uint8_t dash_last_dir;
static lv_obj_t *dash_val_runtime;
static lv_obj_t *dash_val_iq;
static lv_obj_t *dash_val_faults;
static lv_obj_t *dash_dir_icon;
static uint8_t dash_last_faults;
static int16_t dash_last_speed;
static uint8_t dash_speed_band;
static lv_obj_t *dash_mode_val, *dash_target_val, *dash_target_unit; // hero side stacks
static uint8_t   dash_last_mode;
static int32_t   dash_last_target;

#if defined(UI_DEMO_SIM) && UI_DEMO_SIM
uint8_t ui_test_dashboard_speed_band(void)
{
    return dash_speed_band;
}

lv_color_t ui_test_dashboard_speed_color(void)
{
    return dash_gauge ? lv_obj_get_style_arc_color(dash_gauge, LV_PART_INDICATOR)
                      : lv_color_black();
}
#endif

// Big primary button (START/STOP-style). No icon glyph, so it can
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
    lv_obj_set_style_text_font(lbl, &ui_font_sans22, LV_PART_MAIN);
    lv_obj_set_style_text_letter_space(lbl, 2, LV_PART_MAIN);
    lv_obj_center(lbl);
    if (out_lbl) *out_lbl = lbl;
    return btn;
}

// Direction toggle button: ROTATION ICON ONLY, centered. The former icon +
// CW/CCW caption pair sat off-left and clipped the icon; the rotate arrow alone
// reads cleanly at 52px). The tick recolors the alpha icon per state.
static lv_obj_t *create_dir_icon_btn(lv_obj_t *parent, const lv_img_dsc_t *icon,
                                     lv_obj_t **out_img)
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

static uint8_t dash_fault_count(uint8_t faults)
{
    uint8_t visible = faults & (FAULT_OC | FAULT_OV | FAULT_UV | FAULT_OT | FAULT_COMM);
    uint8_t count = 0;
    while (visible != 0)
    {
        count += visible & 1u;
        visible >>= 1;
    }
    return count;
}

void create_screen_dashboard(lv_obj_t *parent)
{
    init_screen_content();
    dash_last_dir = 0xFF;
    dash_last_faults = 0xFF;
    dash_last_speed = -1;
    dash_speed_band = 0xFF;
    dash_last_mode = 0xFF;
    dash_last_target = -1;

    // A dedicated hero card gives the gauge the same surface hierarchy as the
    // selected telemetry-gauge reference. The pre-rendered face supplies the
    // static track/ticks; the live arc above it is indicator-only.
    lv_obj_t *gauge_card = lv_obj_create(parent);
    lv_obj_set_size(gauge_card, 696, 300);
    lv_obj_set_pos(gauge_card, UI_GAP, UI_GAP);
    lv_obj_set_style_bg_color(gauge_card, COLOR_CARD_BG, LV_PART_MAIN);
    lv_obj_set_style_bg_opa(gauge_card, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_border_width(gauge_card, 0, LV_PART_MAIN);
    lv_obj_set_style_radius(gauge_card, 14, LV_PART_MAIN);
    style_card_shadow(gauge_card);
    lv_obj_set_style_pad_all(gauge_card, 0, LV_PART_MAIN);
    lv_obj_clear_flag(gauge_card, LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *gauge_face = lv_img_create(gauge_card);
    lv_img_set_src(gauge_face, &ui_img_dashboard_gauge);
    lv_obj_align(gauge_face, LV_ALIGN_CENTER, 0, 12); // ring center sits below the frame center
    lv_obj_set_style_img_recolor(gauge_face, COLOR_TEXT_H, LV_PART_MAIN);
    lv_obj_set_style_img_recolor_opa(gauge_face, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_clear_flag(gauge_face, LV_OBJ_FLAG_CLICKABLE);

    dash_gauge = lv_arc_create(gauge_card);
    lv_obj_set_size(dash_gauge, 274, 274); // 129px indicator center radius with 16px live stroke
    lv_obj_align(dash_gauge, LV_ALIGN_CENTER, 0, 12); // follows the face (ring moved down)
    lv_arc_set_rotation(dash_gauge, 135);
    lv_arc_set_bg_angles(dash_gauge, 0, 270);
    lv_arc_set_range(dash_gauge, 0, UI_MAX_RPM);
    lv_arc_set_value(dash_gauge, 0);
    lv_obj_set_style_arc_opa(dash_gauge, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_set_style_arc_color(dash_gauge, COLOR_SPEED_SIGNAL, LV_PART_INDICATOR);
    lv_obj_set_style_arc_width(dash_gauge, 8, LV_PART_INDICATOR);
    lv_obj_set_style_arc_rounded(dash_gauge, false, LV_PART_INDICATOR);
    lv_obj_remove_style(dash_gauge, NULL, LV_PART_KNOB);
    lv_obj_clear_flag(dash_gauge, LV_OBJ_FLAG_CLICKABLE);

    dash_lbl_rpm = lv_label_create(gauge_card);
    label_bind_buffer(dash_lbl_rpm, "0");
    lv_obj_set_width(dash_lbl_rpm, 240);
    lv_obj_set_style_text_align(dash_lbl_rpm, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
    lv_obj_align(dash_lbl_rpm, LV_ALIGN_CENTER, 0, 16); // a bit below the geometric
                                                       // ring center: the 270deg arc opens
                                                       // at the bottom, so this reads centered
    lv_obj_set_style_text_font(dash_lbl_rpm, &ui_font_mono66, LV_PART_MAIN);
    lv_obj_set_style_text_color(dash_lbl_rpm, COLOR_TEXT_H, LV_PART_MAIN); // same ink as the SPEED readout

    lv_obj_t *rpm_unit = lv_label_create(gauge_card);
    lv_label_set_text_static(rpm_unit, "RPM");
    lv_obj_align(rpm_unit, LV_ALIGN_CENTER, 0, 66);
    lv_obj_set_style_text_font(rpm_unit, &ui_font_mono22, LV_PART_MAIN);
    lv_obj_set_style_text_color(rpm_unit, COLOR_TEXT_H, LV_PART_MAIN); // same ink as the speed number
    lv_obj_set_style_text_letter_space(rpm_unit, 4, LV_PART_MAIN);

    dash_dir_icon = lv_img_create(gauge_card);
    lv_img_set_src(dash_dir_icon, &ui_icon_cw);
    lv_obj_align(dash_dir_icon, LV_ALIGN_CENTER, 0, 112);
    lv_obj_set_style_img_recolor(dash_dir_icon, COLOR_TEXT_H, LV_PART_MAIN);
    lv_obj_set_style_img_recolor_opa(dash_dir_icon, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_clear_flag(dash_dir_icon, LV_OBJ_FLAG_CLICKABLE);

    // The wide hero card left big empty flanks around the centered gauge —
    // they now carry the operating context (operator review): MODE on the
    // left, the commanded TARGET on the right, both owned by motorCmd.
    lv_obj_t *mode_stack = flat_cont(gauge_card);
    lv_obj_align(mode_stack, LV_ALIGN_LEFT_MID, 30, 0);
    lv_obj_set_size(mode_stack, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
    lv_obj_set_layout(mode_stack, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(mode_stack, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_row(mode_stack, 6, LV_PART_MAIN);
    lv_obj_t *mode_cap = lv_label_create(mode_stack);
    lv_label_set_text_static(mode_cap, "MODE");
    lv_obj_set_style_text_font(mode_cap, &ui_font_sans20, LV_PART_MAIN);
    lv_obj_set_style_text_color(mode_cap, COLOR_TEXT_L, LV_PART_MAIN);
    lv_obj_set_style_text_letter_space(mode_cap, 2, LV_PART_MAIN);
    dash_mode_val = lv_label_create(mode_stack);
    label_bind_buffer(dash_mode_val, "SPEED");
    lv_obj_set_style_text_font(dash_mode_val, &ui_font_mono22, LV_PART_MAIN);
    lv_obj_set_style_text_color(dash_mode_val, COLOR_TEXT_H, LV_PART_MAIN);

    lv_obj_t *tgt_stack = flat_cont(gauge_card);
    lv_obj_align(tgt_stack, LV_ALIGN_RIGHT_MID, -30, 0);
    lv_obj_set_size(tgt_stack, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
    lv_obj_set_layout(tgt_stack, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(tgt_stack, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(tgt_stack, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_END, LV_FLEX_ALIGN_END);
    lv_obj_set_style_pad_row(tgt_stack, 6, LV_PART_MAIN);
    lv_obj_t *tgt_cap = lv_label_create(tgt_stack);
    lv_label_set_text_static(tgt_cap, "TARGET");
    lv_obj_set_style_text_font(tgt_cap, &ui_font_sans20, LV_PART_MAIN);
    lv_obj_set_style_text_color(tgt_cap, COLOR_TEXT_L, LV_PART_MAIN);
    lv_obj_set_style_text_letter_space(tgt_cap, 2, LV_PART_MAIN);
    lv_obj_t *tgt_row = flex_row(tgt_stack);
    lv_obj_set_size(tgt_row, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
    lv_obj_set_style_pad_column(tgt_row, 6, LV_PART_MAIN);
    lv_obj_set_flex_align(tgt_row, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_END, LV_FLEX_ALIGN_END);
    dash_target_val = lv_label_create(tgt_row);
    label_bind_buffer(dash_target_val, "0");
    lv_obj_set_style_text_font(dash_target_val, &ui_font_mono30, LV_PART_MAIN);
    lv_obj_set_style_text_color(dash_target_val, COLOR_TEXT_H, LV_PART_MAIN);
    dash_target_unit = lv_label_create(tgt_row);
    lv_label_set_text_static(dash_target_unit, "RPM");
    lv_obj_set_style_text_font(dash_target_unit, &ui_font_mono20, LV_PART_MAIN);
    lv_obj_set_style_text_color(dash_target_unit, COLOR_TEXT_VL, LV_PART_MAIN);
    lv_obj_set_style_pad_bottom(dash_target_unit, 3, LV_PART_MAIN);

    lv_obj_t *runtime = create_mcard(parent, "RUN TIME", "", COLOR_TEXT_H,
                                     &dash_val_runtime);
    lv_obj_set_size(runtime, 226, 90);
    lv_obj_set_pos(runtime, 8, 316);
    lv_obj_set_style_radius(runtime, 14, LV_PART_MAIN);
    lv_obj_set_style_text_font(dash_val_runtime, &ui_font_mono38, LV_PART_MAIN); // same size tier as FAULTS

    lv_obj_t *iq = create_mcard(parent, "IQ CURRENT", "A", COLOR_TEXT_H,
                                &dash_val_iq);
    lv_obj_set_size(iq, 226, 90);
    lv_obj_set_pos(iq, 242, 316);
    lv_obj_set_style_radius(iq, 14, LV_PART_MAIN);
    lv_obj_set_style_text_font(dash_val_iq, &ui_font_mono38, LV_PART_MAIN); // unified card value size

    lv_obj_t *faults = create_mcard(parent, "FAULTS", "", COLOR_OK,
                                    &dash_val_faults);
    lv_obj_set_size(faults, 226, 90);
    lv_obj_set_pos(faults, 476, 316);
    lv_obj_set_style_radius(faults, 14, LV_PART_MAIN);
    lv_obj_set_style_text_font(dash_val_faults, &ui_font_mono38, LV_PART_MAIN);
}

void tick_screen_dashboard(ui_lane_t lane)
{
    if (ui_current_tab != TAB_DASHBOARD || lane != UI_LANE_SLOW)
        return;

    char buf[24];
    const MotorStatusFast_t *fast = ui_motor_status_fast();
    int32_t rpm = (int32_t) fast->bits.actualRpm;
    int32_t raw_speed = rpm < 0 ? -rpm : rpm;
    int32_t speed = raw_speed;
    if (speed > UI_MAX_RPM) speed = UI_MAX_RPM;
    label_set_if_changed(dash_lbl_rpm, fmt_rpm(buf, sizeof(buf), speed));
    label_set_if_changed(dash_val_iq,
                         fmt_scaled(buf, sizeof(buf), (int32_t) fast->bits.iqCurrent, 10, 1));

    uint32_t run_seconds = ui_runtime_seconds();
    uint32_t hours = run_seconds / 3600u;
    if (hours > 99999u) hours = 99999u;
    snprintf(buf, sizeof(buf), "%02lu:%02lu:%02lu",
             (unsigned long) hours,
             (unsigned long) ((run_seconds / 60u) % 60u),
             (unsigned long) (run_seconds % 60u));
    label_set_if_changed(dash_val_runtime, buf);

    uint8_t fault_count = dash_fault_count((uint8_t) fast->bits.fault);
    if (fault_count != dash_last_faults)
    {
        dash_last_faults = fault_count;
        snprintf(buf, sizeof(buf), "%u", (unsigned) fault_count);
        label_set_if_changed(dash_val_faults, buf);
        label_color_if_changed(dash_val_faults,
                               fault_count == 0 ? COLOR_OK : COLOR_DANGER);
    }

    uint8_t direction = (uint8_t) fast->bits.dir;
    if (direction != dash_last_dir)
    {
        dash_last_dir = direction;
        lv_img_set_src(dash_dir_icon,
                       direction == MOTOR_DIR_REV ? &ui_icon_ccw : &ui_icon_cw);
    }

    if (speed != dash_last_speed)
    {
        dash_last_speed = (int16_t) speed;
        lv_arc_set_value(dash_gauge, speed);
    }

    // Match Control's utilization semantics without writing a style every
    // SLOW tick: normal SPEED blue below 85%, amber in the reviewed high band,
    // and red only for a real wire fault or telemetry beyond the hard limit.
    uint8_t speed_band = ((uint8_t) fast->bits.fault != 0 || raw_speed > UI_MAX_RPM) ? 2
                       : (raw_speed >= UI_MAX_RPM * 85 / 100) ? 1 : 0;
    if (speed_band != dash_speed_band)
    {
        dash_speed_band = speed_band;
        lv_obj_set_style_arc_color(dash_gauge,
            speed_band == 0 ? COLOR_SPEED_SIGNAL
                            : speed_band == 1 ? COLOR_WARN : COLOR_DANGER,
            LV_PART_INDICATOR);
    }

    // Hero side stacks: MODE + TARGET follow the COMMAND (motorCmd), like
    // Control — usable while disconnected, never stale from a telemetry echo.
    uint8_t opMode = (uint8_t) motorCmd.bits.opMode;
    if (opMode != OP_MODE_SPEED && opMode != OP_MODE_TORQUE && opMode != OP_MODE_POSITION)
        opMode = OP_MODE_SPEED;
    if (opMode != dash_last_mode)
    {
        dash_last_mode = opMode;
        static const char * const MODE_TXT[4] = {NULL, "SPEED", "TORQUE", "POSITION"};
        label_set_if_changed(dash_mode_val, MODE_TXT[opMode]);
        const char *unit = opMode == OP_MODE_SPEED ? "RPM" : opMode == OP_MODE_TORQUE ? "A" : "\xC2\xB0";
        label_set_static_if_changed(dash_target_unit, unit);
    }
    int32_t tgt_raw = (int32_t) motorCmd.bits.ctrlValRaw;
    if (tgt_raw != dash_last_target)
    {
        dash_last_target = tgt_raw;
        if (opMode == OP_MODE_SPEED)
            snprintf(buf, sizeof(buf), "%d", (int) tgt_raw);
        else if (opMode == OP_MODE_TORQUE)
            fmt_scaled(buf, sizeof(buf), tgt_raw, 1000, 1);
        else
            fmt_scaled(buf, sizeof(buf), tgt_raw, 10, 1);
        label_set_if_changed(dash_target_val, buf);
    }
}

// Monitor: eight equal read-only telemetry cards in a 4x2 grid.
static lv_obj_t *mon_val_iq, *mon_val_irms;
static lv_obj_t *mon_val_speed, *mon_val_voltage, *mon_val_runtime;
static lv_obj_t *mon_val_power, *mon_val_faults;
static lv_obj_t *mon_dir_icon;
static uint8_t   mon_last_dir;
static uint8_t   mon_update_phase;
static uint8_t   mon_force_all;

// Minimal telemetry cards: white surface, neutral caption and one centered
// value. Units remain static captions so the 5 Hz lane invalidates only the
// number glyphs. Keeping the caption directly on the card also removes the
// former eight decorative header objects from the LVGL heap.
static lv_obj_t *mon_metric_card(lv_obj_t *parent, int32_t x, int32_t y,
                                 const char *title_text, const char *unit_text,
                                 metric_id_t metric, lv_obj_t **out_val)
{
    const metric_palette_t *p = &metric_palette[metric];

    lv_obj_t *card = lv_obj_create(parent);
    lv_obj_set_size(card, 168, 195);
    lv_obj_set_pos(card, x, y);
    lv_obj_set_style_bg_color(card, COLOR_CARD_BG, LV_PART_MAIN);
    lv_obj_set_style_border_width(card, 0, LV_PART_MAIN);
    lv_obj_set_style_radius(card, 12, LV_PART_MAIN);
    style_card_shadow(card);
    lv_obj_set_style_pad_all(card, 0, LV_PART_MAIN);
    lv_obj_clear_flag(card, LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *title = lv_label_create(card);
    lv_label_set_text_static(title, title_text);
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 13);
    lv_obj_set_style_text_align(title, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
    lv_obj_set_style_text_font(title, &ui_font_sans20, LV_PART_MAIN);
    lv_obj_set_style_text_color(title, p->ink, LV_PART_MAIN);
    lv_obj_set_style_text_letter_space(title, 1, LV_PART_MAIN);

    lv_obj_t *val = NULL;
    if (out_val)
    {
        val = lv_label_create(card);
        label_bind_buffer(val, "0");
        lv_obj_set_width(val, 150); // fits "00:00:00" on the shared value font
        lv_obj_set_style_text_align(val, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
        lv_obj_align(val, LV_ALIGN_CENTER, 0, 14);
        lv_obj_set_style_text_font(val, &ui_font_mono30, LV_PART_MAIN); // default value tier (RUN TIME)
        lv_obj_set_style_text_color(val, COLOR_TEXT_H, LV_PART_MAIN);
    }

    if (unit_text && unit_text[0] != '\0')
    {
        lv_obj_t *unit = lv_label_create(card);
        lv_label_set_text_static(unit, unit_text);
        lv_obj_align(unit, LV_ALIGN_BOTTOM_MID, 0, -15);
        lv_obj_set_style_text_font(unit, &ui_font_mono20, LV_PART_MAIN);
        lv_obj_set_style_text_color(unit, COLOR_UNIT, LV_PART_MAIN);
        lv_obj_set_style_text_letter_space(unit, 1, LV_PART_MAIN);
    }
    if (out_val) *out_val = val;
    return card;
}

// Current hero: title + one large changing number. There is deliberately no
// nominal/setpoint caption because Monitor is read-only actual telemetry.
static void mon_direction_card(lv_obj_t *parent, int32_t x, int32_t y,
                               metric_id_t metric)
{
    lv_obj_t *card = mon_metric_card(parent, x, y, "DIRECTION", "", metric, NULL);
    mon_dir_icon = lv_img_create(card);
    lv_img_set_src(mon_dir_icon, &ui_icon_cw);
    lv_obj_set_style_img_recolor(mon_dir_icon, COLOR_TEXT_M, LV_PART_MAIN); // icon stays the classic dark ink
    lv_obj_set_style_img_recolor_opa(mon_dir_icon, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_clear_flag(mon_dir_icon, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_align(mon_dir_icon, LV_ALIGN_CENTER, 0, 13);
}

void create_screen_monitor(lv_obj_t *parent)
{
    init_screen_content();
    mon_force_all = 1;
    mon_last_dir  = 0xFF;

    // Domain reorder: Motion (SPEED/DIRECTION), Current
    // (IQ/RMS), Electrical (VOLTAGE/POWER), System (RUN TIME/FAULTS). Scanning
    // starts with SPEED.
    mon_metric_card(parent, 8, 8, "SPEED", "RPM", METRIC_SPEED, &mon_val_speed);
    lv_obj_set_style_text_font(mon_val_speed, &ui_font_mono44, LV_PART_MAIN);
    mon_metric_card(parent, 184, 8, "IQ CURRENT", "A", METRIC_IQ, &mon_val_iq);
    lv_obj_set_style_text_font(mon_val_iq, &ui_font_mono44, LV_PART_MAIN); // everything except RUN TIME at 44
    mon_metric_card(parent, 360, 8, "VOLTAGE", "V", METRIC_ENERGY, &mon_val_voltage);
    lv_obj_set_style_text_font(mon_val_voltage, &ui_font_mono44, LV_PART_MAIN);
    mon_metric_card(parent, 536, 8, "RUN TIME", "HH:MM:SS", METRIC_RUNTIME, &mon_val_runtime);

    // Data in neutral ink — color is reserved for state (thresholds, health).
    // RUN TIME stays on the smaller 30px tier (its 8 glyphs do not fit 44).
    lv_obj_set_style_text_color(mon_val_runtime, COLOR_TEXT_M, LV_PART_MAIN); // value stays the classic dark ink
    mon_direction_card(parent, 8, 211, METRIC_DIRECTION);
    mon_metric_card(parent, 184, 211, "RMS CURRENT", "A", METRIC_IRMS, &mon_val_irms);
    lv_obj_set_style_text_font(mon_val_irms, &ui_font_mono44, LV_PART_MAIN);
    mon_metric_card(parent, 360, 211, "POWER", "W", METRIC_ENERGY, &mon_val_power);
    lv_obj_set_style_text_font(mon_val_power, &ui_font_mono44, LV_PART_MAIN);
    mon_metric_card(parent, 536, 211, "FAULTS", "COUNT", METRIC_HEALTH, &mon_val_faults);
    lv_obj_set_style_text_font(mon_val_faults, &ui_font_mono44, LV_PART_MAIN);
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
// Two SEPARATE widths (operator review): the MODE segmented control is wide
// enough for SPEED/TORQUE/POSITION to breathe, while the COMMAND panel stays
// narrow so the LIVE FEEDBACK tiles get the width.
#define CTRL_MODE_W 360
#define CTRL_RCOL_W 280

// Only ONE of these mode subtrees exists at a time. Keeping the inactive tree
// hidden used 14 extra objects in LEVEL and 34 extra objects in POSITION on the
// 52 KB heap. A LEVEL <-> POSITION change requests a deferred Control rebuild;
// SPEED <-> TORQUE keeps the shared LEVEL tree and only reconfigures it.
static lv_obj_t *ctrl_level_layout;   // SPEED/TORQUE: SPEED+Iq readout (left) + control column (right)
static lv_obj_t *ctrl_pos_layout;     // POSITION: one neutral ring + a charcoal handle
// LEVEL readout (design "Divided", no gauge): SPEED | IQ CURRENT.
static lv_obj_t *ctrl_spd_num, *ctrl_iq_num, *ctrl_spd_bar, *ctrl_iq_bar;
static uint8_t   ctrl_spd_band, ctrl_iq_band; // recolor each bar's fill only on a band transition
static lv_obj_t *ctrl_lvl_title, *ctrl_lvl_num, *ctrl_lvl_unit, *ctrl_lvl_slider; // reused per level mode
// Secondary LIMIT control in the level column (current limit for SPEED, speed
// limit for TORQUE) — one reused row + slider, reconfigured by the
// tick like the primary one. Writes MotorCmd_t.limitRaw.
static lv_obj_t *ctrl_lim_title, *ctrl_lim_num, *ctrl_lim_unit, *ctrl_lim_slider;
static lv_obj_t *ctrl_btn_fwd, *ctrl_img_fwd, *ctrl_btn_rev, *ctrl_img_rev;
static lv_obj_t *ctrl_pos_ring, *ctrl_pos_dot; // POSITION: display-only arc + a manual handle inside
static lv_obj_t *ctrl_pos_area;                // POSITION: the ring's touch target (drag), for the tick's drag guard
static lv_obj_t *ctrl_pos_num;                 // POSITION: target angle readout (degrees) in the ring centre
static lv_obj_t *ctrl_pos_lim_num, *ctrl_pos_lim_slider; // POSITION: current-limit row (right column)
static int32_t   ctrl_pos_raw;                  // live target; ring itself is a static track with no indicator fill
static uint32_t  ctrl_pos_text_ms;              // direct-manipulation text is capped; handle still tracks every input sample
static lv_obj_t *ctrl_btn_start, *ctrl_lbl_start, *ctrl_btn_stop, *ctrl_lbl_stop;
static uint8_t   ctrl_last_mode, ctrl_last_dir, ctrl_lvl_mode;
// Disabled-state caches (START/STOP arming + transition lock). File-scope so
// create_screen_control() can reset them to 0xFF on every rebuild — a stale
// static-local cache made freshly built widgets skip their first state apply.
static uint8_t   ctrl_last_start_ok, ctrl_last_stop_ok, ctrl_last_locked;

// The MODE segmented control's visual order is not the OpMode_e value order.
// Value 0 stays reserved on the wire, so only the three supported values appear.
static const uint8_t ctrl_mode_order[] = {OP_MODE_SPEED, OP_MODE_TORQUE, OP_MODE_POSITION};
#define CTRL_MODE_COUNT ((uint16_t) (sizeof(ctrl_mode_order) / sizeof(ctrl_mode_order[0])))

static lv_obj_t *ctrl_mode_btns[CTRL_MODE_COUNT]; // segmented SPEED/TORQUE/POSITION, top-right of the card
static lv_obj_t *ctrl_mode_lbls[CTRL_MODE_COUNT];

typedef struct
{
    uint32_t target;
    uint16_t limit;
    uint8_t  valid;
} ctrl_mode_setting_t;

// Process-lifetime UI cache: survives Control-screen teardown/rebuild. Each
// mode keeps values in its own physical units, so switching away and back can
// safely restore the previous target and limit instead of resetting to zero.
static ctrl_mode_setting_t ctrl_mode_settings[4]; // enum values are 1..3; slot 0 is unused

static bool ctrl_mode_supported(uint8_t mode)
{
    return mode == OP_MODE_SPEED || mode == OP_MODE_TORQUE || mode == OP_MODE_POSITION;
}

static void ctrl_store_active_setting(void)
{
    uint8_t mode = (uint8_t) motorCmd.bits.opMode;
    if (!ctrl_mode_supported(mode)) return;
    ctrl_mode_settings[mode].target = (uint32_t) motorCmd.bits.ctrlValRaw;
    ctrl_mode_settings[mode].limit  = (uint16_t) motorCmd.bits.limitRaw;
    ctrl_mode_settings[mode].valid  = 1;
}

static void ctrl_restore_setting(uint8_t mode)
{
    if (!ctrl_mode_supported(mode)) return;
    motorCmd.bits.opMode = mode;
    if (ctrl_mode_settings[mode].valid)
    {
        motorCmd.bits.ctrlValRaw = ctrl_mode_settings[mode].target;
        motorCmd.bits.limitRaw   = ctrl_mode_settings[mode].limit;
    }
    else
    {
        motorCmd.bits.ctrlValRaw = 0;
        motorCmd.bits.limitRaw   = 0;
    }
}

// Apply the operator-selected command mode immediately. Control is also used
// while commissioning with no motor attached, so its layout must not wait for
// a telemetry echo before showing the matching controls. Feedback values still
// come from MotorStatusFast; only the selected command layout follows motorCmd.
static void ctrl_apply_mode_view(uint8_t opMode)
{
    if (!ctrl_mode_supported(opMode)) return;

    ctrl_last_mode = opMode;
    // Segmented control highlight follows the command-owned mode (Quantum
    // Signal: active = Electric Cyan Ink #0057A8, inactive = white + muted border).
    for (uint16_t i = 0; i < CTRL_MODE_COUNT; i++)
    {
        lv_obj_t *b = ctrl_mode_btns[i];
        if (!b) continue;
        bool active = (ctrl_mode_order[i] == opMode);
        lv_obj_set_style_bg_color(b, active ? COLOR_ACCENT : COLOR_CARD_BG, LV_PART_MAIN);
        lv_obj_set_style_border_color(b, active ? COLOR_ACCENT : COLOR_BORDER_MUTED, LV_PART_MAIN);
        if (ctrl_mode_lbls[i])
            lv_obj_set_style_text_color(ctrl_mode_lbls[i], active ? lv_color_white() : COLOR_TEXT_M,
                                        LV_PART_MAIN);
    }

    if (opMode == OP_MODE_POSITION)
    {
        return;
    }

    if (!ctrl_level_layout) return;
    ctrl_lvl_mode = opMode;

    const char *unit;
    const char *lim_title;
    const char *lim_unit;
    int32_t max;
    int32_t lim_max;
    if (opMode == OP_MODE_TORQUE)
    {
        unit = "A";
        max = UI_MAX_CURRENT_MA;
        lim_title = "Speed Limit";
        lim_unit = "RPM";
        lim_max = UI_CTRL_MAX_RPM;
    }
    else
    {
        unit = "RPM";
        max = UI_CTRL_MAX_RPM;
        lim_title = "Iq Limit";
        lim_unit = "A";
        lim_max = UI_MAX_CURRENT_MA;
    }

    label_set_static_if_changed(ctrl_lvl_unit, unit);
    lv_slider_set_range(ctrl_lvl_slider, 0, max);
    label_set_static_if_changed(ctrl_lim_title, lim_title);
    label_set_static_if_changed(ctrl_lim_unit, lim_unit);
    lv_slider_set_range(ctrl_lim_slider, 0, lim_max);

    // Mode changes alter labels/ranges, not decorative color. One neutral ink
    // keeps SPEED and TORQUE visually stable and avoids recoloring large fills.
    lv_obj_set_style_bg_color(ctrl_lvl_slider, COLOR_ACCENT, LV_PART_INDICATOR);
    lv_obj_set_style_bg_color(ctrl_lim_slider, COLOR_ACCENT, LV_PART_INDICATOR);
}

// The single reused level slider drives whichever level mode is showing, so
// dispatch to that mode's action_*_change (each reads the slider + writes
// motorCmd). ctrl_lvl_mode is set by the tick when it reconfigures the panel.
// (SPEED's handler is also used directly by the Dashboard slider — shared.)
static void ctrl_setpoint_cb(lv_event_t *e)
{
    switch (ctrl_lvl_mode)
    {
        case OP_MODE_TORQUE: action_torque_change(e);      break;
        default:             action_motor_speed_change(e); break; // SPEED
    }
}

// Shared mode switch: save the active mode's target/limit, then restore the
// selected mode's own pair. Never reinterpret one mode's raw units as another's.
static void ctrl_set_mode(uint8_t mode)
{
    if (!ctrl_mode_supported(mode) || mode == (uint8_t) motorCmd.bits.opMode) return;
    uint8_t old_mode = (uint8_t) motorCmd.bits.opMode;
    ctrl_store_active_setting();
    ctrl_restore_setting(mode);
    if ((old_mode == OP_MODE_POSITION) != (mode == OP_MODE_POSITION))
        ui_request_current_rebuild();
    else
        ctrl_apply_mode_view(mode); // SPEED <-> TORQUE reuses the resident LEVEL tree
}

// Segmented control click: the mode comes through user_data (array index).
static void ctrl_mode_btn_cb(lv_event_t *e)
{
    uintptr_t idx = (uintptr_t) lv_event_get_user_data(e);
    if (idx < CTRL_MODE_COUNT) ctrl_set_mode(ctrl_mode_order[idx]);
}

#if defined(UI_DEMO_SIM) && UI_DEMO_SIM
uint8_t ui_test_control_select_mode(uint8_t mode)
{
    if (ui_current_tab != TAB_CONTROL || !ctrl_mode_btns[0] || !ctrl_mode_supported(mode))
        return 0;
    for (uint16_t i = 0; i < CTRL_MODE_COUNT; i++)
    {
        if (ctrl_mode_order[i] != mode) continue;
        ctrl_set_mode(mode);
        return 1;
    }
    return 0;
}
#endif

// The reused LEVEL limit slider (SPEED → current mA, TORQUE → speed
// RPM). Value goes straight onto limitRaw. Pin opMode to the panel's mode so
// the shared limitRaw isn't misread under another mode (same guard as the
// primary handlers).
static void ctrl_limit_cb(lv_event_t *e)
{
    int32_t v = lv_slider_get_value(lv_event_get_target(e));
    motorCmd.bits.opMode   = ctrl_lvl_mode;
    motorCmd.bits.limitRaw = (uint32_t) v;
}

// POSITION's current-limit slider (mA → limitRaw). Pins opMode to POSITION.
static void ctrl_pos_limit_cb(lv_event_t *e)
{
    int32_t v = lv_slider_get_value(lv_event_get_target(e));
    motorCmd.bits.opMode   = OP_MODE_POSITION;
    motorCmd.bits.limitRaw = (uint32_t) v;
}

// POSITION: place the charcoal handle INSIDE the ring, near its inner rim, at the
// angle of the current arc value (the arc's own knob is hidden). rotation 270
// puts value 0 at the top; screen trig (0°=3 o'clock, clockwise, y down).
#define CTRL_POS_DOT_R 74 // ~10% smaller with the 212px position ring
static void ctrl_pos_update_dot(void)
{
    if (!ctrl_pos_ring || !ctrl_pos_dot) return;
    int32_t v   = ctrl_pos_raw;
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
    fmt_scaled(b, sizeof(b), ctrl_pos_raw, 10, 1);
    label_set_if_changed(ctrl_pos_num, b);
}

// POSITION drag → computs the target angle STRAIGHT from the touch point (see
// ctrl_pos_math.h: continuous, no lv_arc full-circle seam snap = fixes the
// "jump" bug), then move the ring value + the azure dot + write ctrlValRaw.
// Bound to ctrl_pos_layout (the ring itself is display-only) for PRESSED +
// PRESSING so a press or a drag both track the finger.
static void ctrl_pos_drag_cb(lv_event_t *e)
{
    lv_indev_t *indev = lv_indev_get_act();
    if (!indev || !ctrl_pos_ring) return;
    lv_point_t p;
    lv_indev_get_point(indev, &p);
    lv_area_t a;
    lv_obj_get_coords(ctrl_pos_ring, &a);
    int32_t v = ctrl_pos_angle_to_raw((int) (p.x - (a.x1 + a.x2) / 2), (int) (p.y - (a.y1 + a.y2) / 2),
                                      UI_POSITION_MAX_RAW);
    motorCmd.bits.opMode     = OP_MODE_POSITION;
    motorCmd.bits.ctrlValRaw = (uint32_t) v;
    bool changed = v != ctrl_pos_raw;
    if (changed)
    {
        // The arc has no indicator fill: changing its LVGL value only asks
        // LVGL to recalculate/invalidate arc segments that are visually
        // transparent. Keep the target separately so a drag redraws just the
        // 24 px handle and centre digits, not the 212 px ring.
        ctrl_pos_raw = v;
        ctrl_pos_update_dot();
    }

    // Pointer input is sampled at ~33 Hz. Redrawing the large 44 px number at
    // that rate dominates xSPI traffic while adding little perceived motion;
    // the 24 px handle remains fully rate-matched to touch. Refresh digits at
    // 10 Hz and always commit the exact final value on release.
    uint32_t now = lv_tick_get();
    if (lv_event_get_code(e) == LV_EVENT_RELEASED ||
        (changed && now - ctrl_pos_text_ms >= 100u))
    {
        ctrl_pos_text_ms = now;
        ctrl_pos_update_readout();
    }
}

static void ctrl_pos_press_lost_cb(lv_event_t *e)
{
    (void) e;
    // A gesture can be cancelled without RELEASED. Keep the displayed number
    // synchronized with the last target already committed by PRESSING.
    ctrl_pos_text_ms = lv_tick_get();
    ctrl_pos_update_readout();
}

// Layout: the mode selector is pinned to the card's top-right; a big white card
// fills the middle and START/STOP sit below. The card builds only the active
// mode subtree:
//   • LEVEL (SPEED/TORQUE): SPEED + Iq feedback on the left
//     the LEFT + a compact right column (title/value, slider, FWD/REV).
//   • POSITION: one neutral ring with a charcoal handle INSIDE it near the rim.
void create_screen_control(lv_obj_t *parent)
{
    init_screen_content();
    ctrl_last_mode       = 0xFF;
    ctrl_last_dir        = 0xFF;
    ctrl_spd_band        = 0xFF;
    ctrl_iq_band         = 0xFF;
    ctrl_last_start_ok   = 0xFF;
    ctrl_last_stop_ok    = 0xFF;
    ctrl_last_locked     = 0xFF;
    ctrl_lvl_mode        = OP_MODE_SPEED; // boot default; tick reconfigures on mode echo
    ctrl_level_layout    = NULL;
    ctrl_pos_layout      = NULL;
    ctrl_spd_num         = NULL;
    ctrl_iq_num          = NULL;
    ctrl_spd_bar         = NULL;
    ctrl_iq_bar          = NULL;
    ctrl_lvl_title       = NULL;
    ctrl_lvl_num         = NULL;
    ctrl_lvl_unit        = NULL;
    ctrl_lvl_slider      = NULL;
    ctrl_lim_title       = NULL;
    ctrl_lim_num         = NULL;
    ctrl_lim_unit        = NULL;
    ctrl_lim_slider      = NULL;
    ctrl_btn_fwd         = NULL;
    ctrl_img_fwd         = NULL;
    ctrl_btn_rev         = NULL;
    ctrl_img_rev         = NULL;
    ctrl_pos_ring        = NULL;
    ctrl_pos_dot         = NULL;
    ctrl_pos_area        = NULL;
    ctrl_pos_num         = NULL;
    ctrl_pos_lim_num     = NULL;
    ctrl_pos_lim_slider  = NULL;

    lv_obj_t *root = flat_cont(parent);
    lv_obj_set_size(root, UI_CONTENT_W, UI_CONTENT_H);
    lv_obj_set_pos(root, 0, 0);
    lv_obj_set_style_pad_hor(root, UI_GAP, LV_PART_MAIN);
    lv_obj_set_style_pad_ver(root, UI_GAP, LV_PART_MAIN);
    lv_obj_set_style_pad_row(root, UI_GAP, LV_PART_MAIN);
    lv_obj_set_layout(root, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(root, LV_FLEX_FLOW_COLUMN);
    lv_obj_add_flag(root, LV_OBJ_FLAG_OVERFLOW_VISIBLE); // card + button shadows draw outside

    // ---- Central white card: MODE row at the top (labeled), then the readout
    // + control layouts below. The segmented control was floating top-right
    // before; a dedicated labeled row at the card head makes the mode switch
    // the FIRST thing the operator reads (operator review). ----
    lv_obj_t *card = flat_cont(root);
    lv_obj_set_width(card, LV_PCT(100));
    lv_obj_set_flex_grow(card, 1);
    lv_obj_set_style_bg_color(card, COLOR_CARD_BG, LV_PART_MAIN);
    lv_obj_set_style_bg_opa(card, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_border_width(card, 0, LV_PART_MAIN);
    lv_obj_set_style_radius(card, 10, LV_PART_MAIN);
    style_card_shadow(card);
    lv_obj_set_style_pad_hor(card, 18, LV_PART_MAIN);
    lv_obj_set_style_pad_ver(card, 12, LV_PART_MAIN); // trimmed to give the bigger gauge room
    lv_obj_set_style_pad_row(card, 10, LV_PART_MAIN);
    lv_obj_set_layout(card, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(card, LV_FLEX_FLOW_COLUMN);
    lv_obj_add_flag(card, LV_OBJ_FLAG_OVERFLOW_VISIBLE); // slider knob mustn't clip

    // MODE row: small tracked label + the segmented control right-aligned to
    // the same width as the control column below (shared left edge).
    lv_obj_t *mode_row = flex_row(card);
    lv_obj_set_size(mode_row, LV_PCT(100), LV_SIZE_CONTENT);
    lv_obj_set_style_pad_column(mode_row, 12, LV_PART_MAIN);
    lv_obj_t *mode_cap = lv_label_create(mode_row);
    lv_label_set_text_static(mode_cap, "MODE");
    lv_obj_set_style_text_font(mode_cap, &ui_font_sans20, LV_PART_MAIN);
    lv_obj_set_style_text_color(mode_cap, COLOR_TEXT_L, LV_PART_MAIN);
    lv_obj_set_style_text_letter_space(mode_cap, 2, LV_PART_MAIN);
    lv_obj_set_flex_grow(mode_cap, 1);

    lv_obj_t *seg_wrap = flex_row(mode_row);
    lv_obj_set_size(seg_wrap, CTRL_MODE_W, LV_SIZE_CONTENT);
    lv_obj_set_style_pad_column(seg_wrap, 6, LV_PART_MAIN);

    static const char * const MODE_NAMES[CTRL_MODE_COUNT] = {"SPEED", "TORQUE", "POSITION"};
    for (uint16_t i = 0; i < CTRL_MODE_COUNT; i++)
    {
        lv_obj_t *b = lv_btn_create(seg_wrap);
        lv_obj_set_flex_grow(b, 1);
        lv_obj_set_height(b, 44);
        lv_obj_set_style_radius(b, 10, LV_PART_MAIN);
        lv_obj_set_style_shadow_width(b, 0, LV_PART_MAIN);
        lv_obj_set_style_border_width(b, 1, LV_PART_MAIN);
        lv_obj_set_style_pad_all(b, 0, LV_PART_MAIN);
        lv_obj_set_style_pad_hor(b, 2, LV_PART_MAIN); // POSITION/TORQUE sat too close to the edges
        lv_obj_add_event_cb(b, ctrl_mode_btn_cb, LV_EVENT_CLICKED,
                            (void *) (uintptr_t) i);
        lv_obj_t *lbl = lv_label_create(b);
        lv_label_set_text_static(lbl, MODE_NAMES[i]);
        lv_obj_set_style_text_font(lbl, &ui_font_sans20, LV_PART_MAIN);
        lv_obj_set_style_text_letter_space(lbl, 0, LV_PART_MAIN);
        lv_obj_center(lbl);
        ctrl_mode_btns[i] = b;
        ctrl_mode_lbls[i] = lbl;
    }
    // Boot highlight is applied at the END of this function — the level/position
    // layouts must exist first or ctrl_apply_mode_view() early-returns.

    // Readout + control area. Only the selected structural layout is built;
    // SPEED and TORQUE share LEVEL while POSITION gets its compact subtree.
    lv_obj_t *content_wrap = flat_cont(card);
    lv_obj_set_width(content_wrap, LV_PCT(100));
    lv_obj_set_flex_grow(content_wrap, 1);
    lv_obj_add_flag(content_wrap, LV_OBJ_FLAG_OVERFLOW_VISIBLE);

    uint8_t initial_mode = (uint8_t) motorCmd.bits.opMode;
    if (!ctrl_mode_supported(initial_mode)) initial_mode = OP_MODE_SPEED;

    // ---- LEVEL layout: dashboard-style actualRpm gauge (left) + control (right) ----
    if (initial_mode != OP_MODE_POSITION)
    {
    ctrl_level_layout = flat_cont(content_wrap);
    lv_obj_set_size(ctrl_level_layout, LV_PCT(100), LV_PCT(100));
    lv_obj_set_layout(ctrl_level_layout, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(ctrl_level_layout, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(ctrl_level_layout, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_column(ctrl_level_layout, 20, LV_PART_MAIN);
    lv_obj_add_flag(ctrl_level_layout, LV_OBJ_FLAG_OVERFLOW_VISIBLE);

    // ---- LEVEL left: LIVE FEEDBACK — two equal outlined white cards.
    lv_obj_t *garea = flat_cont(ctrl_level_layout);
    lv_obj_set_flex_grow(garea, 1);
    lv_obj_set_height(garea, LV_PCT(100));
    lv_obj_set_layout(garea, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(garea, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(garea, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);
    lv_obj_set_style_pad_row(garea, 12, LV_PART_MAIN);

    // SPEED tile: quiet caption/divider, value + unit right (baseline aligned),
    // and a neutral progress mark. The outline keeps it distinct from the white
    // Control background without a decorative color block.
    lv_obj_t *scol = flat_cont(garea);
    lv_obj_set_flex_grow(scol, 1);
    lv_obj_set_width(scol, LV_PCT(100));
    lv_obj_set_layout(scol, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(scol, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(scol, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);
    style_instrument_frame(scol);

    // Three children, SPACE_BETWEEN: caption, value row, progress bar.
    lv_obj_t *slab = lv_label_create(scol);
    lv_label_set_text_static(slab, "SPEED");
    lv_obj_set_size(slab, LV_PCT(100), 40);
    lv_obj_set_style_bg_opa(slab, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_set_style_border_side(slab, LV_BORDER_SIDE_BOTTOM, LV_PART_MAIN);
    lv_obj_set_style_border_color(slab, COLOR_BORDER, LV_PART_MAIN);
    lv_obj_set_style_border_width(slab, 1, LV_PART_MAIN);
    lv_obj_set_style_pad_hor(slab, 14, LV_PART_MAIN);
    lv_obj_set_style_pad_top(slab, 10, LV_PART_MAIN);
    lv_obj_set_style_text_font(slab, &ui_font_sans20, LV_PART_MAIN);
    lv_obj_set_style_text_color(slab, metric_palette[METRIC_SPEED].signal, LV_PART_MAIN);
    lv_obj_set_style_text_letter_space(slab, 1, LV_PART_MAIN);
    lv_obj_t *svalrow = flex_row(scol);
    lv_obj_set_size(svalrow, LV_PCT(100), LV_SIZE_CONTENT);
    lv_obj_set_style_pad_column(svalrow, 6, LV_PART_MAIN);
    lv_obj_set_style_pad_hor(svalrow, 14, LV_PART_MAIN);
    lv_obj_set_flex_align(svalrow, LV_FLEX_ALIGN_END, LV_FLEX_ALIGN_END, LV_FLEX_ALIGN_END);
    ctrl_spd_num = lv_label_create(svalrow);
    label_bind_buffer(ctrl_spd_num, "0");
    lv_obj_set_style_text_font(ctrl_spd_num, &ui_font_mono56, LV_PART_MAIN);
    lv_obj_set_style_text_color(ctrl_spd_num, COLOR_TEXT_H, LV_PART_MAIN);
    lv_obj_t *sunit = lv_label_create(svalrow);
    lv_label_set_text_static(sunit, "RPM");
    lv_obj_set_style_text_font(sunit, &ui_font_mono20, LV_PART_MAIN);
    lv_obj_set_style_text_color(sunit, COLOR_UNIT, LV_PART_MAIN);
    lv_obj_set_style_pad_bottom(sunit, 3, LV_PART_MAIN); // sit on the value baseline

    ctrl_spd_bar = lv_bar_create(scol);
    lv_obj_set_size(ctrl_spd_bar, LV_PCT(100), 8);
    lv_bar_set_range(ctrl_spd_bar, 0, 100);
    lv_bar_set_value(ctrl_spd_bar, 0, LV_ANIM_OFF);
    lv_obj_set_style_bg_color(ctrl_spd_bar, lv_color_hex(0xD7E0EA), LV_PART_MAIN);
    lv_obj_set_style_radius(ctrl_spd_bar, 4, LV_PART_MAIN);
    lv_obj_set_style_bg_color(ctrl_spd_bar, COLOR_ACCENT, LV_PART_INDICATOR);
    lv_obj_set_style_radius(ctrl_spd_bar, 5, LV_PART_INDICATOR);

    // IQ CURRENT card, same minimal structure.
    lv_obj_t *tcol = flat_cont(garea);
    lv_obj_set_flex_grow(tcol, 1);
    lv_obj_set_width(tcol, LV_PCT(100));
    lv_obj_set_layout(tcol, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(tcol, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(tcol, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);
    style_instrument_frame(tcol);

    // Same three-child structure as the SPEED tile.
    lv_obj_t *tlab = lv_label_create(tcol);
    lv_label_set_text_static(tlab, "IQ CURRENT");
    lv_obj_set_size(tlab, LV_PCT(100), 40);
    lv_obj_set_style_bg_opa(tlab, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_set_style_border_side(tlab, LV_BORDER_SIDE_BOTTOM, LV_PART_MAIN);
    lv_obj_set_style_border_color(tlab, COLOR_BORDER, LV_PART_MAIN);
    lv_obj_set_style_border_width(tlab, 1, LV_PART_MAIN);
    lv_obj_set_style_pad_hor(tlab, 14, LV_PART_MAIN);
    lv_obj_set_style_pad_top(tlab, 10, LV_PART_MAIN);
    lv_obj_set_style_text_font(tlab, &ui_font_sans20, LV_PART_MAIN);
    lv_obj_set_style_text_color(tlab, metric_palette[METRIC_IQ].signal, LV_PART_MAIN);
    lv_obj_set_style_text_letter_space(tlab, 1, LV_PART_MAIN);
    lv_obj_t *tvalrow = flex_row(tcol);
    lv_obj_set_size(tvalrow, LV_PCT(100), LV_SIZE_CONTENT);
    lv_obj_set_style_pad_column(tvalrow, 6, LV_PART_MAIN);
    lv_obj_set_style_pad_hor(tvalrow, 14, LV_PART_MAIN);
    lv_obj_set_flex_align(tvalrow, LV_FLEX_ALIGN_END, LV_FLEX_ALIGN_END, LV_FLEX_ALIGN_END);
    ctrl_iq_num = lv_label_create(tvalrow);
    label_bind_buffer(ctrl_iq_num, "0.0");
    lv_obj_set_style_text_font(ctrl_iq_num, &ui_font_mono56, LV_PART_MAIN);
    lv_obj_set_style_text_color(ctrl_iq_num, COLOR_TEXT_H, LV_PART_MAIN);
    lv_obj_t *tunit = lv_label_create(tvalrow);
    lv_label_set_text_static(tunit, "A");
    lv_obj_set_style_text_font(tunit, &ui_font_mono20, LV_PART_MAIN);
    lv_obj_set_style_text_color(tunit, COLOR_UNIT, LV_PART_MAIN);
    lv_obj_set_style_pad_bottom(tunit, 3, LV_PART_MAIN); // sit on the value baseline

    ctrl_iq_bar = lv_bar_create(tcol);
    lv_obj_set_size(ctrl_iq_bar, LV_PCT(100), 8);
    lv_bar_set_range(ctrl_iq_bar, 0, 100);
    lv_bar_set_value(ctrl_iq_bar, 0, LV_ANIM_OFF);
    lv_obj_set_style_bg_color(ctrl_iq_bar, lv_color_hex(0xD7E0EA), LV_PART_MAIN);
    lv_obj_set_style_radius(ctrl_iq_bar, 4, LV_PART_MAIN);
    lv_obj_set_style_bg_color(ctrl_iq_bar, COLOR_ACCENT, LV_PART_INDICATOR);
    lv_obj_set_style_radius(ctrl_iq_bar, 5, LV_PART_INDICATOR);

    // Right COMMAND panel: restore the reviewed neutral-gray grouping surface.
    // It separates controls from the white telemetry instruments without
    // bringing back the old full-color SPEED/Iq bodies.
    lv_obj_t *rcol = flat_cont(ctrl_level_layout);
    lv_obj_set_size(rcol, CTRL_RCOL_W, LV_PCT(100));
    lv_obj_set_layout(rcol, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(rcol, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(rcol, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);
    lv_obj_set_style_bg_color(rcol, COLOR_CMD_PANEL, LV_PART_MAIN);
    lv_obj_set_style_bg_opa(rcol, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_radius(rcol, 12, LV_PART_MAIN);
    lv_obj_set_style_pad_all(rcol, 14, LV_PART_MAIN);
    lv_obj_add_flag(rcol, LV_OBJ_FLAG_OVERFLOW_VISIBLE);

    // --- TARGET group (title row + its slider, kept together) ---
    lv_obj_t *g_target = flat_cont(rcol);
    lv_obj_set_width(g_target, LV_PCT(100));
    lv_obj_set_height(g_target, LV_SIZE_CONTENT);
    lv_obj_set_layout(g_target, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(g_target, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_row(g_target, 18, LV_PART_MAIN); // breathing room title -> slider
    lv_obj_add_flag(g_target, LV_OBJ_FLAG_OVERFLOW_VISIBLE);

    lv_obj_t *trow = flex_row(g_target);
    lv_obj_set_size(trow, LV_PCT(100), LV_SIZE_CONTENT);
    lv_obj_set_flex_align(trow, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_END, LV_FLEX_ALIGN_END);
    ctrl_lvl_title = lv_label_create(trow); // left-aligned title above the slider
    lv_label_set_text_static(ctrl_lvl_title, "Target");
    lv_obj_set_style_text_font(ctrl_lvl_title, &ui_font_sans20, LV_PART_MAIN);
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
    lv_obj_set_style_text_color(ctrl_lvl_unit, COLOR_UNIT, LV_PART_MAIN);

    ctrl_lvl_slider = lv_slider_create(g_target);
    lv_obj_set_width(ctrl_lvl_slider, LV_PCT(100));
    lv_obj_set_height(ctrl_lvl_slider, 12);
    lv_slider_set_range(ctrl_lvl_slider, 0, UI_CTRL_MAX_RPM);
    lv_slider_set_value(ctrl_lvl_slider, 0, LV_ANIM_OFF);
    style_slider(ctrl_lvl_slider);
    lv_obj_add_event_cb(ctrl_lvl_slider, ctrl_setpoint_cb, LV_EVENT_VALUE_CHANGED, NULL);

    // --- LIMIT group (title left + value/unit right, then its slider) — the
    // secondary safety limit. Title/unit/range reconfigured per mode by the tick
    // (SPEED → current "A", TORQUE → speed "RPM"). ---
    lv_obj_t *g_limit = flat_cont(rcol);
    lv_obj_set_width(g_limit, LV_PCT(100));
    lv_obj_set_height(g_limit, LV_SIZE_CONTENT);
    lv_obj_set_layout(g_limit, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(g_limit, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_row(g_limit, 18, LV_PART_MAIN);
    lv_obj_add_flag(g_limit, LV_OBJ_FLAG_OVERFLOW_VISIBLE);

    lv_obj_t *lrow = flex_row(g_limit);
    lv_obj_set_size(lrow, LV_PCT(100), LV_SIZE_CONTENT);
    lv_obj_set_flex_align(lrow, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_END, LV_FLEX_ALIGN_END);
    ctrl_lim_title = lv_label_create(lrow);
    lv_label_set_text_static(ctrl_lim_title, "Iq Limit");
    lv_obj_set_style_text_font(ctrl_lim_title, &ui_font_sans20, LV_PART_MAIN);
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
    lv_obj_set_style_text_color(ctrl_lim_unit, COLOR_UNIT, LV_PART_MAIN);

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
    ctrl_btn_fwd = create_dir_icon_btn(drow, &ui_icon_cw, &ctrl_img_fwd);
    lv_obj_add_event_cb(ctrl_btn_fwd, action_motor_dir_fwd, LV_EVENT_CLICKED, NULL);
    ctrl_btn_rev = create_dir_icon_btn(drow, &ui_icon_ccw, &ctrl_img_rev);
    lv_obj_add_event_cb(ctrl_btn_rev, action_motor_dir_rev, LV_EVENT_CLICKED, NULL);
    }

    // ---- POSITION layout: compact gray ring + dot (LEFT) + current-limit column
    // (RIGHT). Mirrors the LEVEL layout's gauge-left / controls-right split so
    // the two modes feel consistent — which also shifts the ring left of centre
    // (user request) and makes room for the new limit control. ----
    if (initial_mode == OP_MODE_POSITION)
    {
    ctrl_pos_layout = flat_cont(content_wrap);
    lv_obj_set_size(ctrl_pos_layout, LV_PCT(100), LV_PCT(100));
    lv_obj_set_layout(ctrl_pos_layout, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(ctrl_pos_layout, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(ctrl_pos_layout, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_column(ctrl_pos_layout, 20, LV_PART_MAIN);
    lv_obj_add_flag(ctrl_pos_layout, LV_OBJ_FLAG_OVERFLOW_VISIBLE);

    // Left: transparent ring/touch area. The white Control card is the only
    // background surface; avoiding another opaque panel removes one clipped
    // fill pass whenever the handle/readout invalidates a small region.
    lv_obj_t *parea = flat_cont(ctrl_pos_layout);
    ctrl_pos_area = parea;
    lv_obj_set_flex_grow(parea, 1);
    lv_obj_set_height(parea, LV_PCT(100));
    lv_obj_add_flag(parea, LV_OBJ_FLAG_OVERFLOW_VISIBLE);

    ctrl_pos_ring = lv_arc_create(parea);
    lv_obj_set_size(ctrl_pos_ring, 212, 212); // ~10% smaller than the previous 236px ring
    lv_obj_align(ctrl_pos_ring, LV_ALIGN_CENTER, 0, 0);
    lv_arc_set_rotation(ctrl_pos_ring, 270);        // value 0 at the top
    lv_arc_set_bg_angles(ctrl_pos_ring, 0, 360);    // full gray ring
    lv_arc_set_range(ctrl_pos_ring, 0, UI_POSITION_MAX_RAW);
    lv_arc_set_value(ctrl_pos_ring, 0);
    ctrl_pos_raw = 0;
    ctrl_pos_text_ms = 0;
    lv_obj_set_style_arc_color(ctrl_pos_ring, COLOR_BORDER_MUTED, LV_PART_MAIN);
    lv_obj_set_style_arc_width(ctrl_pos_ring, 16, LV_PART_MAIN);
    lv_obj_set_style_arc_opa(ctrl_pos_ring, LV_OPA_TRANSP, LV_PART_INDICATOR); // no value fill
    lv_obj_remove_style(ctrl_pos_ring, NULL, LV_PART_KNOB);                    // no on-stroke knob
    // DISPLAY-ONLY: we do NOT use lv_arc's built-in drag — for a gapless
    // full-circle arc its seam heuristic snaps the value to min/max near the
    // top (the erratic jump). Touch is handled on parea via
    // ctrl_pos_angle_to_raw() (ctrl_pos_math.h), which is continuous.
    lv_obj_clear_flag(ctrl_pos_ring, LV_OBJ_FLAG_CLICKABLE);

    // Handle INSIDE the ring near the inner rim (created after the ring → on
    // top; non-clickable → drags pass through to parea). Charcoal/navy marks
    // an interactive control and matches the slider knobs without a color zone.
    ctrl_pos_dot = flat_cont(parea);
    lv_obj_set_size(ctrl_pos_dot, 24, 24);
    lv_obj_set_style_radius(ctrl_pos_dot, LV_RADIUS_CIRCLE, LV_PART_MAIN);
    lv_obj_set_style_bg_color(ctrl_pos_dot, COLOR_NAVY_BTN, LV_PART_MAIN);
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
    lv_obj_set_style_text_color(pdeg, COLOR_UNIT, LV_PART_MAIN);
    ctrl_pos_update_dot();
    ctrl_pos_update_readout();

    // The ring area is the touch target (parea, not the whole layout — so the
    // limit slider in the right column isn't treated as a ring drag). Press or
    // drag anywhere in it sets the angle.
    lv_obj_add_flag(parea, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(parea, ctrl_pos_drag_cb, LV_EVENT_PRESSED, NULL);
    lv_obj_add_event_cb(parea, ctrl_pos_drag_cb, LV_EVENT_PRESSING, NULL);
    lv_obj_add_event_cb(parea, ctrl_pos_drag_cb, LV_EVENT_RELEASED, NULL);
    lv_obj_add_event_cb(parea, ctrl_pos_press_lost_cb, LV_EVENT_PRESS_LOST, NULL);

    // Right: the same neutral-gray command surface used by LEVEL modes.
    lv_obj_t *pcol = flat_cont(ctrl_pos_layout);
    lv_obj_set_size(pcol, CTRL_RCOL_W, LV_PCT(100));
    lv_obj_set_layout(pcol, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(pcol, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(pcol, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);
    lv_obj_set_style_bg_color(pcol, COLOR_CMD_PANEL, LV_PART_MAIN);
    lv_obj_set_style_bg_opa(pcol, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_radius(pcol, 12, LV_PART_MAIN);
    lv_obj_set_style_pad_all(pcol, 14, LV_PART_MAIN);
    lv_obj_set_style_pad_row(pcol, 18, LV_PART_MAIN); // gap title -> slider (matches rcol)
    lv_obj_add_flag(pcol, LV_OBJ_FLAG_OVERFLOW_VISIBLE);

    lv_obj_t *plrow = flex_row(pcol);
    lv_obj_set_size(plrow, LV_PCT(100), LV_SIZE_CONTENT);
    lv_obj_set_flex_align(plrow, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_END, LV_FLEX_ALIGN_END);
    lv_obj_t *plt = lv_label_create(plrow);
    lv_label_set_text_static(plt, "Iq Limit");
    lv_obj_set_style_text_font(plt, &ui_font_sans20, LV_PART_MAIN);
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
    lv_label_set_text_static(plu, "A");
    lv_obj_set_style_text_font(plu, &ui_font_mono20, LV_PART_MAIN);
    lv_obj_set_style_text_color(plu, COLOR_TEXT_VL, LV_PART_MAIN);

    ctrl_pos_lim_slider = lv_slider_create(pcol);
    lv_obj_set_width(ctrl_pos_lim_slider, LV_PCT(100));
    lv_obj_set_height(ctrl_pos_lim_slider, 12);
    lv_slider_set_range(ctrl_pos_lim_slider, 0, UI_MAX_CURRENT_MA);
    lv_slider_set_value(ctrl_pos_lim_slider, 0, LV_ANIM_OFF);
    style_slider(ctrl_pos_lim_slider);
    lv_obj_set_style_bg_color(ctrl_pos_lim_slider, COLOR_ACCENT, LV_PART_INDICATOR);
    lv_obj_add_event_cb(ctrl_pos_lim_slider, ctrl_pos_limit_cb, LV_EVENT_VALUE_CHANGED, NULL);
    }

    // ---- Action bar — START / STOP, equal width ----
    lv_obj_t *actions = flex_row(root);
    lv_obj_set_size(actions, LV_PCT(100), 54);
    lv_obj_set_style_pad_column(actions, 10, LV_PART_MAIN);
    lv_obj_add_flag(actions, LV_OBJ_FLAG_OVERFLOW_VISIBLE); // button shadows draw outside the row

    // Resting look = stopped (START armed green, STOP idle red-outline); the
    // tick flips emphasis on the running-state transition AND applies real
    // LV_STATE_DISABLED to the non-actionable button (operator review: a
    // button that the logic ignores must not look pressable).
    ctrl_btn_start = create_action_btn(actions, "START", &ctrl_lbl_start);
    lv_obj_set_style_bg_color(ctrl_btn_start, COLOR_OK, LV_PART_MAIN);
    lv_obj_set_style_border_color(ctrl_btn_start, COLOR_OK, LV_PART_MAIN);
    lv_obj_set_style_text_color(ctrl_lbl_start, lv_color_white(), LV_PART_MAIN);
    style_card_shadow(ctrl_btn_start);
    lv_obj_add_event_cb(ctrl_btn_start, action_motor_start, LV_EVENT_CLICKED, NULL);
    ctrl_btn_stop = create_action_btn(actions, "STOP", &ctrl_lbl_stop);
    lv_obj_set_style_bg_color(ctrl_btn_stop, COLOR_CARD_BG, LV_PART_MAIN);
    lv_obj_set_style_border_color(ctrl_btn_stop, COLOR_DANGER, LV_PART_MAIN);
    lv_obj_set_style_text_color(ctrl_lbl_stop, COLOR_DANGER, LV_PART_MAIN);
    style_card_shadow(ctrl_btn_stop);
    lv_obj_add_event_cb(ctrl_btn_stop, action_motor_stop, LV_EVENT_CLICKED, NULL);

    ctrl_apply_mode_view(initial_mode); // boot highlight + per-mode configuration
}

// =============================================================================
// GRAPHS
// =============================================================================

static lv_obj_t *chart_rpm, *chart_iq, *chart_voltage, *chart_irms;
static lv_obj_t *graph_val_rpm, *graph_val_iq, *graph_val_voltage, *graph_val_irms;

// Rolling history for the 4 series (0=RPM, 1=Iq A, 2=voltage V, 3=Irms A),
// collected EVERY chart tick regardless of the active tab so the Graphs screen
// is pre-drawn on entry instead of starting empty (user request). Values are
// the integer the chart plots (same as lv_chart_set_next_value gets). Size ==
// the chart point count (create_chart_widget: 50). New sample appended; when
// full, the oldest is dropped (shift). ~50*4*2 = 400 B.
#define GRAPH_PTS 50
#define GRAPH_CURRENT_MAX_A    2
#define GRAPH_CURRENT_MAX_TEXT "2"

// Graph palette: the surface/identity colors come from the shared semantic
// palette (COLOR_BG / COLOR_CARD_BG / COLOR_BORDER) so the page matches every
// other tab. Only the grid, axis text and the four stable series identities
// stay local to this screen — each trend repeats its hue in its top rule,
// title, live value and trace.
#define GRAPH_COLOR_GRID    lv_color_hex(0xD8DEE7) // softer grid for the tinted plot field
#define GRAPH_COLOR_AXIS    lv_color_hex(0x252A2F)
#define GRAPH_COLOR_SPEED   lv_color_hex(0x075FE8)
#define GRAPH_COLOR_IQ      lv_color_hex(0x00705E)
#define GRAPH_COLOR_VOLTAGE lv_color_hex(0xB84500)
#define GRAPH_COLOR_IRMS    lv_color_hex(0x591A8F)

static int16_t g_hist[4][GRAPH_PTS];
static uint8_t g_hist_n[4]; // valid samples per series (0..GRAPH_PTS)
static uint8_t g_hist_head[4]; // oldest sample; O(1) append after the series fills

static void graph_hist_push(uint8_t s, int16_t v)
{
    if (g_hist_n[s] < GRAPH_PTS)
    {
        uint8_t tail = (uint8_t) (g_hist_head[s] + g_hist_n[s]);
        if (tail >= GRAPH_PTS) tail = (uint8_t) (tail - GRAPH_PTS);
        g_hist[s][tail] = v;
        g_hist_n[s]++;
    }
    else
    {
        g_hist[s][g_hist_head[s]] = v;
        if (++g_hist_head[s] >= GRAPH_PTS) g_hist_head[s] = 0;
    }
}

// Replay a series' history into its (freshly built) chart so it shows on entry.
static void graph_prefill(lv_obj_t *chart, uint8_t s)
{
    if (!chart) return;
    lv_chart_series_t *ser = lv_chart_get_series_next(chart, NULL);
    for (uint8_t i = 0; i < g_hist_n[s]; i++)
    {
        uint8_t slot = (uint8_t) (g_hist_head[s] + i);
        if (slot >= GRAPH_PTS) slot = (uint8_t) (slot - GRAPH_PTS);
        lv_chart_set_next_value(chart, ser, g_hist[s][slot]);
    }
}

// Redesigned from 4 stacked charts to a 2x2 grid (fits the new 20px+ type
// scale without shrinking each chart to a sliver), each with a Y-axis
// min/max readout hugging the card's left edge — user request: show the
// scale at a glance, and keep the axis column narrow so the line itself
// gets the width.
static void create_chart_widget(lv_obj_t *parent, const char *title_text, lv_color_t line_color,
                                metric_id_t metric, int32_t min,
                                int32_t max, const char *min_text, const char *max_text,
                                lv_obj_t **out_chart, lv_obj_t **out_val)
{
    (void) metric;

    lv_obj_t *cont = lv_obj_create(parent);
    lv_obj_set_size(cont, LV_PCT(100), LV_PCT(100)); // cross-axis fill in either a row or column parent
    lv_obj_set_flex_grow(cont, 1);                   // main-axis grow in either a row or column parent
    lv_obj_set_style_bg_color(cont, COLOR_CARD_BG, LV_PART_MAIN);
    lv_obj_set_style_border_width(cont, 0, LV_PART_MAIN);
    lv_obj_set_style_radius(cont, 12, LV_PART_MAIN);
    style_card_shadow(cont);
    lv_obj_set_style_pad_all(cont, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_row(cont, 4, LV_PART_MAIN); // gap between the header and the plot
    lv_obj_clear_flag(cont, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_layout(cont, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(cont, LV_FLEX_FLOW_COLUMN);

    // Header without background (flat card style): title + value use series color
    lv_obj_t *head = flex_row(cont);
    lv_obj_set_size(head, LV_PCT(100), 46); // keep same header height tier to preserve chart geometry & xSPI budget
    lv_obj_set_flex_align(head, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_hor(head, 14, LV_PART_MAIN);

    lv_obj_t *title = lv_label_create(head);
    lv_label_set_text_static(title, title_text);
    lv_obj_set_style_text_font(title, &ui_font_sans20, LV_PART_MAIN);
    lv_obj_set_style_text_color(title, line_color, LV_PART_MAIN);
    lv_obj_set_style_text_letter_space(title, 1, LV_PART_MAIN);

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
    lv_obj_set_style_pad_hor(body, 10, LV_PART_MAIN);
    lv_obj_set_style_pad_bottom(body, 6, LV_PART_MAIN);
    lv_obj_set_style_pad_column(body, 6, LV_PART_MAIN);

    lv_obj_t *axis = flat_cont(body);
    lv_obj_set_size(axis, 40, LV_PCT(100));
    lv_obj_set_layout(axis, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(axis, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(axis, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);
    lv_obj_t *axis_max = lv_label_create(axis);
    lv_label_set_text_static(axis_max, max_text);
    lv_obj_set_style_text_font(axis_max, &ui_font_mono20, LV_PART_MAIN);
    lv_obj_set_style_text_color(axis_max, GRAPH_COLOR_AXIS, LV_PART_MAIN);
    lv_obj_t *axis_min = lv_label_create(axis);
    lv_label_set_text_static(axis_min, min_text);
    lv_obj_set_style_text_font(axis_min, &ui_font_mono20, LV_PART_MAIN);
    lv_obj_set_style_text_color(axis_min, GRAPH_COLOR_AXIS, LV_PART_MAIN);

    lv_obj_t *chart = lv_chart_create(body);
    lv_obj_set_height(chart, LV_PCT(100));
    lv_obj_set_flex_grow(chart, 1);
    lv_obj_set_style_bg_color(chart, COLOR_PLOT_BG, LV_PART_MAIN); // soft plot field, not transparent
    lv_obj_set_style_bg_opa(chart, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_border_width(chart, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_all(chart, 2, LV_PART_MAIN);
    lv_obj_set_style_line_color(chart, GRAPH_COLOR_GRID, LV_PART_MAIN); // division lines
    lv_obj_set_style_line_width(chart, 1, LV_PART_MAIN);
    lv_obj_set_style_line_width(chart, 3, LV_PART_ITEMS); // series line
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
    init_screen_content();

    lv_obj_t *main_cont = flat_cont(parent);
    lv_obj_set_size(main_cont, UI_CONTENT_W, UI_CONTENT_H);
    lv_obj_set_pos(main_cont, 0, 0);
    lv_obj_set_style_pad_hor(main_cont, UI_GAP, LV_PART_MAIN);
    lv_obj_set_style_pad_ver(main_cont, UI_GAP, LV_PART_MAIN);
    lv_obj_add_flag(main_cont, LV_OBJ_FLAG_OVERFLOW_VISIBLE); // row/card shadows draw outside
    lv_obj_set_style_pad_row(main_cont, 12, LV_PART_MAIN);
    lv_obj_set_layout(main_cont, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(main_cont, LV_FLEX_FLOW_COLUMN);

    lv_obj_t *row1 = flex_row(main_cont);
    lv_obj_set_size(row1, LV_PCT(100), 50);
    lv_obj_set_flex_grow(row1, 1);
    lv_obj_set_style_pad_column(row1, 12, LV_PART_MAIN);
    lv_obj_add_flag(row1, LV_OBJ_FLAG_OVERFLOW_VISIBLE); // card shadows draw outside the row
    // Speed follows the 250 RPM product ceiling. Both current plots follow the 2.0 A limit
    // (scaled in deciAmps 0..20 for smooth 10x resolution).
    create_chart_widget(row1, "SPEED", GRAPH_COLOR_SPEED, METRIC_SPEED, 0, UI_MAX_RPM, "0", UI_MAX_RPM_TEXT,
                        &chart_rpm, &graph_val_rpm);
    create_chart_widget(row1, "IQ CURRENT", GRAPH_COLOR_IQ, METRIC_IQ, 0, GRAPH_CURRENT_MAX_A * 10, "0", GRAPH_CURRENT_MAX_TEXT,
                        &chart_iq, &graph_val_iq);

    lv_obj_t *row2 = flex_row(main_cont);
    lv_obj_set_size(row2, LV_PCT(100), 50);
    lv_obj_set_flex_grow(row2, 1);
    lv_obj_set_style_pad_column(row2, 12, LV_PART_MAIN);
    lv_obj_add_flag(row2, LV_OBJ_FLAG_OVERFLOW_VISIBLE); // card shadows draw outside the row
    create_chart_widget(row2, "VOLTAGE", GRAPH_COLOR_VOLTAGE, METRIC_ENERGY, 0, 80, "0", "80",
                        &chart_voltage, &graph_val_voltage);
    create_chart_widget(row2, "RMS CURRENT", GRAPH_COLOR_IRMS, METRIC_IRMS, 0, GRAPH_CURRENT_MAX_A * 10, "0", GRAPH_CURRENT_MAX_TEXT,
                        &chart_irms, &graph_val_irms);

    // Pre-draw from the rolling history so the charts aren't empty on entry.
    graph_prefill(chart_rpm, 0);
    graph_prefill(chart_iq, 1);
    graph_prefill(chart_voltage, 2);
    graph_prefill(chart_irms, 3);

    // Seed the value labels too so they don't flash "-- --" until the round-robin catches up.
    char vb[24], vn[16];
    snprintf(vb, sizeof(vb), "%d rpm", (int) ui_motor_status_fast()->bits.actualRpm);
    label_set_if_changed(graph_val_rpm, vb);
    snprintf(vb, sizeof(vb), "%s A",
             fmt_scaled(vn, sizeof(vn), (int32_t) ui_motor_status_fast()->bits.iqCurrent, 10, 1));
    label_set_if_changed(graph_val_iq, vb);
    snprintf(vb, sizeof(vb), "%s V",
             fmt_scaled(vn, sizeof(vn), (int32_t) ui_motor_status_fast()->bits.voltage, 1000, 2));
    label_set_if_changed(graph_val_voltage, vb);
    snprintf(vb, sizeof(vb), "%s A",
             fmt_scaled(vn, sizeof(vn), (int32_t) ui_motor_status_fast()->bits.phaseCurrentRms, 1000, 1));
    label_set_if_changed(graph_val_irms, vb);
}

// =============================================================================
// DIAGNOSTICS
// =============================================================================

#define FAULT_COUNT 5
#define DIAG_FAULT_MASK (FAULT_OC | FAULT_OV | FAULT_UV | FAULT_OT | FAULT_COMM)
static lv_obj_t *diag_fault_marks[FAULT_COUNT]; // right-side status glyph (check / x), per fault bit
static lv_obj_t *diag_fault_names[FAULT_COUNT];
static lv_obj_t *diag_banner;      // summary rollup strip at the top
static lv_obj_t *diag_banner_icon; // check / x
static lv_obj_t *diag_banner_text; // "ALL SYSTEMS NORMAL" / "FAULT DETECTED"
static lv_obj_t *diag_banner_ratio; // "N / 5 OK"
static uint8_t   diag_last_faults;  // UI-visible subset of MotorStatusFast_t.fault

// Left type-icon per fault bit (0..6), custom alpha images matching the mockup:
// Hall and encoder remain reserved in the wire fault byte but are intentionally
// omitted from this operator-facing page.
static const lv_img_dsc_t * const FAULT_ICONS[FAULT_COUNT] = {
        &ui_icon_bolt, &ui_icon_bolt, &ui_icon_bolt, &ui_icon_thermo, &ui_icon_link,
};
static const char * const FAULT_NAMES[FAULT_COUNT] = {
        "OVERCURRENT", "OVERVOLTAGE", "UNDERVOLTAGE", "OVER TEMP", "RS-485 LINK",
};
static const uint8_t FAULT_BITS[FAULT_COUNT] = {
        FAULT_OC, FAULT_OV, FAULT_UV, FAULT_OT, FAULT_COMM,
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

// Blink only marks a NEW (unacknowledged) fault. After the deadline the row
// settles to steady red — endless blinking for an acknowledged fault is eye
// fatigue without new information (operator review).
#define DIAG_BLINK_MS 6000u
static uint32_t diag_blink_until[FAULT_COUNT]; // 0 = no blink scheduled

// One fault row: [type icon] [name, grows] [status mark] — FIVE full-width
// VERTICAL rectangles stacked down the card (operator redesign: the 2-column
// grid became five horizontal bars, one per fault).
static lv_coord_t diag_col_dsc[] = {LV_GRID_FR(1), LV_GRID_TEMPLATE_LAST};
static lv_coord_t diag_row_dsc[] = {LV_GRID_CONTENT, LV_GRID_CONTENT, LV_GRID_CONTENT, LV_GRID_CONTENT, LV_GRID_CONTENT, LV_GRID_TEMPLATE_LAST};

static void diag_cell(lv_obj_t *parent, int idx)
{
    lv_obj_t *cell = flex_row(parent);
    lv_obj_set_height(cell, LV_SIZE_CONTENT); // else it keeps lv_obj's default height and the row balloons
    lv_obj_set_grid_cell(cell, LV_GRID_ALIGN_STRETCH, 0, 1, LV_GRID_ALIGN_CENTER, idx, 1);
    lv_obj_set_style_pad_ver(cell, 11, LV_PART_MAIN);
    lv_obj_set_style_pad_column(cell, 8, LV_PART_MAIN);
    // Bottom divider between rows — NOT on the last one (RS-485 LINK).
    if (idx < FAULT_COUNT - 1)
    {
        lv_obj_set_style_border_side(cell, LV_BORDER_SIDE_BOTTOM, LV_PART_MAIN);
        lv_obj_set_style_border_color(cell, COLOR_BORDER, LV_PART_MAIN);
        lv_obj_set_style_border_width(cell, 1, LV_PART_MAIN);
    }

    lv_obj_t *ic = lv_img_create(cell);
    lv_img_set_src(ic, FAULT_ICONS[idx]);
    lv_obj_set_style_img_recolor(ic, COLOR_TEXT_VL, LV_PART_MAIN);
    lv_obj_set_style_img_recolor_opa(ic, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_clear_flag(ic, LV_OBJ_FLAG_CLICKABLE);

    lv_obj_t *nm = lv_label_create(cell);
    lv_label_set_text_static(nm, FAULT_NAMES[idx]);
    lv_obj_set_style_text_font(nm, &ui_font_sans20, LV_PART_MAIN);
    lv_obj_set_style_text_color(nm, COLOR_TEXT_L, LV_PART_MAIN);
    lv_obj_set_flex_grow(nm, 1);

    // Status mark: check (OK, green) / x (fault, red) — swapped + recolored in
    // the tick. Alpha image so one recolor tints it, like the mockup's bars ✓.
    lv_obj_t *mk = lv_img_create(cell);
    lv_img_set_src(mk, &ui_icon_check);
    lv_obj_set_style_img_recolor(mk, COLOR_OK, LV_PART_MAIN);
    lv_obj_set_style_img_recolor_opa(mk, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_clear_flag(mk, LV_OBJ_FLAG_CLICKABLE);

    diag_fault_names[idx] = nm;
    diag_fault_marks[idx] = mk;
}

// CLEAR FAULTS has no wire command yet (see action_clear_faults in actions.c) —
// an enabled button that silently does nothing is a real UX hazard. The visible
// surface/shadow is kept on a normal-state wrapper because LVGL's disabled-state
// opacity also fades a button's shadow until it is effectively invisible.
static void diag_clear_button(lv_obj_t *parent)
{
    lv_obj_t *frame = lv_obj_create(parent);
    lv_obj_set_size(frame, LV_PCT(100), 46);
    lv_obj_set_style_bg_color(frame, COLOR_CARD_BG, LV_PART_MAIN);
    lv_obj_set_style_bg_opa(frame, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_border_color(frame, COLOR_BORDER, LV_PART_MAIN);
    lv_obj_set_style_border_width(frame, 1, LV_PART_MAIN);
    lv_obj_set_style_radius(frame, 10, LV_PART_MAIN);
    lv_obj_set_style_pad_all(frame, 0, LV_PART_MAIN);
    lv_obj_clear_flag(frame, LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(frame, LV_OBJ_FLAG_OVERFLOW_VISIBLE);
    style_card_shadow(frame);

    // Transparent child preserves a real disabled button for input semantics;
    // only the wrapper owns the surface so disabled opacity cannot erase shadow.
    lv_obj_t *btn_clear = lv_btn_create(frame);
    lv_obj_remove_style_all(btn_clear);
    lv_obj_set_size(btn_clear, LV_PCT(100), LV_PCT(100));
    lv_obj_center(btn_clear);
    lv_obj_add_state(btn_clear, LV_STATE_DISABLED);
    lv_obj_add_event_cb(btn_clear, action_clear_faults, LV_EVENT_CLICKED, NULL);
    lv_obj_t *lbl_clear = lv_label_create(btn_clear);
    lv_label_set_text_static(lbl_clear, "CLEAR UNAVAILABLE");
    lv_obj_set_style_text_font(lbl_clear, &ui_font_sans22, LV_PART_MAIN);
    lv_obj_set_style_text_color(lbl_clear, COLOR_TEXT_L, LV_PART_MAIN);
    lv_obj_set_style_text_letter_space(lbl_clear, 2, LV_PART_MAIN);
    lv_obj_center(lbl_clear);
}

void create_screen_diagnostics(lv_obj_t *parent)
{
    init_screen_content();
    diag_last_faults = 0xFF;

    lv_obj_t *root = flat_cont(parent);
    lv_obj_set_size(root, UI_CONTENT_W, UI_CONTENT_H);
    lv_obj_set_pos(root, 0, 0);
    lv_obj_set_style_pad_hor(root, UI_GAP, LV_PART_MAIN);
    lv_obj_set_style_pad_ver(root, UI_GAP, LV_PART_MAIN);
    lv_obj_set_style_pad_row(root, UI_GAP, LV_PART_MAIN);
    lv_obj_set_layout(root, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(root, LV_FLEX_FLOW_COLUMN);
    lv_obj_add_flag(root, LV_OBJ_FLAG_OVERFLOW_VISIBLE); // card shadow draws outside

    // ---- Summary banner (health rollup, read before the per-bit detail) ----
    diag_banner = flex_row(root);
    lv_obj_set_size(diag_banner, LV_PCT(100), LV_SIZE_CONTENT);
    lv_obj_set_style_bg_opa(diag_banner, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_border_width(diag_banner, 0, LV_PART_MAIN);
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
    lv_obj_set_style_text_font(diag_banner_text, &ui_font_sans22, LV_PART_MAIN);
    lv_obj_set_style_text_letter_space(diag_banner_text, 1, LV_PART_MAIN);

    lv_obj_t *bspacer = flat_cont(diag_banner);
    lv_obj_set_height(bspacer, 1);
    lv_obj_set_flex_grow(bspacer, 1);

    diag_banner_ratio = lv_label_create(diag_banner);
    label_bind_buffer(diag_banner_ratio, "5 / 5 OK");
    lv_obj_set_style_text_font(diag_banner_ratio, &ui_font_mono20, LV_PART_MAIN);

    // ---- Fault register card: title + 3-column wrapping grid + clear btn ----
    lv_obj_t *card = lv_obj_create(root);
    lv_obj_set_width(card, LV_PCT(100));
    lv_obj_set_flex_grow(card, 1);
    lv_obj_set_style_bg_color(card, COLOR_CARD_BG, LV_PART_MAIN);
    lv_obj_set_style_border_width(card, 0, LV_PART_MAIN);
    lv_obj_set_style_radius(card, 10, LV_PART_MAIN);
    style_card_shadow(card);
    lv_obj_set_style_pad_hor(card, 12, LV_PART_MAIN);
    lv_obj_set_style_pad_ver(card, 12, LV_PART_MAIN);
    lv_obj_set_style_pad_row(card, 8, LV_PART_MAIN);
    lv_obj_clear_flag(card, LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_layout(card, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(card, LV_FLEX_FLOW_COLUMN);

    lv_obj_t *title = lv_label_create(card);
    lv_label_set_text_static(title, "FAULT REGISTER");
    lv_obj_set_style_text_font(title, &ui_font_sans20, LV_PART_MAIN);
    lv_obj_set_style_text_color(title, COLOR_TEXT_VL, LV_PART_MAIN);
    lv_obj_set_style_text_letter_space(title, 1, LV_PART_MAIN);

    lv_obj_t *grid = flat_cont(card);
    lv_obj_set_width(grid, LV_PCT(100));
    lv_obj_set_height(grid, LV_SIZE_CONTENT);
    lv_obj_set_style_pad_column(grid, 8, LV_PART_MAIN);
    lv_obj_set_style_pad_row(grid, 0, LV_PART_MAIN);
    lv_obj_set_grid_dsc_array(grid, diag_col_dsc, diag_row_dsc);
    lv_obj_set_layout(grid, LV_LAYOUT_GRID);
    for (int i = 0; i < FAULT_COUNT; i++) diag_cell(grid, i);

    // CLEAR lives BELOW the fault card (outside the frame), bottom of the page.
    diag_clear_button(root);
}

// =============================================================================
// SETTINGS
// =============================================================================

static lv_obj_t *set_link_dot;
static lv_obj_t *set_link_text;
static lv_obj_t *set_dd_baud;
static lv_obj_t *set_dd_parity;
static lv_obj_t *set_dd_stopbits;
static lv_obj_t *set_btn_save;
static lv_obj_t *set_lbl_save;
static lv_obj_t *set_save_icon;  // white check inside SAVE during the SAVED feedback
static uint8_t   set_last_connected;

// Last COMMITTED configuration — the SAVE button's disabled state compares the
// pending globals against this snapshot. Updated on load and on every save.
static ui_rs485_config_t s_rs485_saved;
static uint32_t          s_save_feedback_until; // transient SAVED/FAILED deadline (0 = none)

static uint8_t rs485_config_dirty(void)
{
    return ui_rs485_baud != s_rs485_saved.baud ||
           ui_rs485_parity != s_rs485_saved.parity ||
           ui_rs485_stopbits != s_rs485_saved.stopbits;
}

// 0 = idle "SAVE", 1 = "SAVED" (green + check icon), 2 = "FAILED" (red),
// 3 = "STOP FIRST" (warn — the motor must be stopped before the UART config
// can change). 1/2/3 are transient: they revert after ~1.5 s (expiry in
// tick_screen_settings). Captions are process-lifetime literals.
static void settings_show_feedback(uint8_t state)
{
    if (!set_btn_save || !set_lbl_save) return;
    if (state == 1u || state == 2u || state == 3u) s_save_feedback_until = lv_tick_get() + 1500u;
    else s_save_feedback_until = 0;
    const char *text = state == 1u ? "SAVED" : state == 2u ? "FAILED" :
                       state == 3u ? "STOP FIRST" : "SAVE";
    lv_color_t color = state == 1u ? COLOR_OK : state == 2u ? COLOR_DANGER :
                       state == 3u ? COLOR_WARN : COLOR_ACCENT;
    label_set_static_if_changed(set_lbl_save, text);
    lv_obj_set_style_bg_color(set_btn_save, color, LV_PART_MAIN);
    if (set_save_icon)
    {
        if (state == 1u) lv_obj_clear_flag(set_save_icon, LV_OBJ_FLAG_HIDDEN);
        else             lv_obj_add_flag(set_save_icon, LV_OBJ_FLAG_HIDDEN);
    }
}

// Sync the SAVE button with the dirty state (real disabled state when nothing
// changed). No "UNSAVED CHANGES" caption — the operator dropped it. A transient
// SAVED/FAILED feedback is left alone until its deadline passes.
static void settings_sync_controls(void)
{
    if (ui_current_tab != TAB_SETTINGS || !set_btn_save) return;
    uint8_t dirty = rs485_config_dirty();
    if (s_save_feedback_until != 0 &&
        (int32_t) (lv_tick_get() - s_save_feedback_until) < 0) return; // feedback still showing
    if (dirty) lv_obj_clear_state(set_btn_save, LV_STATE_DISABLED);
    else       lv_obj_add_state(set_btn_save, LV_STATE_DISABLED);
    settings_show_feedback(0);
}
// One settings row: label on the left, a control/value on the right, with a
// hairline border-bottom — the mockup's .frow list style. Returns the row; the
// caller adds the right-hand widget.
static lv_obj_t *set_frow(lv_obj_t *parent, const char *name)
{
    lv_obj_t *row = flex_row(parent);
    styles_ensure();
    lv_obj_add_style(row, &st_listrow, LV_PART_MAIN); // shared bottom divider
    // All four RS-485 rows share one fixed height. 68 px gives the 44 px
    // dropdown ~12 px breathing room above it AND below it (to the divider).
    lv_obj_set_size(row, LV_PCT(100), 74);
    lv_obj_set_flex_align(row, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_ver(row, 0, LV_PART_MAIN);

    lv_obj_t *lbl = lv_label_create(row);
    lv_label_set_text_static(lbl, name);
    lv_obj_set_style_text_font(lbl, &ui_font_sans20, LV_PART_MAIN);
    lv_obj_set_style_text_color(lbl, COLOR_TEXT_L, LV_PART_MAIN);
    return row;
}

static lv_obj_t *create_dd_row(lv_obj_t *parent, const char *name, const char *options, uint16_t selected)
{
    lv_obj_t *row = set_frow(parent, name);

    lv_obj_t *dd = lv_dropdown_create(row);
    lv_obj_set_width(dd, 280); // full-width Settings card: roomy, right-aligned field
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

// RS-485 link config — the ACTUAL values (not widget indices), exposed via
// screens.h so main.c can configure the UART peripheral. Defaults: 921600 8N1.
uint32_t ui_rs485_baud     = UI_RS485_DEFAULT_BAUD;
uint8_t  ui_rs485_parity   = UI_RS485_DEFAULT_PARITY;   // None
uint8_t  ui_rs485_stopbits = UI_RS485_DEFAULT_STOPBITS; // 1 bit

static ui_rs485_load_cb_t   s_rs485_load_cb;
static ui_rs485_commit_cb_t s_rs485_commit_cb;

// Baud dropdown index → actual baud. Order MUST match the BAUD RATE options
// string below. main.c reads ui_rs485_baud, never the index.
static const uint32_t RS485_BAUDS[] = {9600, 19200, 38400, 57600, 115200, 230400, 460800, 921600};
#define RS485_BAUD_COUNT ((int) (sizeof(RS485_BAUDS) / sizeof(RS485_BAUDS[0])))
static uint16_t rs485_baud_to_index(uint32_t baud);

static uint8_t rs485_config_valid(const ui_rs485_config_t *config)
{
    if (!config || config->parity > 2u || config->stopbits > 1u) return 0;
    for (int i = 0; i < RS485_BAUD_COUNT; i++)
        if (RS485_BAUDS[i] == config->baud) return 1;
    return 0;
}

static void rs485_config_assign(const ui_rs485_config_t *config)
{
    ui_rs485_baud = config->baud;
    ui_rs485_parity = config->parity;
    ui_rs485_stopbits = config->stopbits;
}

void ui_rs485_set_config_hooks(ui_rs485_load_cb_t load_cb, ui_rs485_commit_cb_t commit_cb)
{
    s_rs485_load_cb = load_cb;
    s_rs485_commit_cb = commit_cb;
}

void ui_rs485_load_config(void)
{
    // Preserve legacy firmware that seeds the globals before ui_init() when no
    // storage hook is registered. A registered-but-invalid record falls back
    // to the explicit factory defaults.
    ui_rs485_config_t config = {ui_rs485_baud, ui_rs485_parity, ui_rs485_stopbits};
    if (s_rs485_load_cb)
    {
        ui_rs485_config_t stored = {0};
        if (s_rs485_load_cb(&stored) && rs485_config_valid(&stored))
            config = stored;
        else
        {
            config.baud = UI_RS485_DEFAULT_BAUD;
            config.parity = UI_RS485_DEFAULT_PARITY;
            config.stopbits = UI_RS485_DEFAULT_STOPBITS;
        }
    }
    if (!rs485_config_valid(&config))
    {
        config.baud = UI_RS485_DEFAULT_BAUD;
        config.parity = UI_RS485_DEFAULT_PARITY;
        config.stopbits = UI_RS485_DEFAULT_STOPBITS;
    }
    rs485_config_assign(&config);
    s_rs485_saved = config; // boot-time load IS the committed state
}

uint8_t ui_rs485_save_config(void)
{
    const ui_rs485_config_t config = {ui_rs485_baud, ui_rs485_parity, ui_rs485_stopbits};
    // Safety interlock (P0 review): changing baud/parity/stop bits mid-run can
    // drop the control channel while the motor spins. Only commit when the
    // motor is settled STOPPED with no command pending — otherwise show WHY.
    if (motorCmd.bits.cmd ||
        ui_motor_status_fast()->bits.motorState != MOTOR_STATE_STOPPED)
    {
        settings_show_feedback(3);
        return 0;
    }
    if (!rs485_config_valid(&config) || (s_rs485_commit_cb && !s_rs485_commit_cb(&config)))
    {
        settings_show_feedback(2);
        return 0;
    }
    // With no hook, SAVE still commits the pending globals for simulator and
    // legacy RAM-only integrations. Persistence requires a target callback.
    s_rs485_saved = config;
    settings_show_feedback(1);
    return 1;
}

void ui_rs485_reset_config(void)
{
    const ui_rs485_config_t defaults = {
        UI_RS485_DEFAULT_BAUD, UI_RS485_DEFAULT_PARITY, UI_RS485_DEFAULT_STOPBITS
    };
    rs485_config_assign(&defaults);
    if (ui_current_tab == TAB_SETTINGS)
    {
        if (set_dd_baud) lv_dropdown_set_selected(set_dd_baud, rs485_baud_to_index(UI_RS485_DEFAULT_BAUD));
        if (set_dd_parity) lv_dropdown_set_selected(set_dd_parity, UI_RS485_DEFAULT_PARITY);
        if (set_dd_stopbits) lv_dropdown_set_selected(set_dd_stopbits, UI_RS485_DEFAULT_STOPBITS);
        settings_sync_controls();
    }
}

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
    settings_sync_controls();
}
static void rs485_parity_cb(lv_event_t *e)
{
    ui_rs485_parity = (uint8_t) lv_dropdown_get_selected(lv_event_get_target(e));
    settings_sync_controls();
}
static void rs485_stopbits_cb(lv_event_t *e)
{
    ui_rs485_stopbits = (uint8_t) lv_dropdown_get_selected(lv_event_get_target(e));
    settings_sync_controls();
}

void create_screen_settings(lv_obj_t *parent)
{
    init_screen_content();
    set_last_connected = 0xFF;

    // One full-width RS-485 card. Settings deliberately contains no duplicate
    // motor parameters or fixed INTERFACE row: every visible value here belongs
    // to the actual serial configuration or its live link status.
    lv_obj_t *config = flat_cont(parent);
    lv_obj_set_size(config, 696, 342);
    lv_obj_set_pos(config, UI_GAP, UI_GAP);
    lv_obj_set_style_bg_color(config, COLOR_CARD_BG, LV_PART_MAIN);
    lv_obj_set_style_bg_opa(config, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_border_width(config, 0, LV_PART_MAIN);
    lv_obj_set_style_radius(config, 10, LV_PART_MAIN);
    style_card_shadow(config);
    lv_obj_set_style_pad_hor(config, 20, LV_PART_MAIN);
    lv_obj_set_style_pad_ver(config, 20, LV_PART_MAIN); // even breathing room above/below the divider rows
    lv_obj_set_style_pad_row(config, 2, LV_PART_MAIN);
    lv_obj_set_layout(config, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(config, LV_FLEX_FLOW_COLUMN);
    // SPACE_EVENLY spreads the section label, the four config rows and the
    // button row evenly across the card height — the rows used to bunch at the
    // top with one big dead gap above the buttons.
    lv_obj_set_flex_align(config, LV_FLEX_ALIGN_SPACE_EVENLY, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);

    create_section_label(config, "RS-485 CONFIGURATION");

    // Baud ceiling 921600. Each dropdown is initialised FROM the ui_rs485_*
    // globals (so a boot default set by main.c shows up) and writes back to
    // them on change — see screens.h. Order must match RS485_BAUDS / the codes.
    set_dd_baud = create_dd_row(config, "BAUD RATE",
                                "9600\n19200\n38400\n57600\n115200\n230400\n460800\n921600",
                                rs485_baud_to_index(ui_rs485_baud));
    lv_obj_add_event_cb(set_dd_baud, rs485_baud_cb, LV_EVENT_VALUE_CHANGED, NULL);
    set_dd_parity = create_dd_row(config, "PARITY", "None\nEven\nOdd", ui_rs485_parity);
    lv_obj_add_event_cb(set_dd_parity, rs485_parity_cb, LV_EVENT_VALUE_CHANGED, NULL);
    set_dd_stopbits = create_dd_row(config, "STOP BITS", "1\n2", ui_rs485_stopbits);
    lv_obj_add_event_cb(set_dd_stopbits, rs485_stopbits_cb, LV_EVENT_VALUE_CHANGED, NULL);

    // LINK is the same three-state motorConnected source used by the top dot.
    lv_obj_t *link_row = set_frow(config, "LINK STATUS");
    // It is the final configuration row, so no trailing divider is needed.
    lv_obj_set_style_border_side(link_row, LV_BORDER_SIDE_NONE, LV_PART_MAIN);
    lv_obj_set_style_border_width(link_row, 0, LV_PART_MAIN);
    lv_obj_t *link_val = flex_row(link_row);
    lv_obj_set_size(link_val, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
    lv_obj_set_style_pad_column(link_val, 8, LV_PART_MAIN);
    set_link_dot = flat_cont(link_val);
    lv_obj_set_size(set_link_dot, 9, 9);
    lv_obj_set_style_radius(set_link_dot, LV_RADIUS_CIRCLE, LV_PART_MAIN);
    lv_obj_set_style_bg_color(set_link_dot, COLOR_DANGER, LV_PART_MAIN);
    lv_obj_set_style_bg_opa(set_link_dot, LV_OPA_COVER, LV_PART_MAIN);
    set_link_text = lv_label_create(link_val);
    lv_label_set_text_static(set_link_text, "DISCONNECTED");
    lv_obj_set_style_text_font(set_link_text, &ui_font_mono22, LV_PART_MAIN);
    lv_obj_set_style_text_color(set_link_text, COLOR_DANGER, LV_PART_MAIN);

    // The button row lives BELOW the config card (outside the frame), bottom of
    // the page, with the shared card shadows.
    lv_obj_t *btn_row = flex_row(parent);
    lv_obj_set_size(btn_row, 696, LV_SIZE_CONTENT);
    lv_obj_set_pos(btn_row, UI_GAP, UI_GAP + 342 + UI_GAP);
    lv_obj_set_style_pad_column(btn_row, 8, LV_PART_MAIN);
    lv_obj_add_flag(btn_row, LV_OBJ_FLAG_OVERFLOW_VISIBLE); // button shadows draw outside

    // Primary action: solid ink fill (vs. DEFAULTS' neutral outline below) —
    // gives the pair real hierarchy instead of two same-weight azure buttons.
    // The button carries a white check icon shown during the transient SAVED
    // feedback, and gets a REAL disabled state whenever nothing is dirty.
    set_btn_save = lv_btn_create(btn_row);
    lv_obj_set_flex_grow(set_btn_save, 1);
    lv_obj_set_height(set_btn_save, 48);
    lv_obj_set_style_bg_color(set_btn_save, COLOR_ACCENT, LV_PART_MAIN);
    lv_obj_set_style_border_width(set_btn_save, 0, LV_PART_MAIN);
    lv_obj_set_style_radius(set_btn_save, 10, LV_PART_MAIN);
    style_card_shadow(set_btn_save);
    lv_obj_add_event_cb(set_btn_save, action_save_config, LV_EVENT_CLICKED, NULL);
    // Check icon floats inside the button (no extra flex container — one less
    // heap object); the caption stays centered as before.
    set_save_icon = lv_img_create(set_btn_save);
    lv_img_set_src(set_save_icon, &ui_icon_check);
    lv_obj_add_flag(set_save_icon, LV_OBJ_FLAG_FLOATING);
    lv_obj_set_style_img_recolor(set_save_icon, lv_color_white(), LV_PART_MAIN);
    lv_obj_set_style_img_recolor_opa(set_save_icon, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_clear_flag(set_save_icon, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_flag(set_save_icon, LV_OBJ_FLAG_HIDDEN);
    lv_obj_align(set_save_icon, LV_ALIGN_LEFT_MID, 18, 0);
    set_lbl_save = lv_label_create(set_btn_save);
    lv_label_set_text_static(set_lbl_save, "SAVE");
    lv_obj_set_style_text_font(set_lbl_save, &ui_font_sans22, LV_PART_MAIN);
    lv_obj_set_style_text_color(set_lbl_save, lv_color_white(), LV_PART_MAIN);
    lv_obj_set_style_text_letter_space(set_lbl_save, 1, LV_PART_MAIN);
    lv_obj_center(set_lbl_save);

    lv_obj_t *btn_def = lv_btn_create(btn_row);
    lv_obj_set_flex_grow(btn_def, 1);
    lv_obj_set_height(btn_def, 48);
    lv_obj_set_style_bg_color(btn_def, COLOR_CARD_BG, LV_PART_MAIN); // white outlined button on gray page
    lv_obj_set_style_border_color(btn_def, COLOR_BORDER, LV_PART_MAIN);
    lv_obj_set_style_border_width(btn_def, 1, LV_PART_MAIN);
    lv_obj_set_style_radius(btn_def, 10, LV_PART_MAIN);
    style_card_shadow(btn_def);
    lv_obj_add_event_cb(btn_def, action_load_defaults, LV_EVENT_CLICKED, NULL);
    lv_obj_t *lbl_def = lv_label_create(btn_def);
    // "LOAD DEFAULTS", not "RESET": the button only restores the pending
    // values — nothing is saved until the operator presses SAVE (review).
    lv_label_set_text_static(lbl_def, "LOAD DEFAULTS");
    lv_obj_set_style_text_font(lbl_def, &ui_font_sans22, LV_PART_MAIN);
    lv_obj_set_style_text_color(lbl_def, COLOR_TEXT_L, LV_PART_MAIN);
    lv_obj_set_style_text_letter_space(lbl_def, 1, LV_PART_MAIN);
    lv_obj_center(lbl_def);

    // Apply the dirty state on FIRST build too — otherwise the fresh SAVE
    // button renders enabled until the first dropdown change (P0 review).
    settings_sync_controls();
}

// =============================================================================
// TICKS (monitor / control / graphs / diagnostics / settings)
// =============================================================================

static uint8_t mon_visible_fault_count(uint8_t faults)
{
    uint8_t visible = faults & DIAG_FAULT_MASK;
    uint8_t count = 0;
    while (visible != 0)
    {
        count += visible & 1u;
        visible >>= 1;
    }
    return count;
}

void tick_screen_monitor(ui_lane_t lane)
{
    if (ui_current_tab != TAB_MONITOR || lane != UI_LANE_SLOW)
        return;
    char num[16];

    const MotorStatusFast_t *fast = ui_motor_status_fast();
    int32_t iqCurrent    = (int32_t) fast->bits.iqCurrent;
    int32_t phaseCurrent = (int32_t) fast->bits.phaseCurrentRms;
    int32_t dcBusVoltage = (int32_t) fast->bits.voltage;
    bool all = mon_force_all != 0;
    uint8_t phase = mon_update_phase++ & 1u;
    mon_force_all = 0;

    // Units are static card captions; only the number labels invalidate.
    // After the first full frame, cap each Monitor service to four labels. This
    // halves the xSPI dirty-region burst during START without changing the
    // underlying 10 Hz telemetry producer.
    if (all || phase == 0)
    {
        label_set_if_changed(mon_val_iq,
                             fmt_scaled(num, sizeof(num), iqCurrent, 10, 1));
        label_set_if_changed(mon_val_irms,
                             fmt_scaled(num, sizeof(num), phaseCurrent, 1000, 2));
        label_set_if_changed(mon_val_voltage,
                             fmt_scaled(num, sizeof(num), dcBusVoltage, 1000, 1));
        label_color_if_changed(mon_val_voltage,
                               dcBusVoltage < 12000 ? COLOR_DANGER : COLOR_TEXT_H);
        label_set_if_changed(mon_val_power,
                             fmt_scaled(num, sizeof(num), (int32_t) motor_power_deciwatt(), 10, 1));
    }

    if (all || phase == 1)
    {
        label_set_if_changed(mon_val_speed,
                             fmt_scaled(num, sizeof(num), (int32_t) fast->bits.actualRpm, 1, 0));

        uint32_t run_seconds = ui_runtime_seconds();
        uint32_t hours = run_seconds / 3600u;
        if (hours > 99999u) hours = 99999u;
        snprintf(num, sizeof(num), "%02lu:%02lu:%02lu",
                 (unsigned long) hours,
                 (unsigned long) ((run_seconds / 60u) % 60u),
                 (unsigned long) (run_seconds % 60u));
        label_set_if_changed(mon_val_runtime, num);

        uint8_t fault_count = mon_visible_fault_count((uint8_t) fast->bits.fault);
        snprintf(num, sizeof(num), "%u", (unsigned) fault_count);
        label_set_if_changed(mon_val_faults, num);
        label_color_if_changed(mon_val_faults,
                               fault_count == 0 ? metric_palette[METRIC_HEALTH].ink : COLOR_DANGER);

        uint8_t direction = (uint8_t) fast->bits.dir;
        if (all || direction != mon_last_dir)
        {
            mon_last_dir = direction;
            lv_img_set_src(mon_dir_icon,
                           direction == MOTOR_DIR_REV ? &ui_icon_ccw : &ui_icon_cw);
        }
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

    // MODE is a command setting, so its layout follows motorCmd immediately.
    // This remains fully usable with no connected motor and avoids a stale
    // telemetry echo snapping the controls back to the previous mode.
    uint8_t opMode = (uint8_t) motorCmd.bits.opMode;
    if (!ctrl_mode_supported(opMode)) opMode = OP_MODE_SPEED;
    if (opMode != ctrl_last_mode)
    {
        // A command writer outside this screen may change mode. LEVEL and
        // POSITION have different object trees, so request the same deferred
        // rebuild path used by the mode buttons. SPEED/TORQUE remain in-place.
        if ((ctrl_last_mode == OP_MODE_POSITION) != (opMode == OP_MODE_POSITION))
        {
            ui_request_current_rebuild();
            return;
        }
        ctrl_apply_mode_view(opMode);
    }

    // Active level value (NUMBER only; unit is the static sibling). The reused
    // panel represents ctrl_lvl_mode, so trust ctrlValRaw only when
    // motorCmd.opMode matches it — else show 0. This is the shared-field guard
    // (UART_PROTOCOL.md sec 2.1): all supported modes share ctrlValRaw, so a stale
    // value from another mode must never render here in this mode's unit.
    if (opMode != OP_MODE_POSITION && ctrl_lvl_num)
    {
        int32_t raw    = (motorCmd.bits.opMode == ctrl_lvl_mode) ? (int32_t) motorCmd.bits.ctrlValRaw : 0;
        // Sliders work in raw wire units: RPM for SPEED, mA for TORQUE/Iq.
        int32_t sldval = raw;
        if (ctrl_lvl_mode == OP_MODE_TORQUE)
            fmt_scaled(buf, sizeof(buf), raw, 1000, 1);
        else
            snprintf(buf, sizeof(buf), "%d", (int) sldval);
        label_set_if_changed(ctrl_lvl_num, buf);
        if (ctrl_lvl_slider && !lv_obj_has_state(ctrl_lvl_slider, LV_STATE_PRESSED) &&
            lv_slider_get_value(ctrl_lvl_slider) != sldval)
            lv_slider_set_value(ctrl_lvl_slider, sldval, LV_ANIM_OFF);

        // LIMIT value: TORQUE → speed limit (RPM, integer); SPEED →
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
        if (deciDeg > UI_POSITION_MAX_RAW) deciDeg = UI_POSITION_MAX_RAW;
        if ((!ctrl_pos_area || !lv_obj_has_state(ctrl_pos_area, LV_STATE_PRESSED)) &&
            ctrl_pos_raw != deciDeg)
        {
            ctrl_pos_raw = deciDeg;
            ctrl_pos_update_dot();
            ctrl_pos_update_readout();
        }
        // Current-limit value + slider (mA on wire, A on screen).
        if (ctrl_pos_lim_num)
        {
            int32_t lraw = (motorCmd.bits.opMode == OP_MODE_POSITION) ? (int32_t) motorCmd.bits.limitRaw : 0;
            fmt_scaled(buf, sizeof(buf), lraw, 1000, 1);
            label_set_if_changed(ctrl_pos_lim_num, buf);
            if (ctrl_pos_lim_slider && !lv_obj_has_state(ctrl_pos_lim_slider, LV_STATE_PRESSED) &&
                lv_slider_get_value(ctrl_pos_lim_slider) != lraw)
                lv_slider_set_value(ctrl_pos_lim_slider, lraw, LV_ANIM_OFF);
        }
    }

    // LEVEL readout: actual SPEED + Iq current (two columns; no state/dir here —
    // run-state is on the START/STOP buttons, direction on the FWD/REV toggle).
    // Each column has a level bar: SPEED = %/200 RPM, Iq = %/3.0 A,
    // fill recolored by band (green / amber / red) only on a transition.
    if (ctrl_spd_num)
    {
        int32_t rpm = (int32_t) ui_motor_status_fast()->bits.actualRpm;
        snprintf(buf, sizeof(buf), "%d", (int) rpm);
        label_set_if_changed(ctrl_spd_num, buf);
        int32_t iq = (int32_t) motor_iq_deciamp();
        label_set_if_changed(ctrl_iq_num, fmt_scaled(buf, sizeof(buf), iq, 10, 1));

        int spct = rpm * 100 / UI_MAX_RPM;
        if (spct > 100) spct = 100;
        lv_bar_set_value(ctrl_spd_bar, spct, LV_ANIM_OFF);
        // Utilization bands (operator): <85% signal, 85-100% amber, RED only on
        // a real fault or a telemetry value past the hard limit — a normal RUN
        // near the configured limit must NOT look like a danger.
        uint8_t wire_faults = (uint8_t) ui_motor_status_fast()->bits.fault;
        uint8_t sb = (wire_faults != 0 || rpm > UI_MAX_RPM) ? 2
                   : (rpm >= UI_MAX_RPM * 85 / 100) ? 1 : 0;
        if (sb != ctrl_spd_band)
        {
            ctrl_spd_band = sb;
            lv_obj_set_style_bg_color(ctrl_spd_bar,
                sb == 0 ? metric_palette[METRIC_SPEED].signal : sb == 1 ? COLOR_WARN : COLOR_DANGER, LV_PART_INDICATOR);
        }
        const int32_t iq_max_deciamp = UI_MAX_CURRENT_MA / 100;
        int ipct = (int) ((iq * 100 + iq_max_deciamp / 2) / iq_max_deciamp);
        if (ipct > 100) ipct = 100;
        if (ipct < 0) ipct = 0;
        lv_bar_set_value(ctrl_iq_bar, ipct, LV_ANIM_OFF);
        uint8_t ib = (wire_faults != 0 || iq > iq_max_deciamp) ? 2
                   : (ipct >= 85) ? 1 : 0;
        if (ib != ctrl_iq_band)
        {
            ctrl_iq_band = ib;
            lv_obj_set_style_bg_color(ctrl_iq_bar,
                ib == 0 ? metric_palette[METRIC_IQ].signal : ib == 1 ? COLOR_WARN : COLOR_DANGER, LV_PART_INDICATOR);
        }
    }

    // ---- Safety state: real disabled states + emphasis (operator review).
    // A button the logic ignores must LOOK unpressable, and while the motor is
    // ramping (STARTING/STOPPING) the mode/direction/slider controls lock so
    // the operator cannot stack ambiguous commands on a moving transition.
    uint8_t motorState = (uint8_t) ui_motor_status_fast()->bits.motorState;
    uint8_t running    = motor_is_running() ? 1 : 0;
    uint8_t start_ok   = !(motorCmd.bits.cmd || running || motorState == MOTOR_STATE_FAULT);
    uint8_t stop_ok    = (motorCmd.bits.cmd || running);
    uint8_t locked     = (motorState == MOTOR_STATE_STARTING || motorState == MOTOR_STATE_STOPPING);

    if (start_ok != ctrl_last_start_ok || stop_ok != ctrl_last_stop_ok)
    {
        ctrl_last_start_ok = start_ok;
        ctrl_last_stop_ok  = stop_ok;
        // Armed button = solid fill; the other = outline + real disabled state.
        // START green / STOP red. STOP stays armed whenever a command is out.
        lv_obj_set_style_bg_color(ctrl_btn_start, start_ok ? COLOR_OK : COLOR_CARD_BG, LV_PART_MAIN);
        lv_obj_set_style_border_color(ctrl_btn_start, COLOR_OK, LV_PART_MAIN);
        lv_obj_set_style_text_color(ctrl_lbl_start, start_ok ? lv_color_white() : COLOR_OK, LV_PART_MAIN);
        if (start_ok) lv_obj_clear_state(ctrl_btn_start, LV_STATE_DISABLED);
        else          lv_obj_add_state(ctrl_btn_start, LV_STATE_DISABLED);

        lv_obj_set_style_bg_color(ctrl_btn_stop, stop_ok ? COLOR_DANGER : COLOR_CARD_BG, LV_PART_MAIN);
        lv_obj_set_style_border_color(ctrl_btn_stop, COLOR_DANGER, LV_PART_MAIN);
        lv_obj_set_style_text_color(ctrl_lbl_stop, stop_ok ? lv_color_white() : COLOR_DANGER, LV_PART_MAIN);
        if (stop_ok) lv_obj_clear_state(ctrl_btn_stop, LV_STATE_DISABLED);
        else         lv_obj_add_state(ctrl_btn_stop, LV_STATE_DISABLED);
    }
    if (locked != ctrl_last_locked)
    {
        ctrl_last_locked = locked;
        for (uint16_t i = 0; i < CTRL_MODE_COUNT; i++)
        {
            if (locked) lv_obj_add_state(ctrl_mode_btns[i], LV_STATE_DISABLED);
            else        lv_obj_clear_state(ctrl_mode_btns[i], LV_STATE_DISABLED);
        }
        if (locked)
        {
            if (ctrl_btn_fwd)        lv_obj_add_state(ctrl_btn_fwd, LV_STATE_DISABLED);
            if (ctrl_btn_rev)        lv_obj_add_state(ctrl_btn_rev, LV_STATE_DISABLED);
            if (ctrl_lvl_slider)     lv_obj_add_state(ctrl_lvl_slider, LV_STATE_DISABLED);
            if (ctrl_lim_slider)     lv_obj_add_state(ctrl_lim_slider, LV_STATE_DISABLED);
            if (ctrl_pos_lim_slider) lv_obj_add_state(ctrl_pos_lim_slider, LV_STATE_DISABLED);
        }
        else
        {
            if (ctrl_btn_fwd)        lv_obj_clear_state(ctrl_btn_fwd, LV_STATE_DISABLED);
            if (ctrl_btn_rev)        lv_obj_clear_state(ctrl_btn_rev, LV_STATE_DISABLED);
            if (ctrl_lvl_slider)     lv_obj_clear_state(ctrl_lvl_slider, LV_STATE_DISABLED);
            if (ctrl_lim_slider)     lv_obj_clear_state(ctrl_lim_slider, LV_STATE_DISABLED);
            if (ctrl_pos_lim_slider) lv_obj_clear_state(ctrl_pos_lim_slider, LV_STATE_DISABLED);
        }
    }

    uint8_t direction = (uint8_t) ui_motor_status_fast()->bits.dir;
    if (direction != ctrl_last_dir && ctrl_btn_fwd)
    {
        ctrl_last_dir     = direction;
        lv_obj_t *on      = direction ? ctrl_btn_rev : ctrl_btn_fwd;
        lv_obj_t *off     = direction ? ctrl_btn_fwd : ctrl_btn_rev;
        lv_obj_t *on_ic   = direction ? ctrl_img_rev : ctrl_img_fwd;
        lv_obj_t *off_ic  = direction ? ctrl_img_fwd : ctrl_img_rev;
        // Active = solid accent (nav-pill charcoal) + white icon; idle = white.
        lv_obj_set_style_bg_color(on, COLOR_ACCENT, LV_PART_MAIN);
        lv_obj_set_style_border_color(on, COLOR_ACCENT, LV_PART_MAIN);
        lv_obj_set_style_img_recolor(on_ic, lv_color_white(), LV_PART_MAIN);
        lv_obj_set_style_bg_color(off, COLOR_CARD_BG, LV_PART_MAIN);
        lv_obj_set_style_border_color(off, COLOR_BORDER_MUTED, LV_PART_MAIN);
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
            int32_t iq_deciamp = (int32_t) ui_motor_status_fast()->bits.iqCurrent;
            graph_hist_push(1, (int16_t) iq_deciamp);
            if (on && chart_iq)
            {
                lv_chart_set_next_value(chart_iq, lv_chart_get_series_next(chart_iq, NULL), (int16_t) iq_deciamp);
                snprintf(buf, sizeof(buf), "%s A", fmt_scaled(num, sizeof(num), iq_deciamp, 10, 1));
                label_set_if_changed(graph_val_iq, buf);
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
            int32_t irms_ma = (int32_t) ui_motor_status_fast()->bits.phaseCurrentRms;
            int32_t irms_deciamp = irms_ma / 100;
            graph_hist_push(3, (int16_t) irms_deciamp);
            if (on && chart_irms)
            {
                lv_chart_set_next_value(chart_irms, lv_chart_get_series_next(chart_irms, NULL), (int16_t) irms_deciamp);
                snprintf(buf, sizeof(buf), "%s A", fmt_scaled(num, sizeof(num), irms_ma, 1000, 1));
                label_set_if_changed(graph_val_irms, buf);
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

    // Blink deadline: even when the fault bits did not change, a scheduled
    // "new fault" blink must settle to steady red after DIAG_BLINK_MS.
    uint32_t now = lv_tick_get();
    for (int i = 0; i < FAULT_COUNT; i++)
    {
        if (diag_blink_until[i] != 0 && (int32_t) (now - diag_blink_until[i]) >= 0)
        {
            diag_blink_until[i] = 0;
            stop_blink(diag_fault_marks[i]);
        }
    }

    uint8_t faults = (uint8_t) ui_motor_status_fast()->bits.fault & DIAG_FAULT_MASK;
    if (faults == diag_last_faults) return;

    uint8_t changed = faults ^ diag_last_faults;
    for (int i = 0; i < FAULT_COUNT; i++)
    {
        uint8_t bit = FAULT_BITS[i];
        if (!(changed & bit)) continue;
        bool active = (faults & bit) != 0;
        // Right-side status mark swaps check(green) <-> x(red) image + blinks on
        // a NEW fault (then settles); the name text goes to ink on fault so an
        // active row reads heavier.
        lv_img_set_src(diag_fault_marks[i], active ? &ui_icon_xmark : &ui_icon_check);
        lv_obj_set_style_img_recolor(diag_fault_marks[i], active ? COLOR_DANGER : COLOR_OK, LV_PART_MAIN);
        lv_obj_set_style_text_color(diag_fault_names[i], active ? COLOR_TEXT_H : COLOR_TEXT_L, LV_PART_MAIN);
        if (active)
        {
            start_blink(diag_fault_marks[i]);
            diag_blink_until[i] = now + DIAG_BLINK_MS;
        }
        else
        {
            diag_blink_until[i] = 0;
            stop_blink(diag_fault_marks[i]);
        }
    }
    diag_last_faults = faults;

    // Summary banner rollup over the five visible fault classes.
    uint8_t nfault = 0;
    for (int i = 0; i < FAULT_COUNT; i++) nfault += (faults & FAULT_BITS[i]) != 0;
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
    snprintf(ratio, sizeof(ratio), "%d / 5 OK", (int) (FAULT_COUNT - nfault));
    label_set_if_changed(diag_banner_ratio, ratio);
    lv_obj_set_style_text_color(diag_banner_ratio, fg, LV_PART_MAIN);
}

void tick_screen_settings(ui_lane_t lane)
{
    if (ui_current_tab != TAB_SETTINGS || lane != UI_LANE_SLOW)
        return;

    // Transient SAVED/FAILED feedback reverts to the idle SAVE caption after
    // ~1.5 s.
    if (s_save_feedback_until != 0 &&
        (int32_t) (lv_tick_get() - s_save_feedback_until) >= 0)
    {
        s_save_feedback_until = 0;
        settings_sync_controls();
    }

    uint8_t link_state = link_state_normalize(ui_motor_connected());
    if (link_state == set_last_connected || !set_link_dot || !set_link_text) return;
    set_last_connected = link_state;
    lv_color_t color = link_state_color(link_state);
    lv_obj_set_style_bg_color(set_link_dot, color, LV_PART_MAIN);
    label_set_static_if_changed(set_link_text, link_state_text(link_state));
    lv_obj_set_style_text_color(set_link_text, color, LV_PART_MAIN);
}
