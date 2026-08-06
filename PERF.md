# PERF.md — Performance Architecture & Firmware Integration Guide

This UI is designed for an 80 MHz MCU with 1 MB Flash, 128 KB RAM, driving an 800x480 RGB565 display via xSPI (TR230S module with onboard GRAM — MCU pushes dirty regions over the bus). The primary system bottleneck is **xSPI bus bandwidth**, so every optimization technique below aims to minimize dirty region redraw area and eliminate heap churn.

> **Current Profile (2026-08-05):** LVGL heap **52 KB** + single draw buffer **800×10 RGB565 = 16,000 B**. Validated metrics, root causes of Demo START issues, and exact integration steps are documented in `docs/MCU_MEMORY_PROFILE.md`. `ui_mcu_profile.h` is the single source of truth for definitions.

---

## 1. Measured Heap Footprint (Simulator on 52 KB Heap Budget)

| Screen Tab | LVGL Heap Used (`LV_MEM_SIZE = 52 KB`) |
|---|---|
| Dashboard | 67% (17,712 B free) |
| Monitor | 59% (22,328 B free) |
| Control | 79% (11,712 B free; highest utilization) |
| Graphs | 66% (18,352 B free) |
| Diagnostics | 61% (20,944 B free) |
| Settings | 70% (16,424 B free) |

> **Key Architectural Takeaway:** The initial Monitor implementation (17 dynamic values at 5 Hz with changing decimal precision) pushed heap usage past safe limits and flooded dirty regions across xSPI. The updated design retains 8 essential metrics. Leaving `LV_MEM_SIZE` at default 32 KB causes memory overflow — 52 KB is the minimum stress-tested threshold.

The figures above include top bar and tab bar chrome (permanently resident on `lv_layer_top()`).
Screen transitions invoke `lv_obj_clean()` on the outgoing screen BEFORE creating the incoming screen, keeping peak heap usage limited to a single screen at a time.

**Flash Asset Footprint (Measured from Object Files):**
Logo splash ~48 KB + 4 custom mono fonts (44/30/22/20px) + 16 custom icons (alpha 4-bit, ~288 B each ≈ 4.6 KB) ≈ **~80 KB total**. Pre-rendered gauge dial background images (~87 KB) and FontAwesome icon fonts (~6 KB) have been removed. All icons are custom 4-bit alpha arrays (`ui_icon_*`) recolored dynamically at runtime.

---

## 2. Recommended RAM Budget Allocation (128 KB Total SRAM)

| Item | Allocation |
|---|---|
| LVGL Heap (`LV_MEM_SIZE`) | 52 KB |
| Single Draw Buffer (800×10×2B) | 16,000 B |
| SRAM Released (vs legacy profile) | 28,288 B |

---

## 3. Recommended Firmware `lv_conf.h` Settings

```c
#define LV_COLOR_DEPTH 16
#define LV_COLOR_16_SWAP 0            // Match xSPI byte ordering for TR230S
#include "ui_mcu_profile.h"
#define LV_MEM_CUSTOM 0
#define LV_MEM_SIZE UI_LVGL_HEAP_BYTES
#define LV_DISP_DEF_REFR_PERIOD 30    // ~33 fps frame cap; LVGL renders dirty regions only
#define LV_INDEV_DEF_READ_PERIOD 30   // GT911 touch IC over I2C
#define LV_USE_LOG 0                  // Disable logging in production
#define LV_IMG_CACHE_DEF_SIZE 0
#define LV_SHADOW_CACHE_SIZE 0
#define LV_FONT_MONTSERRAT_20 1
#define LV_FONT_DEFAULT &lv_font_montserrat_20
```

**Driver Flush Implementation:**
Declare a single draw buffer `UI_DRAW_BUF_PIXELS` (800×10). In `flush_cb`, transmit the dirty `area` over xSPI using DMA. Invoke `lv_disp_flush_ready()` inside the DMA transfer completion callback.

---

## 4. Applied Optimization Techniques

1. **Widget-based Arc Gauge with Minimal Redraw:** Gauge components use `lv_arc` track and indicator lines without static bitmap dial images. Only indicator arc and needle lines update per tick. Custom 4-bit alpha icons (`ui_icon_*`) recolor on status change.
2. **Dual Refresh Lanes (`ui_tick()` in `ui.c`):**
   - **SLOW 5 Hz:** Updates text labels and colors (Monitor split into alternating subsets of ≤4 values per pulse).
   - **CHART 8 Hz:** Updates 4 charts round-robin (each chart updates at 2 Hz staggered by 125 ms to prevent multi-chart redraw spikes in a single frame).
3. **Value-Change Guards:** `label_set_if_changed()` prevents label invalidation if text remains unchanged, eliminating unnecessary bus traffic.
4. **Zero Heap Churn for Strings:** Static labels use `lv_label_set_text_static()` referencing Flash memory. Dynamic numeric labels use a fixed buffer pool (`label_bind_buffer()`).
5. **Fixed-point Formatting (`fmt_scaled()`):** Complete integer-based fixed-point formatting; no floating-point soft-float calls (`sinf`/`cosf`/`atan2f`) in production UI tick paths.
6. **Single Resident Screen Architecture:** Screen switching clears previous widgets (`lv_obj_clean()`) before instantiating new tabs, preventing dual-screen memory overhead.

---

## 5. Firmware Integration Steps

1. Add all root `.c/.h` files, `fonts/*.c`, and asset files to the build project. Define `LV_LVGL_H_INCLUDE_SIMPLE` globally.
2. Main Loop Structure:
   ```c
   ui_init();
   for (;;) {
       ui_tick();            // Rate-limited, safe to call continuously
       lv_timer_handler();   // Standard LVGL timer handler (~5ms interval)
   }
   ```
3. Data Binding & Handoff:
   - **TX:** Periodically transmit global `motorCmd` (declared in `screens.h`) over UART according to `motor_comm_protocol.h`.
   - **RX:** Parse incoming frames and write directly into `motorStatusFast` and `motorStatusSlow` global structs.
   ```c
   void on_uart_rx_frame(uint8_t id, const uint8_t *payload, uint8_t len) {
       if (id == MSG_ID_MOTOR_STATUS_FAST && len == MOTOR_STATUS_FAST_LEN)
           memcpy(motorStatusFast.bytes, payload, len);
       else if (id == MSG_ID_MOTOR_STATUS_SLOW && len == MOTOR_STATUS_SLOW_LEN)
           memcpy(motorStatusSlow.bytes, payload, len);
   }
   ```
4. Set `motorConnected = 0` if valid `motorStatusFast` frames timeout (> 500 ms). Never invoke LVGL functions directly inside ISRs.
5. Compile standalone demonstration builds with `-DUI_DEMO_SIM=1`.
