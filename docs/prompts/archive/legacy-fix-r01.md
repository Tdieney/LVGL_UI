# Fix prompt · Smart Hub UI implementation review round 1

> ARCHIVED — historical context only, not the current implementation request.
> Original location: `docs/reviews/implementation-r1/FIX_PROMPT.md`.
> Use [CURRENT.md](../CURRENT.md) for the active handoff and
> [prompt rules](../README.md) before creating or updating any prompt.

Independent review of the LVGL implementation currently in the working tree
(branch `ui/smart-hub`, uncommitted). The build is clean, but the application
**hard-hangs**, renders **corrupted glyphs**, and the state model conflicts with
the contract in `docs/LVGL_IMPLEMENTATION_PROMPT.md`.

Copy everything below the line to the coding AI.

---

You are fixing defects in your own LVGL v8.4 Smart Hub UI implementation in this
repository (`C:\Users\vanth\Documents\MyDocument\Embedded\LVGL_UI`, branch
`ui/smart-hub`). Read `AGENTS.md`, `docs/LVGL_IMPLEMENTATION_PROMPT.md` and
`docs/TECHNICAL_LEAD_REVIEW.md` first. Do not reset, clean, stash, push or change
branches. Preserve the uncommitted design work. Do not redesign the UI — the
study 11 layout and palette stay as they are; fix the defects below.

Every defect listed here was reproduced. Do not dismiss one as "cannot reproduce"
without showing the command and output you used.

## P0 — the application hangs (must fix first)

**1. LVGL heap exhaustion inside the gradient renderer causes an infinite loop.**

`sim_pc\build\sim_pc.exe --shots <dir>` writes shots 1–6 and then hangs forever,
burning 100% CPU. Reproduce:

```powershell
sim_pc\build\sim_pc.exe --shots %TEMP%\shots
# hangs after home_sensor_fail.raw, before scenario 7 (home_relay_pending)
```

Attached debugger, thread 1:

```
#0  lv_gradient_get ()          <- 0x...260: jmp 0x...260   (self-jump)
#1  draw_bg ()
#2  lv_draw_sw_rect ()
...
#10 lv_timer_handler ()
#11 advance_ui ()
#12 run_scenario_shots ()
```

The instruction at the stop address is `jmp` to itself, immediately after a call
to `_lv_log_add`. That is `LV_ASSERT_MALLOC` → `LV_ASSERT_HANDLER` (`while(1);`)
inside `allocate_item()` in `lv_draw_sw_gradient.c`. Root cause chain:

- `LV_GRAD_CACHE_DEF_SIZE` defaults to `0`, so LVGL allocates a temporary
  gradient map from the **LVGL heap on every gradient draw**
  (`ALIGN(sizeof(lv_grad_t)) + ALIGN(max(w,h) * sizeof(lv_color_t))`;
  ≈1.6 KB for the 800-px canvas, ≈0.5 KB per card).
- The widget tree already consumes the budget: `--regression` reports
  `used=86% free=6288 biggest=4208 frag=34%` on a 42 KiB heap. One extra
  object (the `Sending…` label in scenario 7) pushes the largest free block
  below the gradient request and the allocation fails.
- A failed `lv_mem_alloc` in that path is not recoverable — it deadlocks. On the
  MCU this is a silent lockup of the whole UI, not a visual glitch.

Requirements:

- Make the UI fit the **42 KiB** budget with real headroom, and state the measured
  peak. Do not raise `LV_MEM_SIZE`/`UI_LVGL_HEAP_BYTES` to hide the problem. If
  the study 11 design genuinely cannot fit, report the measured numbers and the
  specific trade-off instead of silently changing the budget.
- The object count per page is the main cost: many wrapper containers exist only
  to hold one label plus one icon (`co2_head`, `co2_read`, `tile_head`,
  `tile_bot`, `state_box`, `temp_info`, `right_box`, `uptime_box`, `link_box`,
  `stats_box` and one `item` per stat, …). Collapse them, or lay out
  labels/icons directly on the card with `lv_obj_set_pos`/`lv_obj_align`.
- Decide and justify how gradients are drawn under this budget. Options:
  size the gradient cache explicitly (`lv_gradient_set_cache_size()` /
  `LV_GRAD_CACHE_DEF_SIZE`) so the map is allocated once instead of per draw,
  and/or reduce the number of distinct gradient surfaces. Measure the effect on
  both heap and redraw cost with the 800×10 partial buffer — do not guess.
