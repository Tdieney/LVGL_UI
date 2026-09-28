ARCHIVED — historical context only

Superseded after owner rejection and R2 review. Use [CURRENT](../CURRENT.md).

# CURRENT — restore Quiet Architecture fidelity and close known proof gaps

Date: 2026-09-18. Status: **IMPLEMENTED — awaiting review**.
Scope: one consolidated LVGL implementation/fix delivery, F1–F6 plus V1.
Owner accepted the complete Quiet Architecture design, requested readable copy
and >=18px text, then rejected the implementation's visual fidelity.
The reviewer now supplies this prompt for another coding AI. No staged handoffs.

## Authority and prerequisite reading

Read AGENTS.md, docs/prompts/README.md, this entire CURRENT, docs/README.md,
docs/HARDWARE_OVERVIEW.md, docs/MCU_BASELINE.md, docs/UI_DESIGN_BRIEF.md,
docs/user/SMART_HUB_UI_GUIDE.md, docs/user/LORA_GUIDE.md and LORA_PROTOCOL.md.
Read [the consolidated review](../../reviews/2026-09-18-design-fidelity/REVIEW.md)
and open [its comparison](../../reviews/2026-09-18-design-fidelity/COMPARE.html).

Actually VIEW all seven approved images under docs/drafts/2026-09-17-pro-max/:
- dashboard-quiet-architecture.png
- trends-quiet-architecture.png
- devices-quiet-architecture.png
- settings-quiet-architecture.png
- auto-co2-quiet-architecture.png
- auto-humidity-quiet-architecture.png
- auto-voc-invalid-quiet-architecture.png

These images, the latest owner requests and the functional contract below are
the authority. The previous native renders are the submitted implementation,
NOT an accepted visual replacement. Study12/browser prototype, old440px modal,
old icon-only bottom tabs and historical borderless/no-gradient prohibitions do
not override the later accepted direction. Do not use dashboard-concept.png.

Use ui-ux-pro-max according to its instructions. Adapt it to LVGL8.4 embedded
800x480, not a web/mobile framework. Do not generate another aesthetic direction.
Inspect the complete current diff and preserve dirty owner work. Snapshot/hash
the starting sources and document exactly what you change.

The geometry/font ranges below are technical-lead translation targets, not an
owner-approved pixel specification. Resolve fit using the accepted composition,
18px minimum, touch safety and actual LVGL text metrics. Document necessary
adaptations; never silently delete reference components to make fitting easy.

## Work boundary

Implement the fidelity corrections and bounded inherited proof gaps together.
Do not commit/push, change branches, reset/clean/stash, change the packet schema,
add radio/RS485/Flash/CRC drivers, Node automation, multi-node support or a new
framework. No speculative Auto-policy or persistence changes. Preserve splash.

Do not use full-screen screenshot backgrounds with invisible hitboxes. Build
real LVGL controls, labels, icons and charts. Reuse the current model/engine,
page lifecycle and asset pipeline where appropriate. Regenerate native-size
icon/font tables if needed, keeping generator, declarations, build and export
manifest in sync. Do not enlarge tiny bitmaps with visibly blurred scaling.

## F1 — shared system and Overview

- Restore warm stone canvas, porcelain surfaces, dark forest actions/hero and
  muted celadon accents. Use fine separators, restrained outlines and subtle
  depth as in the references, not the old gray inset buttons/colored icon plates.
  Keep text contrast strong; no low-opacity body copy, heavy outlines or glows.
- Use semantic shared tokens for typography, colors, radii, strokes and spacing.
  Start with24px outer inset,12–16px primary gaps and consistent internal rows.
  Numeric adaptations are allowed if the native composition remains faithful.
- Restore prominent Studio/home symbol and compact capsule top navigation.
  Suggested type roles:18 secondary/units/errors,24 labels/actions,28–34 major
  headings, roughly42–48 hero readings and48–56 verdict when it fits. Use a
  SMALL coherent font set, not every intermediate size. >=18 for ALL visible
  text, including dropdown, axes, disabled controls, feedback and glyph fallback.
  Never silently fall back to LVGL's14px default.
