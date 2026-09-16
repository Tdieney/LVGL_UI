# Fix prompt · Smart Hub UI review round 2 (study 12 alignment)

> ARCHIVED — historical context only, not the current implementation request.
> Original location: `docs/reviews/implementation-r2/FIX_PROMPT.md`.
> Use [CURRENT.md](../CURRENT.md) for the active handoff and
> [prompt rules](../README.md) before creating or updating any prompt.

Second independent review of the LVGL implementation on branch `ui/smart-hub`.
Round 1 fixed the hang and the corrupted glyphs. What remains is (a) the design
moved to **study 12** — flat, neutral, RGB565-exact surfaces — and the code still
renders study 10/11 gradients, and (b) a set of deviations from the prototype,
including three regressions introduced by the round-1 fixes.

Copy everything below the line to the coding AI.

---

You are continuing work on the LVGL v8.4 Smart Hub UI in this repository
(`C:\Users\vanth\Documents\MyDocument\Embedded\LVGL_UI`, branch `ui/smart-hub`).
Read `AGENTS.md`, `docs/LVGL_IMPLEMENTATION_PROMPT.md` (updated to study 12) and
`docs/TECHNICAL_LEAD_REVIEW.md` first. Do not reset, clean, stash, push or change
branches. Preserve uncommitted work.

**The design reference is the current browser prototype**, `docs/prototype/`
(`index.html`, `styles.css`, and the five 800×480 PNGs re-rendered on 2026-09-07).
Open it and compare against your own LVGL screenshots. Do not redesign anything:
layout, proportions, typography, icons, shadows, radii and hit areas are settled.

## Already fixed in round 1 — do not undo

Verified independently: `--shots` completes all 12 scenarios instead of hanging;
odd-width glyphs (`0 7 8 9 - ₂ %`) render correctly; the selected Trends metric
selector is visible; heap is 81% used with 0% fragmentation and no drift across
navigation cycles. Event-context deletion, snapshot/command-state separation,
signed temperature formatting, the history window and minute-of-day wrapping were
reworked — I did not re-audit each of those, so leave them alone unless a change
below forces a touch.

## P0 — migrate the implementation to study 12 (flat, neutral, RGB565-exact)

The owner removed every gradient from the design and asked for a plain, non-blue
background. `ui_theme.c` still builds eight gradient styles.

**1. Replace every gradient with a single flat fill.**

In `ui_theme.c`, delete all `lv_style_set_bg_grad_color()` and
`lv_style_set_bg_grad_dir()` calls and set one `bg_color` per surface. After this
change there must be **no** `bg_grad_dir`/`bg_grad_color` anywhere in the code, on
any object, part or state.

| Style | New flat `bg_color` |
|---|---|
| `ui_style_canvas` | `0xEFEFEF` |
| `ui_style_card_co2` | `0xD6E7FF` |
| `ui_style_card_voc` | `0xE7DBFF` |
| `ui_style_card_temp` | `0xFFEBCE` |
| `ui_style_card_humid` | `0xCEEFE7` |
| `ui_style_card_device` | `0xF7F7F7` |
| `ui_style_card_chart` (chart card + settings dialog) | `0xFFFFFF` |
| `ui_style_action_grad` → rename `ui_style_action` | `0x215DDE` |
| `ui_style_tab_active` | `0x215DDE` |
| `ui_style_tab_inactive` | `0xDEEBF7` |

The canvas, device cards and chart/dialog are **pure neutral greys**
(`#EFEFEF` < `#F7F7F7` < `#FFFFFF`). Do not put a blue tint back on them. Surface
color now belongs only to the four metric hues, the blue action fill, the icon
plates and the semantic badges. Keep `lv_style_set_bg_opa(..., LV_OPA_COVER)` on
every one of these styles — round 1 already found that a style built on
`lv_obj_remove_style_all()` renders invisible without it.

**2. Update the remaining fill tokens in `ui_theme.h` to RGB565-exact values.**

| Token | Old | New |
|---|---|---|
| `UI_COLOR_BLUE` (switch on, chart line) | `0x1765E8` | `0x1865E7` |
| `UI_COLOR_BLUE_SOFT` (selected settings mode) | `0xEDF4FF` | `0xEFF3FF` |
| `UI_COLOR_GREEN_SOFT` | `0xE9F8EF` | `0xE7F7EF` |
| `UI_COLOR_AMBER_SOFT` | `0xFFF3DA` | `0xFFF3DE` |
| `UI_COLOR_RED_SOFT` | `0xFFF0ED` | `0xFFEFEF` |
| `UI_COLOR_NEUTRAL_SOFT` (Unknown badge) | `0xF0F3F8` | `0xEFF3F7` |
| `UI_COLOR_LINE` (separators, chart grid) | `0xDCE5F3` | `0xDEE7F7` |
| `UI_COLOR_PLATE_BLUE` (Devices icon plate) | `0xDFEAFF` | `0xDEEBFF` |
| `UI_COLOR_SWITCH_OFF` | `0xBCC7D6` | `0xBDC7D6` |

