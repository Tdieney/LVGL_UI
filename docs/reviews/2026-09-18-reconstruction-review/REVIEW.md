# Independent reconstruction review — 2026-09-18

Verdict: **CHANGES REQUIRED**. This is materially closer to Quiet Architecture,
but the submission's “all resolved / zero clipping / all verification closed”
claim is not supported. Keep accepted work; implement ONE remaining handoff:
[CURRENT](../../prompts/CURRENT.md).

## Scope and independently executed checks

Reviewed the submitted working-tree changes across UI/theme/assets, Auto and
wire integration, simulator/test assertions, export/verification tools and
documentation. Compared all seven approved concepts, inspected icon contact
sheet and native normal/fault/editor renders. The old dirty diff includes earlier
accepted integration work; it must not be mistaken for all-new work.

- Cached build: PASS (only upstream CMake CMP0177 development warning).
- Full regression wrapper: PASS, 32/32, smoke and 27 scenario captures.
- Export manifest: PASS.
- Full production verification: PASS outside sandbox, including isolated C99
  build without test hooks and deliberate failure exit 1/expected stderr.
  The first sandbox run failed to terminate an owned watchdog worker; see
  [sandbox log](PRODUCTION_SANDBOX.txt). Elevated rerun passes; **not classified
  as a reproduced exporter runtime defect**. Successful verifier cleaned its own
  checked temporary staging; failed-run evidence was retained.
- Independently regenerated all 27 standard RAW captures: SHA256 identical to
  submitted captures. [Source identity](SOURCE_IDENTITY.md).
- Added review-only [probe.c](probe.c), compiled against unchanged production
  objects, and captured 13 additional native boundary/recovery states.
  [Probe log](PROBE.txt), [regression log](REGRESSION.txt),
  [production log](PRODUCTION.txt), [visual evidence](COMPARE.html).
- CHECK31: 6,016 bytes minimum settled free, 10,232 minimum immediate observed
  (at Start, not “transient dialog rendering”), +8 byte cycle10→50 drift.
  CHECK8 observes a lower Home value: 5,504 bytes. These are sampled host
  measurements, not proof of absolute peak or absence of all leaks.
- No production code/assets/test suite/tool implementation changed by reviewer.
  No commit/push. Physical LCD/touch/QSPI/RF/MCU linker map not tested.

## Accepted improvements — do not redo these

- Separate header/tab rows; 416/320 Overview split; 368×164 Devices cards;
  184/552 Trends proportions; horizontal environment tiles.
- CO2 1420 unit collision fixed; Home 10000 and signed temperature extremes
  readable in added probe. Auto live strip now fits 10000 ppm.
- Auto paused fits normal fault/offline mode lanes. Header fits actual
  1000-hour uptime + Disconnected in added probe.
- Invalid VOC draft has stable field outlines, two-line repair text and disabled
  Save; Back preserves invalid draft with visible parent warning.
- Cylindrical purifier, four-blade fan, clean drop and connected lamp are genuine
  vector improvements. No need to throw away the whole family.
- Exact ZIP membership/duplicate checks and negative controls improved.
  CHECK1 now crosses the real LVGL clock rollover. CHECK28 second setup asserts
  PENDING and latch. Preserve these and the accepted Auto/wire behavior.

## Q1 — P2: overlapping readings and controls remain in real boundary flows

1. **Trends 10000 ppm:** Current and Min strings run into adjacent columns;
   the axis unit crowds the five-digit ticks. All three stat labels use font34
   with embedded units and only 170/180px between starts.
   [Native reproduction](probe_trends_10000.png).
   Source: ui.c:1272, 1284, 1296, 1349–1370.
   Signed wire-end temperature likewise crowds plot/axis lanes:
   [negative temperature](probe_trends_negative.png).
   This numeric-end case tests rendering robustness, not a claimed sensor range.
2. **Recovered Auto error:** actual Auto command timeout sets phase=ERROR,
   latch=1; late readback settles phase=IDLE while latch stays1. Showing both
   switch and Retry moves switch over device name on **both** Overview and
   Devices. [Overview](probe_home_latched_recovery.png),
   [Devices](probe_devices_latched_recovery.png).
   Source: ui.c:1125 and update_devices_page_widgets is_latched branch.
   Preserve explicit re-arm and separate hit targets; fix the visual lanes.
3. **Settings large thresholds:** valid Fan ON10000/OFF9950 summary reaches
   under the right chevron. [Capture](probe_settings_max_summary.png).
   Source: ui.c:2548.
4. **Rejected Settings Save:** while real command phase=PENDING, try polarity
   change and Save. Guard correctly refuses commit, but its error text is
   drawn over Output polarity label/control row.
   [Capture](probe_settings_pending_rejection.png). Source: ui.c:2394.
   Fix error placement without weakening pending-command protection.

All these pictures return PASS from the submitted “glyph containment” audit.

## Q2 — P2: Trends coverage is still invented for partial/gapped history

ui.c:1493 derives duration from (valid_cnt-1)*5, floors singleton at5 and caps15.
But ui_feed_history timestamps samples one minute apart. Two samples at23:59
and00:00 display Last5min; axis extends to00:14.
[Native two-sample capture](probe_trends_two_samples.png).
Counting valid points also misrepresents windows with gaps.

Use stored sample timestamps/count and explicit plotted-domain semantics.
A 16-slot fixed axis may remain if clearly represented as a window, but it must
not present future empty slots as recorded coverage. One point is not5 minutes.
Keep missing samples missing; preserve midnight behavior and point inspection.