- Maintain 44px minimum hit regions,48px steppers/actions, and8px separation
  between adjacent independent hit areas. A smaller painted switch/icon can
  have an expanded hit area. Do not overlap Retry and switch targets.
- Overview retains one forest hero plus two lower environmental tiles on the
  left and a four-row device panel on the right. Restore prominent air verdict,
  check/status symbol and air glyph, divided CO2/VOC columns, large digits and
  separate smaller units. Long values and Unavailable must fit without clipping.
- Short filler such as “Your room feels fresh” may stay removed. Metric quality
  words may remain for warning attribution, but should not compete with the
  main verdict. Keep aggregate unavailable behavior and unitless VOC.
- Temperature/humidity: larger coherent outline symbols; align title and
  reading to the same text column; separate smaller °C/% units and balance
  vertical whitespace. Do not invent comfort ratings or restore filler solely
  to occupy space. Handle full existing signed-temperature range/no-data.
- Device panel: larger outline icons, name/mode aligned in one text column,
  subtle row dividers, vertically centered switches, stronger panel heading.
  Restore sliders shortcut as navigation to Devices (no new global settings).
  Error/retry layouts may adapt but must retain clear separate operational lanes.

## F2 — Trends

- Restore the four selectable metric CARDS: icon, name, current reading/unit,
  clear selected forest/celadon treatment. Not name-only centered buttons.
- Restore chart metric heading and separators; make Current/Min/Max readings
  prominent with subordinate units. Keep axis labels >=18px.
- Forest line with subtle area treatment and endpoint/selected-point emphasis;
  remove equally prominent decorative dots on every sample. Preserve real
  point inspection and sample identity, including invalid/gap behavior.
- Display real session data only (16 samples, RAM). No new logging/flash and no
  smooth invented data. Do not hardcode “Last15min” when coverage is different.
  Preserve truthful existing time labels or derive relative labels from actual
  sample timing; test rollover. Empty history remains explicitly empty.
- Selected metric and units must remain clear even when values are unavailable.
  Selector live values, chart Current and stale/fault masking must agree.

## F3 — Devices

- Preserve2x2 cards; restore large outline appliance symbols (roughly44–52px),
  coherent stroke family, title/mode aligned to a shared text column and the
  larger switch at top-right centered against that group.
- Auto mode shows its sensor association (CO2/VOC/RH); Manual does not need a
  redundant ON/OFF label. Actual startup defaults stay Manual; never change
  production defaults to imitate a screenshot fixture.
- Restore separator plus Settings row with sliders symbol and chevron, not the
  current inset filled rectangle. Give the row a real safe touch target.
- Preserve all configured presets, long names, pending/unknown/Auto paused,
  error latch and explicit Retry. No hidden/overlapping actions when these
  states replace the nominal layout. Correct icon must follow preset changes.

## F4 — Settings

- Device-context title and unboxed back chevron with full hit target. Concise
  “Ventilation · Settings” is acceptable; generic “Device settings” loses context.
- Four aligned rows with coherent leading symbols and fine separators:
  Device type; Control mode; Auto thresholds; Output polarity.
- Visible outlined dropdown, large readable selected value and proper chevron;
  joined Manual/Auto segmented selector, dark forest selected Auto;
  threshold summary in the value column with ON/OFF association + chevron;
  explicit Active high / Active low segmented choices (not an output switch).
- Threshold-row unavailable/invalid feedback stays inline. Unsupported presets
  are Manual-only; Auto and threshold entry must not falsely look available.
- Secondary outlined Cancel and primary forest Save, consistent action font and
  baseline; preserve the reference's intentional two-action composition.
- Polarity is configuration, not an immediate actuation shortcut. Keep the
  existing pending-command restrictions and commit/readback mapping unchanged.

## F5 — Auto editor and validation

- Device-context title; celadon sensor/current-value strip with large icon,
  sensor name, separated live Current reading and smaller unit.
  Mask offline/invalid readings with No live data rather than retaining a number.
- Two equal ON/OFF cards below, with matching internal baselines and inclusive
  comparator text. Restore central outlined value fields, subordinate units
  and outlined48px-or-larger +/- buttons with clearly visible glyphs.
  These fields remain stepper-controlled; no new keyboard/numeric-entry feature.
