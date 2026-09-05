// Demo telemetry simulator — see demo_sim.h. Entire file compiles away
// unless UI_DEMO_SIM=1.
//
// Stands in for the real Motor board: reads `motorCmd` (what the UI just
// asked for) and fabricates private fast/slow status frames (as if a motor were
// replying over UART). UI accessors select those frames only in Demo mode; the
// real UART-owned globals remain independent.
#if defined(UI_DEMO_SIM) && UI_DEMO_SIM

#include "demo_sim.h"
#include "screens.h"

#define SIM_PERIOD_MS 100 // 10 Hz fast-struct cadence (finer gauge/graph motion)

// FLOAT-FREE demo. The old version called sinf() twice per tick + rand() ~7x +
// dozens of float mul/div — expensive on a no-FPU 80MHz MCU (soft-float). This
// rewrite is all integer / fixed-point: a 32-entry int16 sine LUT (linearly
// interpolated) drives the two graph sweeps, the RPM chase and temperatures ease
// with integer division, and there is NO rand() (the sine sweeps supply the
// motion — which also removes the per-tick jitter that used to churn the
// displayed digits and their redraws). Slow-telemetry fields (temps/eff/pwm)
// refresh at ~1 Hz to match the real motorStatusSlow cadence and cut redraws.

// sin(2*pi*i/32) * 1000, i = 0..31.
static const int16_t SIN32[32] = {
       0,   195,   383,   556,   707,   831,   924,   981,
    1000,   981,   924,   831,   707,   556,   383,   195,
       0,  -195,  -383,  -556,  -707,  -831,  -924,  -981,
   -1000,  -981,  -924,  -831,  -707,  -556,  -383,  -195,
};

// Phase is fixed-point: 16 sub-steps per LUT entry, 512 per full period.
// Returns a value in [-1000, 1000], linearly interpolated between LUT entries.
static int sin_lut(uint16_t phase)
{
    uint8_t i = (uint8_t) ((phase >> 4) & 31u);
    uint8_t f = (uint8_t) (phase & 15u);
    int a = SIN32[i], b = SIN32[(i + 1) & 31];
    return a + (b - a) * f / 16;
}

static int32_t  s_state_countdown_ms = -1; // >=0: counting down to next motorState
static uint8_t  s_state_after_countdown;
static int32_t  s_rpm;                      // actual rpm (integer accumulator)
static uint16_t s_vph, s_cph;               // voltage / current sweep phases (fixed-point)
static int16_t  s_mt10 = 253, s_inv10 = 271; // motor / inverter temp in 0.1 C
static uint8_t  s_slow_div;                 // divides SIM_PERIOD_MS down to ~1 Hz for the slow struct
static MotorStatusFast_t s_demo_fast;
static MotorStatusSlow_t s_demo_slow;
static uint8_t           s_demo_connected;
static lv_timer_t       *s_timer;
// Runtime on/off lives in screens.c. The real UART structs are never modified;
// accessors select these private values only while Demo is active.

