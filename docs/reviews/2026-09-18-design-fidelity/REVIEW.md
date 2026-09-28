# Review — Quiet Architecture design fidelity

Date: 2026-09-18. Verdict: **CHANGES REQUIRED — visual implementation not accepted**.

Review-only delivery requested by the owner. No UI, engine, asset, test or tool
source was modified in this review. The earlier implementation's functional
results are retained, but its claim of visual completion is withdrawn.

- [Single consolidated implementation/fix handoff](../../prompts/CURRENT.md)
- [Side-by-side comparison](COMPARE.html) — approved concepts versus actual renders.
- [Fresh regression output](REGRESSION.txt)
- [Commands and source identity](VERIFICATION.md)

## What was compared

Seven accepted Quiet Architecture concepts under
[the design folder](../../drafts/2026-09-17-pro-max/), the submitted native UI,
its constructors/styles and affected simulator tests. The local pre-implementation
snapshot distinguishes the redesign from the older dirty worktree. Also reviewed:
Auto draft/save/cancel, bounds, invalid data, pending/error/retry presentation,
existing test coverage and the outstanding S1–S3 verifier handoff.

Concept images are visual references, not executable layouts. Compare at the
same 800x480 scale, allowing the owner's 18px text floor and necessary truthful
error states. Different demo values, startup Manual mode, removal of filler
sentences and the lack of photorealistic raster texture are NOT defects.

## Findings

### F1 — P1: shared hierarchy and Overview have been flattened

Evidence: ui.c:544, :669, :742; ui_theme.c; [actual Overview](home_good.png).

Studio is 24px, the hero verdict and readings only 34px, and icons mostly 24px.
The concept gives the verdict a dominant display role, subordinate metric labels,
large readings with separate smaller units, and substantially larger appliance
icons. The code loses the check/air symbols, metric divider, device-row dividers
and the device-panel shortcut. Modes start under the icon rather than under the
device name, and switches sit beside the second line instead of centering on
the row. Temperature/humidity numbers no longer align with their text column.

This is not fixed by recoloring cards. Restore hierarchy and column alignment,
compact capsule navigation, coherent icon sizes and restrained surface detail.
Minimum18px is a floor, not a reason to render nearly everything at18/24/34px.

### F2 — P1: Trends is a different component composition

Evidence: ui.c:1028; [actual Trends](trends_data.png).

The left metric cards lost their icons and live values; they became plain
name-only buttons. The chart lost its metric heading, separating rules and
prominent Current/Min/Max figures (now24px). Every sample has an equally strong
dot, unlike the understated line/area with emphasized endpoint in the reference.
Restore this composition, without manufacturing a fixed15-minute history or
changing real samples to match the illustrative chart.

### F3 — P1: Devices retains old small-icon/inset-button styling

Evidence: ui.c:1323; [actual Devices](devices_grid.png).

24px icons on small colored plates replace the large outline appliances.
Mode is detached from the name column; Auto has no adjacent sensor association.
Switches are displaced downward. Settings is an inset gray button rather than
a separated full-width row with sliders icon and chevron. These changes alter
both visual balance and the intended grouping.

Restore the card anatomy, keeping real readback, Manual override, Unknown,
Sending, Auto paused and independently tappable Retry behavior.

### F4 — P1: Settings changes the approved control vocabulary

Evidence: ui.c:2016; [actual Settings](devices_settings.png).

The generic heading loses device context. Icons and row separators are absent.
The dropdown is visually almost plain text, with an18px value and tiny chevron.
Mode is two detached buttons rather than one segmented control. Auto thresholds
becomes a full-width filled bar, breaking the form's label/value columns.
Output polarity becomes a relay-like switch instead of explicit Active high /
Active low choices. These controls work but do not match the approved design;
the polarity switch also weakens the distinction between wiring configuration
and operating an appliance.

Restore the four aligned rows, explicit polarity choices, contextual heading
and clear dropdown boundary. Keep concise copy and draft-only editing.

