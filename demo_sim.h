#ifndef DEMO_SIM_H
#define DEMO_SIM_H

#ifdef __cplusplus
extern "C"
{
#endif

#include "motor_comm_protocol.h"

// Standalone telemetry simulator for demos/exhibitions — fakes the whole
// motor (RPM ramp, FOC currents, temperatures) so the HMI runs alive with no
// hardware attached. Ported from the React prototype's simulation loop.
// Compile with -DUI_DEMO_SIM=1 and call demo_sim_init() after ui_init().
void demo_sim_init(void);

// Runtime on/off (default OFF). Real UART telemetry and private demo telemetry
// remain separate; the UI accessors select one source. Toggling OFF commands a
// safe STOP without erasing the latest real frame. In demo builds, hold the
// top-left brand for about 800 ms to toggle it.
void demo_sim_toggle(void);
int  demo_sim_enabled(void);

const MotorStatusFast_t *demo_sim_status_fast(void);
const MotorStatusSlow_t *demo_sim_status_slow(void);
uint8_t                  demo_sim_connected(void);

#ifdef __cplusplus
}
#endif

#endif /* DEMO_SIM_H */