- Three equally sized footer actions across available width: Cancel, Reset
  limits, Save. Same typography/height, consistent gaps; secondary outlined,
  Save forest. Keep helper/error slot between cards and footer, without moving
  buttons when text wraps. Two readable lines are acceptable; never shrink.
- CO2/VOC ON-at-or-above, OFF-at-or-below; humidity reverses both directions.
  Explicit ON/OFF and units are never removed as “redundant copy”.
- Invalid drafts: stable outlined VALUE fields with error color (no0->2px
  whole-card border reflow), warning icon, cause and repair instruction.
  Examples: raise ON/lower OFF for CO2/VOC; raise OFF/lower ON for humidity.
  Disabled Save must be noninteractive. Invalid parent indicator remains after
  Back. Test valid->invalid->valid bounds stay identical.
- Fit10000ppm, VOC1/500 and RH0/100; disable steppers at range ends. Keep exact
  inclusive validation/gaps and clamp edited value only, never shift its partner.

## Frozen functional and MCU contract

- LVGL8.4,800x480 RGB565,42KiB heap, ONE800x10 partial draw buffer. No MCU
  framebuffer, heavy compositing or per-tick allocation/float/trig.
- Static lifetime captions with lv_label_set_text_static; fixed dynamic buffers;
  bounded change-guarded redraw, LVGL only in application context.
- Keep page widgets released while focused editors are open; model/history/
  command tracking and Auto survive page rebuilds. Preserve pointer safety.
- Restore restrained visual depth/gradient only with a bounded implementation
  compatible with current allocation/cache constraints. Do not blindly enable
  gradient caching or allocate a large intermediate image. Measure transient
  and settled memory; retain >=18px and component hierarchy if effects need a
  documented low-cost approximation. Texture/photorealism is not required.
- ui.c defines the six globals exactly once: lora_hub_cmd, lora_node_status,
  lora_last_rx_tick_ms, lora_rx_revision, lora_last_snr, lora_last_rssi.
  Headers extern; firmware/simulator do not redefine. Plain #include "lvgl.h".
- Hub owns semantics/polarity/Auto; Node raw GPIO. Existing8-byte command and
  20-byte status, ui_is_tx_ready gating and direct wire globals unchanged.
- Uptime (not wall clock), Connected/Disconnected, signal bars, splash, English,
  °C and session history remain. No Details tab, notification panel, invented
  AQI/ppb-to-index algorithm or new health guarantee. Short operational feedback
  may remain if it does not obscure navigation/content or replace inline faults.
- Relay switches reflect Node readback, not optimistic intended values.

| Auto rule | ON / OFF defaults | Bounds | Step | Inclusive gap |
|---|---|---|---|---|
| Fan / CO2 ppm | >=1000 / <=800 | 0..10000 | 50 | ON>=OFF+50 |
| Purifier / unitless VOC | >=150 / <=100 | 1..500 | 5 | ON>=OFF+5 |
| Humidifier / %RH | <=40 / >=50 | 0..100 | 1 | OFF>=ON+5 |

Imported valid integers need not be step multiples. Preset change loads defaults;
unsupported types Manual-only. Missing/invalid/legacy config defaults Manual;
valid explicit Auto opts in. No new CRC/persistence guarantee.
Three eligible fresh frames plus10s hold after enable, committed limit change,
confirmed transition, boot/reconnect and sensor recovery. No Auto decision
offline/invalid/pending/latched. Manual override has no timed Auto return.
Retry or deliberate Manual->Auto re-arms; ordinary edits/reconnect/late ACK do
not. No forced-OFF on link loss. No-op Save emits no callback/reset.
Back from Auto preserves shared draft; Back from Settings/Cancel discards;
Save commits once. Reset changes draft thresholds only.

## V1 — retain and close the bounded inherited proof gaps

These are verification corrections, not permission to redesign engine behavior.

- tools/verify_production.ps1: validate exact names/membership and streamed
  byte equality or SHA256 for EVERY manifest entry (including required docs)
  against fixture sources in both concurrent default ZIPs and the explicit-race
  winner. Length equality is insufficient. Same-length tampering must fail.
  Validate returned paths; keep atomic non-overwrite, one race winner/one loser.
