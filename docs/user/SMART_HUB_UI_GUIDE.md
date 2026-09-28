# Smart Hub UI — Integration and Operation Guide

## Current interface (Quiet Architecture, 2026-09-17)

Use the native simulator and [current captures](../reviews/2026-09-17-quiet-implementation/REPORT.md).
All visible text is at least18px. Overview/Trends/Devices are top tabs; Settings
and Auto limits are focused full-width editors, not small modal dialogs.
Back from Auto retains edits, Back from Settings/Cancel discards, Save commits
the whole draft. Auto limits are editable even in Manual mode. UI configuration
stays in RAM unless application firmware explicitly implements persistence.
The redesign does not require Flash.

**2026-09-25 owner changes:** LoRa protocol v2 (`LORA_PROTO_VERSION 0x02`)
is request/response. The Hub polls with `GET_STATUS` and sends `HUB_CMD` only
for pending commands. Telemetry is eCO₂ in ppm and TVOC in ppb, and the device
configuration schema is v3. The Trends page was simplified: no Current/Min/Max
row and no point tooltip. Sections 4, 7, 8 and 9 below describe the current
behavior. The host simulator and regression tests are implemented and host-verified
(42/42 checks pass; see [PLAN.md](../../PLAN.md) and [delivery report](../reviews/2026-09-25-lora-v2-trends-delivery/REPORT.md)).

This document is intended for engineers **porting this UI to actual STM32 firmware**. Upon reading, you should understand: which functions to call, where to inject data, where to retrieve output data, what the UI handles, what firmware handles, and what remains to be done.

Related documents:
- [`LORA_GUIDE.md`](LORA_GUIDE.md) for radio implementation details
- [`LORA_PROTOCOL.md`](../../LORA_PROTOCOL.md) for data frame layouts
- [`MCU_BASELINE.md`](../MCU_BASELINE.md) for memory footprint metrics

---

## 1. Architectural Principles

Three principles govern the entire design:

**1. The UI reads and writes global variables directly; there is no push API.**
Following the `motor_comm.h` convention from the motor-control project. Firmware copies received radio frames directly into `lora_node_status`, and the UI reads it during `ui_tick()`. The UI writes intended GPIO levels into `lora_hub_cmd`, and firmware transmits them. There is no `ui_update_snapshot()` — it has been removed.

**2. The UI displays what the Node REPORTS, not what the Hub JUST COMMANDED.**
Tapping a switch toggle does not immediately show it as ON. The UI records the intended level, then waits for the Node to report the actual physical level back. Discrepancies display as `Sending…`; after 3 seconds without confirmation, it displays `Unknown` + `Retry`.
Principle: *a user intent is not an execution confirmation*.

**3. Device configuration is Hub-local and never sent over the radio.**
Device names, icon presets, Auto/Manual mode, and `active_low` polarity mapping reside exclusively on the Hub. Changes take effect immediately without `Sending…` or remote acknowledgment. The Node only receives and executes raw 0/1 GPIO levels.

```
        ┌──────────────── Smart Hub (LCD_Shield_LoRa) ────────────────┐
        │                                                             │
 SX1276 │  firmware radio         global variables           ui.c      │   LCD
   ◀────┼──▶ lora_read_packet() ──▶ lora_node_status  ──▶  ui_tick() ─┼──▶ QSPI
        │    lora_send()        ◀── lora_hub_cmd      ◀──             │
        │                          lora_last_snr/rssi                 │   touch
        │                          lora_rx_revision                   │◀──┐
        │                                                             │   │
        │  flash/NVS  ◀── ui_config_changed_cb ── Hub-local config    │   │
        └─────────────────────────────────────────────────────────────┘   │
                                      ▲                                   │
                                      └─── lv_indev read_cb ──────────────┘
```

---

## 2. Source Files Required on Target MCU

`tools/export_mcu.bat` exports this exact manifest. Run `tools/export_mcu.bat <dest_dir>` to create the bundle.

| File | Role |
|---|---|
| `ui.c` / `ui.h` | Complete UI application layer |
| `ui_auto.c` / `ui_auto.h` | Hub-side Auto v1 automation module (pure C, zero-LVGL) |
| `ui_internal.h` | Internal UI state structures (shared with test APIs) |
| `ui_types.h` | Enums and Hub-local device configuration definitions |
| `ui_theme.c` / `.h` | Quiet Architecture palette, shared static styles |
| `ui_icons.c` / `.h` | 4bpp alpha icons |
| `ui_fonts.c` / `.h` | Pre-rendered font tables (see Section 9) |
| `ui_splash_logo.c` | Splash screen logo bitmap |
| `ui_mcu_profile.h` | Screen geometry, draw buffer sizes, heap allocations |
| `lora_comm.h` | Wire protocol structs — **shared with Node project** |
| `lora_hub_link.h` | Hub-side link state and SNR-to-signal-bar mapping |
| `LORA_PROTOCOL.md` | Wire protocol specification |

**DO NOT EXPORT:** anything in `sim_pc/` (`main.c`, `ui_test_api.*`, `ui_test_hooks.h`, simulator `lv_conf.h`). Those are host-only test harnesses.

