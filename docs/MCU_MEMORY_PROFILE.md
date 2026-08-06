# MCU memory profile and Demo-start hardening

This is the validated integration profile for the 128 KB SRAM target. It only
changes the LVGL UI/integration footprint; motor-control firmware is untouched.

## Result

The old recommendation reserved 64 KB for the LVGL heap and 32 KB for one
800x20 RGB565 draw buffer. The new profile uses:

| Item | Old | Validated | SRAM released |
|---|---:|---:|---:|
| LVGL heap | 65,536 B | 53,248 B | 12,288 B |
| One draw buffer | 32,000 B | 16,000 B | 16,000 B |
| Total | 97,536 B | 69,248 B | **28,288 B** |

If the reported 98.5% figure came from exactly the old profile on a 128 KB
device, the same link would fall to roughly 77% before unrelated application
changes. Confirm the real result from the target `.map` file because stacks,
DMA sections and vendor libraries vary.

Flash remains plentiful. Static label captions now point directly at const
strings in Flash instead of duplicating them in the LVGL heap; const pointer
tables also remain read-only. Dynamic values use eight reusable fixed buffers,
so changing digits no longer allocates and frees LVGL strings at 5 Hz.

## Measured LVGL heap

Simulator: LVGL v8.4, `LV_MEM_SIZE = 52 KB`, one 800x10 draw buffer, Release
build, Demo enabled, 10 seconds virtual time per screen.

| Screen | Used | Free | Biggest free block | Fragmentation |
|---|---:|---:|---:|---:|
| Dashboard | 67% | 17,712 B | 17,512 B | 2% |
| Monitor | 59% | 22,328 B | 17,512 B | 22% |
| Control | 79% | 11,712 B | 10,848 B | 8% |
| Graphs | 66% | 18,352 B | 17,512 B | 5% |
| Diagnostics | 61% | 20,944 B | 17,512 B | 17% |
| Settings | 70% | 16,424 B | 15,624 B | 5% |

The 120-second Demo/START/tab-rebuild regression recorded 10,256 B minimum
free, 10,168 B minimum biggest block, 22% maximum fragmentation, and finished
at the commanded 320 RPM. This is the guardrail: a future change must not drive
minimum free below 6 KB or biggest block below 4 KB in `--stress-demo`.

## Firmware configuration

`ui_mcu_profile.h` is the single source of truth. In the target `lv_conf.h`,
include it before defining LVGL memory:

```c
#include "ui_mcu_profile.h"

#define LV_COLOR_DEPTH 16
#define LV_MEM_CUSTOM 0
#define LV_MEM_SIZE UI_LVGL_HEAP_BYTES
#define LV_IMG_CACHE_DEF_SIZE 0
#define LV_SHADOW_CACHE_SIZE 0
#define LV_USE_LOG 0
```

Use one partial draw buffer in the display port:

```c
static lv_color_t draw_pixels[UI_DRAW_BUF_PIXELS];
static lv_disp_draw_buf_t draw_buf;

lv_disp_draw_buf_init(&draw_buf, draw_pixels, NULL, UI_DRAW_BUF_PIXELS);
disp_drv.draw_buf = &draw_buf;
```

Place the array in the target's DMA-capable SRAM section if required by the
peripheral. Do not put it on the stack and do not add a second buffer unless a
measured throughput gain justifies another 16 KB.

Ten lines do not change the number of pixels transferred for a dirty region;
they split a tall region into more flush callbacks. With xSPI as the bottleneck,
the SRAM saving is more valuable than a 20-line staging buffer. The driver must
call `lv_disp_flush_ready()` only after DMA has completed.

## Why Demo START could corrupt the presentation

There were three reinforcing problems:

1. Real UART parsing and Demo wrote the same 64-bit bitfield unions. On a 32-bit
   MCU, the UI could observe mixed/torn fields during the ownership hand-off.
2. Monitor called `lv_label_set_text()` for changing stack strings. LVGL frees
   and reallocates the exact-sized label text whenever a value changes, causing
   heap churn and fragmentation during the busiest transition.
3. Demo toggle forced all UI lanes from inside an LVGL input event, immediately
   followed by new 10 Hz telemetry. START also jumped current to a large value
   before mechanical speed had ramped, dirtying several rows together.

The fix separates real and Demo status storage, selects one source through
accessors, defers repaint to the normal lane, uses fixed label buffers, caps a
settled Monitor tick at four values, and scales simulated current with the RPM
ramp. UART reception may now remain enabled during Demo without racing UI data.

## Verification commands

```powershell
tools/run_regression.bat
# Reuse the current sim_pc/build/sim_pc.exe:
tools/run_regression.bat -SkipBuild
```

For the MCU build, also inspect the `.map` file and record stack high-water
marks. A linker RAM percentage does not prove runtime stack headroom.

After linking, `tools/mcu_size.bat -Elf path/to/firmware.elf` reports the
standard GNU `text + data` Flash and `data + bss` static RAM totals.
