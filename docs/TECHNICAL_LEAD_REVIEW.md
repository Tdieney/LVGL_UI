# Technical-lead review · Smart Hub study 12

The owner assigns implementation to a separate coding AI and independent review
to this assistant. Use [current implementation/fix prompt](prompts/CURRENT.md)
for the implementation handoff. The current simulator implementation exists;
the latest independent disposition is in
[the W1–W4 result review](reviews/2026-09-16-auto-w1-w4-independent/REVIEW.md),
with X1–X4 requiring correction; Retry/heap improvements and visual direction retained.
Older review-round notes are historical context;
the current prompt governs changed requirements and acceptance tests.

## Automatic corrective handoff (Owner, 2026-09-15)

Every user-facing review with remaining defects or issues must also deliver an
updated [CURRENT.md](prompts/CURRENT.md), without waiting for another request.
Read [prompt lifecycle rules](prompts/README.md), archive a superseded handoff,
and link the active prompt in the same final review response. Include unresolved
evidence/acceptance gaps, not only visible code bugs. If an essential owner decision
is missing, label the prompt DRAFT with that blocker; do not invent approval.
This rule authorizes documentation/handoff preparation, not implementing fixes,
committing or pushing during review. Do not create a fix prompt with no actionable
items when the review has passed.

## Evidence required from the coding AI

- Local diff or commit reference, changed files and the UI input/output contract.
- Exact build/test/export commands actually run and their results.
- Native 800×480 screenshots from **LVGL**, not the browser reference.
- Peak/free heap, largest free block, fragmentation, static buffers and image/font
  size, with the scenario and repeat-navigation conditions used for measurement.
- Remaining assumptions, visual deviations, failed checks and real blockers.

## Independent review priorities

1. **Match the latest design.** Study 12: separate CO₂/VOC cards, no Room Air overview,
   no visible Index; equal 62 px main readings, stacked comfort cards, balanced
   grid and flat tonal surfaces. Every surface fill is one flat, RGB565-exact
   color: reject any `bg_grad_dir`/`bg_grad_color` and any fill outside the
   study 12 table. Metric colors must not imply quality. Keep
   readable text, zero borders, exactly three tabs, uptime and Connected/Disconnected. No Details,
   bell, notification panel or redundant normal On/Off captions. Splash-only brand.
   Bottom navigation has only 32 px icons, no visible captions; preserve accessible
   names, equal-width 48 px-high targets and white-on-blue selected state.
   Cards alone receive the approved faint soft shadow (CSS reference: 0/2/10/0,
   #182B4D at 6%); verify the LVGL rendering, clipping and redraw cost. Bars stay
   shadow-free; no new borders or heavy halos. Confirm each declared fill equals
   what the RGB565 framebuffer contains (sample the shot, do not trust the CSS),
   that no two surfaces share a fill they should not, and check 98 px comfort-card
   clipping with missing data; no overlap into the device row.
2. **Correct state handling.** Verify the `lora_comm.h` wire globals contract and
   the desired versus reported state model: settled when equal, `Sending…` while
   younger than 3000 ms, and `Unknown` + `Retry` past timeout. Verify Hub-local
   device configuration persistence without remote reversion, Hub polarity resolution
   (`active_low`), link staleness detection (5000 ms bound), offline/partial sensor
   data, chart gaps/extrema, and uptime rollover. No invented sensor conversions,
   automatic control rules or fake hardware confirmations.
3. **Safe LVGL lifetimes.** Check labels and their backing buffers, deleted-object
   callbacks, timers, thread ownership and bounded updates across navigation/dialogs.
4. **Measured MCU cost.** Keep the 42 KiB LVGL heap, one 800×10 draw buffer and no
   MCU framebuffer. No per-tick allocation. Measure transition/splash/dialog/chart
   peaks and repeated-navigation memory stability; account for static data too.
5. **Reproducible delivery.** Independently run build, meaningful regression and
   MCU export checks; verify assets/fonts and all production dependencies export
   without Windows, simulator fixtures or browser files.

Report concrete findings by severity, with file/line, reproduction, impact and
required correction. Distinguish accepted implementation, changes required and
insufficient evidence. Do not call simulator-only results hardware validation.
Physical-LCD readability remains an on-target check. Do not silently push, merge
or rewrite the implementation as part of an ordinary review.