## Q3 — P2: explicit fidelity work remains, not a new redesign request

The restored composition is accepted, but several already-specified features
were omitted. Compare [seven reference pairs](COMPARE.html).

- Studio still24 and tabs18 vs handed-off approximately34/24 hierarchy.
  Devices panel title24 and Devices-card Settings-row captions18 remain visually weak; do not
  use18 everywhere merely because18 is the minimum.
- Dominant Good verdict lacks reference status/check badge; code creates hidden
  CO2/VOC quality labels solely for tests. Replace tests with visible semantics.
  “Indoor air parameters optimal” is broader than CO2/VOC data, particularly
  when temperature/humidity invalid. Prefer neutral scope-specific copy.
- Unit hierarchy is lost across Trends and Auto: numbers and units same large
  font; restore measured smaller-unit groups (also solves Q1).
- Chart remains thin line + equal6px point dots; no area fill or endpoint emphasis,
  despite report saying both are implemented.
- Home/Devices Auto shows only text “Auto · CO2”, no leaf association.
  Generated check, plus/minus and down-chevron assets are not wired where required:
  Settings Back is literal '<'; steppers remain '+' and em dash; tiny font dropdown
  symbol remains. Source: ui.c:2448, 2727, 2752. Use the new assets consistently.
- Warm/celadon tonal depth is still flat; no bounded-effect implementation or
  measured rationale for omission. Request remains subtle, not full-image texture.
  Keep memory contract; no requirement to recreate photographic raster noise.
- Compare matched Auto fixtures: current supplied overview/devices/settings are
  Manual, and CO2 editor shows1200 instead of420. Do not change production defaults.

These were in the previous CURRENT, not optional new taste changes. Pixel-perfect
AI raster texture and tiny unrequested aesthetic differences are non-blocking.

## Q4 — P2: tests still give false confidence / inherited proof gaps

- sim_pc/main.c:368 only checks label y bounds against immediate non-scrollable
  parent, +/-2 tolerance. It does not inspect x, glyph ink, clipping ancestors,
  sibling lanes or text under controls. Added probes demonstrate real false PASS.
  Preserve font audit; add geometry/visibility assertions and negative controls
  that reject these actual collisions, not only small-font objects.
- CHECK31 now requires exact counters, but sim_pc/main.c:4676 still counts
  “command sent” merely because phase==IDLE. Record/assert changed sequence,
  actual TX packet target and confirmed readback for BOTH transitions in each of
  five scheduled lifecycles; an idle no-op must fail this check.
- Watchdog proof still incomplete: single helper checks parent cmd PID, not its
  PowerShell child; concurrent test checks only the worker PID mentioned by
  timeout (not all workers/descendants). Concurrent cleanup swallows errors.
  Source: tools/verify_production.ps1:120–133, 502–558.
  Passing full suite is acknowledged; complete tree termination is not established.
  Keep task ownership/bounded cleanup/diagnostics; no arbitrary process killing.
- tools/build_review_pack.py:385 logs regression returncode but never propagates
  failure; main returns0. A review-pack command must not report success when its
  validation failed. Add a controlled nonzero-return test.

## Q5 — P2: evidence and MCU documentation overstate results

REPORT/COMPARE/brief/log claim zero clipping, all ancestors inspected, 1000-hour
offline capture (actually00:00:03), all icons used/consistent, area fill and all
R7 closed. Findings above contradict those statements.
MCU_BASELINE labels host object sizes as firmware Flash/RAM and promises remaining
space on1/2MB parts, omitting target toolchain/link layout, LVGL, firmware and
buffers; it also retains old “four sizes / ~39KB / active Study12 permanent flat”
as current. Host CHECK31 +8bytes cannot prove zero leaks.

Correct active docs; preserve historical evidence with explicit supersession
notes. Do not replace approved concepts with current output, change tests to
approve overlaps, increase heap, or describe PC results as hardware acceptance.

## Consolidated delivery

[One complete fix prompt](../../prompts/CURRENT.md) includes Q1–Q5, accepted-work
freeze, native fixtures, architecture/Auto contract, final verification and honest
reporting. The next coding agent must fix and self-review all in-scope regressions
before handing back; these IDs are NOT implementation phases.

## Reproduce the review-only boundary probe

From repo root after tools/build_sim.bat (GCC path from this workstation):

```powershell
$reviewObjects = @(Get-ChildItem -LiteralPath sim_pc/build/CMakeFiles/sim_pc.dir -Recurse -Filter '*.obj' | Where-Object { $_.Name -ne 'main.c.obj' } | ForEach-Object { $_.FullName })
& C:/Toolchains/w64devkit/bin/gcc.exe -std=c11 -O2 -Wall -Wextra -Werror -DLV_CONF_INCLUDE_SIMPLE -DLV_LVGL_H_INCLUDE_SIMPLE -DUI_TEST_HOOKS -I. -Isim_pc -Isim_pc/build/_deps/lvgl-src docs/reviews/2026-09-18-reconstruction-review/probe.c @reviewObjects sim_pc/build/lib/liblvgl.a -lgdi32 -luser32 -o docs/reviews/2026-09-18-reconstruction-review/probe.exe
& docs/reviews/2026-09-18-reconstruction-review/probe.exe
```

Probe saves RAW captures; tools/raw2png.py converts them at800×480. Preserve
this historical evidence folder: copy the probe into a NEW review folder and
adjust its output path before rerunning against a future implementation.
Probe exit0 means scenarios ran, not visual acceptance; intentionally calls the
submitted audit to demonstrate its false PASS results. See source hashes above.
