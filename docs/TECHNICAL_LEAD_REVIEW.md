# Technical-lead review · Smart Hub Quiet Architecture

> Latest owner-reported live-path investigation:
> [Trends/Auto](reviews/2026-09-23-trends-auto-investigation/REVIEW.md).
> Current is live while history is never fed by production UI; Manual→Auto
> restarts a10s hold and needs three fresh reports. One CURRENT covers these
> paths plus previously open RAM/fault evidence. Earlier banners below are
> historical; preserve accepted visual design.

> Latest independent disposition: [G1–G4 review](reviews/2026-09-21-g1-g4-independent/REVIEW.md).
> Functional repairs accepted; no new reproduced production UI bug. One CURRENT
> completes E1 RAM evidence/report corrections and E2 fault-matrix assertions.
> Default/40KiB34/34, isolated production and full assert-enabled smoke PASS.
> Keep accepted appearance and42KiB/10lines; target memory sign-off remains open.
> Earlier status banners below are historical, not additional active scope.

> Latest independent disposition:
> [const-style review](reviews/2026-09-21-const-style-review/REVIEW.md).
> Missing-data copy and old uptime fix accepted. Remaining G1–G4 cover new link
> clipping, constant-style config portability, RAM placement/candidate proof and
> durable tests. Default and reviewer40KiB suites both34/34 PASS; MCU sign-off pending.

> Latest independent disposition,2026-09-21:
> [implementation follow-up](reviews/2026-09-21-runtime-followup/REVIEW.md).
> R1–R7 repairs verified; R8 partial. ONE CURRENT covers remaining header overlap,
> owner-approved unavailable copy simplification, regression gaps and RAM evidence.
> All34 existing checks and isolated production verification pass; target sign-off
> remains pending real MCU artifacts. Earlier review criteria/status below are historical.

> Owner update,2026-09-21: current native appearance is accepted; older fidelity
> criteria below are historical where they conflict. The latest disposition is
> [runtime/RAM review](reviews/2026-09-21-runtime-ram-review/REVIEW.md), with one
> [active handoff](prompts/CURRENT.md). Freeze accepted pixels while correcting
> demonstrated runtime states and optimizing RAM/redraw.42KiB/10lines remain the
> reference profile; smaller candidates require evidence, not weakened tests.

The owner assigns implementation to a separate coding AI and independent review
to this assistant. Use [current implementation/fix prompt](prompts/CURRENT.md)
for the implementation handoff. The current simulator implementation exists;
the latest independent disposition is in
[the reconstruction review](reviews/2026-09-18-reconstruction-review/REVIEW.md).
MCU include/global ownership and core Auto behavior remain accepted, as do
restored major proportions and specific fixes recorded there. Remaining Q1–Q5
cover reproduced boundary/recovery overlap, history coverage, already-requested
reference fidelity, verification assertions and inaccurate reporting.
This is completion of the approved design, not a new design direction.
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

1. **Match the latest design.** The seven Quiet Architecture concepts under
   `drafts/2026-09-17-pro-max/` and CURRENT govern visual acceptance; Study12's
   bottom icon navigation, separate metric cards and permanently flat palette
   are historical, not current constraints. Preserve the two-row shell,
   asymmetric Overview, focused editors and minimum18px text; restore the
   reference hierarchy, coherent icon roles and subordinate units. Restrained
   tonal depth must respect measured MCU cost, not use a full-screen bitmap.
   Test actual glyph/control lanes in boundary/recovery states, not only
   nominal captures. No Details, bell/notification panel, visible VOC Index
   suffix or redundant normal On/Off captions. Uptime and Connected/Disconnected
   remain. Native captures are evidence, not replacement approved concepts.
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
