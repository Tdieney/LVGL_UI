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

// Motor actions write directly into `motorCmd` (Tx, see motor_comm_protocol.h
// / screens.h) — no command-callback layer in between. main.c's only job is
// to transmit whatever is currently in `motorCmd` on its own periodic timer.
// State changes the UI actually SHOWS (motor state, direction, mode) come
// back through `motorStatusFast`/`motorStatusSlow` (Rx), never read back from
// `motorCmd` — see the "wait for telemetry echo" comments in screens.c.

void action_motor_start(lv_event_t * e) {
    (void) e;
    if (motor_is_running()) return;
    motorCmd.bits.cmd = 1;
}

void action_motor_stop(lv_event_t * e) {
    (void) e;
    if (!motor_is_running()) return;
    motorCmd.bits.cmd = 0;
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
// motor_comm_protocol.h) — moving the speed slider always forces opMode back
// to SPEED so the value is never misinterpreted as e.g. a torque command.
void action_motor_speed_change(lv_event_t * e) {
    lv_obj_t *slider = lv_event_get_target(e);
    int32_t   val    = lv_slider_get_value(slider); // RPM, 1:1 with ctrlValRaw in SPEED mode
    motorCmd.bits.opMode     = OP_MODE_SPEED;
    motorCmd.bits.ctrlValRaw = (uint32_t) val;
}

// Switching mode always zeros ctrlValRaw. Without this, the raw value left
// over from whichever mode was active before would get reinterpreted in the
// new mode's units the instant you switch (e.g. "50" meant as a torque
// percent suddenly read back as 50 RPM) — a real safety hazard, not just a
// display glitch. The operator must explicitly set a new value after
// switching mode; nothing carries over.
void action_mode_select(lv_event_t * e) {
    int32_t mode = (int32_t) (uintptr_t) lv_event_get_user_data(e);
    motorCmd.bits.opMode     = (uint32_t) mode;
    motorCmd.bits.ctrlValRaw = 0;
    motorCmd.bits.limitRaw   = 0; // secondary limit is also opMode-shared — clear on switch
}

// The Control torque slider now runs directly in mN.m (0..UI_MAX_TORQUE_MNM),
// and the wire field IS an absolute mN.m magnitude — so the value goes
// straight onto the wire, no percent conversion.
void action_torque_change(lv_event_t * e) {
    lv_obj_t *slider = lv_event_get_target(e);
    int32_t   mnm    = lv_slider_get_value(slider); // mN.m, 0..UI_MAX_TORQUE_MNM
    motorCmd.bits.opMode     = OP_MODE_TORQUE;
    motorCmd.bits.ctrlValRaw = (uint32_t) mnm;
}

// OPEN_LOOP's ctrlValRaw is PWM duty, 0.1%/LSB (motor_comm_protocol.h) — the
// slider is a plain 0-100% control, so scale by 10 going onto the wire.
void action_openloop_change(lv_event_t * e) {
    lv_obj_t *slider = lv_event_get_target(e);
    int32_t   pct    = lv_slider_get_value(slider); // 0-100%
    motorCmd.bits.opMode     = OP_MODE_OPEN_LOOP;
    motorCmd.bits.ctrlValRaw = (uint32_t) (pct * 10);
}

// POSITION's ctrlValRaw is a target ANGLE at 0.1 deg/LSB (0..3600 = 0..360 deg,
// see motor_comm_protocol.h). The Control POSITION control is an lv_arc rotary
// knob whose value range IS 0..UI_POSITION_MAX_RAW, so the value goes straight
// onto ctrlValRaw — read it with lv_arc_get_value, no conversion.
void action_position_change(lv_event_t * e) {
    lv_obj_t *knob    = lv_event_get_target(e);
    int32_t   deciDeg = lv_arc_get_value(knob); // 0.1 deg/LSB
    motorCmd.bits.opMode     = OP_MODE_POSITION;
    motorCmd.bits.ctrlValRaw = (uint32_t) deciDeg;
}

// No wire message exists yet for any of these 4 — motor_comm_protocol.h only
// defines MotorCmd_t (start/stop/dir/opMode/ctrlValRaw). See UART_PROTOCOL.md
// sec. 6 for what each would need before it can do anything real.

void action_calibrate(lv_event_t * e) {
    (void) e;
    // TODO: no MsgId_e for this yet.
}

void action_clear_faults(lv_event_t * e) {
    (void) e;
    // TODO: no MsgId_e for this yet — motorStatusFast.bits.fault is Rx-only,
    // the UI cannot clear it locally (that would just get overwritten by the
    // next status frame if the real fault condition is still present).
}

void action_save_config(lv_event_t * e) {
    (void) e;
    // TODO: no config read/write message defined yet.
}

void action_load_defaults(lv_event_t * e) {
    (void) e;
    // TODO: no config read/write message defined yet.
}