---

## 3. Boundary of Responsibilities

| Firmware Responsibilities | UI Provided Responsibilities |
|---|---|
| QSPI LCD driver + `flush_cb` | Complete rendering, layouts, animations |
| Touch driver + `lv_indev` `read_cb` | Touch interaction, navigation, dialogs |
| LVGL tick source (same clock as `lora_last_rx_tick_ms`) | Uptime counters, command timeouts, auto timing |
| Radio driver, populating status globals | Telemetry parsing, quality badges, visual status |
| `GET_STATUS` polling; sending `lora_hub_cmd` while a command is pending | Determining contents of `lora_hub_cmd`; `ui_has_pending_command()` |
| Flash/NVS persistence | Managing RAM configuration + change callbacks |
| Sensor polling (on Node side) | Display formatting, metric validity masking |
| Long-term history logging (if required) | Displaying 16-point sliding trend graphs |

---

## 4. Lifecycle & Minimal Boot Code

```c
#include "lvgl.h"
#include "ui.h"

int main(void)
{
    HAL_Init();
    SystemClock_Config();
    MX_GPIO_Init();
    MX_SPI1_Init();          /* LoRa */
    MX_QUADSPI_Init();       /* LCD */
    MX_I2C1_Init();          /* Touch */
    MX_TIM6_Init();          /* 1 ms tick timer */

    lcd_init();
    touch_init();
    lora_reset_and_probe();
    lora_init();

    lv_init();
    lvgl_display_init();     /* Section 5 */
    lvgl_touch_init();       /* Section 6 */

    config_load_from_flash();            /* Section 8 */
    ui_set_config_changed_cb(on_cfg);    /* Section 8 */

    ui_init();               /* Builds shell + initial page, calls lv_scr_load */
    ui_replay_splash();      /* Optional: runs 2.5 s splash screen */

    for (;;) {
        /* 1) Inbound radio frames */
        lora_service_rx();               /* Section 7.1 */

        /* 2) UI & Auto logic: uptime, timeouts, RX reconciliation, Auto evaluation */
        ui_tick();                       /* Polls global variables, updates widgets */
        lv_timer_handler();              /* LVGL render pass */

        /* 3) Outbound radio frames: GET_STATUS poll (always, incl. boot), or
         *    HUB_CMD when ui_is_tx_ready() && ui_has_pending_command() */
        lora_service_tx();               /* LORA_GUIDE.md section 9.2 */
    }
}
```

### Key Lifecycle APIs

**`ui_init()`** — Call **after** `lv_init()` and display driver registration. Initializes themes, top header, bottom navigation bar, toast notifications, active page, and calls `lv_scr_load()`.

**`ui_tick()`** — Call **every loop iteration** in application context. **NEVER call from an ISR**. Performs:
1. Increments uptime and refreshes header clock every second;
2. Scans command timeouts and link staleness;
3. Updates widgets if telemetry or states have changed;
4. Evaluates Auto rule logic and executes automated transitions.

`ui_tick()` is **computationally lightweight when nothing changes** — see Section 10.

**`ui_is_tx_ready()`** — Returns `true` once the first fresh Node report has been reconciled. It becomes `false` again on link loss or when a Node reboot is detected (`uptime_sec` decreased). **Mandatory guard** before sending `HUB_CMD`. Do not use it to gate `GET_STATUS`: the Node replies only to requests, so boot synchronization depends on polling.

**`ui_has_pending_command()`** — Returns `true` while at least one relay command is waiting for Node confirmation (before `UI_CMD_TIMEOUT_MS`). Firmware sends `HUB_CMD` only while this is `true`, with at most three attempts per `seq`. Otherwise it sends `GET_STATUS`.

### LVGL tick source

`ui.c` compares `lora_last_rx_tick_ms` with `lv_tick_get()`, so both must come from one clock. `lora_hub_link.h` documents the RX timestamp as `HAL_GetTick()`. The simplest consistent setup is therefore `LV_TICK_CUSTOM 1` with `LV_TICK_CUSTOM_SYS_TIME_EXPR (HAL_GetTick())`, with no `lv_tick_inc()` call. If you keep a separate 1 ms timer instead, write `lora_last_rx_tick_ms = lv_tick_get()`. Otherwise the link-age subtraction can wrap and show `Disconnected` permanently.

Timer-driven alternative: call `lv_tick_inc(1)` from a 1 ms timer interrupt:

```c
void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim)
{
    if (htim->Instance == TIM6) lv_tick_inc(1);
}
```
This is the only LVGL function permitted inside an ISR.

---

## 5. Display Driver

Screen configuration is defined in [`ui_mcu_profile.h`](../../ui_mcu_profile.h):

```c
#define UI_DISPLAY_HOR_RES  800u
#define UI_DISPLAY_VER_RES  480u
#define UI_COLOR_BYTES      2u        /* RGB565 */
#define UI_DRAW_BUF_LINES   10u
#define UI_DRAW_BUF_PIXELS  (800 * 10)   /* 8000 px */
#define UI_DRAW_BUF_BYTES   16000        /* 16 KB */
#define UI_LVGL_HEAP_BYTES  (42u * 1024u)
```

