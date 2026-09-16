> ARCHIVED — historical context only. Use [CURRENT.md](../CURRENT.md).
> Preserved 2026-09-15 before the V1–V4 corrective handoff; original body follows unchanged.

# CURRENT — Auto layout polish and remaining review corrections

Date: 2026-09-15. Status: **READY FOR HANDOFF — not implemented or accepted yet**.

Owner request: review the latest R1–R10 submission; fix uneven spacing in Auto
settings and the top-heavy Temperature/Humidity cards; include other necessary
corrections in one new prompt. The owner delegated layout judgment to the
technical lead. The geometry/copy below are the lead's selected implementation
direction, not a claim the owner approved individual pixel coordinates.

This is the ONLY active handoff. Earlier prompts/reports are historical evidence.
The prompt-writing turn made documentation/review diagnostics only, not UI changes.

## 1. Role, scope and reading order

You are the implementation engineer in this repository. Implement the corrections,
test them, inspect native screenshots and submit evidence. Do not stop at a plan.
No commit, push, branch change, reset, clean, unrelated refactor or new features.

Read AGENTS.md, docs/prompts/README.md, this entire file,
docs/reviews/2026-09-15-auto-v1-layout-review/REVIEW.md and check.c,
docs/README.md, docs/HARDWARE_OVERVIEW.md, docs/MCU_BASELINE.md,
docs/UI_DESIGN_BRIEF.md, docs/user/SMART_HUB_UI_GUIDE.md,
docs/user/LORA_GUIDE.md, LORA_PROTOCOL.md and current source/tests.
Inspect Git status and preserve existing edits. Do not edit historical review
evidence to manufacture passing results.

The current build and 24 regressions pass, and independent targeted checks confirm
several prior fixes. That does NOT close all previous acceptance requirements.
Follow findings L1–L6 below; retain working behavior instead of reimplementing Auto.

## 2. Non-negotiable baseline

- LVGL 8.4, 800x480 RGB565; one 800x10 partial draw buffer; unchanged 42 KiB heap.
  No MCU framebuffer, heap enlargement, per-tick allocation, float formatting or
  trigonometry. Static captions use process-lifetime storage and
  lv_label_set_text_static(); changing text uses bounded buffers and change guards.
- Keep Study 12 flat/light palette, existing metric hues, borderless cards and
  existing subtle shadows. No gradient, new border, dark overlay treatment,
  recoloring, new icon family, global typography redesign or layout scale change.
- Keep visible text >=18 px, existing custom fonts, elapsed HH:MM:SS header,
  Connected/Disconnected, SNR bars, splash and three icon-only navigation targets.
  Do not restore Room Air, Details, notification panel, redundant On/Off text or
  normal-state filler captions. Internal LVGL default font 14 may remain enabled.
- Hub owns Auto/preset/polarity; Node is a dumb four-GPIO actuator.
  Preserve direct lora_comm.h/lora_hub_link.h globals and 8-byte/20-byte packets.
  No snapshot layer, Node automation/watchdog, RF/Modbus/Flash drivers, multi-node,
  scenes, schedules, remote-settings ACK or new persistence mechanism.
- Local control and LVGL execute in application context, never ISR.
  Keep 16-sample session-RAM history and existing chart interactions.
- Only improve local layout, remaining correctness, tests and truthful docs.
  Firmware/real-radio/LCD/touch/load validation remains unperformed on target.

## 3. L1 [P2] Rebuild both settings subviews on one spacing system

Keep modal outer bounds (180,56,440,368), radius/palette and 20 px padding.
Its content origin is screen (200,76), content size 400x328.
ALL following coordinates are relative to that content origin, NOT screen or
already-padded coordinates. Use named shared constants. Do not double-add padding.

Common header: y=0..43. X at (356,0), 44x44, always hit-testable/foreground.
In Device view title is vertically centered in this header and ends before X.
In Auto view Back is (0,0,44,44), title starts x=56 and ends by x=344.
Title font 24; labels 18; primary Save 24.
Shared Save settings at (0,280,400,48) on BOTH views. Its bottom matches content
bottom, retaining 20 px physical outer padding. Never move it for validation.

### Device settings

Use horizontal label/control rows; the current stacked labels leave small gaps
inside groups and oversized gaps elsewhere. New rows have a 12 px gap:

