# MCU memory profile and Demo-start hardening

This is the validated integration profile for the 128 KB SRAM target. It only
changes the LVGL UI/integration footprint; motor-control firmware is untouched.

## Result

The old recommendation reserved 64 KB for the LVGL heap and 32 KB for one
800x20 RGB565 draw buffer. The new profile uses:

| Item | Old | Validated | SRAM released |
|---|---:|---:|---:|
| LVGL heap | 65,536 B | 43,008 B | 22,528 B |
| One draw buffer | 32,000 B | 16,000 B | 16,000 B |
| Total | 97,536 B | 59,008 B | **38,528 B** |

If the reported 98.5% figure came from exactly the old profile on a 128 KB
device, the same link would fall to roughly 69.1% before unrelated application
changes. Confirm the real result from the target `.map` file because stacks,
DMA sections and vendor libraries vary.

Flash remains plentiful. Static label captions point directly at const strings
in Flash instead of duplicating them in the LVGL heap; const pointer tables also
remain read-only. Dynamic values use reusable fixed buffers, so changing digits
does not allocate and free LVGL strings at 5 Hz. Control creates only its active
LEVEL or POSITION subtree, and the resident navigation rail uses one custom-drawn
object instead of 18 button/pill/image children. Those changes made it safe to
reduce the configured heap from 52 KB to 42 KB, returning another 10,240 B of
real static SRAM to firmware rather than merely showing more free LVGL heap.

## Measured LVGL heap

Simulator: LVGL v8.4, `LV_MEM_SIZE = 42 KB`, one 800x10 draw buffer, Release
build, Demo enabled, 10 seconds virtual time per screen.

| Screen | Used | Free | Biggest free block | Fragmentation |
|---|---:|---:|---:|---:|
| Dashboard | 52% | 20,800 B | 20,640 B | 1% |
| Monitor | 62% | 16,696 B | 15,904 B | 5% |
| Control SPEED/TORQUE | 73% | 11,776 B | 9,384 B | 21% |
| Control POSITION | 58% | 18,464 B | 16,200 B | 13% |
| Graphs | 66% | 14,880 B | 14,088 B | 6% |
| Diagnostics | 54% | 20,208 B | 19,184 B | 6% |
| Settings | 57% | 18,832 B | 16,816 B | 11% |

The 120-second Demo/START/tab-rebuild regression recorded 11,368 B minimum
free, 10,200 B minimum biggest block and 14% maximum fragmentation while cycling
the SPEED, TORQUE and POSITION Control layouts. The regression guard is now
8 KB for both minimum free and minimum biggest block. Control LEVEL at 73% used /
21% fragmentation is the tight spot; watch it before adding widgets to that
screen. LEVEL↔POSITION is rebuilt only from `ui_tick()`, so event callbacks never
tear down live objects; SPEED↔TORQUE reuses the same LEVEL tree in place.

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