```c
static lv_color_t s_draw_buf[UI_DRAW_BUF_PIXELS];   /* 16 KB RAM */

static void flush_cb(lv_disp_drv_t *drv, const lv_area_t *area, lv_color_t *px)
{
    uint32_t w = area->x2 - area->x1 + 1;
    uint32_t h = area->y2 - area->y1 + 1;

    lcd_set_window(area->x1, area->y1, area->x2, area->y2);
    lcd_write_pixels_dma((uint16_t *)px, w * h);
    /* Call lv_disp_flush_ready(drv) inside DMA transfer completion ISR */
}

static void lvgl_display_init(void)
{
    static lv_disp_draw_buf_t db;
    lv_disp_draw_buf_init(&db, s_draw_buf, NULL, UI_DRAW_BUF_PIXELS);

    static lv_disp_drv_t dd;
    lv_disp_drv_init(&dd);
    dd.hor_res  = UI_DISPLAY_HOR_RES;
    dd.ver_res  = UI_DISPLAY_VER_RES;
    dd.flush_cb = flush_cb;
    dd.draw_buf = &db;
    lv_disp_drv_register(&dd);
}
```

**Never allocate an 800x480 framebuffer on the MCU.** A full framebuffer requires 768 KB RAM — exceeding typical embedded limits. The display panel contains its own GRAM; a partial buffer of 800x10 is sufficient and represents the tested performance baseline.

**Use DMA for `flush_cb`**, invoking `lv_disp_flush_ready()` in the completion ISR. Blocking CPU transfers will stall the processor during page transitions (~900,000 pixels).

If colors appear inverted (red/blue swapped), toggle `LV_COLOR_16_SWAP` in firmware `lv_conf.h` rather than altering the color palette.

---

## 6. Touch Driver

```c
static void touch_read_cb(lv_indev_drv_t *drv, lv_indev_data_t *data)
{
    uint16_t x, y; bool pressed;
    touch_get_point(&x, &y, &pressed);          /* I2C touch controller driver */

    data->point.x = x;
    data->point.y = y;
    data->state   = pressed ? LV_INDEV_STATE_PRESSED : LV_INDEV_STATE_RELEASED;
}

static void lvgl_touch_init(void)
{
    static lv_indev_drv_t id;
    lv_indev_drv_init(&id);
    id.type    = LV_INDEV_TYPE_POINTER;
    id.read_cb = touch_read_cb;
    lv_indev_drv_register(&id);
}
```

`TP-INT` (`PD25` on schematic) can signal pending touch events to avoid redundant I²C reads. `read_cb` must return the current point and state whenever queried by LVGL.

Minimum touch target sizes: **44x44 px** (Retry, Close buttons) and **54x30 px** (switch pill). If coordinates drift by > 10 px, calibration is required.

---

## 7. Feeding Data INTO the UI

### 7.1 Status & Telemetry from Node

The UI component defines all six shared globals **once in `ui.c`**, matching
the `motorCmd` storage convention on `ui/motor-control`. Firmware includes the
headers and accesses the existing storage; **do not define these variables again**.
If an older integration added definitions in `main.c` or a radio module, remove
those duplicate definitions when upgrading, while retaining the RX/TX code.

```c
#include "ui.h"
#include "lora_comm.h"       /* extern command and status */
#include "lora_hub_link.h"   /* extern RX tick, revision, SNR and RSSI */
```

`ui.c` must be compiled/linked once alongside the other exported UI sources.
The UI uses `#include "lvgl.h"` directly: put the directory containing `lvgl.h`
on the target compiler's include path, with the target's own `lv_conf.h`
configuration. No `LV_LVGL_H_INCLUDE_SIMPLE` define is needed by exported UI code.
The application still owns radio I/O and valid-report publication; storage in
`ui.c` does not implement a radio driver. Call `ui_tick()` before considering TX
and respect `ui_is_tx_ready()` and `ui_has_pending_command()` for `HUB_CMD`;
poll with `GET_STATUS` otherwise. Publish RX in application context, never call
LVGL from an ISR.

Upon receiving a valid status frame:

```c
void lora_service_rx(void)
{
    uint8_t buf[64], len; int8_t snr; int16_t rssi;
    if (!lora_read_packet(buf, &len, &snr, &rssi)) return;

    if (len != LORA_NODE_STATUS_LEN)          return;   /* Frame size validation */
    if (buf[0] != LORA_PROTO_VERSION)         return;   /* 0x02: rejects v1 Nodes */
    if (buf[1] != LORA_MSG_NODE_STATUS)       return;

    memcpy(lora_node_status.bytes, buf, LORA_NODE_STATUS_LEN);
    lora_last_snr        = snr;
    lora_last_rssi       = rssi;               /* int16_t: may be below -128 dBm */
    lora_last_rx_tick_ms = lv_tick_get();      /* Same clock as LVGL (Section 4) */
    lora_rx_revision++;                        /* MANDATORY TRIGGER */
}
```

> **`lora_rx_revision++` triggers UI widget re-evaluation.** `ui_tick()` compares this counter with the previous render revision; if unchanged, widget updates are bypassed. Forgetting to increment causes the display to remain static even when new packets arrive.

