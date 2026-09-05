#ifndef SCREENS_H
#define SCREENS_H

#ifdef __cplusplus
extern "C"
{
#endif

#include "lvgl.h"
#include "ui.h"
#include "motor_comm.h"

// Fixed 800x480 shell geometry. Keep these public because ui.c owns the
// persistent content host while screens.c lays out both that host's children
// and the top-layer chrome around it.
#define UI_GAP           8
#define UI_HEADER_BAND_H 58
#define UI_HEADER_H      (UI_GAP + UI_HEADER_BAND_H)
#define UI_SIDEBAR_W     80
#define UI_SCREEN_W      800
#define UI_SCREEN_H      480
#define UI_CONTENT_W     (UI_SCREEN_W - UI_SIDEBAR_W - UI_GAP)
#define UI_CONTENT_H     (UI_SCREEN_H - UI_HEADER_H)
#define UI_CONTENT_X     (UI_SIDEBAR_W + UI_GAP)
#define UI_CONTENT_Y     UI_HEADER_H

// Motor's real max speed (GIM6010-8 @ 24V). Shared by screens.c (gauge/graphs
// range, gauge band thresholds, level bar 100% calculation) and demo_sim.c
// (simulated speed ceiling).
#define UI_MAX_RPM      250
#define UI_MAX_RPM_TEXT "250"

// Operational speed setpoint limit for the Control screen sliders (SPEED target
// and TORQUE secondary speed limit). Providing a 200 RPM limit against the 250 RPM
// telemetry scale maintains a safe 50 RPM operating headroom (stays within the safe
// <85% utilization band).
#define UI_CTRL_MAX_RPM 200

// POSITION mode: MotorCmd_t.ctrlValRaw is a target ANGLE at 0.1 deg/LSB, so a
// full turn is 0..3600 (0..360.0 deg) — see motor_comm.h. The Control
// POSITION knob is a full 360deg ring whose value range IS this, mapped 1:1
// onto ctrlValRaw (no conversion; see action_position_change).
#define UI_POSITION_MAX_RAW 3600

// Current ceiling for TORQUE-mode Iq targets and MotorCmd_t.limitRaw in SPEED
// and POSITION modes (1 mA/LSB). Commanded Iq/current and Graphs current scales
// are limited to 2.0 A (2000 mA).
#define UI_MAX_CURRENT_MA 2000

// Common colors — LIGHT theme, palette lifted from Apple's HIG system colors
// High-contrast color palette optimized for 16-bit RGB565 embedded displays.
// Background/border/text values are Apple's actual tokens; DANGER/WARN/OK use
// Apple's documented "increased contrast" tint of each system color since the
// standard vivid tint (e.g. systemGreen #34C759) is tuned for icons/fills and
// fails text contrast on white — the HIG ships a specific darker variant for
// exactly this case.
// **Cheap IPS/TFT legibility pass (2026):** the palette started from Apple HIG
// (near-white grays, hairline borders, chrome == page). On low-cost embedded
// panels — poor contrast ratio, narrow viewing angle, RGB565 quantization —
// those subtle gray-on-gray steps wash out and near-white surfaces are
// indistinguishable. So the neutrals were pushed for CONTRAST, not fidelity:
// a clearly-gray page under white surfaces, a visibly darker border, white
// chrome (not page-colored), and a darker/more-separated text ramp. Semantic
// colors were saturated ~10-20% and WARN brightened so it no longer collapses
// into DANGER after 565 quantization. Don't "restore" the lighter Apple tokens
// — that's exactly the wash-out this pass fixes.
#define COLOR_BG      lv_color_hex(0xE9ECF1) // page: airier, cooler light gray — white cards float on it
                                              // instead of being boxed in; still clearly darker than white
                                              // so card edges stay visible on cheap panels
#define COLOR_TOP_BG  lv_color_hex(0xFFFFFF) // top/tab bars: white now (was == page) — chrome must not be
                                              // gray-on-gray with the page; the page being clearly gray
                                              // keeps white chrome crisp rather than glaring
#define COLOR_CARD_BG lv_color_hex(0xFFFFFF) // white surfaces (cards, chrome, mode buttons, chart cards)
#define COLOR_BORDER  lv_color_hex(0xC2C8D1) // whisper-light hairline — cards read via page/card contrast
                                              // (operator-chosen "floating card" shell), not a heavy outline;
                                              // nudged slightly darker after on-panel review so edges survive
                                              // cheap-panel wash-out and wide viewing angles
#define COLOR_ACCENT  lv_color_hex(0x2C2E33) // deep charcoal — high-contrast solid fill for primary marks
                                              // (primary buttons, active tab/toggle, slider fill, radio)
#define COLOR_DANGER  lv_color_hex(0xD70015) // Apple systemRed — already bold/high-contrast, kept
#define COLOR_WARN    lv_color_hex(0xE24700) // brighter, clearer orange (was 0xC93400 dark orange-red) so
                                              // WARN is unmistakably distinct from DANGER red on cheap panels
// Text ramp: darker overall AND more separated in luminance (H≫M≫L≫VL), so
// tiers don't blur together and the quietest tier is still clearly readable.
#define COLOR_TEXT_H  lv_color_hex(0x141417) // High emphasis — near-black
#define COLOR_TEXT_M  lv_color_hex(0x2E3138) // Medium emphasis
#define COLOR_TEXT_L  lv_color_hex(0x4B4F57) // Labels / subdued
#define COLOR_TEXT_VL lv_color_hex(0x6A6E77) // Very low (units) — still clearly readable, not a faint gray

// The 3 real wire structs from motor_comm.h. main.c transmits
// `motorCmd` periodically and fills `motorStatusFast`/`motorStatusSlow` from
// parsed UART frames. UI actions write motorCmd; UI reads status through the
// active-source accessors below so Demo can remain isolated:
//   motorCmd          — Tx, UI-owned. actions.c writes cmd/dir/opMode/
//                        ctrlValRaw; main.c just sends whatever is in it.
//   motorStatusFast    — Rx, 100ms. actualRpm/iqCurrent/phaseCurrentRms/dir/
//                        motorState/opMode/voltage/fault — filled from UART.
//   motorStatusSlow    — Rx, 1000ms. mtTemp/invTemp/efficiency/pwmDuty —
//                        main.c fills this from UART.
extern MotorCmd_t        motorCmd;
extern MotorStatusFast_t motorStatusFast;
extern MotorStatusSlow_t motorStatusSlow;

// UI-local, NOT a wire field. Keep values 0/1 backward-compatible with the old
// boolean contract; the UART owner may additionally publish NO_RESPONSE after
// a valid link stops replying. The UI maps all three values consistently in the
// top bar and Settings. See UART_PROTOCOL.md sec. 5.
typedef enum
{
    UI_LINK_DISCONNECTED = 0,
    UI_LINK_CONNECTED    = 1,
    UI_LINK_NO_RESPONSE  = 2
} ui_link_state_t;

extern uint8_t motorConnected;

#define UI_RS485_DEFAULT_BAUD     921600u
#define UI_RS485_DEFAULT_PARITY   0u
#define UI_RS485_DEFAULT_STOPBITS 0u

// RS-485 link configuration — dropdowns edit these pending values immediately.
// RESET restores the defaults above in RAM; SAVE commits through the optional
// platform hook below. Values are actual settings, not widget indices.
extern uint32_t ui_rs485_baud;      // bits/s: 9600..921600
extern uint8_t  ui_rs485_parity;    // 0 = None, 1 = Even, 2 = Odd
extern uint8_t  ui_rs485_stopbits;  // 0 = 1 bit, 1 = 2 bits

typedef struct
{
    uint32_t baud;
    uint8_t  parity;
    uint8_t  stopbits;
} ui_rs485_config_t;

// Register before ui_init(). LOAD returns 1 only for a valid stored record.
// COMMIT must apply the UART setting and, if persistence is required, store it
// to non-volatile memory. Returning 0 makes the SAVE button report FAILED.
// Both callbacks are optional so the standalone simulator remains self-contained.
typedef uint8_t (*ui_rs485_load_cb_t)(ui_rs485_config_t *config);
typedef uint8_t (*ui_rs485_commit_cb_t)(const ui_rs485_config_t *config);
void    ui_rs485_set_config_hooks(ui_rs485_load_cb_t load_cb, ui_rs485_commit_cb_t commit_cb);
void    ui_rs485_load_config(void);
uint8_t ui_rs485_save_config(void);
void    ui_rs485_reset_config(void);

// Demo simulator runtime on/off (defined in screens.c, always compiled). Only
// the telemetry accessor selection changes; real UART structs stay untouched.
// Prefer demo_sim_toggle() so safe-idle/reset side effects also run.
extern uint8_t  demo_sim_active;

// Active telemetry source used by every UI read. In production these return
// the real UART-owned globals above. In a UI_DEMO_SIM build they return the
// simulator's private structs while demo mode is active, so UART reception can
// keep running without racing the demo generator during a source hand-off.
const MotorStatusFast_t *ui_motor_status_fast(void);
const MotorStatusSlow_t *ui_motor_status_slow(void);
uint8_t                  ui_motor_connected(void);

#if defined(UI_DEMO_SIM) && UI_DEMO_SIM
// Simulator-only hook used by the regression to exercise the real dropdown
// callback/cache path while no telemetry source is connected.
uint8_t ui_test_control_select_mode(uint8_t mode);
// Current Dashboard utilization band: 0=normal, 1=high, 2=danger.
uint8_t ui_test_dashboard_speed_band(void);
// Current live-arc color, used to lock Dashboard/Control SPEED identity parity.
lv_color_t ui_test_dashboard_speed_color(void);
#endif

// UI-local run timer. It resets on each accepted START command, survives tab
// teardown/rebuild, and is cleared by a device reboot (BSS reset). STOP or a
// confirmed FAULT freezes the elapsed value until the next START.
void     ui_runtime_on_start(void);
void     ui_runtime_on_stop(void);
uint32_t ui_runtime_seconds(void);

// True for STARTING or RUNNING (MotorState_e). The wire protocol has no
// separate "running" flag — motorState alone is the source of truth.
static inline uint8_t motor_is_running(void)
{
    uint8_t state = (uint8_t) ui_motor_status_fast()->bits.motorState;
    return state == MOTOR_STATE_STARTING || state == MOTOR_STATE_RUNNING;
}

// DERIVED display estimate, not a wire field: power isn't transmitted directly.
// The existing product UI estimates it from bus voltage and phase RMS current.
static inline uint32_t motor_power_deciwatt(void)
{
    const MotorStatusFast_t *s = ui_motor_status_fast();
    uint32_t mv = (uint32_t) s->bits.voltage;
    uint32_t ma = (uint32_t) s->bits.phaseCurrentRms;
    return (mv * ma + 50000u) / 100000u;
}

// q-axis current feedback in 0.1 A/LSB from the compact 8-bit fast field.
static inline uint16_t motor_iq_deciamp(void)
{
    return (uint16_t) ui_motor_status_fast()->bits.iqCurrent;
}

// Screen creation functions
void create_screen_dashboard(lv_obj_t *parent);
void create_screen_monitor(lv_obj_t *parent);
void create_screen_control(lv_obj_t *parent);
void create_screen_graphs(lv_obj_t *parent);
void create_screen_diagnostics(lv_obj_t *parent);
void create_screen_settings(lv_obj_t *parent);

// Tick functions — called by ui_tick() for the active screen only, once per
// update lane (see ui_lane_t). Must never create or destroy widgets.
void tick_screen_dashboard(ui_lane_t lane);
void tick_screen_monitor(ui_lane_t lane);
void tick_screen_control(ui_lane_t lane);
void tick_screen_graphs(ui_lane_t lane);
void tick_screen_diagnostics(ui_lane_t lane);
void tick_screen_settings(ui_lane_t lane);

// Common UI creation (Top bar, Tabs)
void create_common_ui(lv_obj_t *parent);

// Highlight the active tab + update the page name in the top bar.
void ui_tabbar_set_active(ui_tab_t tab);

// Slow-lane updates for the shared top bar (motor state text, link dot).
void tick_common_ui(void);

// Boot splash on the top layer; deletes itself after ~1.4s of animation.
void create_splash(void);

#ifdef __cplusplus
}
#endif

#endif /* SCREENS_H */
