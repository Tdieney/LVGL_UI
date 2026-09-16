# Fix prompt · Smart Hub UI review round 3

> ARCHIVED — historical context only, not the current implementation request.
> Original location: `docs/reviews/implementation-r3/FIX_PROMPT.md`.
> Use [CURRENT.md](../CURRENT.md) for the active handoff and
> [prompt rules](../README.md) before creating or updating any prompt.

Third independent review of the LVGL implementation on branch `ui/smart-hub`.
Round 2 landed the study 12 migration cleanly: verified 12/12 regression checks,
`--shots` completing, no gradients left, `LV_GRAD_CACHE_DEF_SIZE 0`, and all
thirteen surface fills sampled from the framebuffer matching the contract exactly
with CO₂ and VOC no longer colliding. The three round-1 regressions are gone and
the Montserrat removal saved real Flash.

What remains: one visible regression, two state-machine defects, a set of
per-frame costs that will hurt on the MCU, and test-only code shipping to
firmware.

Copy everything below the line to the coding AI.

---

You are continuing work on the LVGL v8.4 Smart Hub UI in this repository
(`C:\Users\vanth\Documents\MyDocument\Embedded\LVGL_UI`, branch `ui/smart-hub`).
Read `AGENTS.md`, `docs/LVGL_IMPLEMENTATION_PROMPT.md` and
`docs/TECHNICAL_LEAD_REVIEW.md` first. Do not reset, clean, stash, push or change
branches.

**Do not redesign anything.** The layout, palette and typography of study 12 are
settled and now match the prototype closely. Everything below is a defect fix, a
performance fix or a cleanup. Preserve the round-2 work: the flat RGB565-exact
palette, the removal of Montserrat, the in-place widget updates, the command-state
separation and the 12-check regression suite.

## P0 — correctness

**1. The splash background regressed and now shows a rectangle around the logo.**