static void sim_step(lv_timer_t *timer)
{
    (void) timer;

    if (!demo_sim_active) return;

    // Demo boots IDLE (no auto-start) — the operator presses START on the UI.
    //
    // Drive the state machine toward the COMMANDED state every tick
    // (LEVEL-driven, not edge-driven). This tolerates START/STOP pressed
    // mid-transition: pressing START during the STOPPING ramp re-arms STARTING
    // instead of being swallowed. The old edge-detect (`s_lastCmd`) lost a
    // press that landed while motorState was still STARTING/STOPPING, leaving
    // the motor STOPPED with cmd=1 and no pending edge → "STOP then START again
    // won't start". Re-evaluating the level each tick fixes that.
    uint8_t cmd = motorCmd.bits.cmd;
    uint8_t st  = s_demo_fast.bits.motorState;
    if (cmd == 1)
    {
        // Want RUNNING. Arm STARTING from a settled STOPPED or a reversing STOPPING.
        if (st == MOTOR_STATE_STOPPED || st == MOTOR_STATE_STOPPING)
        {
            s_demo_fast.bits.motorState = MOTOR_STATE_STARTING;
            s_state_after_countdown         = MOTOR_STATE_RUNNING;
            s_state_countdown_ms            = 1600;
        }
    }
    else // cmd == 0: want STOPPED. Arm STOPPING from STARTING or RUNNING.
    {
        if (st == MOTOR_STATE_STARTING || st == MOTOR_STATE_RUNNING)
        {
            s_demo_fast.bits.motorState = MOTOR_STATE_STOPPING;
            s_state_after_countdown         = MOTOR_STATE_STOPPED;
            s_state_countdown_ms            = 1100;
        }
    }
    if (s_state_countdown_ms >= 0)
    {
        s_state_countdown_ms -= SIM_PERIOD_MS;
        if (s_state_countdown_ms < 0) s_demo_fast.bits.motorState = s_state_after_countdown;
    }

    // Direction/mode "apply instantly" — immediate echo is close enough for a demo.
    s_demo_fast.bits.dir    = motorCmd.bits.dir;
    s_demo_fast.bits.opMode = motorCmd.bits.opMode;

    bool running = s_demo_fast.bits.motorState == MOTOR_STATE_STARTING ||
                   s_demo_fast.bits.motorState == MOTOR_STATE_RUNNING;

    // Mechanical speed eases toward the SPEED target. In TORQUE mode, use the
    // restored speed limit as a demo-only ceiling and scale it by requested Iq
    // so the mode remains visibly alive without pretending it is position math.
    int32_t target = 0;
    if (motorCmd.bits.opMode == OP_MODE_SPEED)
        target = (int32_t) motorCmd.bits.ctrlValRaw;
    else if (motorCmd.bits.opMode == OP_MODE_TORQUE)
    {
        int32_t iq_ma = (int32_t) motorCmd.bits.ctrlValRaw;
        if (iq_ma > UI_MAX_CURRENT_MA) iq_ma = UI_MAX_CURRENT_MA;
        target = (int32_t) motorCmd.bits.limitRaw * iq_ma / UI_MAX_CURRENT_MA;
    }
    if (running)
    {
        int32_t d = target - s_rpm;
        // d/16 eases; the +/-1 guarantees progress so integer truncation can't
        // stall a few short of the target (|d|<16 would give d/16 == 0).
        s_rpm += d / 16 + (d > 0 ? 1 : (d < 0 ? -1 : 0));
    }
    else
    {
        s_rpm -= s_rpm / 16 + 1; // always ease down to a full stop (no <16 stall)
        if (s_rpm < 0) s_rpm = 0;
    }
    if (s_rpm > UI_MAX_RPM) s_rpm = UI_MAX_RPM;
    s_demo_fast.bits.actualRpm = (uint32_t) s_rpm;

    int32_t load_pct = (s_rpm * 100) / UI_MAX_RPM; // 0..100

    // Bus voltage: slow sweep across the full 12.0..56.0 V chart range (a demo
    // aesthetic — keeps the voltage graph lively). Runs whether or not spinning.
    s_vph += 5; // ~10 s period at 100 ms/step
    s_demo_fast.bits.voltage = (uint32_t) (34000 + 22 * sin_lut(s_vph)); // mV, spans [12000,56000]

    // Iq + total phase RMS current. TORQUE mode follows the requested Iq;
    // SPEED mode retains the smooth load-based demo waveform.
    int32_t cur_mA = 0;
    if (running && motorCmd.bits.opMode == OP_MODE_TORQUE)
    {
        cur_mA = (int32_t) motorCmd.bits.ctrlValRaw;
        if (cur_mA > UI_MAX_CURRENT_MA) cur_mA = UI_MAX_CURRENT_MA;
    }
    else if (running && s_rpm > 5)
    {
        s_cph += 8; // ~6.4 s period
        cur_mA = 11700 + 67 * sin_lut(s_cph) / 10;
        // Ramp electrical load with speed. Previously the very first STARTING
        // sample jumped straight to a large current, dirtying multiple readouts
        // together while the new screen/source was still settling.
        cur_mA = cur_mA * load_pct / 100;
        if (cur_mA < 0) cur_mA = 0;
    }
    if (cur_mA > UI_MAX_CURRENT_MA) cur_mA = UI_MAX_CURRENT_MA;
    s_demo_fast.bits.phaseCurrentRms = (uint32_t) cur_mA;
    s_demo_fast.bits.iqCurrent       = (uint32_t) ((cur_mA + 50) / 100); // 0.1 A/LSB

    // Slow telemetry (temps / efficiency / PWM) at ~1 Hz — matches the real
    // motorStatusSlow rate and keeps the Monitor temp/eff rows from re-drawing
    // 10x/s. Temps ease toward a load-based target; cool toward idle when stopped.
    if (++s_slow_div >= 10)
    {
        s_slow_div = 0;
        if (running && s_rpm > 5)
        {
            int16_t mt_t  = (int16_t) (253 + load_pct * 4); // ~65 C at full load
            int16_t inv_t = (int16_t) (271 + load_pct * 3); // ~57 C at full load
            s_mt10  = (int16_t) (s_mt10  + (mt_t  - s_mt10)  / 6);
            s_inv10 = (int16_t) (s_inv10 + (inv_t - s_inv10) / 6);
            s_demo_slow.bits.efficiency = (uint32_t) (875 + load_pct * 62 / 100); // 0.1 %
            s_demo_slow.bits.pwmDuty    = (uint32_t) (load_pct * 94 / 10);         // 0.1 %
        }
        else
        {
            s_mt10  = (int16_t) (s_mt10  + (253 - s_mt10)  / 8);
            s_inv10 = (int16_t) (s_inv10 + (271 - s_inv10) / 8);
            s_demo_slow.bits.efficiency = 0;
            s_demo_slow.bits.pwmDuty    = 0;
        }
        s_demo_slow.bits.mtTemp  = (uint16_t) (int16_t) s_mt10;
        s_demo_slow.bits.invTemp = (uint16_t) (int16_t) s_inv10;
    }

    s_demo_connected = UI_LINK_CONNECTED;
}