| Row | y | Height | Horizontal arrangement |
|---|---:|---:|---|
| Device type | 56 | 44 | Label x=0,w=128; dropdown x=140,w=260 |
| Control mode | 112 | 44 | Label x=0,w=128; Manual x=140,w=124; Auto x=276,w=124 |
| Set limits | 168 | 44 | Full-width secondary action, x=0,w=400 |
| Active low polarity | 224 | 44 | Label left; visual switch right; effective target >=54x44 |
| Save settings | 280 | 48 | Shared footer |

Center labels vertically with the actual font line height. The small visual
polarity switch can stay 54x30 inside its 44 px target; do not overlap adjacent
targets or let an extended target consume another control.
Manual-only presets use the Set limits row for the existing explanation and
disable Auto; rows/Save stay fixed. Dropdown items also need readable labels,
>=44 px touch rows, safe height/scrolling and no clipping beyond the modal/display.
Do not expose unsupported automation presets.

### Auto thresholds

| Element | Position/size |
|---|---|
| Rule context | x=0,y=56,w=400,h=25 |
| ON row | y=92,h=48 |
| OFF row | y=152,h=48 |
| Reset limits | x=0,y=212,w=132,h=44 |
| Inline validation | x=144,y=212,w=256,h<=56 |
| Save settings | x=0,y=280,w=400,h=48 |

For BOTH threshold rows: condition label x=0,w=136; minus x=148,w=48;
numeric value x=208,w=132; plus x=352,w=48. Buttons are 48x48.
Center each numeric value within its fixed column; align labels/numbers/buttons
vertically to the same row center. The unit belongs in the rule context:
Ventilation Fan — CO2 (ppm), Air Purifier — VOC, Humidifier — Humidity (%RH).
Use the existing supported CO2 glyph spelling; do not introduce missing glyphs.
VOC is unitless: remove the visible "(index)" suffix, not the wire field name
or the underlying 1..500 data contract.

Retain comparator direction: Fan/Purifier ON >= and OFF <=; Humidifier reversed.
Use available readable comparator glyphs, or existing >=/<= if custom fonts lack them.
Keep the largest value 10000 fully visible at font 24 without resizing the column.

The validation slot is intentionally reserved: invalid drafts must not push rows
or Save around. Use short precise English copy fitting <=2 lines at font 18,
for example "ON must be at least 50 ppm above OFF." Include "at least" because
the required gap is inclusive. Handle runtime commit rejection visibly too,
without replacing a useful validation message with an unrelated error.
Disabled Save keeps readable text contrast; no new outline to indicate an error.

Shared draft semantics MUST survive:
Back retains ALL draft fields; X cancels even invalid drafts; Reset affects only
draft limits; Save validates/commits once from either view. Current Auto uses
committed configuration while the editor is open. No draft writes to the wire.
Real pointer tests, not direct close/save callbacks, must prove these paths.
Keep bounded long-press repeat and no extra increment on release.

## 4. L2 [P2] Balance Temperature/Humidity without false status text

Keep both cards 236x98 at screen (544,68) and (544,178), preserving their 12 px
gap and alignment with the combined 208 px CO2/VOC height.
Keep their backgrounds and full-card tap-to-Trends behavior.

Use a shared two-line composition. Coordinates here are relative to CARD EDGES
before padding, not the existing padded object origin:

- Icon 24x24 at x=12,y=37 (centered vertically).
- Text column x=44, right padding >=12, available width 180.
- Title font 18 at y=12 (current line height 25).
- Value font 34 at y=41 (current line height 46).
- This produces a 75 px title/value group, with ~12/11 px top/bottom balance and
  a 4 px line-box gap. Use measured font metrics if implementation differs;
  do not confuse font nominal size with line height.
- Keep units inline with valid numbers: 23.5°C, -2.5°C, 48%.
  Test supported numeric extremes, including negative temperature and 100% RH.
- Do NOT restore Ideal/Comfortable/Healthy/Good or static ranges to fill the footer.
  There is no approved comfort evaluation for these two sensors.
- Invalid reading: keep the same title/value-row anchors, show a large em dash
  without a unit and a compact "No data" font-18 note beside it, vertically centered
  in the value row. Offline uses "Offline". Keep >=8 px horizontal gap after the
  dash and all text within the column. The note is not a third footer line.
  Do not show stale numbers, numeric zero, or an invented comfort status.
- Valid/invalid/offline transitions must neither move card bounds nor shift
  neighboring widgets; valid titles/values retain the same anchors on recovery.
  Shared styling/layout helper is acceptable; do not add per-tick layout churn.

Native before/after screenshots are required; geometry alone is not visual approval.

## 5. L3 [P2] Make device status layouts collision-free; L4 [P2] show link pause