### 7.2 Field Definitions

| Field | Units | UI Application |
|---|---|---|
| `co2_ppm` + `co2_valid` | eCO₂ ppm (valid `400..60000`) | eCO₂ hero card, trends, Fan Auto |
| `tvoc_ppb` + `voc_valid` | TVOC ppb (valid `0..60000`) | TVOC hero card, trends, Purifier Auto |
| `temp_deci_c` + `temp_valid` | 0.1 °C, **signed** | Temperature comfort card & trends |
| `humidity_pct` + `humid_valid` | Integer % | Humidity comfort card, trends, Humidifier Auto |
| `relay1..4_gpio` | Raw GPIO 0/1 | Physical relay readback state |
| `seq_echo` | Counter | Confirmation that Node executed current command |
| `uptime_sec` | Seconds | A decrease is treated as a Node reboot (re-sync) |
| `sample_seq` | Counter | New sensor sample; only new samples advance Auto qualification |
| `rs485_fault`, `co2_timeout`, `th_timeout` | Flags | Fault masking and Auto suspension |

Values outside the valid range are treated like `valid = 0` for that metric.

**The `*_valid` bits are essential.** If a sensor fails, set `valid = 0`. The UI will render an em-dash `—` and `Unknown` status **exclusively for that metric**, while other metrics continue updating normally. Do not transmit a value of 0 to represent invalid data.

### 7.3 Inferred Connection State

Link status is inferred: `lv_tick_get() - lora_last_rx_tick_ms < LORA_LINK_TIMEOUT_MS` (5000 ms).
There is no manual connection flag. Updating `lora_last_rx_tick_ms` on valid packets handles link status automatically. In v2 packets arrive only as replies, so the link stays `Connected` only while the Hub keeps polling with `GET_STATUS`.

Upon link loss, the UI automatically hides telemetry values, displays `Unknown`, and **inhibits manual switch toggles**.

### 7.4 Historical Trends Data & Autonomous Sampling

The Trends screen displays a 16-point time series for each of the 4 air metrics (eCO₂, TVOC, Temperature, Humidity).

**Page layout (2026-09-25):**
- The left rail has four selector cards. Each shows the metric's live value and unit, or an em dash when unavailable. The selected card is highlighted in solid forest green (`UI_COLOR_FOREST`) with white text/icons and a celadon indicator dot at (160, 14); unselected cards use porcelain styling with ink text.
- Selection happens on touch-down (`LV_EVENT_PRESSED`). The selector card changes state immediately. The chart card is redrawn once, after taps have been quiet for `UI_TRENDS_SETTLE_MS` (150 ms, overridable at compile time), so rapid taps do not render every intermediate metric.
- The chart card shows the metric title at (20, 14) and, in a fixed top-right slot (-20, 23), a `Data unavailable` badge when the live value is missing. Below them are the unit at (12, 96), three right-aligned round-number Y-axis labels at y 136/191/246, the line chart at (72, 136) and four time labels at y 276. There is no Current/Min/Max row, no coverage/span label and no point-inspection tooltip; the live value appears only on the selector card.
- The chart uses LVGL's native line and 4 px markers, with no custom area shading. Values are normalized to a 0..1000 plot range using 64-bit arithmetic, so large eCO₂/TVOC values cannot overflow the 16-bit `lv_coord_t`. The Y-axis labels use round numbers (`lo`, `lo+s`, `lo+2s` with $s \in \{1, 2, 2.5, 5\} \times 10^k$) matching the 3 grid lines.
- With no valid samples the card shows `No readings yet`. With exactly one it shows `Collecting readings…` and the hint `The trend appears after the next reading`, instead of a lone point.

1. **Autonomous Session History (Default):**
   - The UI automatically accumulates session history within `ui.c` (16 points $\times$ 4 metrics) without requiring external firmware feeds or Flash persistence.
   - On the first accepted valid report after boot, the UI captures one reading (`count = 1`). The chart appears from the second valid reading.
   - On subsequent minutes, the UI advances to the next slot (1 slot per elapsed session minute). Within the current minute the newest slot follows the latest valid reading, so the rightmost `Now` point and the Y range track the live value within about one report (owner decision 2026-09-26). When the minute ends, the slot keeps its last valid reading. An unavailable reading never erases a minute that already has data, and an unchanged value does not trigger a redraw. If packets are dropped or link drops, elapsed minutes are retained as gaps (`valid = false`), avoiding synthetic interpolation.
   - Time labels for autonomous history format across a fixed 15-minute right-aligned window ending at `Now` (`-15 min`, `-10 min`, `-5 min`, `Now`) using fonts $\ge 18\text{px}$, with newest sample mapped to slot 15.
   - Min/max of the retained valid points are used only to scale the plot and calculate round-number Y-axis labels; they are no longer shown as statistics.

