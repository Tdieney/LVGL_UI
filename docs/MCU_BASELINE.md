# Smart Hub MCU/UI baseline

> Baseline Status Update (2026-09-25):
> LoRa protocol v2 (0x02) request/response, TVOC in ppb, sample_seq-gated Auto, config schema v3,
> restored hero Trends indicator (UI_COLOR_FOREST + celadon dot), 15-minute fixed window, and round-number Y ticks verified.
> Both default (42 KiB) and 40 KiB profiles pass all 42/42 checks in the regression suite (2026-09-26, incl. live current-minute Trends slot; heap figures unchanged).
>
> - **Host UI Component Ledger (Default 42 KiB):**
>   - Pool + Draw Subtotal: 59,008 B (fixed LVGL pool 43,008 B + 10-line draw buffer 16,000 B).
>   - Static UI Data: 4,704 B (`.data = 576 B`, `.bss = 4,128 B`).
>   - Style RAM: 0 B (`.data = 0`, `.bss = 0`, all styles are `const` in Flash `.rodata`).
>   - Gradient Cache: 0 B (`LV_GRAD_CACHE_DEF_SIZE 0`).
>   - Settled Pool Free: 8,072 B minimum (Home page) with 0 B drift across 50 stress cycles.
>   - Contemporaneous Largest Block at Peak Use: 6,584 B.
>   - Allocation-Level Transient Min Free: 3,664 B (contemporaneous largest block 3,664 B).
>   - Allocation-Level Largest Request: 1,168 B; Failure Count: 0.
>   - Trends Two-Stage Switching Latency: Stage 1 (tactile highlight) = 5 flushes / 34,532 px; Stage 2 (chart render) = 14 flushes / 89,388 px; 0 B drift over 100 switches.
>
> - **Host UI Component Ledger (40 KiB Profile):**
>   - Pool + Draw Subtotal: 56,960 B (fixed LVGL pool 40,960 B + 10-line draw buffer 16,000 B).
>   - Settled Pool Free: 5,984 B minimum (>4,096 B safety threshold) with 0 B drift across 50 stress cycles.
>   - Contemporaneous Largest Block at Peak Use: 4,856 B.
>   - Allocation-Level Transient Min Free: 1,616 B (contemporaneous largest block 1,616 B).
>   - Allocation-Level Largest Request: 1,168 B; Failure Count: 0.
>
> - **Host-Only Instrumentation & Tooling:**
>   Dynamic allocation tracing (`__wrap_lv_mem_alloc`, `__wrap_lv_mem_realloc`, `s_alloc_transient_min_free`) relies on GCC `--wrap` linker flags on the PC simulator. It is host diagnostic tooling, not included in exported MCU code.
>
> - **Target Firmware Disclaimer & Memory Map Boundary:**
>   The figures above represent host UI component ledgers only, NOT the owner's complete target MCU firmware
>   RAM map. Target firmware ELF/map analysis, FreeRTOS task stacks, DMA buffers, and radio packet queues
>   are required before target sign-off on the owner's ~99.9%-utilized MCU. Real physical LCD touch-to-photon
>   latency and DMA timings remain UNVERIFIED until measured with hardware instruments on the final board.

The current repository starts from the proven display-side constraints of the
previous 7-inch HMI:

| Item | Starting value |
|---|---:|
| Resolution | 800x480 |
| Color | RGB565 (2 bytes/pixel) |
| Display transport | QSPI/xSPI, display-owned GRAM |
| LVGL heap | 42 KiB |
| Draw buffer | one 800x10 buffer = 16,000 bytes |

These values are encoded in `ui_mcu_profile.h` so the PC simulator exercises a
constrained heap. They are a starting point, not yet a Smart Hub validation.

## Required MCU `lv_conf.h` Configuration

### MCU integration ownership (2026-09-17)

- Exported UI sources use `#include "lvgl.h"`. Add the directory containing that
  header to the target include paths and configure LVGL with the target's
  `lv_conf.h`; the UI does not require `LV_LVGL_H_INCLUDE_SIMPLE`.
- Compile `ui.c`, `ui_auto.c`, `ui_theme.c`, `ui_icons.c`, `ui_fonts.c` and
  `ui_splash_logo.c` once, together with LVGL and the application's display/input
  port. Do not add simulator sources to the firmware build.