- Fragmentation is already 34% because the whole page is torn down and rebuilt
  (see P1.3). Fixing that reduces both peak use and fragmentation.
- Whatever you choose, an allocation failure must not be reachable in normal
  operation. Also report what happens on the MCU if it is: LVGL's default assert
  handler is an infinite loop.

**2. `--shots` and the regression suite hide the hang.**

`tools/run_regression.bat` only runs `--smoke`, `--regression` and `--shot`, so it
reports PASS on a build whose scenario generator locks up. Add the scenario
generator (or the equivalent state coverage: pending, retry, trends, devices,
settings dialog) to the regression suite, with a bounded wall-clock timeout so a
lockup fails the run instead of hanging CI. `--regression` must also assert
heap headroom, not just zero drift.

## P1 — correctness

**3. Deleting the object that is currently handling its own event (use-after-free).**

`toggle_device()` ([ui.c:675](../../../ui.c#L675)) calls `refresh_active_page()`,
which does `lv_obj_del(s_page_container)` ([ui.c:1232](../../../ui.c#L1232)) — the
switch/button that is mid-event is a descendant of that container. LVGL v8
continues to touch the object after the user callback returns (`lv_switch`'s own
class handler invalidates and animates it, and `event_send_core` walks its
descriptor list), so this is a use-after-free, not a style issue. Same pattern in:

- `on_home_card_click` → `ui_select_trend_metric` → `ui_navigate_to_page`
- `on_metric_tab_click`, `on_device_retry_click`, `on_device_switch_click`
- `on_dialog_close_click` / `on_dialog_save_click` → `close_device_dialog()`
  deletes the overlay containing the clicked button
  ([ui.c:1223](../../../ui.c#L1223))

Fix by deferring the teardown out of the event context (`lv_obj_del_async`, or an
`lv_async_call`/one-shot timer that performs the rebuild), for every path that can
destroy the widget tree from inside a callback. LVGL documents this explicitly.

**4. The application snapshot and the UI's own command state share one struct.**

`ui_update_snapshot()` does `s_snapshot = *snapshot`
([ui.c:1280](../../../ui.c#L1280)) while `toggle_device()` and
`on_dialog_save_click()` write `pending`, `error`, `pending_req_id`, `on`, `mode`,
`preset` and `name` into that same `s_snapshot`. Consequences:

- Any snapshot push while a command is in flight wipes `pending` /
  `pending_req_id`, so the acknowledgement can never be correlated and the tile is
  stuck showing the reported state.
- The requested target is never stored. `ui_handle_command_result()` does
  `d->on = !d->on` ([ui.c:1312](../../../ui.c#L1312)) — it flips whatever the
  current value happens to be instead of applying the value that was requested.
  Any intervening snapshot makes the confirmed state wrong.
- `on_dialog_save_click()` writes `preset`, `name` and `mode` locally and
  announces "Device settings updated." before the application confirms anything.
  The handoff requires that a user intent is not an acknowledgement.

Implement the separation the contract asks for: an immutable input snapshot from
the application, plus a separate UI view/command state holding per-device
`requested_on`, `requested_mode`, `requested_preset`, `req_id` and phase
(idle/pending/failed). Apply the *requested* value on success, correlate by
request id, ignore stale/unknown ids, and refuse to issue a second command while
one is pending. Keep saved settings pending until confirmed.

**5. `lv_label_set_text_static()` pointing into mutable, non-const storage.**

Device tiles and cards pass `d->name` — a `char[24]` inside `s_snapshot` — to
`add_static_label()` ([ui.c:601](../../../ui.c#L601),
[ui.c:968](../../../ui.c#L968)), which calls `lv_label_set_text_static()`. The
next `ui_update_snapshot()` memcpy rewrites those bytes underneath the live label
with no invalidation and no re-measure, so the label can show torn text at a
stale width. The handoff requires process-lifetime const storage for static text,
or a persistent per-field buffer that is explicitly re-applied. Fix all such uses
(names, notes and every shared `s_*_buf` that is re-formatted while a label
still points at it).

**6. Chart point inspection never fires.**

`on_chart_click` is registered on `LV_EVENT_CLICKED`
([ui.c:889](../../../ui.c#L889)), but `lv_chart_event()` resets
`chart->pressed_point_id = LV_CHART_POINT_NONE` on `LV_EVENT_RELEASED`
(`lv_chart.c:731`), which LVGL sends *before* `LV_EVENT_CLICKED`. So
`lv_chart_get_pressed_point()` always returns `LV_CHART_POINT_NONE`, the
`point_id < UI_HISTORY_CAPACITY` guard always fails, and no tooltip is ever
shown. Use `LV_EVENT_PRESSED`/`LV_EVENT_PRESSING` or the chart's own
`LV_EVENT_VALUE_CHANGED`, and add a test that asserts a tooltip is produced.

**7. Negative temperatures print corrupted values.**

The Home card guards the sign ([ui.c:532](../../../ui.c#L532)) but Trends
Current/Min/Max ([ui.c:803](../../../ui.c#L803) and neighbours) and the chart
tooltip do not: `-2.5 °C` (`value = -25`) formats as `-2.-5°C` because
`%d.%d` is fed a negative remainder. The handoff explicitly requires negative
temperatures. Add one shared signed fixed-point formatter and use it everywhere.

**8. `ui_feed_history()` keeps the oldest samples and never wraps the clock.**

[ui.c:1293](../../../ui.c#L1293): `n = min(count, UI_HISTORY_CAPACITY)` then
copies indices `0..n-1`, so when the application supplies more samples than the
capacity the **newest** readings are discarded and the chart shows stale history.
Keep the most recent window. `minute_of_day = start_minute + i`
([ui.c:1297](../../../ui.c#L1297)) is never taken modulo 1440, so the axis labels
and tooltips print `24:07`; the x-axis labels at
[ui.c:901](../../../ui.c#L901) have the same defect. Also document the actual
capacity (`UI_HISTORY_CAPACITY = 16`), the sample cadence and the overflow
behaviour, and reconcile them with the "15-minute rolling view" requirement — the
axis currently always claims a 15-minute span even when fewer points exist.

**9. Fabricated comfort text.**

[ui.c:541](../../../ui.c#L541) and [ui.c:574](../../../ui.c#L574) hardcode
`"Comfortable"` for temperature and humidity whenever the link is up — including
when `metrics[].valid == false` or the supplied `quality` is Moderate/Poor. The
contract says supplied categories are shown, never invented. Use the snapshot's
category/note, and show an unavailable state when the metric is invalid.

**10. Simulator copy leaked into production UI.**

[ui.c:858](../../../ui.c#L858): the empty-trends state reads "This session is
empty. Choose a scenario to load a sample session." — "scenario" is a simulator
fixture concept that does not exist on the device. Replace with product copy.

**11. `ui_replay_splash()` deletes the active screen.**

[ui.c:1392](../../../ui.c#L1392) calls `lv_obj_del(s_splash_screen)` without
loading another screen first. The completion path at
[ui.c:1378](../../../ui.c#L1378) correctly does `lv_scr_load(s_screen)` before
deleting; replay does not, so replaying while the splash is still on screen
deletes the display's active screen. Load `s_screen` first (or use
`lv_scr_load` + `lv_obj_del_async`).

**12. Unbounded periodic timers.**

`ui_announce()` creates the toast timer with `lv_timer_create(..., 2600u, NULL)`
([ui.c:183](../../../ui.c#L183)) and never sets `lv_timer_set_repeat_count(t, 1)`
and never deletes it, so it keeps waking every 2.6 s for the rest of the process
lifetime. The splash timer runs at 20 ms and re-applies `img_opa` and
`lv_obj_align` on every tick even during the 600–2100 ms steady phase, needlessly
invalidating the logo ~75 times. Make the toast one-shot and drive the splash with
`lv_anim` or at least skip no-op writes. The handoff requires bounded redraw lanes.

**13. Repeated taps on the active tab leak styles.**

`update_navigation_selection()` calls `lv_obj_add_style()` on the selected button
every time ([ui.c:318](../../../ui.c#L318)) without removing it first. Selecting
the page that is already selected appends the same style again, growing the
object's style list without bound. Remove before adding, or skip when unchanged.
The existing 50-cycle memory check does not catch this because it always
alternates pages.

**14. Dangling dialog pointers.**

`close_device_dialog()` clears `s_dialog_overlay` and `s_dialog_preset_dd` but
leaves `s_dialog_mode_btns[0..1]` pointing at freed objects
([ui.c:1223](../../../ui.c#L1223)). Clear them.

**15. Full page rebuild on every snapshot.**

`ui_update_snapshot()` and `ui_feed_history()` call `refresh_active_page()`, which
destroys and recreates the entire page. The handoff requires bounded update lanes
that touch only changed values. This is also the direct cause of the 34%
fragmentation and it destroys in-progress touch interactions. Introduce per-field
update functions and keep the widget tree alive across snapshots.

## P2 — rendering and layout defects

**16. Custom font bitmaps use per-row byte padding; LVGL reads a continuous bitstream.**

This is why the UI shows garbage glyphs. In `tools/generate_assets.py` the packer
emits `stride = (cw + 1) // 2` bytes per row. LVGL's
`draw_letter_normal()` (`lv_draw_sw_letter.c:238`) computes
`width_bit = box_w * bpp` and advances with
`col_bit += (box_w - col_end + col_start) * bpp; map_p += col_bit >> 3;` — glyph
rows are packed **continuously with no byte alignment**, exactly as
`lv_font_conv` emits them. At 4 bpp every glyph whose `box_w` is odd therefore
desynchronises by 4 bits per row and renders as a diagonal smear.

Affected glyphs today:

| Table | Odd-width glyphs (broken) |
|---|---|
| `s_glyphs_62` | `0` `7` `8` `9` `-` |
| `s_glyphs_fb_16/18/20/24/30/34` | `₂`, `%` (per size) |

Confirmed visually: CO₂ `428` renders as `42` plus a diagonal blob, VOC `85`
renders as a blob plus `5`, and the `CO₂` heading subscript is garbage in
`home_good.raw`/`home_sensor_fail.raw` and in
`docs/reviews/implementation-r1/review_trends.png`. Decoding the arrays with the
padded stride reproduces clean glyphs, which proves the **data is fine and the
layout contract is wrong**.

Fix the generator to emit a continuous 4 bpp bitstream per glyph (accumulate
nibbles across row boundaries), regenerate `ui_fonts.c`, and add a check that
renders every glyph of every custom font and compares it against the Pillow
reference — a smoke test that only asks "did a glyph descriptor exist" (current
regression CHECK 4) passes while the pixels are garbage.

**17. The selected Trends metric selector has no background.**

`ui_style_tab_active` ([ui_theme.c:114](../../../ui_theme.c#L114)) sets
`bg_color`, `bg_grad_color`, `bg_grad_dir`, `border_width`, `radius` and
`text_color` but **never `bg_opa`**. The buttons are created with
`lv_obj_remove_style_all()`, so `bg_opa` falls back to the LVGL default of
`LV_OPA_TRANSP` (`lv_style.c:63`) and the gradient is never painted: the active
tab is white text on the light canvas, i.e. invisible. Visible in
`docs/reviews/implementation-r1/review_trends.png`, where the selected `CO₂`
selector is blank. Set `bg_opa` and audit every other style built on top of
`lv_obj_remove_style_all()` for the same omission.

**18. Devices page bottom row overflows its card.**

Cards are 136 px with `pad_all = 16` → 104 px of content height, but the bottom
row is placed at `y = 60` with height 48 ([ui.c:984](../../../ui.c#L984)) = 108 px.
Scrolling is disabled, so the switch/Retry row is clipped by 4 px. Re-check every
hardcoded offset against its padded content box.

**19. Device name wraps and overflows the tile header.**

`lv_obj_set_width(lbl_name, 120)` ([ui.c:602](../../../ui.c#L602)) is narrower
than the prototype, so "Ventilation Fan" wraps to two lines inside a 40 px header
and pushes into the row below (compare `home_good.png` with
`docs/prototype/home.png`). The handoff explicitly requires handling two-line
device names without overlap. Give the name the available width and define the
two-line behaviour (grow the header, or ellipsize with `LV_LABEL_LONG_DOT`).

**20. Error state is labelled "Unknown".**

Both device views print `"Unknown"` for `!valid` *and* for `error`
([ui.c:617](../../../ui.c#L617), [ui.c:1000](../../../ui.c#L1000)). A failed
command and an unknown output are different conditions and the handoff asks for
distinct `Sending` / `Unknown` / `Retry` feedback. Differentiate the text.

**21. Settings mode buttons do not change their label colour.**

`on_dialog_mode_btn_click()` sets `text_color` on the button
([ui.c:1047](../../../ui.c#L1047)), but the caption is a child label with its own
explicit `text_color` from `add_static_label()`
([ui.c:1189](../../../ui.c#L1189)), which wins. Selecting a mode repaints the
background but not the text. Set the colour on the label, or let the label inherit.

**22. Saving while disconnected fails silently.**

`on_dialog_save_click()` closes the dialog and returns with no feedback when the
link is down ([ui.c:1063](../../../ui.c#L1063)). The Save button is disabled but
`ui_save_device_settings()` reaches the same path. Report the refusal.

**23. Stale pixels in the r1 Trends screenshot.**

`docs/reviews/implementation-r1/review_trends.png` shows purple/pink wash across
the selector row and the left of the chart card — the colour of the Home VOC card
gradient, not of any Trends surface. Determine whether this is incomplete
invalidation on page rebuild or a harness that relies on `lv_refr_now()` without
a full invalidate, and fix the real cause. Do not re-publish screenshots with
that artifact.

## P3 — assets, portability, documentation

**24. Icon generation depends on a headless Edge screenshot at a fixed grid.**

`generate_assets.py` renders all icons into one HTML page, screenshots it with
`msedge.exe --headless --window-size=800,600` and crops by precomputed
coordinates. This silently breaks with a different Edge version, device pixel
ratio or page zoom, and it hardcodes
`C:\Program Files (x86)\Microsoft\Edge\Application\msedge.exe`. Make asset
generation reproducible without a browser dependency (rasterize the SVG paths
directly), or pin and verify the output (checksums plus a visual diff) and fail
loudly when the environment differs. The decoded icon bitmaps themselves are
currently intact — do not regenerate them blind and lose that.

**25. Undeclared Montserrat dependency and unused fonts.**

`ui_font_16..34` delegate to `lv_font_montserrat_16/18/20/24/30/34`, which exist
only because `sim_pc/lv_conf.h` enables them. `tools/export_mcu.bat` exports
`ui_fonts.c` but nothing states that the firmware's `lv_conf.h` must enable those
six Montserrat sizes, and `LV_FONT_MONTSERRAT_14` and `_48` are enabled but
unused. Document the required `lv_conf.h` symbols in the MCU export, drop the
unused sizes, and report the Flash cost of six full Montserrat faces against the
target budget. Also note the handoff asks for semibold-equivalent supporting
text; Montserrat regular is a deviation that must be either fixed or declared.

**26. `ui_fonts_init_fallbacks()` is an empty function.**

It is exported, called from `ui_init()` and does nothing. Remove it or give it a
purpose.

**27. `DEV_LOG.md` does not record the implementation.**

The new `## 2026-09-07` entries end with "no C/LVGL implementation changed" while
`ui.c` grew by ~1,550 lines and seven new production files were added. The log is
now actively wrong. Add an honest entry for this implementation and for this fix
round, and update `PLAN.md` Phase 3.

## Verification required

Run and paste real output for:

```powershell
tools/build_sim.bat
tools/run_regression.bat
tools/export_mcu.bat --verify
sim_pc\build\sim_pc.exe --shots <dir>     # must complete, not hang
```

Then:

1. Convert every scenario `.raw` with `tools/raw2png.py` and inspect all of them
   at 800×480. Confirm: no garbled glyphs (check `0 7 8 9 - ₂ % ° … ·` and
   four-digit CO₂ such as 1420 and 850), the selected Trends selector is visible,
   nothing clipped or overlapping on Devices, two-line device names.
2. Report LVGL heap peak, largest free block and fragmentation for: Home, Trends
   with data, Trends empty, Devices, Devices + settings dialog open, splash → Home,
   and after 50 page/dialog cycles. State the headroom against 42 KiB.
3. Show tests that cover: negative temperature formatting, request/ack correlation
   across an intervening snapshot push, no command issued while pending, cancelled
   settings not mutating state, chart point inspection returning a tooltip,
   history overflow keeping the newest window, minute-of-day wrapping past
   midnight, and page rebuild triggered from inside an event callback.
4. List every deviation from study 11 that remains, and every fix you could not
   complete, explicitly. Do not report a check as passed unless you ran it.