Reason: an exact RGB565 color has red and blue of the form `(v<<3)|(v>>2)` and
green of the form `(v<<2)|(v>>4)`. A non-exact value is rounded by the panel, and
the rounding is not uniform — `0xF0F3F8` reaches the LCD as `#F7F3FF`, a visible
magenta cast. Every value in both tables above is already exact; use them
verbatim, do not "improve" them, and pick any new fill from the same sets.

Also replace the two hardcoded literals that bypass the palette:
`lv_color_hex(0x985116)` and `lv_color_hex(0x17665D)` in the comfort cards should
use the existing `UI_COLOR_TEMP_ICON` / `UI_COLOR_HUMID_ICON` tokens, and the
settings dropdown background `lv_color_hex(0xF0F4FA)` becomes `0xF7F7F7`.

**3. Return the gradient cache to the heap.**

`sim_pc/lv_conf.h` sets `LV_GRAD_CACHE_DEF_SIZE 1800`. With no gradients left,
set it back to `0` and reclaim those 1800 bytes. Do the same in the MCU config
guidance in `docs/MCU_BASELINE.md`, which is currently marked superseded.

Keep it at `0` permanently and do not reintroduce gradients: LVGL v8.4 keys its
gradient cache on `descriptor address ^ size ^ (w >> 1)` (`lv_draw_sw_gradient.c`,
`compute_key`) with the colors omitted, and the descriptor is a stack temporary in
`lv_draw_rect_dsc_t`. That made the equally sized 250×208 CO₂ and VOC cards
collide and render byte-identical fills. With the cache disabled instead, every
gradient draw allocated ~1.6 KB from the 42 KiB heap until `LV_ASSERT_MALLOC`
locked the UI in `while(1)`. Flat fills remove both hazards.

**4. Re-measure and re-document.** Report heap peak, largest free block and
fragmentation per screen after the change, and update the figures in
`docs/MCU_BASELINE.md` — they were measured on the gradient build.

## P1 — regressions introduced by the round-1 fixes

**5. The Temperature card shows a radiator, not a thermometer.**

`ui.c:512` passes `&ui_icon_heat_24` (three vertical bars on a base, the heater
preset icon). The `ui_icon_thermometer_24` asset is present and decodes to a
correct thermometer; it is simply not used. Use it. Compare the top-right card in
your Home screenshot with `docs/prototype/home.png`.

**6. Bottom navigation buttons are too narrow.**

`ui.c:369` sets `lv_obj_set_size(btn, 168, 48)`. Measured on the rendered
framebuffers, the selected pill spans `x 108..275` (168 px) while the prototype
spans `x 20..264` (245 px). Round 1 changed this from a correct 245 px. Restore
equal-width buttons that fill the 760 px content width of the 64 px bar with
12 px gaps, matching the prototype, and keep the 48 px height and 14 px radius.

**7. Forbidden On/Off captions came back.**

The handoff states: no normal `On`/`Off` captions. `ui.c:693` writes
`"Active"`/`"Off"` on the Home tiles, and `ui.c:1094-1095` writes
`"<mode> · Active"`/`"<mode> · Off"` plus `"Running normally"`/`"Ready"` on the
Devices cards. The prototype shows only `Auto` or `Manual`; normal state is
expressed by the switch alone. Remove all four strings. Keep `Sending…`,
`Unknown` and `Retry`, which are real state feedback.

## P2 — remaining deviations from the prototype

**8. The Humidity note is clipped.** `ui.c:541-542` places the note at `y = 68` inside
a 98 px card with `pad_all = 14`, i.e. a 70 px content box; a 16 px label needs
~20 px and runs past the bottom edge. Visible in `home_poor.raw`, where
`High humidity` loses its descenders. Re-check every hardcoded `y` against its
padded content box, on both comfort cards.

**9. `Ventilation Fan` wraps to two lines on the Home tile.** `ui.c:560` sets the
name width to 125 px at font 18; the string needs about 130 px. The prototype
keeps it on one line. Reclaim the width (the tile content box is 157 px and the
name starts at `x = 32`), or reduce the icon gap. Whatever you choose, a
two-line name must not overflow the 40 px header row — the handoff requires
two-line device names to render without overlap, so keep a case that proves it.

**10. Trends chart has no Y-axis labels.** The prototype prints three values on
the left (e.g. 920 / 784 / 647). Add them with `lv_chart_set_axis_tick` plus a
`LV_EVENT_DRAW_PART_BEGIN` handler, or with static labels you update in the same
bounded lane as the rest of the page. Keep the work bounded and allocation-free.