- `ui.c` defines `lora_hub_cmd`, `lora_node_status`, `lora_last_rx_tick_ms`,
  `lora_rx_revision`, `lora_last_snr` and `lora_last_rssi`, like `motorCmd` in
  the old UI. Application code includes `lora_comm.h`/`lora_hub_link.h` and uses
  their `extern` declarations. Remove old application-side definitions when
  upgrading; otherwise the linker will report multiple definitions.
  Since protocol v2 (2026-09-25), `lora_last_rssi` is `volatile int16_t`; update
  any application-side `extern` or temporary that used `int8_t`.
- Firmware still publishes validated Node reports and link metadata and increments
  `lora_rx_revision`. With protocol v2 it polls with `GET_STATUS` at all times,
  including before `ui_is_tx_ready()`, because the Node replies only to requests.
  It sends `HUB_CMD` after `ui_tick()` reconciliation only while `ui_is_tx_ready()`
  and `ui_has_pending_command()` are both true. Storage ownership is not a radio driver.
- `lora_last_rx_tick_ms` must use the same clock as `lv_tick_get()`. If firmware
  writes `HAL_GetTick()` (as `lora_hub_link.h` documents), set `LV_TICK_CUSTOM 1`
  with `LV_TICK_CUSTOM_SYS_TIME_EXPR (HAL_GetTick())` in the target `lv_conf.h`.
- A host C99 build without `LV_LVGL_H_INCLUDE_SIMPLE` verifies include/link
  compatibility, not the Cortex-M33 memory map, display port or hardware behavior.

### LVGL configuration

To compile and link the Smart Hub UI on the target MCU, the firmware's `lv_conf.h`
must enable the following symbols:

```c
/* Built-in Montserrat 14 is retained (1) as the internal LVGL default font (LV_FONT_DEFAULT).
 * Larger built-in Montserrat fonts (16..48) are disabled (0) to save ~128 KB Flash.
 * Current visible typography uses custom Plus Jakarta Sans tables in ui_fonts.c (>=18px). */
#define LV_FONT_MONTSERRAT_14  1
#define LV_FONT_MONTSERRAT_16  0
#define LV_FONT_MONTSERRAT_18  0
#define LV_FONT_MONTSERRAT_20  0
#define LV_FONT_MONTSERRAT_24  0
#define LV_FONT_MONTSERRAT_30  0
#define LV_FONT_MONTSERRAT_34  0
#define LV_FONT_MONTSERRAT_48  0

/* Accepted current surfaces are flat. Match ui_theme_init and simulator config. */
#define LV_GRAD_CACHE_DEF_SIZE 0
```

### Quiet Architecture Baseline (Flat RGB565 Surfaces & Zero-Byte Gradient Cache)

The Quiet Architecture baseline standardizes on clean, solid RGB565 flat fills:
- Zero-byte gradient cache: `LV_GRAD_CACHE_DEF_SIZE 0` in `lv_conf.h` and `lv_gradient_set_cache_size(0)` in `ui_theme_init()`, permanently eliminating 1 KiB–1.8 KiB heap overhead and avoiding MCU scanline gradient compute.
- All surfaces (Canvas `#EEECE7`, Surface `#F3F1EC`, Hero Card `#214539`, Celadon pills `#D7E0BC`, and card containers) use bit-exact flat fills with `LV_OPA_COVER`.

### Font Flash Footprint & Typography Baseline (Plus Jakarta Sans)

Typography is standardized on **Plus Jakarta Sans** (`PlusJakartaSans.ttf`, SemiBold weight 600) with a strict minimum font size of **18 px**:

1. `ui_font_18` (18 px) — Secondary captions, unit labels, comfort card notes, axis labels, dialog helper text.
2. `ui_font_24` (24 px) — Navigation tabs, top bar hub/uptime status, device card headers, modal dialog titles.
3. `ui_font_34` (34 px) — Screen titles ("Trends", "Devices"), Overview hero metrics (`s_home_co2_val`, `s_home_voc_val`), decoupled Auto threshold values.
4. `ui_font_48` (48 px) — Comfort card verdict/summary text.
5. `ui_font_digits_62` (62 px) — Hero metric digits (`0`–`9`, `-`, `.`, `—`).

- **Flash Footprint:**
  - Compiled Plus Jakarta Sans tables in `ui_fonts.c` occupy **~100.3 KB** host object text covering ASCII, Greek/subscripts (`₂`, `—`, `…`, `°`, `·`), and symbols.
  - All visible glyphs adhere strictly to the $\ge 18\text{ px}$ contract.