2. **External History Feed (`ui_feed_history`):**
   ```c
   void ui_feed_history(ui_metric_id_t metric,
                        const int32_t *values, const bool *valid,
                        uint16_t count, uint16_t start_minute);
   ```
   - If application firmware maintains an external ring buffer (e.g., from an RTC or host synchronized database), calling `ui_feed_history()` takes exclusive precedence for that metric.
   - Capacity: **16 points** per metric (`UI_HISTORY_CAPACITY`). If `count > 16`, newest window is retained.
   - `start_minute`: Minute of day (`14*60+17` = 14:17). External history displays wall-clock timestamps (`%02u:%02u`).
   - `valid[i] = false` produces a gap in the line chart.

---

## 8. Hub-local Configuration & Automation (Auto v1)

```c
#define UI_CONFIG_SCHEMA_VERSION 0x03u

typedef struct {
    char name[24];
    uint8_t preset;            /* 0..7, see ui_preset_id_t */
    ui_device_mode_t mode;     /* UI_MODE_AUTO (0) / UI_MODE_MANUAL (1) */
    uint8_t active_low;        /* 0 = Active High (GPIO 1 is ON), 1 = Active Low (GPIO 0 is ON) */
    uint8_t schema_version;    /* UI_CONFIG_SCHEMA_VERSION = 0x03 */
    int32_t auto_on_thresh;    /* Turn-on threshold (eCO2 ppm, TVOC ppb, %RH) */
    int32_t auto_off_thresh;   /* Turn-off threshold */
} ui_hub_device_config_t;

const ui_hub_device_config_t *ui_get_device_config(uint8_t idx);
bool ui_set_device_config(uint8_t idx, const ui_hub_device_config_t *cfg);
void ui_set_config_changed_cb(ui_config_changed_cb_t cb);
bool ui_is_tx_ready(void);
bool ui_has_pending_command(void);
```

The v3 layout is byte-identical to v2; only the purifier threshold unit changed (VOC index → TVOC ppb). `ui_set_device_config()` accepts a v2 record, sets `schema_version` to `0x03` and, for an Air Purifier, replaces its thresholds with the ppb defaults (660/220). There is no valid index-to-ppb conversion. Other presets keep their values. A restore at boot does not fire `ui_config_changed_cb`. To avoid repeating the migration on every boot, firmware should rewrite the record from `ui_get_device_config(i)` when the stored schema was `0x02`.

### 8.1 Auto v1 Automation Rules (Hub-side)

`ui_auto.c` executes in pure C on the Hub. It is evaluated when a status report carries a new sensor sample (`sample_seq` changed, or the first sample after boot, reconnect or Node reboot):

1. **Supported Appliances:**
   - **Ventilation Fan:** Evaluates eCO₂ (ppm). Turns ON when >= `auto_on_thresh` (default 1000), turns OFF when <= `auto_off_thresh` (default 800). Bounds: 0..60000 ppm, step 50 ppm, minimum ON - OFF gap: 50 ppm. Readings below 400 ppm are treated as invalid.
   - **Air Purifier:** Evaluates TVOC (ppb; 0 is a valid reading). Turns ON when >= `auto_on_thresh` (default 660 ppb, the Moderate/Poor boundary), turns OFF when <= `auto_off_thresh` (default 220 ppb, back inside Good). Bounds: 1..60000 ppb, step 5 ppb, minimum ON - OFF gap: 5 ppb.
   - **Humidifier:** Evaluates Humidity %RH (inverted logic: activates when dry, turns off when moist). Turns ON when <= `auto_on_thresh` (default 40%), turns OFF when >= `auto_off_thresh` (default 50%). Bounds: 0%..100%, step 1%, minimum OFF - ON gap: 5% RH. If current humidity is above 50% while OFF, it correctly remains OFF.
   - **Other Presets (Dehumidifier, Heater, Light, Smart Socket, Generic):** Manual mode only. Auto selection is disabled with caption `"Manual only for this device type"`.

2. **Hysteresis & Anti-Hunting Protection:**
   - **3 Consecutive Qualifying Samples:** Requires 3 consecutive new sensor samples (distinct `sample_seq`) meeting threshold conditions before triggering a relay state change. Repeated replies with the same `sample_seq` do not count. Faults or values entering the deadband reset the qualification counter.
   - **Confirmed Transition Hold:** The 10-second anti-hunting hold begins ONLY upon actual confirmed physical relay transitions (echo sequence and reported GPIO match), link reconnection, or sensor recovery. Switching from Manual to Auto in settings arms immediately without an artificial 10s wait, evaluating on the next 3 qualifying frames.
   - **Sensor Fault Masking:** When the associated sensor reports invalid telemetry (`valid = false`, RS-485 bus fault, or timeout), Auto suspends transitions and maintains current relay state. Recovery from suspension executes a single 10-second hold before re-evaluating.

3. **Manual Override:**
   - Toggling a device switch on Home or Devices instantly switches that device to `Manual` mode, updates the snapshot, and cancels pending Auto timers. Auto remains disarmed until explicitly re-enabled in settings.

4. **Error Latching & Re-arm (Auto Paused & Retry):**
   - If an automated relay command times out (> 3000 ms), the Hub displays `"Auto paused"` in amber alongside a `"Retry"` button.
   - Tapping `"Retry"` verifies current reported GPIO readback from Node, clears the error latch, and safely re-arms Auto evaluation without re-transmitting stale commands.

