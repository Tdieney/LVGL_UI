#ifndef ACTIONS_H
#define ACTIONS_H

#ifdef __cplusplus
extern "C" {
#endif

#include "lvgl.h"

// Tab bar actions
void action_tab_dashboard(lv_event_t * e);
void action_tab_monitor(lv_event_t * e);
void action_tab_control(lv_event_t * e);
void action_tab_graphs(lv_event_t * e);
void action_tab_diagnostics(lv_event_t * e);
void action_tab_settings(lv_event_t * e);

// Dashboard actions
void action_motor_start(lv_event_t * e);
void action_motor_stop(lv_event_t * e);
void action_motor_dir_fwd(lv_event_t * e);
void action_motor_dir_rev(lv_event_t * e);
void action_motor_speed_change(lv_event_t * e);

// Control screen actions
void action_mode_select(lv_event_t * e); // user_data = (void*)(uintptr_t) op mode 0..3
void action_torque_change(lv_event_t * e);
void action_openloop_change(lv_event_t * e);
void action_position_change(lv_event_t * e);
void action_calibrate(lv_event_t * e);

// Diagnostics / Settings actions
void action_clear_faults(lv_event_t * e);
void action_save_config(lv_event_t * e);
void action_load_defaults(lv_event_t * e);

#ifdef __cplusplus
}
#endif

#endif /* ACTIONS_H */
