#include "actions.h"
#include "ui.h"
#include "screens.h"

ui_tab_t ui_current_tab = TAB_DASHBOARD;

// The tab buttons only REQUEST a switch — the actual clean+rebuild is coalesced
// and throttled in ui_tick() (see ui_request_tab / ui.c). Doing the rebuild
// here, inside the click event, is what let a burst of fast taps thrash the
// LVGL heap and hang the UI. ui_request_tab is cheap and idempotent, so the
// old "already on this tab?" guard is no longer needed (the servicer checks it).
void action_tab_dashboard(lv_event_t * e)   { (void) e; ui_request_tab(TAB_DASHBOARD); }
void action_tab_monitor(lv_event_t * e)     { (void) e; ui_request_tab(TAB_MONITOR); }
void action_tab_control(lv_event_t * e)     { (void) e; ui_request_tab(TAB_CONTROL); }
void action_tab_graphs(lv_event_t * e)      { (void) e; ui_request_tab(TAB_GRAPHS); }
void action_tab_diagnostics(lv_event_t * e) { (void) e; ui_request_tab(TAB_DIAGNOSTICS); }
void action_tab_settings(lv_event_t * e)    { (void) e; ui_request_tab(TAB_SETTINGS); }

// Motor actions write directly into `motorCmd` (Tx, see motor_comm.h
// / screens.h) — no command-callback layer in between. main.c's only job is
// to transmit whatever is currently in `motorCmd` on its own periodic timer.
// Feedback and confirmed motor state/direction come back through
// `motorStatusFast`/`motorStatusSlow` (Rx). The Control mode editor is the one
// exception: it follows the operator's `motorCmd.opMode` immediately so it is
// still usable while disconnected.

void action_motor_start(lv_event_t * e) {
    (void) e;
    // Command state is the immediate source of truth while feedback is still
    // in flight. Without this guard, tapping START twice before the first
    // status echo resets the run-time origin on the second tap. A confirmed
    // FAULT also blocks START — the operator must resolve the fault first
    // (the Control screen mirrors this by dimming the button).
    if (motorCmd.bits.cmd || motor_is_running()) return;
    if (ui_motor_status_fast()->bits.motorState == MOTOR_STATE_FAULT) return;
    motorCmd.bits.cmd = 1;
    ui_runtime_on_start();
}

void action_motor_stop(lv_event_t * e) {
    (void) e;
    // STOP must never depend on a connected/updated status frame. In the
    // START-to-first-echo window (or on a silent link) motor_is_running() can
    // still be false while cmd is already 1; returning there would leave the
    // transmitted command latched at START until a later valid status frame.
    if (!motorCmd.bits.cmd && !motor_is_running()) return;
    motorCmd.bits.cmd = 0;
    ui_runtime_on_stop();
}

void action_motor_dir_fwd(lv_event_t * e) {
    (void) e;
    motorCmd.bits.dir = MOTOR_DIR_FWD;
}

void action_motor_dir_rev(lv_event_t * e) {
    (void) e;
    motorCmd.bits.dir = MOTOR_DIR_REV;
}

// ctrlValRaw is a single field shared by every opMode (see
// motor_comm.h) — moving the speed slider always forces opMode back
// to SPEED so the value is never misinterpreted as e.g. a torque command.
void action_motor_speed_change(lv_event_t * e) {
    lv_obj_t *slider = lv_event_get_target(e);
    int32_t   val    = lv_slider_get_value(slider); // RPM, 1:1 with ctrlValRaw in SPEED mode
    motorCmd.bits.opMode     = OP_MODE_SPEED;
    motorCmd.bits.ctrlValRaw = (uint32_t) val;
}

// TORQUE keeps its on-wire OpMode_e value, but its primary target is now q-axis
// current in mA. The slider operates directly in that raw unit.
void action_torque_change(lv_event_t * e) {
    lv_obj_t *slider = lv_event_get_target(e);
    int32_t   ma     = lv_slider_get_value(slider); // mA, 0..UI_MAX_CURRENT_MA
    motorCmd.bits.opMode     = OP_MODE_TORQUE;
    motorCmd.bits.ctrlValRaw = (uint32_t) ma;
}

// POSITION's ctrlValRaw is a target ANGLE at 0.1 deg/LSB (0..3600 = 0..360 deg,
// see motor_comm.h). The Control POSITION control is an lv_arc rotary
// knob whose value range IS 0..UI_POSITION_MAX_RAW, so the value goes straight
// onto ctrlValRaw — read it with lv_arc_get_value, no conversion.
void action_position_change(lv_event_t * e) {
    lv_obj_t *knob    = lv_event_get_target(e);
    int32_t   deciDeg = lv_arc_get_value(knob); // 0.1 deg/LSB
    motorCmd.bits.opMode     = OP_MODE_POSITION;
    motorCmd.bits.ctrlValRaw = (uint32_t) deciDeg;
}

// Diagnostics still has no clear-fault wire command. RS-485 Settings are local
// to the HMI and commit through the platform hooks documented in screens.h.

void action_clear_faults(lv_event_t * e) {
    (void) e;
    // TODO: no MsgId_e for this yet — motorStatusFast.bits.fault is Rx-only,
    // the UI cannot clear it locally (that would just get overwritten by the
    // next status frame if the real fault condition is still present).
}

void action_save_config(lv_event_t * e) {
    (void) e;
    (void) ui_rs485_save_config();
}

void action_load_defaults(lv_event_t * e) {
    (void) e;
    // RESET edits the pending UI values only. The operator must press SAVE to
    // apply/persist them, preventing an accidental reset tap from changing UART.
    ui_rs485_reset_config();
}