### 8.2 Dialog Navigation & Threshold Configuration

- On the Devices tab, tapping the gear icon opens the **Device settings** modal (440x368).
- If the preset supports Auto, a `"Set limits..."` button appears below the mode selector.
- Tapping `"Set limits..."` opens the **Auto thresholds** subview within the same modal bounds (440x368, zero additional heap allocation):
  - Metric name and units (`eCO₂` ppm, `TVOC` ppb, `Humidity` %RH).
  - ON and OFF threshold values in 24px bold font, with `+` / `—` stepper buttons (48x48px touch targets).
  - `"Reset limits"` button restoring preset defaults.
  - Red inline validation warning if threshold gap rules are violated (e.g. ON must exceed OFF by >= 50 ppm). Save is disabled during validation errors.
  - Back button (`<`) returns to Device settings preserving drafts.
  - Close button (`X`) discards all uncommitted drafts.
  - `"Save settings"` commits changes and fires `ui_config_changed_cb`.

### 8.3 `active_low` Polarity Mapping

The UI handles bidirectional conversion between application logic and physical GPIO levels:

```
outbound_gpio = desired_on XOR active_low
reported_on   = inbound_gpio XOR active_low
```

Set `active_low = 1` when loads connect to relay Normally Closed (**NC**) terminals or when driver circuitry inverts logic levels.

### 8.4 Flash Persistence Implementation

The UI maintains active configurations in RAM and invokes registered callbacks when saved:

```c
static void on_cfg(uint8_t idx, const ui_hub_device_config_t *cfg)
{
    nvs_write_device_config(idx, cfg);     /* Firmware implementation */
}

/* Bounded length & version-aware restore reference implementation */
#define NVS_MAX_REC_LEN 64

typedef enum {
    NVS_RECORD_KIND_UNKNOWN    = 0,
    NVS_RECORD_KIND_CURRENT    = 1,   /* v2 or v3 layout (identical size) */
    NVS_RECORD_KIND_LEGACY_V1  = 2
} nvs_record_kind_t;

static uint8_t get_channel_default_preset(uint8_t channel_idx)
{
    switch (channel_idx) {
        case 0:  return UI_PRESET_PURIFIER;
        case 1:  return UI_PRESET_FAN;
        case 2:  return UI_PRESET_HUMIDIFIER;
        case 3:  return UI_PRESET_LIGHT;
        default: return UI_PRESET_GENERIC;
    }
}

static void config_load_from_flash(void)
{
    for (uint8_t i = 0; i < UI_DEVICE_COUNT; i++) {
        uint8_t raw_buf[NVS_MAX_REC_LEN];
        size_t rec_len = 0;
        nvs_record_kind_t kind = NVS_RECORD_KIND_UNKNOWN;

        if (!nvs_read_device_record(i, raw_buf, sizeof(raw_buf), &rec_len, &kind)) {
            /* Missing record: populate default configuration in Manual mode */
            ui_hub_device_config_t def_cfg;
            ui_auto_get_defaults(i, get_channel_default_preset(i), &def_cfg);
            def_cfg.mode = UI_MODE_MANUAL;
            ui_set_device_config(i, &def_cfg);
            continue;
        }

        ui_hub_device_config_t cand;
        bool candidate_ready = false;

        if (kind == NVS_RECORD_KIND_CURRENT && rec_len == sizeof(ui_hub_device_config_t)) {
            uint8_t ver = raw_buf[offsetof(ui_hub_device_config_t, schema_version)];
            if (ver == UI_CONFIG_SCHEMA_VERSION || ver == 0x02u) {  /* v2 migrated by UI */
                memcpy(&cand, raw_buf, sizeof(ui_hub_device_config_t));
                candidate_ready = true;
            }
        } else if (kind == NVS_RECORD_KIND_LEGACY_V1 && rec_len == sizeof(ui_hub_device_config_v1_t)) {
            ui_hub_device_config_v1_t leg;
            memcpy(&leg, raw_buf, sizeof(leg));
            if (ui_auto_migrate_legacy_config(&leg, sizeof(leg), &cand)) {
                candidate_ready = true;
            }
        }

        if (!candidate_ready || !ui_set_device_config(i, &cand)) {
            ui_hub_device_config_t def_cfg;
            ui_auto_get_defaults(i, get_channel_default_preset(i), &def_cfg);
            def_cfg.mode = UI_MODE_MANUAL;
            ui_set_device_config(i, &def_cfg);
        }
    }
}
```

> **Callback Policy:** Flash restore operations do not trigger callbacks; rejected edits, no-ops, drafts, and cancellations trigger 0 callbacks; only successful user Save actions invoke the callback.

Invoke `config_load_from_flash()` **before** `ui_init()` (supports pre-init staging).
Firmware may poll with `GET_STATUS` from boot. It sends `HUB_CMD` only after `ui_is_tx_ready()` returns `true`, and only while `ui_has_pending_command()` is `true`.

---

## 9. Extracting Output Data FROM the UI

### 9.1 Relay Commands

