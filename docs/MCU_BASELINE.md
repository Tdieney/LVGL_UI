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

Before feature implementation is considered complete:

1. Confirm the Hub MCU, SRAM/Flash, LCD controller, QSPI mode/rate, and DMA rules.
2. Inspect the target linker map including LoRa, protocol, storage, and sensor data.
3. Measure stack high-water and LVGL heap on the highest-memory screen.
4. Profile dirty pixels/flush callbacks under live updates.
5. Call `lv_disp_flush_ready()` only after DMA completion and keep all LVGL API
   calls in one execution context.

Do not allocate a full 800x480 RGB565 framebuffer on the MCU; it would consume
768,000 bytes and the display already owns GRAM.
