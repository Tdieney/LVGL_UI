# Fix prompt · Smart Hub UI round 9 — rebuild-time state, and closing the test gap

> ARCHIVED — historical context only, not the current implementation request.
> Original location: `docs/reviews/implementation-r9/FIX_PROMPT.md`.
> Use [CURRENT.md](../CURRENT.md) for the active handoff and
> [prompt rules](../README.md) before creating or updating any prompt.

Round 8 is verified complete: the floor is applied at the call site, `Disconnected`
now means staleness only, `lora_margin_to_bars()` stayed pure with an explanatory
comment, no redundant hysteresis was layered on the 0↔1 boundary, and the degraded
scenario really navigates to Home before capturing. CHECK 17 gained both requested
assertions. 17/17 pass, export verifies, performance unchanged.

Regenerating the screenshots then exposed a defect that has been latent since round
3. It is small to fix. The important half of this round is the **test gap** that let
it survive four review rounds: nothing in the suite checks that a page rebuilt from
unchanged data looks the same as it did before.

Copy everything below the line to the coding AI.

---

You are fixing one defect and closing a test gap in the LVGL v8.4 Smart Hub UI
(`C:\Users\vanth\Documents\MyDocument\Embedded\LVGL_UI`, branch `ui/smart-hub`).
Read `docs/TECHNICAL_LEAD_REVIEW.md` first. Do not reset, clean, stash, push or
change branches.

Keep everything from rounds 5–8 intact: the `lora_comm.h` contract, the wire-globals
model, Hub-local configuration, desired-versus-reported relay logic, the dirty-check
update path and its measured numbers, the signal-bar work, the study 12 palette, and
CHECK 16 / CHECK 17.

## 1. The defect: the `ppm` unit label is left at its build-time position

[ui.c:874-878](../../../ui.c#L874):

```c
if (set_buffer_and_label_if_changed(s_home_co2_val, s_co2_val_buf, sizeof(s_co2_val_buf), co2_val_tmp)) {
    lv_obj_update_layout(s_home_co2_val);
    lv_coord_t val_w = lv_obj_get_width(s_home_co2_val);
    set_pos_if_changed(s_home_co2_ppm_lbl, val_w + 8, 74);
}
```

The unit label is created at a fixed `x = 140` ([ui.c:699](../../../ui.c#L699)) and
only moved to `val_w + 8` when the **value string changes**. The Home page is
destroyed and rebuilt on every navigation, but `s_co2_val_buf` is process-lifetime
state, so returning to Home while CO₂ is steady leaves the comparison equal, skips
the branch, and strands `ppm` at 140.

Measured on the rendered framebuffers, CO₂ reading `420` in both cases:

| Scenario | value glyph columns | `ppm` columns | gap |
|---|---|---|---|
| `home_good` (fresh value) | 38..101, 108..137 | 149..184 | 12 px |
| `home_link_degraded` (rebuilt, unchanged value) | 38..101, 108..137 | **177..212** | **40 px** |
| `docs/prototype/home.png` (`780`) | 40..133 | 143..178 | 10 px |

Real trigger: view Trends, come back to Home while the reading is stable.

Fix it so the unit position is a function of the value width whenever the widgets
exist, not only when the string changes. Position it during the page build from the
measured width as well as on every value change — do not simply move the whole
update out of the dirty-check branch, because that would reintroduce the per-frame
`lv_obj_update_layout()` cost that round 3 removed. The zero-flush idle result must
survive.

I audited the rest of the file: the only other measured-width positioning is the
header's right-aligned cluster at [ui.c:534-537](../../../ui.c#L534), and that one
is safe because the header lives on the permanent shell and is never rebuilt. Leave
it alone.

## 2. Audit the same hazard class

This bug is a symptom of the dirty-check design, not a one-off. Every
`if (something_changed) { ... }` block in `update_home_page_widgets()`,
`update_trends_page_widgets()` and `update_devices_page_widgets()` is suspect if the
work inside it must **also** be true immediately after a rebuild, because a rebuilt
page starts with stale comparison buffers that suppress the branch.

Walk all three functions and list what you checked and what you found. State the
result even if nothing else is wrong — a clean audit is a useful record.

## 3. Test: rebuild idempotence (the one that would have caught this)

Add a regression check that proves a page rebuilt from unchanged data is
**pixel-identical** to the same page before the rebuild. This is the general form of
the defect and it is cheap.

For each of Home, Trends and Devices:

1. Freeze the clock with `ui_set_uptime_seconds()` so the uptime label cannot differ
   between captures, and make sure no toast is visible and no device command is in
   flight (`ui_test_reset_device_commands()`).
2. Navigate to the page, settle, and copy `s_framebuffer` into buffer A.
3. Navigate to a different page and back, without changing the node status, the
   configuration or anything else.
4. Settle and copy `s_framebuffer` into buffer B.
5. Assert `memcmp(A, B, ...) == 0`.

On failure, print the coordinates and both RGB565 values of the first differing
pixel, and how many pixels differ in total. A check that only says "frames differ"
is not diagnosable.

Do the same for the settings dialog: open it, capture, close and reopen it, capture,
compare.

The buffers are test-only and live in `sim_pc`; do not add anything to `ui.c` for
them.

## 4. Test: value round-trip idempotence

A related asymmetry the suite would also miss: an update path that renders correctly
when a value moves one way but not the other.

Render Home with CO₂ = 420 and capture. Change it to 1420, settle. Change it back to
420, settle, and capture again. The two 420 frames must be identical. Repeat for at
least one metric whose formatting changes width — a negative temperature such as
−2.5 °C against 23.5 °C is a good second case, and four-digit versus three-digit CO₂
is the case that exposed this bug.

## 5. Test: pin the value-to-unit relationship in CHECK 16

CHECK 16 asserts anchor coordinates but not the relationship this bug broke. Add the
CO₂ value's right edge and the `ppm` label's left edge to it, asserting the gap
against the prototype's 10 px with a small tolerance. Do the same for the humidity
and temperature units if they are positioned the same way.

## 6. Verification required

Run and paste real output for:

```powershell
tools/build_sim.bat
tools/run_regression.bat
tools/export_mcu.bat --verify
sim_pc\build\sim_pc.exe --shots <dir>
```

Then:

1. **The new tests fail before the fix.** Apply the tests first, show them failing on
   the current code with the diagnostic output, then apply the fix and show them
   passing. A test that has never failed has not been shown to test anything.
2. **The measured gap:** print the CO₂ value and `ppm` column spans for a freshly
   rendered Home and for a rebuilt Home, and show they now match.
3. **Screenshots:** regenerate all thirteen and confirm `home_link_degraded.png` now
   shows the same value/unit spacing as `home_good.png`.
4. **Performance unchanged:** 0 flushes for ten identical frames, 0 for idle
   sub-second, 1 for idle one second. If your fix touches the update path, prove the
   idle cost did not move.
5. **Audit result** from item 2, written out.
6. Heap peak, largest free block, fragmentation, and the drift against its threshold.
7. Update `DEV_LOG.md`. State explicitly anything you did not complete. Do not report
   a check as passed unless you ran it.
