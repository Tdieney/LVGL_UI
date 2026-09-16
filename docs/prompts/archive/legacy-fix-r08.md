# Fix prompt · Smart Hub UI round 8 — link state semantics

> ARCHIVED — historical context only, not the current implementation request.
> Original location: `docs/reviews/implementation-r8/FIX_PROMPT.md`.
> Use [CURRENT.md](../CURRENT.md) for the active handoff and
> [prompt rules](../README.md) before creating or updating any prompt.

Round 7 is verified good: `lora_comm.h` untouched at 20 bytes, the prototype
updated first with matching bar geometry (4/7/10/14 px on both sides), the
spreading factor as a named constant with a demodulator lookup table, integer
tenths-of-a-dB arithmetic, correct asymmetric Schmitt-trigger hysteresis, the
header right-alignment constant properly updated from 20 to 18, and CHECK 17
covering boundaries, hysteresis, the SF shift and the palette fills. 17/17 checks
pass, performance is unchanged, and the rendered header matches the prototype.

One defect remains, and it originates in the round-7 prompt rather than in the
implementation: the threshold table conflated "zero bars" with "disconnected".
The code followed that instruction faithfully. The instruction was wrong.

Copy everything below the line to the coding AI.

---

You are making a small correction to the LVGL v8.4 Smart Hub UI in this repository
(`C:\Users\vanth\Documents\MyDocument\Embedded\LVGL_UI`, branch `ui/smart-hub`).
Read `lora_hub_link.h` and `docs/TECHNICAL_LEAD_REVIEW.md` first. Do not reset,
clean, stash, push or change branches.

This is a targeted change. Keep everything else from round 7 exactly as it is: the
prototype's signal-bar design, `LORA_SPREADING_FACTOR` and the demodulator table,
the tenths-of-a-dB integer arithmetic, the hysteresis for the 1↔2, 2↔3 and 3↔4
boundaries, the four header rectangles, and the CHECK 17 cases that already pass.
**Do not touch `lora_comm.h`.**

## 1. "Disconnected" must mean no frames, not weak frames

[ui.c:499-501](../../../ui.c#L499):

```c
target_bars = lora_snr_to_bars(lora_last_snr, s_current_link_bars);
if (target_bars == 0) {
    /* Demodulator limit reached or below threshold */
    link_changed = update_label_if_changed(s_link_label, "Disconnected");
```

This branch runs **inside** `if (is_link_connected(now))` — status frames are
arriving normally. Reporting `Disconnected` there contradicts the rest of the UI in
the same frame: `is_link_connected()` is true, so the metric cards show live values
and `toggle_device()` will happily transmit relay commands, while the header claims
there is no link.

It is also physically reachable rather than a theoretical case. The demodulator
limits are typical figures, not hard walls, and the SX127x SNR reading carries its
own error, so a genuinely working link can report a slightly negative margin.

Split the two meanings:

- **`Disconnected`** — no valid status frame within `LORA_LINK_TIMEOUT_MS`. This is
  the existing staleness test at [ui.c:75](../../../ui.c#L75) and it is the *only*
  thing that should produce that word. Bars all inactive.
- **`Connected`** — a frame arrived inside the bound. Always show at least **one**
  bar, even when the SNR margin is negative. The word states that the link is
  reachable; the bars state how healthy it is.

Implement the floor at the call site in the header rather than inside
`lora_margin_to_bars()`. Keep that function pure and unchanged — it is already
tested, and a diagnostics view may legitimately want to distinguish "below the
demodulator limit" from "one bar". Document at its `return 0`
([lora_hub_link.h:98](../../../lora_hub_link.h#L98)) that the header clamps this to
one bar while frames are arriving, so the next reader does not think the clamp is a
bug.

## 2. This also removes an unprotected flicker path

The hysteresis added in round 7 guards 1↔2, 2↔3 and 3↔4, but the 0↔1 boundary is a
bare comparison. That boundary is the expensive one: crossing it changes the label
**text**, which changes `link_w`, which moves all five positions in the
right-aligned cluster and triggers a full-screen `lv_obj_update_layout()`. An SNR
oscillating around the demodulator limit would therefore relayout the header
repeatedly — exactly what the hysteresis exists to prevent, at the worst possible
place.

Fixing item 1 removes the path entirely: with the floor in place, zero bars can only
come from staleness, and staleness already switches on a 5 s timer rather than on a
noisy analogue value, so it cannot chatter.

Do not add hysteresis to the 0↔1 boundary as a separate mechanism. If your fix for
item 1 somehow leaves that boundary reachable from a fresh link, say so and explain
why, rather than layering a second guard on top.

## 3. Rename the misleading scenario capture

[sim_pc/main.c:2121](../../../sim_pc/main.c#L2121) writes `home_link_degraded.raw`,
but the capture is taken on the **Devices** page, not Home. Either move the capture
to Home so the name is accurate, or rename it to match the page it actually shows.
Screenshot filenames are the evidence index for review; a wrong one wastes a
reviewer's time.

## 4. Verification required

Run and paste real output for:

```powershell
tools/build_sim.bat
tools/run_regression.bat
tools/export_mcu.bat --verify
sim_pc\build\sim_pc.exe --shots <dir>
```

Then:

1. **Semantics:** a check that holds the link fresh while driving the SNR far below
   the demodulator limit, and asserts the header reads `Connected` with exactly one
   bar — never `Disconnected`. Add the mirror case: let the frames stop and assert
   `Disconnected` with zero bars.
2. **No contradiction:** in that same sub-limit state, assert that a relay toggle is
   still accepted, since `is_link_connected()` is true. That is the inconsistency
   this round removes, so prove it is gone.
3. **Flicker:** oscillate the SNR across the demodulator limit repeatedly and show
   the label text never changes and the header cluster positions never move.
4. **Existing coverage intact:** CHECK 17's boundary, hysteresis, SF-shift and
   palette assertions still pass unchanged.
5. **Performance unchanged:** 0 flushes for ten identical frames, 0 for idle
   sub-second, 1 for idle one second.
6. **Screenshots:** regenerate all scenarios, confirm the renamed capture matches its
   page, and confirm the header renders correctly in the full, degraded and
   disconnected states.
7. Update `DEV_LOG.md` and, if you changed any stated behaviour, the relevant line in
   `docs/UI_DESIGN_BRIEF.md`. State explicitly anything you did not complete. Do not
   report a check as passed unless you ran it.