const MotorStatusFast_t *demo_sim_status_fast(void) { return &s_demo_fast; }
const MotorStatusSlow_t *demo_sim_status_slow(void) { return &s_demo_slow; }
uint8_t demo_sim_connected(void) { return s_demo_connected; }

int demo_sim_enabled(void)
{
    return demo_sim_active;
}

void demo_sim_toggle(void)
{
    demo_sim_active = !demo_sim_active;
    if (demo_sim_active)
    {
        // Fresh demo run, IDLE: command STOP + zero setpoint (operator presses
        // START), reset the ramps + temps. No auto-start. Seed the slow-struct
        // temps so Monitor shows 25.3/27.1 C immediately (they only refresh ~1 Hz).
        motorCmd.bits.cmd        = 0;
        motorCmd.bits.ctrlValRaw = 0;
        motorCmd.bits.limitRaw   = 0;
        s_demo_fast.raw          = 0;
        s_demo_slow.raw          = 0;
        s_demo_fast.bits.motorState = MOTOR_STATE_STOPPED;
        s_demo_fast.bits.opMode     = motorCmd.bits.opMode;
        s_state_countdown_ms     = -1;
        s_rpm                    = 0;
        s_slow_div               = 0;
        s_mt10                   = 253;
        s_inv10                  = 271;
        s_demo_slow.bits.mtTemp     = (uint16_t) s_mt10;
        s_demo_slow.bits.invTemp    = (uint16_t) s_inv10;
        s_demo_slow.bits.efficiency = 0;
        s_demo_slow.bits.pwmDuty    = 0;
        s_demo_connected            = UI_LINK_CONNECTED;
    }
    else
    {
        // SAFE idle: command STOP + zero setpoint (so the demo's auto-started
        // RUN can't carry into real-UART mode). Private demo telemetry is
        // cleared; real UART telemetry remains untouched for a clean hand-off.
        motorCmd.bits.cmd        = 0;
        motorCmd.bits.ctrlValRaw = 0;
        motorCmd.bits.limitRaw   = 0;
        s_demo_fast.raw          = 0;
        s_demo_slow.raw          = 0;
        s_demo_connected         = UI_LINK_DISCONNECTED;
        s_rpm                    = 0;
    }
}

void demo_sim_init(void)
{
    if (!s_timer) s_timer = lv_timer_create(sim_step, SIM_PERIOD_MS, NULL);
}

#endif /* UI_DEMO_SIM */