### Heap & Draw Buffer Constraints

> **Historical Note:** The detailed tables below reflect the 2026-09-24 baseline
> measurements. The current re-measured 2026-09-25 figures (covering LoRa v2 and
> Trends refinements) are documented in the status block at the top of this file.

- Target LVGL heap: **42 KiB** (`UI_LVGL_HEAP_BYTES = 43008`).
- Opt-in 40 KiB profile: **40 KiB** (`UI_LVGL_HEAP_BYTES = 40960`), passing all 39 checks with >6.2 KiB free.
- One partial draw buffer: **800x10 RGB565** (16,000 bytes).
- Do not allocate a full 800x480 RGB565 framebuffer on the MCU; it would consume
  768,000 bytes and the display already owns GRAM.

#### Measured Heap Figures (Consolidated Delivery — 2026-09-24)

Measured during CHECK 31 across 50 complete stress cycles under the 42 KiB heap contract:

| Measurement Phase | Heap Used | Heap Free | Largest Free Block | Fragmentation | Contract Threshold |
|---|---:|---:|---:|---:|---|
| **Immediate (Transient Editor Creation)** | 67% (28,520 B) | 14,488 B | 7,744 B | 47% | Free > 4,096 B (PASSED) |
| **Settled (Stable Home Page)** | 88% (37,728 B) | 5,280 B | 4,176 B | 21% | Free > 4,096 B (PASSED) |
| **Settled (Device Settings Open)** | 67% (28,520 B) | 14,488 B | 7,744 B | 47% | Free > 4,096 B (PASSED) |
| **Settled (Dropdown List Open)** | 68% (29,096 B) | 13,912 B | 7,744 B | 45% | Free > 4,096 B (PASSED) |
| **Settled (Auto Limits Open)** | 69% (29,432 B) | 13,576 B | 7,744 B | 43% | Free > 4,096 B (PASSED) |
| **Settled (Invalid Draft Visible)** | 69% (29,464 B) | 13,544 B | 7,744 B | 43% | Free > 4,096 B (PASSED) |
| **50-Cycle Stable Drift (Cycle 10 → 50)** | — | — | — | — | **+0 bytes** (|drift| <= 256 B PASSED) |

- **Minimum Free Memory:** 5,280 B settled, 14,488 B immediate on 42 KiB (6,272 B settled on 40 KiB) — guaranteeing margin over the 4,096 B floor.
- **Stable Drift:** `+0 bytes` over 50 complete cycles proves zero progressive heap leakage.
- **Draw Buffer:** Exactly ONE partial draw buffer of `800 × 10` pixels (16,000 bytes) allocated in `ui_mcu_profile.h`. No MCU framebuffer.

#### Host x86_64 GCC Object Sizes (Sim / Verification Build)

Measured with GNU Berkeley `size.exe` across host x86_64 object files compiled with GCC `-O2`:

| Translation Unit / Asset | Text (Code) | Data (Init RAM) | BSS (Zero RAM) | Total Size | Notes |
|---|---:|---:|---:|---:|---|
| `ui_icons.c.obj` | 39,424 B | 0 B | 0 B | 38.50 KiB | Canonical Vector Icons, 4-bit Alpha Masks |
| `ui_fonts.c.obj` | 100,336 B | 0 B | 0 B | 97.98 KiB | Plus Jakarta Sans: 18, 24, 34, 48, 62 px |
| `ui_splash_logo.c.obj` | 48,992 B | 0 B | 0 B | 47.84 KiB | Indexed 8-bit Splash Asset |
| `ui.c.obj` | 97,692 B | 576 B | 4,384 B | 100.25 KiB | Shell, Overview, Trends, Devices, History buffer (+256 B uint32 timestamps) |
| `ui_auto.c.obj` | 3,592 B | 0 B | 0 B | 3.51 KiB | Hub-side Auto Decision Engine (Zero LVGL calls) |
| `ui_theme.c.obj` | 2,944 B | 0 B | 0 B | 2.88 KiB | Flat RGB565 Theme (All 34 styles in `.rodata`) |
| **Total Exported UI Static Footprint** | **292,980 B** | **576 B** | **4,384 B** | **~297.94 KiB** | **Static RAM: 4,960 B** |

#### Whole-UI RAM Accounting (Static + Dynamic)