**11. Trends chart draws vertical grid lines the prototype does not have.**
`ui.c:847` uses `lv_chart_set_div_line_count(chart, 3, 4)`; the reference has
three horizontal lines only. Use `(3, 0)`.

**12. Trend summary values are the wrong size and bunched together.** The handoff
specifies 32 px trend summary values; `ui.c:790/795/800` use `ui_font_24`. The
three groups sit at `x = 0 / 160 / 300` while the prototype spreads them across
the card at roughly `x = 16 / 266 / 516`. Match the reference spacing and size.

**13. Devices cards are missing the separator and the icon plate is too small.**
The prototype draws a thin `UI_COLOR_LINE` hairline between the header row and
the control row, and a 46×46 plate with 12 px radius in `#DEEBFF` behind the
device icon. `ui.c:1003-1009` paints a 36×36 plate at radius 8 in
`UI_COLOR_BLUE_SOFT`, and there is no separator. There is also unused space at
the bottom of each card; compare the vertical rhythm with `docs/prototype/devices.png`.

**14. Settings dialog: mode buttons are in the wrong order and the close button
has no plate.** `ui.c:1280` declares `{ "Auto", "Manual" }`; the prototype puts
**Manual on the left and Auto on the right**. Reorder the buttons without
changing which enum value each one selects. `ui.c:1244` creates the close icon
with no background; the prototype has a rounded neutral plate behind the X.

**15. Supporting text is not semibold.** The handoff asks for semibold-equivalent
glyphs for captions, units and axes; the build uses Montserrat Regular.
`docs/MCU_BASELINE.md` already declares this as a deliberate Flash trade-off. Keep
the trade-off if you still judge it right, but state it explicitly in your report
rather than leaving it as a silent deviation.

**16. The `ppm` unit is positioned by a fixed offset.** `ui.c` places it at
`x = 140` regardless of how wide the reading is; with `1420` the gap collapses to
about 8 px, and a wider value would collide. Position it from the measured width
of the value label (`lv_obj_get_width` after `lv_obj_update_layout`, or a flex row
with baseline alignment) so it cannot overlap.

## P3 — code hygiene (no behavior change)

**17.** `s_trends_chart_series` is declared `lv_obj_t *` but holds an
`lv_chart_series_t *`, forcing casts in both directions. Give it the right type.

**18.** Several labels are positioned by index right after creation, e.g.
`lv_obj_set_pos(lv_obj_get_child(card_temp, 1), 34, 4)` and
`lv_obj_get_child(card, 0/2/4)` on the Trends card. Keep the pointer returned by
`add_static_label()` instead; index lookups silently break when a child is added.

**19.** `ui_style_action_grad` is no longer a gradient. Rename it and any comment
that still says "gradient" so the code matches the design vocabulary.

## Verification required

Run and paste real output for:

```powershell
tools/build_sim.bat
tools/run_regression.bat
tools/export_mcu.bat --verify
sim_pc\build\sim_pc.exe --shots <dir>
```

Then:

1. Convert every scenario `.raw` with `tools/raw2png.py` and put each one
   **side by side with the matching `docs/prototype/*.png`**. For every remaining
   difference, say whether it is intentional and why. Screenshots are the
   acceptance evidence, not the CSS.
2. **Sample the framebuffer, do not trust the source.** For each surface, print
   the actual RGB of a text-free pixel and show it equals the table above exactly:
   canvas, CO₂, VOC, temperature, humidity, device card, chart card, dialog,
   selected nav pill, selected and inactive metric selector. A near-miss such as
   `(255,246,255)` means a non-exact color slipped in.
3. Confirm `grep` finds no `bg_grad_dir`, `bg_grad_color` or `LV_GRAD_DIR_VER` in
   the production sources, and that `LV_GRAD_CACHE_DEF_SIZE` is `0`.
4. Report heap peak, largest free block and fragmentation for Home, Trends with
   data, Trends empty, Devices, Devices with the settings dialog open,
   splash → Home, and after 50 page/dialog cycles, with the headroom against
   42 KiB. State how much the flat-fill change gave back.
5. Re-check the states that exercise the layout fixes: four-digit CO₂ (1420) with
   `ppm`, `High humidity` and other long comfort notes, a two-line device name,
   `Sending…`/`Unknown`/`Retry`, offline, and partial sensor failure. Nothing
   clipped, nothing overlapping.
6. Update `PLAN.md`, `DEV_LOG.md` and `docs/MCU_BASELINE.md`. List every fix you
   could not complete and every remaining deviation from study 12 explicitly. Do
   not report a check as passed unless you ran it.
