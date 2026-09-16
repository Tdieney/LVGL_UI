# Smart Hub UI — Integration and Operation Guide

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
| `ui_theme.c` / `.h` | Study 12 color palette, shared static styles |
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
| `lv_tick_inc()` from 1 ms timer | Uptime counters, command timeouts, auto timing |
| Radio driver, populating status globals | Telemetry parsing, quality badges, visual status |
| Periodic transmission of `lora_hub_cmd` | Determining contents of `lora_hub_cmd` |
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

    uint32_t last_tx = 0;

    for (;;) {
        /* 1) Inbound radio frames */
        lora_service_rx();               /* Section 7.1 */

        /* 2) UI & Auto logic: uptime, timeouts, RX reconciliation, Auto evaluation */
        ui_tick();                       /* Polls global variables, updates widgets */
        lv_timer_handler();              /* LVGL render pass */

        /* 3) Outbound radio frames (gated TX): only transmits once boot state settles */
        uint32_t now = lv_tick_get();
        if (ui_is_tx_ready() && (now - last_tx >= 1000u)) {
            last_tx = now;
            lora_send(lora_hub_cmd.bytes, LORA_HUB_CMD_LEN);
        }
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

**`ui_is_tx_ready()`** — Returns `true` once UI initialization is complete and boot staging has stabilized. **Mandatory guard condition** before calling `lora_send()`, preventing spurious transmissions before Node initial state is recognized.

### `lv_tick_inc()`

LVGL requires elapsed time tracking. Call `lv_tick_inc(1)` from a 1 ms timer interrupt:

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

Firmware defines the extern globals declared in `lora_comm.h` and `lora_hub_link.h`:

```c
lora_hub_cmd_t     lora_hub_cmd;              /* UI writes, firmware sends */
lora_node_status_t lora_node_status;          /* Firmware writes, UI reads */
volatile uint32_t  lora_last_rx_tick_ms;      /* Firmware writes */
volatile uint32_t  lora_rx_revision;          /* Firmware writes */
volatile int8_t    lora_last_snr;             /* Firmware writes */
volatile int8_t    lora_last_rssi;            /* Firmware writes */
```

Upon receiving a valid status frame:

```c
void lora_service_rx(void)
{
    uint8_t buf[64], len; int8_t snr, rssi;
    if (!lora_read_packet(buf, &len, &snr, &rssi)) return;

    if (len != LORA_NODE_STATUS_LEN)          return;   /* Frame size validation */
    if (buf[0] != LORA_PROTO_VERSION)         return;
    if (buf[1] != LORA_MSG_NODE_STATUS)       return;

    memcpy(lora_node_status.bytes, buf, LORA_NODE_STATUS_LEN);
    lora_last_snr        = snr;
    lora_last_rssi       = rssi;
    lora_last_rx_tick_ms = lv_tick_get();
    lora_rx_revision++;                        /* MANDATORY TRIGGER */
}
```

> **`lora_rx_revision++` triggers UI widget re-evaluation.** `ui_tick()` compares this counter with the previous render revision; if unchanged, widget updates are bypassed. Forgetting to increment causes the display to remain static even when new packets arrive.

### 7.2 Field Definitions

| Field | Units | UI Application |
|---|---|---|
| `co2_ppm` + `co2_valid` | ppm | CO₂ hero card & trends |
| `voc_index` + `voc_valid` | Unitless index | VOC hero card & trends |
| `temp_deci_c` + `temp_valid` | 0.1 °C, **signed** | Temperature comfort card & trends |
| `humidity_pct` + `humid_valid` | Integer % | Humidity comfort card & trends |
| `relay1..4_gpio` | Raw GPIO 0/1 | Physical relay readback state |
| `seq_echo` | Counter | Confirmation that Node executed current command |
| `rs485_fault`, `co2_timeout`, `th_timeout` | Flags | Fault masking and Auto suspension |

**The `*_valid` bits are essential.** If a sensor fails, set `valid = 0`. The UI will render an em-dash `—` and `Unknown` status **exclusively for that metric**, while other metrics continue updating normally. Do not transmit a value of 0 to represent invalid data.

### 7.3 Inferred Connection State

Link status is inferred: `lv_tick_get() - lora_last_rx_tick_ms < LORA_LINK_TIMEOUT_MS` (5000 ms).
There is no manual connection flag. Updating `lora_last_rx_tick_ms` on valid packets handles link status automatically.

Upon link loss, the UI automatically hides telemetry values, displays `Unknown`, and **inhibits manual switch toggles**.

### 7.4 Historical Trends Data

```c
void ui_feed_history(ui_metric_id_t metric,
                     const int32_t *values, const bool *valid,
                     uint16_t count, uint16_t start_minute);
```

- Capacity: **16 points** per metric (`UI_HISTORY_CAPACITY`).
- If `count` > 16, the UI keeps the **16 most recent points**.
- `start_minute`: Minute of day for the first point (`14*60+17` = 14:17), auto-wrapped modulo 1440.
- `valid[i] = false` produces a **gap** in the line chart, rather than zero.
- Units follow Section 7.2 (temperature in 0.1 °C).

The UI **does not accumulate historical buffers internally**. Firmware maintains its own ring buffer (e.g. 1-minute samples) and feeds this API. Unpopulated metrics display an empty state.

---

## 8. Hub-local Configuration & Automation (Auto v1)

```c
#define UI_CONFIG_SCHEMA_VERSION 0x02u

typedef struct {
    char name[24];
    uint8_t preset;            /* 0..7, see ui_preset_id_t */
    ui_device_mode_t mode;     /* UI_MODE_AUTO (0) / UI_MODE_MANUAL (1) */
    uint8_t active_low;        /* 0 = Active High (GPIO 1 is ON), 1 = Active Low (GPIO 0 is ON) */
    int32_t auto_on_thresh;    /* Turn-on threshold (ppm, VOC index, %RH) */
    int32_t auto_off_thresh;   /* Turn-off threshold (ppm, VOC index, %RH) */
    uint8_t schema_version;    /* UI_CONFIG_SCHEMA_VERSION = 0x02 */
} ui_hub_device_config_t;

const ui_hub_device_config_t *ui_get_device_config(uint8_t idx);
bool ui_set_device_config(uint8_t idx, const ui_hub_device_config_t *cfg);
void ui_set_config_changed_cb(ui_config_changed_cb_t cb);
bool ui_is_tx_ready(void);
```

### 8.1 Auto v1 Automation Rules (Hub-side)

`ui_auto.c` executes in pure C on the Hub, evaluated whenever a new status frame arrives via LoRa:

1. **Supported Appliances:**
   - **Ventilation Fan:** Evaluates CO₂ (ppm). Turns ON when >= `auto_on_thresh` (default 1000), turns OFF when <= `auto_off_thresh` (default 800). Bounds: 0..10000 ppm, step 50 ppm, minimum ON - OFF gap: 50 ppm.
   - **Air Purifier:** Evaluates VOC (unitless, 1..500). Turns ON when >= `auto_on_thresh` (default 150), turns OFF when <= `auto_off_thresh` (default 100). Bounds: 1..500, step 5, minimum ON - OFF gap: 5.
   - **Humidifier:** Evaluates Humidity %RH (inverted logic: activates when dry, turns off when moist). Turns ON when <= `auto_on_thresh` (default 40%), turns OFF when >= `auto_off_thresh` (default 50%). Bounds: 0%..100%, step 1%, minimum OFF - ON gap: 5% RH.
   - **Other Presets (Dehumidifier, Heater, Light, Smart Socket, Generic):** Manual mode only. Auto selection is disabled with caption `"Manual only for this device type"`.

2. **Hysteresis & Hunting Protection:**
   - **3 Consecutive Qualifying Frames:** Requires 3 consecutive valid status frames meeting threshold conditions before triggering a relay state change. Faults or values entering the deadband reset the qualification counter.
   - **10-Second Post-Switch Hold:** Once a relay switches state, opposing automated transitions are held off for 10 seconds to prevent motor/compressor short cycling.
   - **Sensor Fault Masking:** When the associated sensor reports invalid telemetry (`valid = false`, RS-485 bus fault, or timeout), Auto suspends transitions and maintains current relay state.

3. **Manual Override:**
   - Toggling a device switch on Home or Devices instantly switches that device to `Manual` mode, updates the snapshot, and cancels pending Auto timers. Auto remains disarmed until explicitly re-enabled in settings.

4. **Error Latching & Re-arm (Auto Paused & Retry):**
   - If an automated relay command times out (> 3000 ms), the Hub displays `"Auto paused"` in amber alongside a `"Retry"` button.
   - Tapping `"Retry"` verifies current reported GPIO readback from Node, clears the error latch, and safely re-arms Auto evaluation without re-transmitting stale commands.

### 8.2 Dialog Navigation & Threshold Configuration

- On the Devices tab, tapping the gear icon opens the **Device settings** modal (440x368).
- If the preset supports Auto, a `"Set limits..."` button appears below the mode selector.
- Tapping `"Set limits..."` opens the **Auto thresholds** subview within the same modal bounds (440x368, zero additional heap allocation):
  - Metric name and units (`ppm`, unitless VOC, `%RH`).
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
    NVS_RECORD_KIND_CURRENT_V2 = 1,
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

        if (kind == NVS_RECORD_KIND_CURRENT_V2 && rec_len == sizeof(ui_hub_device_config_t)) {
            if (raw_buf[offsetof(ui_hub_device_config_t, schema_version)] == UI_CONFIG_SCHEMA_VERSION) {
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
Firmware starts periodic transmissions only after `ui_is_tx_ready()` returns `true`.

---

## 9. Extracting Output Data FROM the UI

### 9.1 Relay Commands

The UI writes directly into `lora_hub_cmd` upon switch toggles or automated transitions:
- Sets corresponding GPIO bit (mapped through `active_low`),
- Increments `lora_hub_cmd.bits.seq`.

Firmware transmits when `ui_is_tx_ready() == true`, **every ~1000 ms or immediately upon `seq` changes**.
No transmit history buffer is required — this follows a **level-triggered** architecture where dropped packets resolve on subsequent periodic transmissions.

### 9.2 Desired vs Reported State Machine

| Condition | Visual Representation |
|---|---|
| `reported == desired` | Normal switch toggle |
| Mismatch, < 3 s | `Sending…` (amber), dimmed switch |
| Mismatch, >= 3 s | `Unknown` + `Retry`, hidden switch |
| Link stale | `Unknown` + em-dash `—`, switches disabled |

`UI_CMD_TIMEOUT_MS` = 3000 ms in `lora_comm.h` (configured for SF7–SF10 airtime). **If using SF11/SF12, increase this timeout** to prevent premature command timeout errors.

If the Node reports a state change without a Hub command (local physical override, reboot), the UI **adopts** that reported state and synchronizes `lora_hub_cmd` — the Node is the authoritative source of hardware state.

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
8. [ ] Establish bidirectional LoRa communication (see `LORA_GUIDE.md`).
9. [ ] Populate `lora_node_status` and increment `lora_rx_revision`. Verify live telemetry appears on Home.
10. [ ] Enable periodic `lora_hub_cmd` transmission. Verify switch toggles actuate relays on Node.
11. [ ] Disconnect Node antenna → verify `Disconnected` status appears after 5s; reconnect → verify automatic recovery.
12. [ ] Implement flash persistence and verify configuration retention across power cycles.
13. [ ] Connect `ui_feed_history()` to populate Trends graphs.
14. [ ] Execute 24-hour continuous stress run; log heap stability and fragmentation.

---

## 14. Common Porting Issues

| Symptom | Root Cause |
|---|---|
| Display does not update on new packets | Forgot to increment `lora_rx_revision++` |
| Red and blue color channels swapped | Incorrect `LV_COLOR_16_SWAP` in `lv_conf.h` |
| Hard CPU freeze without crash log | `LV_ASSERT_MALLOC` triggered — heap exhausted. Check `lv_mem_monitor()` |
| Touch coordinates offset or inverted | Touch calibration or display rotation mismatch |
| Top bar clock remains static | `lv_tick_inc()` not being called |
| Switch remains stuck on `Sending…` | Node readback missing, mismatched `active_low`, or `UI_CMD_TIMEOUT_MS` too short for SF airtime |
| Inverted relay state on initial boot | `active_low` polarity configuration needed for that output |
| Device names revert to default on reboot | Flash storage driver not yet implemented (Section 8) |
| Signal bars remain full at distance | `LORA_SPREADING_FACTOR` mismatch against physical RF config |