The UI writes directly into `lora_hub_cmd` upon switch toggles or automated transitions:
- Sets corresponding GPIO bit (mapped through `active_low`),
- Increments `lora_hub_cmd.bits.seq`.

Firmware sends `lora_hub_cmd` as `HUB_CMD` when `ui_is_tx_ready() && ui_has_pending_command()`. It sends immediately on a new `seq`, then retries up to three attempts in total within `UI_CMD_TIMEOUT_MS`. At all other times, including boot and after a timeout, it sends the read-only `GET_STATUS` request at about 1 Hz. No transmit history buffer is required, because `lora_hub_cmd` always holds the complete desired level set. See [`LORA_GUIDE.md`](LORA_GUIDE.md) section 9.2 for a reference loop.

On timeout (`Unknown` + `Retry`) the UI writes the reported GPIO level back into `lora_hub_cmd`, so the failed level is not re-sent by accident.

### 9.2 Desired vs Reported State Machine

| Condition | Visual Representation |
|---|---|
| `reported == desired` | Normal switch toggle |
| Mismatch, < 3 s | `Sending…` (amber), dimmed switch |
| Mismatch, >= 3 s | `Unknown` + `Retry`, hidden switch |
| Link stale | `Unknown` + em-dash `—`, switches disabled |

`UI_CMD_TIMEOUT_MS` = 3000 ms in `lora_comm.h` (configured for SF7–SF10 airtime). **If using SF11/SF12, increase this timeout** to prevent premature command timeout errors.

If the Node reports a state change without a Hub command (local physical override, reboot), the UI **adopts** that reported state and synchronizes `lora_hub_cmd` — the Node is the authoritative source of hardware state. A decrease in `uptime_sec` is treated as a Node reboot: TX readiness is dropped and all four reported levels are adopted together before commands resume.

### 9.3 Status Getters (Optional)

```c
const char *ui_get_uptime_str(void);       /* "HH:MM:SS" */
uint8_t     ui_get_link_bars(void);        /* 0 = Disconnected, 1..4 bars */
const char *ui_get_link_status_str(void);  /* "Connected" / "Disconnected" */
```

### 9.4 Navigation & Notifications

```c
void ui_navigate_to_page(ui_page_id_t page);   /* HOME / TRENDS / DEVICES */
void ui_select_trend_metric(ui_metric_id_t m);
void ui_replay_splash(void);
void ui_announce(const char *msg);             /* 2.6 s toast */
void ui_dismiss_toast(void);
```

`ui_navigate_to_page()` is safe to call inside event callbacks (internally deferred via `lv_async_call()`).

---

## 10. Memory & Performance

### Simulator Benchmark Metrics (42 KiB Heap)

| View | Used Heap | Free Heap | Largest Block | Fragmentation |
|---|---|---|---|---|
| Home | 80% | 8,984 B | 7,120 B | 21% |
| Trends (with data) | 58% | 18,112 B | 11,304 B | 38% |
| Devices | 71% | 12,520 B | 10,656 B | 15% |
| Devices + dialog | 82% | 8,136 B | 7,288 B | 11% |

Measured heap drift after 50 page navigation cycles: **0 bytes** (threshold: 64 B).

### Render Costs (Flush Counts)

| Scenario | Flushes | Pixels Drawn |
|---|---|---|
| 10 **identical** status frames | **0** | **0** |
| 10 status frames with CO₂ changing | 26 | 163,467 |
| 1 cycle: Home→Trends→Devices→Home | 124 | 903,532 |
| Home idle, sub-second | **0** | **0** |
| Home idle, 1-second tick | 1 | 4,230 (uptime header only) |

**Zero flushes on unchanged telemetry** is achieved via dirty-checking: label, color, and coordinate updates route through `set_*_if_changed()`. Avoid adding unconditional redraw calls in firmware.

Page transitions draw ~900,000 pixels as containers rebuild. On resource-constrained hardware, retaining page objects and toggling hidden flags can reduce redraw cycles at the expense of heap memory.

### Embedded Constraints & Warnings

- The 42 KiB heap is a **baseline profile**. Peak usage of 91% in modal limits leaves ~4,296 bytes free. Firmware `lv_conf.h` differences will shift memory usage. **Measure free heap via `lv_mem_monitor()` upon hardware bring-up.**
- If `lv_mem_alloc` exhausts memory, LVGL defaults `LV_ASSERT_HANDLER` to `while(1);` — causing a **silent lockup**, not a visible crash.

---

## 11. Concurrency & ISR Guidelines

`lora_node_status` is not declared `volatile` and does not include atomic double-buffering. On the single-threaded simulator, torn reads never occur.

On hardware, if radio RX interrupts or separate RTOS tasks write to `lora_node_status` while `ui_tick()` reads it, partial reads could occur.

**Recommended Approach:** Copy into `lora_node_status` from the **same thread/context** executing `ui_tick()`. ISRs set notification flags; the main loop parses and unpacks frames.

**If ISR Writes Are Mandatory:** Implement a seqlock pattern:

