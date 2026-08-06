#ifndef SCREENS_H
#define SCREENS_H

#ifdef __cplusplus
extern "C"
{
#endif

#include "lvgl.h"
#include "ui.h"
#include "motor_comm_protocol.h"

// Motor's real max speed (GIM6010-8 @ 24V). Shared by screens.c (gauge/slider
// range, gauge band thresholds) and demo_sim.c (simulated speed ceiling) so
// the two can never drift apart the way UI_MAX_RPM/SIM_MAX_RPM once did as
// two separately-maintained constants.
#define UI_MAX_RPM 400

// Motor's real max torque: 11 N.m = 11000 mN.m (confirmed by the user). The
// Control torque slider now runs directly in mN.m (0..UI_MAX_TORQUE_MNM), and
// MotorCmd_t.ctrlValRaw in TORQUE mode IS an absolute mN.m value — so the
// slider value goes straight onto the wire, no conversion (see action_torque_change).
#define UI_MAX_TORQUE_MNM 11000

// POSITION mode: MotorCmd_t.ctrlValRaw is a target ANGLE at 0.1 deg/LSB, so a
// full turn is 0..3600 (0..360.0 deg) — see motor_comm_protocol.h. The Control
// POSITION knob is a full 360deg ring whose value range IS this, mapped 1:1
// onto ctrlValRaw (no conversion; see action_position_change).
#define UI_POSITION_MAX_RAW 3600

// Current-limit ceiling for MotorCmd_t.limitRaw in SPEED/OPEN_LOOP modes
// (1 mA/LSB). 23400 mA = the GIM6010-8's 23.4 A stall current — the highest a
// current limit would ever sensibly be set to. Speed-limit modes (TORQUE/
// POSITION) reuse UI_MAX_RPM for limitRaw instead.
#define UI_MAX_CURRENT_MA 23400

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
#define COLOR_BG      lv_color_hex(0xE1E4EA) // page: a CLEAR light gray so white cards/chrome pop off it
#define COLOR_TOP_BG  lv_color_hex(0xFFFFFF) // top/tab bars: white now (was == page) — chrome must not be
                                              // gray-on-gray with the page; the page being clearly gray
                                              // keeps white chrome crisp rather than glaring
#define COLOR_CARD_BG lv_color_hex(0xFFFFFF) // white surfaces (cards, chrome, mode buttons, chart cards)
#define COLOR_BORDER  lv_color_hex(0xAEB4BE) // visibly darker than a hairline — outlines/dividers must read
                                              // on a cheap panel (old 0xD2D2D7 vanished next to the page)
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

// The 3 real wire structs from motor_comm_protocol.h. main.c transmits
// `motorCmd` periodically and fills `motorStatusFast`/`motorStatusSlow` from
// parsed UART frames. UI actions write motorCmd; UI reads status through the
// active-source accessors below so Demo can remain isolated:
//   motorCmd          — Tx, UI-owned. actions.c writes cmd/dir/opMode/
//                        ctrlValRaw; main.c just sends whatever is in it.
//   motorStatusFast    — Rx, 100ms. actualRpm/dir/motorState/opMode/current/
//                        voltage/fault — main.c fills this from UART.
//   motorStatusSlow    — Rx, 1000ms. mtTemp/invTemp/efficiency/pwmDuty —
//                        main.c fills this from UART.
extern MotorCmd_t        motorCmd;
extern MotorStatusFast_t motorStatusFast;
extern MotorStatusSlow_t motorStatusSlow;

// UI-local, NOT a wire field: the real UART owner sets
// this to 0 if no valid MotorStatusFast_t frame arrives within its receive
// timeout. See UART_PROTOCOL.md sec. 6.
extern uint8_t motorConnected;

// RS-485 link configuration — owned by the Settings screen (the operator picks
// them from the dropdowns), exposed here so main.c can read them back and
// configure the real UART peripheral. They hold the ACTUAL values, not widget
// indices: ui_rs485_baud is the baud in bits/s (e.g. 921600). Parity/stop-bits
// are small codes (documented at their definition in screens.c). main.c may
// also seed these before ui_init() to make a boot default other than 921600/
// None/1 take effect on the dropdowns. Changing a dropdown updates the global
// immediately; re-applying it to the hardware is main.c's job (poll on change,
// or reconfigure on the SAVE button via action_save_config).
extern uint32_t ui_rs485_baud;      // bits/s: 9600..921600
extern uint8_t  ui_rs485_parity;    // 0 = None, 1 = Even, 2 = Odd
extern uint8_t  ui_rs485_stopbits;  // 0 = 1 bit, 1 = 1.5 bits, 2 = 2 bits

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

// True for STARTING or RUNNING (MotorState_e). The wire protocol has no
// separate "running" flag — motorState alone is the source of truth.
static inline uint8_t motor_is_running(void)
{
    uint8_t state = (uint8_t) ui_motor_status_fast()->bits.motorState;
    return state == MOTOR_STATE_STARTING || state == MOTOR_STATE_RUNNING;
}

// DERIVED, not a wire field: power isn't transmitted directly, only current
// + voltage are — compute it from those every time it's displayed.
static inline uint32_t motor_power_deciwatt(void)
{
    const MotorStatusFast_t *s = ui_motor_status_fast();
    uint32_t mv = (uint32_t) s->bits.voltage;
    uint32_t ma = (uint32_t) s->bits.current;
    return (mv * ma + 50000u) / 100000u;
}

// Output torque in N.m from the wire field actualTorque (0.1 N.m/LSB). Wire
// field is a real feedback value now (see motor_comm_protocol.h) — no longer
// estimated from current at the display site.
static inline uint16_t motor_torque_decinm(void)
{
    return (uint16_t) ui_motor_status_fast()->bits.actualTorque;
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
