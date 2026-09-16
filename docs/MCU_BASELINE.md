# Smart Hub MCU/UI baseline

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

To compile and link the Smart Hub UI on the target MCU, the firmware's `lv_conf.h`
must enable the following symbols:

```c
/* Built-in Montserrat 14 is retained (1) as the internal LVGL default font (LV_FONT_DEFAULT).
 * Larger built-in Montserrat fonts (16..48) are disabled (0) to save ~128 KB Flash.
 * All visible typography is provided by custom Segoe UI Semibold tables in ui_fonts.c (>= 18 px). */
#define LV_FONT_MONTSERRAT_14  1
#define LV_FONT_MONTSERRAT_16  0
#define LV_FONT_MONTSERRAT_18  0
#define LV_FONT_MONTSERRAT_20  0
#define LV_FONT_MONTSERRAT_24  0
#define LV_FONT_MONTSERRAT_30  0
#define LV_FONT_MONTSERRAT_34  0
#define LV_FONT_MONTSERRAT_48  0

/* Gradient cache size: permanently 0 under Study 12 (flat fills only).
 * Reclaims 1,800 bytes of heap and eliminates gradient cache collision/alloc hazards. */
#define LV_GRAD_CACHE_DEF_SIZE 0
```

### Study 12 Baseline (Flat Fills & Neutral Greys)

Design study 12 (2026-09-07) replaces all surface gradients with flat, bit-exact
RGB565 fills. Consequently:
- `LV_GRAD_CACHE_DEF_SIZE` is permanently set to `0`, returning 1,800 bytes directly to the heap.
- LVGL v8.4's gradient cache hash key vulnerability (`descriptor address ^ size ^ (w >> 1)` omitting color keys) and the per-draw temporary gradient buffer allocation hazards are completely avoided.
- All 10 surfaces (canvas `#EFEFEF`, device cards `#F7F7F7`, chart/dialog `#FFFFFF`, metric cards, and navigation pills) use single flat `bg_color` values with `LV_OPA_COVER`.

### Font Flash Footprint & Typography Baseline (User Directive)

Per explicit user directive, typography matches the prototype's authentic **Segoe UI Semibold** (`seguisb.ttf`, weight 600) and simplifies from 6 sizes to **4 standardized sizes**, with a strict minimum font size of **18 px** (size 16 eliminated):

1. `ui_font_18` (18 px) — Minimum font size. Captions, unit labels, comfort status notes, chart axes, modal helper copy, action button labels.
2. `ui_font_24` (24 px) — Top bar hub/uptime status, device card headers, modal dialog titles.
3. `ui_font_34` (34 px) — Screen titles ("Trends", "Devices"), comfort card metrics (`23.5 °C`, `48 %`), Trends summary stats (`420 ppm`).
4. `ui_font_digits_62` (62 px) — Hero metric digits for CO₂ and VOC (`0`–`9`, `-`, `.`, `—`).

- **Flash Footprint:**
  - Standard Montserrat built-in fonts (16..34) previously consumed ~132.5 KB Flash.
  - The 4 Segoe UI Semibold tables in `ui_fonts.c` consume **~39 KB** of binary bitmap data (~60 KB C source), covering full ASCII (32..126), required Unicode symbols (`₂`, `—`, `…`, `°`, `·`), and the dropdown chevron (`LV_SYMBOL_DOWN` `0xF078`).
  - **Net Flash Impact:** **~70 KB Flash saved** compared to the built-in Montserrat baseline, while providing exact typography fidelity to the browser prototype.

### Heap & Draw Buffer Constraints

- Target LVGL heap: **42 KiB** (`UI_LVGL_HEAP_BYTES = 43008`).
- One partial draw buffer: **800x10 RGB565** (16,000 bytes).
- Do not allocate a full 800x480 RGB565 framebuffer on the MCU; it would consume
  768,000 bytes and the display already owns GRAM.

#### Re-measured Heap Figures (Study 12 + Auto v1 Baseline)

Measured across all 24 regression scenarios and continuous lifecycle checks:

| State / Scenario | Heap Used | Heap Free | Largest Free Block | Fragmentation | Headroom vs 42 KiB |
|---|---:|---:|---:|---:|---:|
| Splash screen | 73% (31,016 B) | 11,992 B | 9,736 B | 19% | 12.0 KiB (28%) |
| Splash → Home transition | 71% (30,400 B) | 12,608 B | 9,736 B | 23% | 12.6 KiB (29%) |
| Home page (steady) | 82% (34,984 B) | 8,024 B | 6,160 B | 24% | 8.0 KiB (19%) |
| Trends (with 24h history) | 60% (25,640 B) | 17,368 B | 9,736 B | 44% | 17.4 KiB (40%) |
| Trends (empty / offline) | 60% (25,640 B) | 17,368 B | 9,736 B | 44% | 17.4 KiB (40%) |
| Devices (grid) | 73% (31,336 B) | 11,672 B | 9,736 B | 17% | 11.7 KiB (27%) |
| Devices (settings dialog open) | 88% (37,504 B) | 5,504 B | 4,544 B | 18% | 5.5 KiB (13%) |
| After 50 page navigation cycles | 82% (34,960 B) | 8,048 B | 6,160 B | 24% | 8.0 KiB (19%) |

- **Peak Heap Usage:** 37,504 B (88%) during modal dialog with on-demand subview construction, guaranteeing **> 5.5 KiB (13%) free headroom** well above the 4 KiB requirement.
- **Drift / Leak:** **-8 bytes drift** across 50 complete navigation cycles (0% fragmentation growth).
- **On-demand Subview Memory Optimization:** Instead of creating both Device Settings and Auto Thresholds subviews eagerly (which previously exceeded the 42 KiB heap), subviews are built on demand into the 440x368 dialog box container. Switching subviews reclaims ~3.5 KiB dynamically, keeping peak dialog heap footprint under 37.5 KiB.

#### Firmware Static Footprint (MCU Exported vs Sim-Only Test Surface)

Measured with GNU `size` across compilation profiles:

| Object / Translation Unit | Text (Flash) | Data (RAM) | BSS (Static RAM) | Total Static RAM |
|---|---:|---:|---:|---:|
| `ui.c.obj` (MCU Firmware Export) | 54,200 B | 412 B | 2,688 B | 3,100 B |
| `ui_auto.c.obj` (Auto v1 Module Export) | 3,180 B | 0 B | 64 B | 64 B |
| `ui_test_api.c.obj` (Sim-only Test Surface) | 4,820 B | 0 B | 320 B | 320 B |

- **Pure C Auto Module Footprint:** `ui_auto.c` has zero LVGL dependencies, zero heap allocations (`malloc`/`lv_mem_alloc`), and requires only **64 bytes** of static RAM (`ui_auto_state_t` across 4 channels) and **~3.2 KB** Flash.
- **MCU Firmware Isolation:** All test accessors, hooks, and test fixtures are compiled conditionally under `UI_TEST_HOOKS` and excluded from `tools/export_mcu.bat --verify`.
- **Total Production Static RAM:** **~3.16 KiB** (`ui.c` + `ui_auto.c`).

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

2. **Snapshot Reconciliation & Expiry:**
   - When a command ACK is confirmed by the application via `ui_handle_command_result()`, the UI displays a transient overlay (`has_confirmed_state`) preventing race conditions with stale in-flight telemetry snapshots.
   - As soon as an application snapshot arrives matching the confirmed state, the overlay is cleared immediately.
   - If an intervening snapshot reports conflicting state (e.g. node reset or local manual override), the 3,000 ms bound ensures the overlay expires and yields to the true reported hardware snapshot rather than permanently masking it.


