# Putting the UI into the firmware

For the course ESP-IDF project `lvgl_practice` (ESP32-S3, ST7796 480 x 320
SPI, FT6336 touch). Nothing here has been tried on hardware yet.

## Files

`tools\export_mcu.bat` packs the portable sources: `studio_comm.h`, `ui.c/.h`,
`ui_internal.h`, `ui_types.h`, `ui_model.c/.h`, `ui_theme.c/.h`,
`ui_icons.c/.h`, `ui_fonts.c/.h`, `ui_mcu_profile.h` and these documents.
Add each `.c` once to the component that replaces the course mock `ui/`
sources. The sources use plain `#include "lvgl.h"`.

## LVGL configuration

Match `sim_pc/lv_conf.h`:

- LVGL 8.4, `LV_COLOR_DEPTH 16`, a 48 KiB heap to start with.
- `LV_GRAD_CACHE_DEF_SIZE 0`; themes off; Montserrat 14 only (LVGL's default).
- Widgets used: base object, label, image (`lv_bar`, `lv_switch`, and
  `lv_dropdown` are not required by the UI).
- One partial draw buffer, 480 x 20 RGB565 (19,200 bytes), DMA-capable. Never a
  full 480 x 320 framebuffer.
- RGB565 byte order for the SPI panel is set in the display port
  (`LV_COLOR_16_SWAP` or the flush routine), never in UI code. `ui_theme.c`
  follows `LV_COLOR_16_SWAP` for its constant colours.
- Call `lv_disp_flush_ready()` only when the SPI transfer has finished.

## Calls

All of these run in the GUI task, the one that calls `lv_timer_handler()`.
Never from an ISR or the UART task.

```c
ui_init(on_ui_action, app_context);   /* after display and touch are registered */
ui_apply(&cmd);                       /* each validated command from the PC */
ui_note_rx();                         /* lights the Connection RX indicator dot */
ui_baud_complete(rate, ok);           /* when a baud change has finished */
```

The UI keeps its own LVGL timer (20 ms) and uses `lv_tick_get()`; there is no
`ui_tick()` to call, and `ui_note_rx()` causes zero periodic timer redraws.
Replace the course `ui_init()` call with the one above and do not start the old
mock UI.

## From the PC: UART task to GUI task

1. The UART task reads bytes into a bounded line buffer and splits on `\n`.
2. It parses the JSON and validates types and ranges itself; `ui_apply()` checks
   ranges again but does not parse JSON or check field types.
3. It fills a `studio_cmd_t` (see `studio_comm.h` for the field mapping) and
   puts it in a queue. It never calls LVGL.
4. The GUI task drains the queue: `ui_apply(&cmd)` then `ui_note_rx()`.

`ENV` temperature and humidity are whole integers (`int16_t` `temp` and `0..100`
`humid`): `{"cmd":"ENV","temp":27,"humid":60}` maps directly to `temp = 27`,
`humid = 60`. If a sender passes a float by mistake, check that it is finite and
in range and round to the nearest integer before casting. `valid = false` shows
"—".

## From the LCD: callback to UART

`on_ui_action(const studio_cmd_t *cmd, void *ctx)` runs in the GUI task with a
borrowed `cmd`. Copy it into a queue and return; do not wait for the UART. The
UART side writes it as `{"event":...}\n`. During a brightness drag the UI sends
at most one LIGHT per 100 ms and always the final value; do not drop that final
event when the queue is full.

After a SCENE (from the LCD or the PC), send the resulting LIGHT and AC events
from `ui_get_model()` so the Web page ends up in the assignment's state; the
course Web starter would otherwise show Sleep as light on at 10 %.

## Baud change

1. Stepping `<` / `>` on the Connection panel previews a baud rate locally;
   tapping the Apply pill (`✓`) sends a `STUDIO_CMD_BAUD` action, locks the
   steppers and pill, and shows the pending state (`···`).
2. The firmware tells the PC (`{"event":"BAUD","val":...}`), lets the TX queue
   drain, switches its UART, and waits for the PC to come back.
3. It calls `ui_baud_complete(rate, true)` or `(rate, false)`. Only the pending
   rate completes; success updates the active-rate dot and restores `✓`, while
   failure keeps the old rate and shows `✕` (tapping `✕` retries, or stepping
   `<` / `>` clears the error).

A `{"cmd":"BAUD"}` from the PC means the change has already happened on both
sides; pass it to `ui_apply()` only after the UART has really switched (this
also syncs the stepper selection and clears any prior failed pill state).

The course Web starter needs fixes before a live baud demo: release the reader
and writer locks before `port.close()`, and do not answer a BAUD event with
another BAUD request.

## Open choices

- Which serial port: UART0 (console), native USB Serial/JTAG, or UART1 on
  GPIO17/18.
- Whole-firmware RAM, touch feel, SPI byte order and readability need checking
  on the real panel.