```c
/* Writer (ISR) */
lora_rx_revision++;                 /* Odd = write in progress */
__DMB();
memcpy(lora_node_status.bytes, buf, LORA_NODE_STATUS_LEN);
__DMB();
lora_rx_revision++;                 /* Even = write complete */

/* Reader (ui_tick) — requires modifying ui.c */
uint32_t r1, r2;
do {
    r1 = lora_rx_revision;
    if (r1 & 1u) continue;
    snapshot = lora_node_status;
    r2 = lora_rx_revision;
} while (r1 != r2);
```

---

## 12. Unimplemented Scope / Open Items

Review these items when planning deployment:

| Scope Item | Status |
|---|---|
| Flash persistence driver | **Pending** — Callbacks wired; driver needed |
| Node firmware implementation | **Pending** — Wire contract established |
| Sensor Modbus register mapping | **Pending** — Awaiting physical sensor validation |
| Hardware QSPI LCD & touch drivers | **Pending** — Host GDI is simulator-only |
| RF parameters (frequency, SF, channel) | Open for field configuration |
| Power-on / link-loss relay default policy | Open (safety considerations) |
| Frame encryption & authentication | Plaintext (demo baseline) |
| Hardware acceptance testing | Pending target bench deployment |
| TVOC ppb quality bands | **Decided (2026-09-28)** — Good ≤ 220, Moderate ≤ 660, Poor > 660 ppb (UBA levels); eCO₂ ≤ 800 / ≤ 1200 ppm. Purifier Auto defaults aligned to ON ≥ 660 / OFF ≤ 220 ppb |
| Host simulator & regression for LoRa v2 / Trends 2026-09-25 | **Implemented** — Simulator and 42/40 KiB regression suites pass 42/42 checks |

---

## 13. Bring-up Checklist

Execute bring-up sequentially:

1. [ ] Display static test pattern over QSPI LCD (without LVGL).
2. [ ] Validate 1 ms `lv_tick_inc()` execution via `lv_tick_get()`.
3. [ ] Verify LVGL draws a full-screen solid rectangle. Confirm color channels (`LV_COLOR_16_SWAP`).
4. [ ] Enable DMA in `flush_cb`, calling `lv_disp_flush_ready()` in completion ISR.
5. [ ] Verify touch coordinates with a simple LVGL test button.
6. [ ] Call `ui_init()`. Verify Home screen displays with `—` metrics and `Disconnected` status.
7. [ ] **Measure heap headroom** using `lv_mem_monitor()`. Compare against Section 10.
8. [ ] Establish bidirectional LoRa communication (see `LORA_GUIDE.md`); both boards on protocol `0x02`.
9. [ ] Poll with `GET_STATUS`; populate `lora_node_status` and increment `lora_rx_revision` on each reply. Verify live telemetry appears on Home and `ui_is_tx_ready()` becomes `true`.
10. [ ] Send `HUB_CMD` only while `ui_has_pending_command()` is `true`. Verify switch toggles actuate relays on Node and idle polling does not change outputs.
11. [ ] Disconnect Node antenna → verify `Disconnected` status appears after 5s; reconnect → verify automatic recovery.
12. [ ] Optional: Implement target Flash persistence driver (Section 8) to retain edited device configurations across cold reboots (demo runs out-of-the-box with in-memory persistence).
13. [ ] Trends history: verify autonomous session history populates built-in Trends graphs automatically (optional: connect `ui_feed_history()` if external RTC/time-series database is preferred).
14. [ ] Execute 24-hour continuous stress run; log heap stability and fragmentation.

---

## 14. Common Porting Issues

| Symptom | Root Cause |
|---|---|
| Display does not update on new packets | Forgot to increment `lora_rx_revision++` |
| Red and blue color channels swapped | Incorrect `LV_COLOR_16_SWAP` in `lv_conf.h` |
| Hard CPU freeze without crash log | `LV_ASSERT_MALLOC` triggered — heap exhausted. Check `lv_mem_monitor()` |
| Touch coordinates offset or inverted | Touch calibration or display rotation mismatch |
| Top bar clock remains static | LVGL tick not advancing (`lv_tick_inc()` not called, or `LV_TICK_CUSTOM` expression wrong) |
| Always `Disconnected` although Node replies arrive | `lora_last_rx_tick_ms` written from a different clock than `lv_tick_get()` (e.g. `HAL_GetTick()` with a separate `lv_tick_inc()` counter) |
| Hub never becomes TX-ready after boot | `GET_STATUS` polling gated by `ui_is_tx_ready()`; a v2 Node replies only to requests |
| All frames rejected after an upgrade | One board still on protocol `0x01`; update Hub and Node together |
| Purifier Auto limits reset to 660/220 after upgrade | Expected v2→v3 migration (VOC index → TVOC ppb); rewrite the record to stop repeating it |
| Switch remains stuck on `Sending…` | Node readback missing, mismatched `active_low`, or `UI_CMD_TIMEOUT_MS` too short for SF airtime |
| Inverted relay state on initial boot | `active_low` polarity configuration needed for that output |
| Device names revert to default on reboot | Flash storage driver not yet implemented (Section 8) |
| Signal bars remain full at distance | `LORA_SPREADING_FACTOR` mismatch against physical RF config |