| Component | Default Profile (42 KiB) | Opt-in Profile (40 KiB) | Memory Type |
|---|---:|---:|---|
| Static `.data` (initialized RAM) | 576 B | 576 B | SRAM (`ui.c`) |
| Static `.bss` (zero-initialized RAM) | 4,384 B | 4,384 B | SRAM (`ui.c` including uint32 history timestamps) |
| Single partial draw buffer (800x10 RGB565) | 16,000 B | 16,000 B | SRAM (`ui_mcu_profile.h`) |
| LVGL Heap pool | 43,008 B | 40,960 B | SRAM (`ui_mcu_profile.h`) |
| **Total Whole-UI RAM Footprint** | **63,968 B (~62.47 KiB)** | **61,920 B (~60.47 KiB)** | SRAM |

- **Host vs Target Distinction:** The numbers above represent host x86_64 GCC compiled object sizes from the simulator and isolated host build. They provide relative weight indicators across translation units, not validated target Cortex-M33 Flash/RAM consumption. Target MCU memory utilization will depend on the ARM cross-compiler (e.g. `arm-none-eabi-gcc` vs Keil/IAR), target optimization flags (`-Os` vs `-O2`), linker garbage collection (`--gc-sections`), section placement in Flash/SRAM, and target RTOS integration.
- **Auto Module Independence:** `ui_auto.c` has zero LVGL dependencies, zero dynamic allocations, and compiles to ~3.59 KiB of host object code.

#### Draw & Flush Redraw Benchmark (P1.4, P1.5, P1.6)

Measured via simulator partial-buffer flush callback instrumentation:

| Benchmark Case | Flush Calls | Pixels Pushed | Redraw Reduction vs Uncached |
|---|---:|---:|---:|
| (a) 10 Identical Snapshots (Home) | **0 flushes** | **0 pixels** | **100% eliminated** (0 vs 3,840,000 px) |
| (b) 10 Snapshots (CO₂ value changing) | **27 flushes** | **167,603 pixels** | **95.6% reduction** (16.7 kpix/snap vs 384 kpix/snap full frame) |
| (c) 1 Navigation Cycle (H → T → D → H) | **122 flushes** | **898,412 pixels** | Bounded to newly damaged regions |
| (d) Idle Home Sub-second (500 ms) | **0 flushes** | **0 pixels** | **100% eliminated** |
| (e) Idle Home Full-second (1000 ms) | **1 flush** | **4,230 pixels** | Top-bar uptime label area only |

### Bounded UI Waiting States & LoRa Application Rationale (Round 4)

In embedded systems communicating over wireless fieldbuses (LoRa) with RS-485 downstream nodes, network packets can be delayed, dropped, or corrupted. To ensure the UI never freezes in an indefinite waiting state and never presents misleading telemetry, two coherent tick-based bounds are enforced in `ui_internal.h`:

- `#define UI_CMD_TIMEOUT_MS 3000u` — Pending command timeout.
- `#define UI_OVERLAY_TIMEOUT_MS 3000u` — Confirmed-state overlay timeout.

#### Application-Perspective Rationale

1. **LoRa Transaction Budget:**
   - Uplink command packet transmission: at SF7..SF10 over 125 kHz bandwidth, physical airtime is ~50–250 ms.
   - Node processing & relay actuation: RS-485 bus transaction to Smart Node relay driver takes ~50–100 ms.
   - Downlink ACK packet: ~50–250 ms airtime.
   - Retransmission margin: Under typical LoRa duty-cycle and packet-loss conditions, one link-layer retry adds ~1.0–1.5 s.
   - A **3,000 ms bound** gives ample headroom for successful transmission + retry without prematurely flagging false errors, while guaranteeing that a lost packet or unpowered node transitions the UI tile to the "Unknown" error state with "Retry" action in at most 3 seconds.
   - Protocol v2 makes this explicit: the Hub sends a pending `HUB_CMD` up to three times, and all attempts must fit inside the 3,000 ms bound. After a timeout the UI restores the reported level in `lora_hub_cmd` and firmware falls back to `GET_STATUS` polling.

2. **Snapshot Reconciliation & Expiry:**
   - When a command ACK is confirmed by the application via `ui_handle_command_result()`, the UI displays a transient overlay (`has_confirmed_state`) preventing race conditions with stale in-flight telemetry snapshots.
   - As soon as an application snapshot arrives matching the confirmed state, the overlay is cleared immediately.
   - If an intervening snapshot reports conflicting state (e.g. node reset or local manual override), the 3,000 ms bound ensures the overlay expires and yields to the true reported hardware snapshot rather than permanently masking it.