### F5 — P1: Auto editor omits major structure; P2 validation shifts content

Evidence: ui.c:2145, :2255, :1859;
[actual Auto](devices_auto_thresholds.png),
[invalid draft](devices_auto_invalid_draft.png).

The celadon sensor/current-reading strip was replaced by two plain text labels.
There are no outlined central value fields; units have the same emphasis as
numbers; stepper glyphs are small and the buttons use the old filled style.
Footer widths are192/200/304px instead of a balanced three-column row;
Reset uses smaller text than its neighbors. The layout therefore loses the
reference's spacing rhythm despite having equally sized threshold cards.

Invalid validation switches whole-card border width from0 to2. Captures show
the children shift2px; the reference marks the value fields without moving
content. It also supplies an actionable repair sentence and warning icon,
where current UI only gives the cause. Reserve a stable helper/error area and
constant field bounds. Current invalid capture is CO2; the accepted invalid
reference is VOC150/150, so matching-state visual evidence is still missing.

### F6 — P1: documentation and tests wrongly promoted output into authority

Evidence: former UI_DESIGN_BRIEF opening; sim_pc/main.c:2100 and :4719;
[previous author report](../2026-09-17-quiet-implementation/REPORT.md).

The previous handoff declared native captures the visual reference, superseding
the accepted raster geometry. That was not an owner acceptance of these deviations.
CHECK16 tests a few current object bounds/palette pixels and prints “against
prototype”; CHECK32 checks font eligibility and behavior, not the intended
relative hierarchy. Neither proves design fidelity.

This review restores the seven accepted concepts as the visual source, corrects
the indexes, and requires matched-state side-by-side evidence plus purposeful
geometry/typography assertions. Do not accept revised expectations merely
because they match whatever the new code happens to render.

### V1 — P2: outstanding verification gaps remain open, not new runtime defects

The superseded S1–S3 handoff was not completed by the visual implementation:

- tools/verify_production.ps1:277 still compares ZIP entry lengths rather than
  source content; :367 tests a timeout exception, not actual process-tree reaping.
  Concurrent helper cleanup still ignores/surfaces WaitForExit return values.
- CHECK1 lacks the combined real-clock rollover4999/5000ms UI/TX path.
- CHECK28 needs a genuine pending/timeout/latch precondition in its second
  recovery branch, not just the existing geometry/pointer matrix.
- CHECK31's successful-transition counter can increment merely for IDLE
  (sim_pc/main.c:4634), not confirmed new command/TX/readback. Real stress occurs,
  but its displayed “Commands Sent” count is not independent transmission proof.

These bounded items are retained in the SAME CURRENT, not sent as another phase.
Do not add firmware CRC, flash persistence or change Auto to make tests pass.

## Verified now and limits

- tools/build_sim.bat: exit0, cached dependency, Ninja no work to do.
  Existing CMP0177 CMake developer warning is not a UI finding.
- sim_pc/build/sim_pc.exe --regression: exit0, **32/32**.
- sim_pc/build/sim_pc.exe --shots docs/reviews/2026-09-18-design-fidelity:
  generated24 scenarios; PNGs converted with the existing raw2png tool.
- tools/export_mcu.bat --verify: exit0, manifest PASS.
- Settled stress minimum free9032 bytes; largest block7576; drift0.
  Identical10 frames:0 flushes. Changed CO2:9 flushes/55536 pixels.
- The complete wrapper and isolated production verification were NOT rerun in
  this review; their previous results remain historical, not new verification.
- Actual MCU build, physical LCD colors/legibility, QSPI timing and touch remain
  untested. No claim of zero runtime bugs or pixel-perfect raster replication.

The skill ui-ux-pro-max informed hierarchy, coherent icons, stable feedback and
touch review; its typography-scale search matched this concern. Generic phone
breakpoints, OS accessibility integrations and new themes are out of scope.

## Delivery

One READY handoff covers F1–F6 and V1, retaining accepted runtime behavior.
No production fixes, commits, pushes or branch operations were performed.