- Exercise timeout containment in BOTH process helpers, including known child
  process. Track task-owned PIDs/handles; bounded verification that workers and
  children are dead, unchanged unrelated sentinel hashes, useful per-worker
  stdout/stderr/exit/timeout diagnostics. Cleanup failures must fail; suppress
  successful WaitForExit booleans in result streams. Preserve async draining.
  Use disposable canonical-path-validated fixtures, never owner ZIPs.
- CHECK1: real LVGL clock/last RX crosses UINT32_MAX through ui_tick/integration
  TX; exact4999/5000ms masking/refused user action/Auto/zero stale TX, then valid
  recovery/readback. Keep existing predicate and ordinary-time tests.
- CHECK28: assert genuine PENDING then timeout/latch before the second Retry
  recovery branch. Preserve both actions/pages/polarities/recovery types,
  edge/center exactly-once matrix; adapt geometry to corrected visual layout.
- CHECK31: count actual command/sequence/TX/readback transitions in both
  directions, assert all five scheduled lifecycles. IDLE alone is not a sent
  command. Keep50 cycles, invalid drafts, immediate AND settled sampling,
  >4096 total free and existing drift criterion. Largest block/fragmentation
  remain diagnostics, not invented pass thresholds.
- Keep boot0, restored config, packet fault injection, revision/hold boundaries,
  six Auto wrap directions, production isolation and all prior passing checks.

## F6 — acceptance, evidence and honest handoff

Self-review the ENTIRE final diff and affected flows before handing back once.
Fix in-scope regressions yourself; do not ask the owner to relay issue groups.

1. Add deterministic matched-state native800x480 captures for all seven
   reference screens, including VOC ON=OFF=150, valid CO2/RH and Settings Auto.
   Fixture setup may use test hooks; never alter default production state.
   Provide side-by-side reference/native comparison normalized to the same
   dimensions, plus brief justified deviations. Do not call mismatched states
   pixel-equivalent or replace approved images with your own output.
2. Inspect every screen visually, not just test output. Check icons, hierarchy,
   units, gutters, dividers, selector/card anatomy and control states. Check
   all presets, dropdown top/bottom, Unknown/pending/Retry/Auto paused, offline/
   sensor loss, no history, valid/invalid/bounds and return-from-editor flows.
3. Extend meaningful assertions for shared baselines, non-overlap, hit regions,
   missing reference components, unit/value hierarchy and stable validation
   geometry. Keep the18px font audit with real nominal font-size metadata,
   coverage of all added fonts/glyphs and its rejecting negative control.
   Do not whitelist unknown fonts or delete tests to obtain PASS. Rename
   misleading “verified against prototype” diagnostics to describe real checks.
4. Run after the last relevant change, with cached dependencies:
   tools/build_sim.bat
   tools/run_regression.bat
   tools/export_mcu.bat --verify
   tools/verify_production.bat
   Preserve C99 build/link without LV_LVGL_H_INCLUDE_SIMPLE/UI_TEST_HOOKS and
   strict negative control (launched, exit1, expected stderr; timeout is failure).
5. Report heap/dirty-region metrics, no-change redraw, new font/icon Flash cost,
   immediate and settled memory on the most complex pages/dropdown/error states.
   A PC render does not validate physical LCD or actual target memory/link.
6. Save ONE final report, raw command logs, captures and comparison under a new
   docs/reviews/<date>-quiet-fidelity-fix/ folder. Map F1–F6/V1 to evidence;
   separate confirmed fixes, proof gaps, optional polish and hardware-only limits.
   Preserve historical artifacts. No zero-bug, fake CRC or invented calibration
   claims. Document actual manifest/font set, not old hardcoded footprint data.
7. Update UI_DESIGN_BRIEF, user guide, MCU_BASELINE, PLAN, DEV_LOG and indexes.
   Mark obsolete Study12 font/gradient/heap claims historical where misleading;
   keep actual memory/wire requirements. CURRENT stays this complete spec;
   mark IMPLEMENTED — awaiting review only after full scope is complete.
   Visual acceptance remains pending owner/reviewer approval, not self-declared.

If genuinely blocked, complete unaffected work and report ALL blockers with
attempted checks together. Do not silently simplify the design, loosen budgets
or hand off partial groups as separate rounds.