[ui.c:1742](../../../ui.c#L1742) sets the splash screen to `0xEFEFEF`. The design
contract pins it to **`#E9ECF1`** (`docs/LVGL_IMPLEMENTATION_PROMPT.md`, "The
original splash background remains #E9ECF1 to match its unchanged logo asset"),
because `docs/prototype/splash-logo.png` is an **RGB image with no alpha** — its
own background is baked into the bitmap.

Measured in the generated `splash.raw`: the 380×126 logo area renders
`(239,239,247)` while the surrounding screen renders `(239,239,239)`. That is a
visible bluish rectangle behind the logo. `#E9ECF1` quantises to exactly
`(239,239,247)` in RGB565, so restoring it makes the seam disappear completely.

Set the splash background back to `0xE9ECF1` and leave it there. It is the one
deliberate exception to the neutral palette; add a comment saying why so it does
not get "corrected" again.

**2. Retry sends a stale target when the error came from the application.**

[ui.c:1293-1294](../../../ui.c#L1293):

```c
cmd->requested_on   = is_retry ? cmd->requested_on   : !d->on;
cmd->requested_mode = is_retry ? cmd->requested_mode : UI_MODE_MANUAL;
```

The Retry affordance is shown for both error sources:
`is_error = (cmd->phase == UI_CMD_PHASE_ERROR) || (cmd->phase == UI_CMD_PHASE_IDLE && d->error)`.
In the second case — the application reported `d->error` and the UI never issued a
command — `cmd->requested_on` and `cmd->requested_mode` have never been assigned.
They hold their zero-initialised values, so:

- Retry sends `target_on = false` regardless of what the user is retrying.
- `cmd->requested_mode` is `0` (`UI_MODE_AUTO`).

Second defect on the same path: [ui.c:1299](../../../ui.c#L1299) sends the literal
`UI_MODE_MANUAL` to the application while storing something else in
`cmd->requested_mode`, and [ui.c:1636](../../../ui.c#L1636) then applies the stored
value on acknowledgement (`d->mode = cmd->requested_mode`). Concrete failure: fresh
boot, application reports a device error, user taps Retry → the UI transmits
`(off, MANUAL)` but on success displays mode `Auto`. The transmitted command and
the displayed state disagree.

Fix both: derive the retry target from the reported state when there is no prior
UI request, and send exactly the mode stored in `cmd->requested_mode` so the
transmitted command and the acknowledged state can never diverge. Add a regression
check that retries an application-reported error and asserts both the transmitted
payload and the resulting displayed mode.

**3. The acknowledgement path writes back into the application snapshot.**

[ui.c:1635-1644](../../../ui.c#L1635) sets `d->on`, `d->mode`, `d->preset`,
`d->name` and `d->valid` on `s_snapshot`. Round 2 introduced `s_device_cmds` to
keep requested/pending/confirmed apart precisely so the UI would stop mutating its
input. Writing confirmed values back re-couples them: if the application pushes a
snapshot that still reports the previous state — normal while the node has not
re-reported yet — the tile flips back, then forward again on the next report.

Keep the acknowledgement in UI state (for example a `confirmed_on` / `confirmed_mode`
overlay with the request id that produced it) and let the rendering prefer it until
a newer application snapshot supersedes it. Do not write to `s_snapshot` outside
`ui_update_snapshot()`. Add a check that pushes a stale snapshot after a successful
acknowledgement and asserts the tile does not flip.

## P1 — performance on the MCU

These are the costs that matter with one 800×10 partial buffer and a QSPI display.
Measure before and after; report numbers, not adjectives.

**4. Every snapshot rewrites every label, whether or not anything changed.**

`update_home_page_widgets()` performs about **22** `lv_label_set_text_static()`
calls per snapshot (8 outside the device loop, 3 × 4 inside it, 2 badge labels).
In LVGL 8.4 that call is unconditional: it always runs `lv_label_refr_text()`,
which re-measures the text and ends in `lv_obj_invalidate(obj)`
(`lv_label.c:311`) even when the pointer and the bytes are identical. Every one of
those becomes a dirty area, a redraw and a flush.

At 1 Hz this is tolerable; at a realistic 5–10 Hz telemetry rate it is 110–220
invalidated regions per second for a screen that mostly did not change. Compare the
freshly formatted string with the current buffer contents and skip both the
`snprintf` and the LVGL call when they are equal. Do the same for the repeated
`lv_obj_set_style_text_color()`, `lv_obj_add_flag()`/`clear_flag()` and
`lv_obj_add_state()`/`clear_state()` calls in the same loops — LVGL already
no-ops some of these, but the text and colour paths do not.

**5. Three to six full-screen layout passes per snapshot.**

`lv_obj_update_layout(obj)` does not measure one object. It resolves the object's
**screen** and runs `layout_update_core(scr)` in a loop until no layout
invalidation remains (`lv_obj_pos.c`). Current call sites:

- [ui.c:341-342](../../../ui.c#L341) — two consecutive calls in `update_header()`
- [ui.c:682](../../../ui.c#L682) — one in `update_home_page_widgets()`
- [ui.c:1234](../../../ui.c#L1234) — one **inside the four-iteration device loop**
  in `update_devices_page_widgets()`, so four passes per snapshot

Because each call follows a label write that invalidated the layout, these passes
genuinely run. Restructure to: write all text first, call `lv_obj_update_layout()`
**once**, then read widths and position. On the Devices page that turns four
full-screen passes into one. In `update_header()` one call is enough for both
measurements.

**6. Full-screen invalidate on every page change.**

[ui.c:1559](../../../ui.c#L1559) calls `lv_obj_invalidate(s_screen)` at the end of
`do_navigate_to_page()`. On 800×480 with an 800×10 buffer that is 48 render-and-flush
cycles for every tab press. The comment attributes it to "P2.23 stale artifacts",
but that artifact was the gradient-cache colour collision, which no longer exists
now that all fills are flat. Deleting and creating objects already invalidates the
affected areas.

Remove it, then prove the result: navigate Home → Trends → Devices → Home and
inspect the framebuffers for stale pixels. If some region really does need help,
invalidate only `s_content_area`, not the whole screen, and say which region and why.

**7. The chart tooltip handler runs twice per touch.**

[ui.c:970-971](../../../ui.c#L970) registers `on_chart_pressed` for both
`LV_EVENT_PRESSED` and `LV_EVENT_VALUE_CHANGED`. `event_send_core()` runs
preprocess callbacks, then the class handler, then the normal callbacks. The chart's
class handler updates `pressed_point_id` and sends a nested `LV_EVENT_VALUE_CHANGED`
from inside the `PRESSED` dispatch, so the tooltip is formatted, written and
invalidated twice for a single press, and again on every point change during a drag.
Register `LV_EVENT_VALUE_CHANGED` only.

## P2 — test-only code shipping to firmware

**8. Move the test API out of the production translation unit.**

`ui.h` exports 27+ accessors that exist purely for the regression suite
(`ui_get_home_*`, `ui_get_devices_*`, `ui_get_trends_*`, `ui_get_dialog_*`,
`ui_set_dialog_*`, `ui_inspect_chart_point`, `ui_format_temp`, `ui_get_snapshot`,
`ui_get_history_sample`, …). `tools/export_mcu.bat` exports `ui.c` and `ui.h`, so
all of it lands in firmware.

Worst offender: [ui.c:1906](../../../ui.c#L1906) `ui_inspect_chart_point()` casts
`lv_obj_t *` to `lv_chart_t *` and writes `chart->pressed_point_id` directly — it
reaches into an LVGL private structure from production code.

Move the whole test surface into a separate `ui_test_api.c` / `ui_test_api.h`
compiled only by `sim_pc`, expose whatever minimal internal hooks it needs, exclude
it from the MCU export manifest, and verify with `tools/export_mcu.bat --verify`
that the exported set no longer contains it.

**9. A whole snapshot copy is held in static RAM for one test accessor.**

[ui.c:1864](../../../ui.c#L1864) `static ui_snapshot_t s_view_snapshot;` (roughly
256 bytes) exists only so `ui_get_snapshot()` can synthesise a view. It moves out
with item 8. Report the static-RAM total before and after.

**10. Stale cached widget pointers after navigation.**

[ui.c:1536-1538](../../../ui.c#L1536) resets only three of about forty cached
pointers (`s_home_co2_val`, `s_trends_cur_lbl`, `s_dev_page_names[0]`) after
`lv_obj_del(s_page_container)`. The current flow is safe only because the update
functions happen to check those three sentinels first. Any new code path that
touches `s_trends_chart`, `s_home_dev_switches[i]` or similar without going through
a guard uses freed memory.

Clear the whole set — group the per-page pointers in a struct and `memset` it, or
NULL every one explicitly — so safety does not depend on call order.

**11. Remove the scratch artifacts committed to the working tree.**

`.temp_tiles_lvgl.png`, `.temp_tiles_proto.png` and the `scratch/` directory
(13 comparison crops) are review by-products. Delete them or move them under
`docs/reviews/`, and add an ignore rule so the next round does not leave more.

## P3 — remaining design deviations

**12. Trend summary values are 24 px; the contract asks for 32 px.**
[ui.c:901/906/911](../../../ui.c#L901) use `ui_font_24`. Round 2 fixed the spacing
but not the size. The font set is now 18/24/34 plus the 62 px digits, so the nearest
size without shrinking is **34**. Either use it or state why 24 is the better call
for this layout — do not leave it silently off-spec.

**13. `Sending…` is red on Home and amber on Devices.**
[ui.c:775](../../../ui.c#L775) uses `UI_COLOR_RED`, [ui.c:1259](../../../ui.c#L1259)
uses `UI_COLOR_AMBER`. A command in flight is not a fault. Use amber in both, and
keep red for `Unknown`.

**14. Unit spacing is inconsistent between Current and Min/Max.**
Current formats `"%d ppm"`, `"%s °C"`, `"%d %%"`; Min/Max format `"%d ppm"`,
`"%s°C"`, `"%d%%"`. Pick one convention and route all of them through a single
formatting helper per metric.

**15. The header cluster is not re-aligned when the uptime string widens.**
`update_uptime_string()` only sets the text; the right-alignment maths lives in
`update_header()`. Past 100 hours the string gains a digit. The 14 px gap absorbs
one extra digit, so the collision with the separator starts around 1000 hours —
latent, but the contract explicitly requires the counter not to wrap or break at
hour boundaries. Recompute the alignment when the rendered width changes, and add a
check at 99:59:59 → 100:00:00 and at 999:59:59 → 1000:00:00.

**16. CHECK 7 does not exercise the real touch path.**
It calls `ui_inspect_chart_point()`, which writes `pressed_point_id` directly, so
the event wiring fixed in round 2 is still unproven end to end. Drive the chart with
a synthetic pointer press through the input device (the simulator already registers
one) and assert the tooltip text, then keep the direct helper only as a convenience.

## Verification required

Run and paste real output for:

```powershell
tools/build_sim.bat
tools/run_regression.bat
tools/export_mcu.bat --verify
sim_pc\build\sim_pc.exe --shots <dir>
```

Then:

1. **Splash:** sample the framebuffer inside and outside the logo rectangle and show
   both are `(239,239,247)`.
2. **Performance, measured not asserted:** instrument the simulator flush callback to
   count flush calls and total pixels pushed, and report, before and after your
   changes: (a) ten consecutive identical snapshots on Home, (b) ten snapshots where
   only the CO₂ value changes, (c) one Home → Trends → Devices → Home navigation
   cycle. The identical-snapshot case should approach zero flushed pixels.
3. **Heap and static RAM:** peak, largest free block and fragmentation per screen,
   plus the static footprint before and after moving the test API out.
4. **State machine:** show the new checks for the application-reported-error retry
   (transmitted payload *and* displayed mode) and for a stale snapshot arriving after
   a successful acknowledgement.
5. **Screenshots:** regenerate all twelve, put each next to its
   `docs/prototype/*.png` counterpart, and confirm nothing regressed visually.
6. Update `PLAN.md`, `DEV_LOG.md` and `docs/MCU_BASELINE.md`. List explicitly every
   item you did not complete and every remaining deviation. Do not report a check as
   passed unless you ran it.