Independent reproduction: Home "Auto paused" bounds (32,350)..(137,374) intersect
the switch (135,348)..(188,377). The same collision is visible in the submitted
auto_paused screenshot. Fix ALL four tiles, not just the demonstrated channel.

Reserve independent mode/status and action areas. A suitable direction is a
two-line mode stack ("Auto" / "paused") for paused states, with font >=18,
instead of shrinking text or moving a switch over a label. Keep the existing
Home tile bounds, title/icon and fixed right-hand switch anchor. Allocate a
bounded left column ending >=8 px before the action area. Shorten/stack status
copy where needed; do not ellipsize away "paused" or hide a failure.
Handle Manual, Auto, sensor pause, link loss, Sending, error+Retry and recovery
as complete layouts. A single "Auto paused" label remains fine on Devices if it
fits. Retry must remain >=44x44 with no caption/target overlap.

Manual remains Manual when offline; Auto must visibly indicate suspended
decisions when offline, not plain Auto. Derive this from current link state plus
sensor suspension/error latch in one consistent status policy used on both pages.
A transport pause is NOT a latched command error. Reconnect does not clear a real
timeout latch. Keep honest unknown-output masking while offline; no forced OFF
policy or automatic Retry. Healthy first report restores ordinary eligibility
with existing recovery hold, not an immediate Auto command.

Do not regress switches: position represents reported_gpio XOR active_low,
including the immediate frame after a real pointer gesture and while pending.
Sending is a separate indication; desired intent stays in command state.
A timeout/late confirmation/reconnect must not silently re-enable latched Auto.

## 6. L5 [P1] Complete the integration/config documentation contract

The main loop example was fixed, but docs/user/LORA_GUIDE.md section 9.2 still
contains an unconditional lora_send example and its prose encourages periodic
and sequence-triggered TX without a gate. Fix ALL copyable command send examples
and descriptions, not only the first main loop.

Document and test: publish RX -> reconcile/control in ui_tick -> process LVGL
input in application context -> gated TX. Every new-command and periodic send
must check readiness after reconciliation. No packet before first eligible report,
while stale, or with stale intent on the first reconnect iteration.
Use a recording TX stub that executes the actual integration order, including
periodic and changed-sequence branches. Manifest checks cannot test send ordering.

The Flash integration EXAMPLE still reads directly into the enlarged struct and
only comments "migrate if old". Replace with bounded length/version-aware restore
pseudocode or a host-tested integration adapter: verify record size and schema
before interpreting/copying a complete current record; migrate only a complete
known legacy record; reject unknown/truncated/malformed records; use Manual
fallback/default limits. Do not write a Flash driver or change the packet.
Valid legacy metadata may be preserved; missing thresholds never enable Auto.
Clarify restore invokes no save callback; runtime rejected/no-op/draft/cancel
operations also invoke none, while one actual committed edit invokes exactly one.
Keep observable bool rejection from ui_set_device_config.

Correct stale current docs: examples of 100 ppm gap -> 50 ppm; VOC index as a
visible unit -> unitless VOC; retired snapshot/overlay APIs; 24h history claims;
unmeasured memory claims and false all-findings-accepted statements.
Preserve historical reports but label superseded conclusions in current indexes.

## 7. L6 [P2] Actually verify the standalone export and close test gaps

tools/verify_production.ps1 currently runs export --verify (manifest only), then
builds smoke_production from repository-root sources and the SAME simulator
LVGL library/config. This is NOT a build of the staged MCU export.

Add a reproducible isolated HOST production check:
1. Stage actual exported sources/headers into a fresh validated temp/build directory.
2. Use a complete documented MCU LVGL configuration (including its default font).
3. Build LVGL again for that configuration, reusing only cached LVGL SOURCE offline.
4. Compile/link/run ONLY staged UI sources and minimal application stubs with
   UI_TEST_HOOKS disabled; no simulator/test headers or hidden root includes.
5. Always-on Release checks (-DNDEBUG) must pass; --fail-check must launch
   successfully and terminate with the explicit expected code/message.
   A failed process launch or unrelated crash must FAIL verification, not pass it.
   Remove the current catch-all that converts launch exceptions into success.
6. Propagate all failures; log exact source/config provenance and commands.
   Export manifest, isolated host execution and physical-target validation are
   three distinct levels. Do not call a host result a hardware-readback test.

Keep the new review diagnostic source intact. Promote meaningful regressions into
the maintained simulator suite. Historical observation harnesses alone are not
acceptance: the old close case skips Auto after Device X succeeds; the old
api_hold case no longer reaches pending after hold was fixed. Assert preconditions.

