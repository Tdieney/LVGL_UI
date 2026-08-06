# 7-inch Industrial HMI — BLDC Motor Control

LVGL v8.4 UI source code for an 800x480 (RGB565) industrial HMI. Optimized for mid-range microcontrollers (80 MHz MCU, 128 KB SRAM, 1 MB Flash) driving an xSPI display module with dedicated internal GRAM.

> **Design Philosophy:** Zero fluff. Every widget, buffer, and execution path is strictly engineered to maintain zero dynamic heap allocation during runtime and prevent any memory fragmentation on a 52 KB LVGL heap budget on bare-metal hardware.

---

## 1. Technical Specifications & Memory Budget

This repository complies strictly with the validated MCU integration profile ([docs/MCU_MEMORY_PROFILE.md](docs/MCU_MEMORY_PROFILE.md)):

| Item | Constraint | Technical Note |
|---|---|---|
| **MCU Target** | 80 MHz, 128 KB SRAM, 1 MB Flash | 800x480 RGB565 xSPI display (with internal GRAM) |
| **LVGL Heap** | **52 KB** (`UI_LVGL_HEAP_BYTES`) | Verified under 120s stress test; free heap remains > 10 KB |
| **Draw Buffer** | **16 KB** (Single 800x10 RGB565 buffer) | Allocated in DMA-capable SRAM. Avoid dual-buffering unless measured throughput gain justifies another 16 KB |
| **Allocation Strategy** | Zero-alloc in tick loop | Static strings reference Flash (`lv_label_set_text_static()`). Dynamic labels use `label_bind_buffer()` |
| **Math Execution** | Fixed-point / Integer only | NO floating-point operations (`float`, `sinf`, `cosf`, `atan2f`) in any tick or input hot path |

---

## 2. Execution Architecture & Telemetry

1. **Resident UI Frame & Coalesced Teardown:**
   - Top bar and tab navigation chrome reside permanently on `lv_layer_top()`.
   - Exactly one content screen is resident at any time. Screen teardown and rebuilding are coalesced and executed inside `ui_tick()`, never directly from input event callbacks.

2. **Telemetry Decoupling (UART & Demo Mode):**
   - Real UART telemetry and Demo simulation state are stored in independent private structures.
   - UI code accesses motor status strictly through accessor functions: `ui_motor_status_fast()`, `ui_motor_status_slow()`, and `ui_motor_connected()`.
   - LVGL APIs are never invoked from ISR context. UART interrupts commit frames into safe buffers, deferring rendering to `ui_tick()`.

---

## 3. Repository Structure

```text
├── ui.c / ui.h                   # Core initialization & main ui_tick() loop
├── screens.c / screens.h         # Implementation of 6 HMI screens (Dashboard, Monitor, Control, Graphs, Diag, Settings)
├── actions.c / actions.h         # User interaction callbacks and command dispatch
├── demo_sim.c / demo_sim.h       # Standalone telemetry simulator for Demo mode
├── motor_comm_protocol.h         # UART packet framing and motor control protocol definitions
├── ctrl_pos_math.h               # Fixed-point math utilities for position & angle mapping
├── ui_mcu_profile.h              # Single source of truth for MCU memory limits
├── ui_icon_*.c / ui_image_*.c    # Icon & logo graphics stored as const C-arrays in Flash
├── sim_pc/                       # PC Simulator project (CMake + LVGL v8.4)
├── tools/                        # Automation scripts for simulator builds, regression tests, and MCU export
└── docs/                         # Memory profiling and architectural documentation
```

---

## 4. Build & Verification (PC Simulator)

Prerequisites: Windows environment with CMake and Ninja / MinGW / MSVC.

### 4.1. Build & Run Simulator
```powershell
# Build the simulator from repository root:
tools/build_sim.bat

# Execute headless regression tests (verifies heap stability and captures screenshots across all 6 tabs):
tools/run_regression.bat

# Reuse an existing sim_pc.exe build:
tools/run_regression.bat -SkipBuild
```

### 4.2. MCU Binary Sizing
Inspect static RAM and Flash memory footprint of the MCU firmware ELF binary:
```powershell
tools/mcu_size.bat -Elf path/to/your_firmware.elf
```

---

## 5. Firmware Porting Checklist

When integrating this UI into target MCU firmware:

1. **Configuration Header:** Include `"ui_mcu_profile.h"` at the top of your target `lv_conf.h`.
2. **Display Port Setup:**
   ```c
   static lv_color_t draw_pixels[UI_DRAW_BUF_PIXELS]; // 800x10 RGB565 (16 KB)
   static lv_disp_draw_buf_t draw_buf;
   lv_disp_draw_buf_init(&draw_buf, draw_pixels, NULL, UI_DRAW_BUF_PIXELS);
   disp_drv.draw_buf = &draw_buf;
   ```
3. **Execution Context & DMA:** Call `lv_disp_flush_ready()` only upon xSPI DMA transfer completion. Guarantee `lv_timer_handler()` and `ui_tick()` run within a single thread context.
4. **Linker Map Verification:** Inspect the `.map` file after linking and measure stack high-water mark at runtime during Demo START.

---

## 6. Contribution Rules

- Consult `AGENTS.md` and `docs/MCU_MEMORY_PROFILE.md` before adding or modifying UI widgets.
- Ensure all new image and font assets maintain `const` qualifiers to be placed in Flash memory.
- Always run `tools/run_regression.bat` prior to submitting commits or pull requests.