Required maintained coverage beyond the existing 24 checks:
- Separate real-pointer X tests in both freshly opened subviews, invalid draft,
  Back, Reset/Cancel, polarity -> limits -> Save, callbacks and no click-through.
- Actual pending polarity/preset rejection, invalid and no-op runtime commits,
  manual override, both pages/polarities and switch readback before/after ACK.
- Default/fallback Manual, valid explicit restored Auto, unknown schema, truncated
  records and legacy migration; boot/reconnect GPIO adoption for all 16 masks.
- Sensor pause/recovery hold, link pause/recovery, latched timeout surviving loss,
  late ACK and threshold edits; explicit Retry or committed Manual->Auto re-arm.
- Fixed row/column/button bounds, >=44 px nonoverlapping touch targets, validation
  height and five-digit values; all three presets and Manual-only view.
- Temperature/Humidity geometry and pixels in valid/invalid/offline/recovery states;
  Home and Devices mode/status/action bounds in every state, not just normal.
- Zero redraw on identical telemetry/history excluding uptime/animation; chart
  live-current changes do not repaint history, existing axis/idempotence intact.
- >=50 full page + modal + dropdown + Back/limits + invalid-draft/cancel cycles.
  Track minimum free heap, minimum largest block, fragmentation and stable-state
  drift across the complete loop. Require >4096 free at tested peaks, no leak/hang,
  unchanged 42 KiB. Do not describe page-only cycling as full modal stress.

## 8. Auto contract to preserve (no new policy)

| Rule | Default ON / OFF | Bounds | Step | Inclusive required gap |
|---|---|---|---|---|
| Fan / CO2 ppm | >=1000 / <=800 | 0..10000 | 50 | ON >= OFF+50 |
| Purifier / unitless VOC | >=150 / <=100 | 1..500 | 5 | ON >= OFF+5 |
| Humidifier / %RH | <=40 / >=50 | 0..100 | 1 | OFF >= ON+5 |

Per-device thresholds, including identical presets, are independent. Imported
valid integers need not be exact step multiples. Clamp one edited value at bounds;
invalid gap disables Save without silently changing the other value. Preset
change loads that preset's defaults. Unsupported presets stay Manual-only.
These values are demo policy, not health or appliance-protection standards.
No ppb-to-Sensirion/Bosch index conversion is authorized.

Only explicit complete valid current Auto config can enable/restore Auto.
Require three consecutive eligible newly received frames and the existing
10-second hold after enabling, committed limit change, confirmed output change,
boot/reconnect sync and sensor recovery. Use wrap-safe time/revisions.
Duplicate frames and elapsed time alone cannot trigger decisions. Faults pause
only relevant rules. No new decisions while pending/invalid/offline/latched.
Keep each channel's pending intent independent. Idle unsolicited readback is
adopted before new Auto decisions. Manual override commits Manual + opposite fresh
reported output, no timed return. Retry for Auto adopts CURRENT fresh state and
restarts qualification/hold rather than resending an obsolete target.
No-op Save does not reset hold or callbacks. Ordinary Save/limit edit/reconnect
does not clear a timeout latch. No output is guaranteed safe/OFF during link loss.

## 9. Delivery and acceptance

Run tools/build_sim.bat, tools/run_regression.bat, tools/export_mcu.bat --verify,
and the corrected tools/verify_production.bat. Maintain offline dependency reuse.
Keep test process watchdogs. Before any cleanup, validate exact absolute temp
targets; do not delete shared review evidence or unrelated user files.

Save this round's report, reproducible test sources/logs and native 800x480 PNGs
under docs/reviews/2026-09-15-auto-layout-fixes/. Include:
- L1–L6 -> changed files -> named test -> result/evidence, with residual limitations.
- Home before/after; valid, sensor-invalid, disconnected; all paused/sending/retry
  layouts; both settings subviews; all three rules; 10000; invalid pair; dropdown;
  Manual-only preset and cancellation/recovery evidence.
- Real LVGL renders at 1:1, inspected for optical balance, readable error text,
  row alignment and no collision; not browser mockups or resized composites.
- Actual measured heap/dirty-region results and isolated export provenance.
- Separate implementation-author checks from independent acceptance.

Update PLAN.md, DEV_LOG.md, relevant integration docs and UI_DESIGN_BRIEF.md to
reflect what actually shipped. Do not rewrite this prompt to legalize omissions,
scatter a replacement prompt or overwrite historical evidence. If any requirement
remains unverified, report it OPEN rather than claiming all fixed. No commit/push.
