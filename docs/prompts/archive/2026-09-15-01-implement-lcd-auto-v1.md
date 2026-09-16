# CURRENT — fix six review defects + LCD-configurable Auto v1

> ARCHIVED — historical context only. Preserved before the 2026-09-15 corrective handoff.
> This implementation request produced a submission with unresolved review findings.
> Use [CURRENT.md](../CURRENT.md) for the active request and [rules](../README.md).

Date: 2026-09-14.

Status: READY FOR IMPLEMENTATION PROMPT HANDOFF; implementation NOT completed.
Owner decision (2026-09-14): consolidate prompt storage and make Auto thresholds
editable on the LCD, replacing the earlier fixed-threshold-only proposal.
The detailed v1 rules, defaults and interaction choices below are the technical
lead's implementation specification for that request, not claims of health
standards or separately quoted owner decisions. Read docs/prompts/README.md first.
This file is the only active handoff; archived prompts are historical context.

---

You are the implementation engineer in:
`C:\Users\vanth\Documents\MyDocument\Embedded\LVGL_UI`.

Repair the current implementation; do not rebuild the UI or reproduce an older
design. Read the actual current source, `AGENTS.md`, `docs/README.md`,
`docs/HARDWARE_OVERVIEW.md`, `docs/MCU_BASELINE.md`, `docs/UI_DESIGN_BRIEF.md`,
`docs/user/SMART_HUB_UI_GUIDE.md`, `LORA_PROTOCOL.md`, `docs/prompts/README.md`,
and this complete prompt.
Older claims of completion do not override reproductions below. Check Git status
first and preserve all pre-existing work. Do not reset, clean, change branches,
commit, or push unless separately requested.

## Non-negotiable boundaries

- LVGL 8.4; 800x480 RGB565; one 800x10 draw buffer; current 42 KiB heap profile.
  No MCU framebuffer, heap-budget increase, per-tick allocation, float formatting,
  or trigonometry to make tests pass.
- Keep current Study 12 flat palette, borderless cards, font hierarchy (visible
  text at least 18 px), splash, elapsed-time header, SNR bars and icon-only bottom
  navigation. No new page, notification panel, Details tab or redesign.
- Preserve direct wire globals in `lora_comm.h` and Hub metadata in
  `lora_hub_link.h`. Do not restore a production snapshot/translation layer or a
  separate remote-settings ACK mechanism. Config remains Hub-local.
- Node remains a dumb GPIO actuator; Hub owns names, mode and active_low.
  Do not change the 8-byte command / 20-byte status packet schema or implement
  radio, Modbus, Flash storage, multi-node support or a Node watchdog here.
- All UI/control work runs in application context, never ISR. Keep shared data
  ownership and RX -> reconcile/control -> TX -> rendering ordering explicit.
- The owner request now expands the former automation exclusion narrowly:
  three Hub-side per-device rules with LCD-editable thresholds. Follow the updated
  AGENTS.md scope; do not add a general automation engine.

## A. Fix all six independently verified defects

### A1. Reconcile actual wire levels, not only logical state

Reproduction in current `ui.c` near `ui_set_device_config()` and the idle branch
of `ui_tick()`:

1. Initialize UI; restore relay index 0 with active_low=1 before the first update.
2. Receive GPIO=1 for that relay: reported logical OFF and desired OFF agree.
3. Command GPIO incorrectly remains its initial 0 because the logical comparison
   skips reconciliation.
4. Toggle relay index 1; the combined packet sends GPIO=0 for relay index 0 too.
   The level-triggered Fake Node turns index 0 ON without its switch being used.

Fix the invariant for every settled channel: the command wire level must match
the adopted reported GPIO, even when logical desired/reported states were equal.
Do not overwrite a legitimate in-flight request with an older report. Preserve
per-channel intent, wrap-safe sequence checks, timeout/retry/late-report handling
and simultaneous commands to different channels.

### A2. Define one complete boot/configuration/TX contract

The current guide explicitly loads configuration BEFORE `ui_init()`, which resets
all configurations. Verified result: `Restored light / active_low=1` becomes
`Air Purifier / active_low=0`. Moving the load after init alone leaves A1 exposed.

Implement and document these outcomes:

- Valid restored config survives UI initialization. If keeping pre-init setters,
  stage validated config without LVGL calls and default only unset channels.
- If choosing a different explicit initialization API, migrate every caller,
  simulator fixture and firmware example together; reject unsupported call order
  clearly. Do not silently discard supplied config.
- Validate preset/mode/active_low and terminate bounded names. Invalid storage
  must not create invalid bit masks or out-of-range access. Document fallback.
- Before the first eligible fresh Node report is reconciled with restored config,
  outbound actuator packets must be gated. Supply a small readiness API or an
  equally explicit application contract, implemented in simulator and examples.
  `ui_init()` producing a valid header alone must NOT authorize transmission.
- Startup synchronization adopts all four reported GPIO levels without changing
  physical outputs. No zero-initialized command may leak onto TX, including
  periodic retransmission before the first UI tick. Apply the same principle on
  reconnect so stale intents are not transmitted before reconciliation.
- Distinguish restoring configuration from a deliberate live polarity change.
  Live changes use the same validated command path as dialog Save; a restore is
  not an implicit actuator command. Do not alter another channel's intent.
- Local mode/name/preset saves require no remote ACK; only actual output commands
  enter Sending/timeout tracking. Notify config changes once per committed change,
  not on ticks or restoration. Explain pending/disconnected edit restrictions.
- Keep the external firmware TX example and the level-triggered Fake Node honest:
  readiness gates apply to periodic TX as well as new sequences. Do not make the
  Node ignore inconvenient command bits to hide an initialization bug.

### A3. Fix Trends Y-axis invalidation and size updates

Feed CO2 history 400..415, then 1000..1150 without leaving Trends. Current code
updates plot/Min/Max but leaves old Y ticks 418 / 407 / 397 visible. Label width
stays 27 px when the string becomes `1180`. Forced full redraw changes 915 pixels
in the isolated Y-label test region, without any new data.

Cause: formatting directly into a buffer already bound through
`lv_label_set_text_static()`, then comparing that buffer to itself.

Format into a temporary buffer, compare before replacing persistent storage, and
notify LVGL correctly. Audit the same aliasing pattern elsewhere. Verify text,
object dimensions and actual pixels for growing/shrinking values and metric
switches, including negative temperature. Do not use whole-screen invalidation
or page reconstruction as the fix; ensure the scale labels do not collide with
the plot across documented input ranges.

### A4. Make unchanged Trends updates genuinely idle

Current independent measurement, with stable history/values/link and no uptime
second boundary: 10 identical received frames on Trends cause 200 flushes and
1,303,500 pixels. Home's equivalent is already zero.

Track history/range/axis/live-summary changes separately. Do not unconditionally
refresh the chart, set its range, or reset static time labels. Receiving a new
packet revision is not proof that each widget changed. Refeeding an identical
history window must also be a no-op. A changed live Current value without a new
history window must not repaint the plot. Real history changes must still update
correctly; clear or refresh an inspected tooltip when its referenced sample changes.

### A5. Remove false comfort claims

Current valid readings 50.0 C and 100% RH still produce `Ideal comfort` and
`Ideal humidity`. Do not introduce unapproved comfort/health thresholds to fix it.
Remove those two affirmative static captions for valid readings; retain the
existing card geometry and explicit invalid/disconnected notes. Keep values and
units readable without rebalancing the whole layout. Update prototype references
only for this deliberate copy change and approved Auto controls.

### A6. Make MCU font configuration and validation executable

`docs/MCU_BASELINE.md` tells integrators to disable Montserrat 14 but leaves
`LV_FONT_DEFAULT` unresolved; doing so reproduces
`lv_font_montserrat_14 undeclared`. The simulator currently keeps that font enabled.

Provide one complete, supported default-font configuration and make simulator,
MCU example and documentation agree. Keeping Montserrat 14 for LVGL's internal
default is acceptable; visible UI must retain its custom >=18 px fonts. If using
a custom default instead, supply declarations/configuration in a form valid while
LVGL headers themselves are being compiled; avoid circular include tricks.

Add a production build/link smoke test with UI_TEST_HOOKS disabled, exported
production files, that documented LVGL config, and only minimal application stubs.
Do not link simulator/test APIs into it. Compile LVGL with the same configuration;
do not reuse an incompatible cached library. Verify required fonts resolve and
the export is self-contained. A host toolchain check is useful but is not an MCU
cross-build or target validation; label it accurately.

Keep `export_mcu --verify`'s manifest meaning explicit. Update stale footprint,
heap and retired snapshot/overlay API claims rather than copying old numbers.

## B. Implement Auto v1 with LCD-editable thresholds

### B1. Product scope and initial defaults

Auto is a Hub-local per-device rule, evaluated against fresh received telemetry,
independent of active tab, splash or settings dialog. Use the three previously
proposed mappings; thresholds below are INITIAL DEFAULTS, never hardcoded runtime
conditions after the user has configured a device.

| Preset | Input | Default ON | Default OFF |
|---|---|---|---|
| Ventilation Fan | displayed CO2/eCO2, ppm | >= 1000 | <= 800 |
| Air Purifier | received unitless VOC index | >= 150 | <= 100 |
| Humidifier | humidity, integer %RH | <= 40 | >= 50 |

Between thresholds retain reconciled state/current legitimate intent. Defaults
are demo settings, NOT certified health, comfort or appliance-protection limits.
Do not convert ppb to an invented Sensirion index. Upstream VOC derivation remains
firmware scope; the UI consumes the existing unitless wire field.

Light, Plug, Generic, Heater and Dehumidifier are Manual-only in v1. Default all
devices to Manual unless valid restored config explicitly enables Auto for a
supported preset. No schedules, scenes, global Auto switch, compressor/heater
automation or arbitrary user-authored expressions. Demonstrate with appropriate
low-voltage demo loads, not as safe unattended appliance control.

### B2. LCD editor: inside the existing settings modal

Keep the current 440x368 modal and native 800x480 layout. Use an internal
`Device` / `Auto thresholds` subview with a Back action; this is NOT a new main
page, fourth navigation tab or a second nested modal.

- Provide a visible, discoverable `Set limits` control associated with Auto on
  supported presets. It must be usable in Manual too, so thresholds can be prepared
  without activating the rule. Keep Manual/Auto selection a separate action.
- Make space by replacing redundant helper copy and arranging existing controls
  deliberately. Do not shrink fonts or switch targets to squeeze the editor in.
- Show the device name/preset, sensor/unit, and two clearly labelled rows:
  `Turn on at` and `Turn off at`. Show the correct comparator for the preset.
- Each row has a large integer value, minus and plus controls. No numeric keyboard.
  Ordinary taps change one step; long-press repeats with a bounded rate and no
  extra event after release. Use at least 44x44 nonoverlapping touch targets,
  visible text >=18 px and existing palette/icons. Clamp at allowed boundaries.
- Keep both rows, validation message and primary `Save settings` visible without
  scrolling. Provide `Reset limits` to restore only this draft's default pair;
  it does not change mode, polarity, another device or active runtime config.
- Use ONE modal edit transaction for preset, mode, polarity and thresholds.
  Back only navigates within the draft; it must neither save nor discard it.
  X/cancel discards the entire draft from either subview. Save from either view
  commits the entire valid draft once and closes the modal. Do not add a second
  ambiguous Apply/Done save stage.
- Opening/editing a modal does not pause an already-running Auto rule: it continues
  using committed config. Unsaved edits and invalid drafts must NEVER reach the
  control loop or outgoing packets. Save while invalid is disabled with a concise
  local explanation; X/cancel and Back remain usable.
- Re-entering the modal shows the device's committed values. Devices sharing a
  preset have independent thresholds. Changing the draft to a different preset
  uses that preset's defaults; unsupported presets disable Auto/Set limits and
  stage Manual. No cross-unit reinterpretation of an old threshold pair.
- Do not add alarm banners, modal stacking, graph preview or a threshold editor
  elsewhere in the UI. Native screenshots must demonstrate that the editor fits.

### B3. Validation, units and configuration storage

Use integer ON/OFF thresholds in the Hub-local per-device configuration. One
canonical validator/default table must serve UI, restore and public config APIs.

| Rule input | Editable inclusive limits | Tap step | Valid threshold ordering |
|---|---|---|---|
| CO2 ppm | 0..10000 | 50 | ON >= OFF + 50 |
| VOC, unitless | 1..500 | 5 | ON >= OFF + 5 |
| Humidity %RH | 0..100 | 1 | OFF >= ON + 5 |

These are editor bounds/hysteresis guards, not healthy/unhealthy ranges. Do not
silently adjust the other threshold when the user crosses it: permit an invalid
draft, explain e.g. `On must exceed Off by 50 ppm`, and block Save until corrected.
Steppers clamp individually and must not overflow/underflow. Imported integer
values within bounds and with valid ordering need not be exact step multiples.

Persist thresholds through the existing config-change callback contract ONLY.
Implement no Flash/NVS driver. Add a config schema/version and migration/default
contract so old records without thresholds cannot be mistaken for valid Auto
rules. The integration example must demonstrate old/new record handling, size/
version checking and safe defaults; never memcpy a short legacy record as a
larger struct. Missing/invalid threshold config receives preset defaults and
Manual mode, with a documented recovery indication; invalid stored data must
never energize outputs during boot. Boot restoration is not a save callback.

Saving new limits in active Auto keeps the physical output unchanged at commit,
resets rule qualification/hold, then uses only the committed new values on fresh
samples. An existing in-flight command remains tracked until its outcome is known;
new thresholds must not supersede it. Validate config APIs too, not just widgets.
Document behavior of live preset/polarity changes while a command is pending;
refuse edits that cannot preserve the existing command invariant.

### B4. Timing and noise rejection

- Evaluate on new received revisions, not on every UI tick as another sample.
- An ON/OFF threshold must hold for 3 consecutive eligible fresh frames.
  Nonqualifying/invalid frames, lost link or changed rule reset the corresponding
  counter. Deadband frames do not count toward either transition.
- A new automatic transition also requires a 10-second minimum hold since the
  previous confirmed output transition. On enabling Auto, changing committed
  limits, startup reconciliation, reconnect or recovery from sensor failure,
  start a new 10-second hold. Saving unchanged settings must not reset it.
- After any in-flight command settles, evaluate the then-current configuration;
  don't act on a threshold decision queued under superseded limits.
- No transition fires solely because time passed without another eligible sample.
  Use wrap-safe timestamps, bounded fixed storage and bounded counters.
- Auto cannot replace/reverse a pending command. Wait for confirmation or explicit
  error flow. Stable values must not increment seq or flood announcements.
- Adopt unsolicited reported output changes first, then restart hold/qualification;
  do not immediately fight an external change on that same frame.

### B5. Manual interaction and ownership

- An accepted user switch gesture in Auto atomically commits Manual for that
  device and requests the opposite of its last fresh reported output. No timed
  return to Auto; config-change callback occurs once.
- Reject disconnected/pending gestures as before. Rejected gestures must not
  silently change mode. Selecting Auto again is an explicit settings action.
- Entering Auto preserves the physical output initially and begins qualification.
  Saving Manual stops new automatic decisions, not an already-transmitted packet:
  continue tracking its outcome; never claim remote cancellation without evidence.
- Unsupported presets show concise `Manual only` explanation. Changing to one
  stages Manual in the same settings save.
- Keep the switch tied to confirmed reported output, with separate Sending/error
  indication. Enabling Auto or saving thresholds is NOT an output confirmation.

### B6. Failure and recovery: honest behavior, no fake safety guarantees

- Require a fresh link and a valid, in-range input for that rule. Relevant sensor/
  bus fault bits suspend it; unrelated sensor failure must not pause a healthy
  rule. Input validation follows wire ranges, not the editor threshold range.
- On invalid/missing input, stop NEW automatic decisions and reset qualification.
  Keep last commanded level; it is NOT a safe state guarantee. An already-sent
  command may still complete. Fresh relay readback and command outcomes remain
  visible even if the rule's sensor is unavailable.
- On disconnect, show unknown outputs and block manual/automatic output changes.
  Hub cannot guarantee OFF without a functioning link. Node watchdog/failsafe
  implementation remains out of scope, explicitly documented as a limitation.
- On reconnect, reconcile fresh reports before periodic TX. Do not blindly replay
  pre-disconnect Auto intent; invalidate abandoned command tracking so stale
  reports cannot revive it. Restart hold/qualification before automatic action.
- Automatic command timeout latches a paused-error state for that device.
  Do not create repeated automatic command sequences. Transport retransmission/
  late completion can still occur and must be represented honestly.
- `Retry` in paused Auto means re-arm against a fresh CURRENT report, resetting
  hold/counters; it must NOT blindly resend an obsolete ON target. Manual Retry
  retains its manual requested target.
- A late matching report may update actual output but must not silently clear
  the Auto paused-error latch. Editing limits alone does not clear that latch.
  Explicit Retry, or a deliberate Manual -> Auto re-enable, re-arms it.

### B7. UI feedback and implementation structure

Keep existing Home/Devices mode labels `Auto` / `Manual`; use `Auto paused`
for suspended control without overlapping Sending/Unknown/Retry status. Existing
command/error indications take priority in their own status area. Use current
modal typography, light surfaces and no card borders; no global design changes.

Factor a small testable control module if useful, without LVGL widget ownership.
Do not add a scheduler framework, event bus or duplicate production snapshot.
Manual, Auto and polarity changes share one command arbitration path. Add new
production modules to build/export manifests, keep tests simulator-only, and
measure the extra config/state and editor peak memory within the existing budget.

## C. Required verification and handoff evidence

First add failing regression reproductions for A1-A6. Diagnostic material from the
independent review is under `.codex-tmp/review-20260914/` if available, but permanent
tests must live in maintained test sources, not depend on that ignored directory.

Required tests in addition to preserving the existing 19 checks and 13 scenarios:

1. Restore-before-init and the documented boot sequence, missing/invalid config,
   both polarities, all 16 reported GPIO masks; first/periodic TX readiness;
   operating one channel never changes another absent its legitimate request.
2. Reconnect/remote flip/pending command interactions, concurrent channels,
   sequence wrap, UI tick wrap, late reports and polarity changes.
3. Y-axis pixel correctness before/after in-place range change; grow/shrink,
   negative temperature and metric switch; full redraw should not reveal stale
   labels. Use pixel evidence, not only `lv_label_get_text()` assertions.
4. Zero flush/pixels for 10 identical frames AND identical history refeeds on Home,
   Trends and Devices with no uptime boundary/animation/active command. Isolate
   permitted header updates in a separate full-second check. Verify real updates
   still render and live-only changes do not repaint the plot.
5. Extreme and invalid comfort values never show false `Ideal` claims.
6. Clean production compile/link with the documented font config, no test hooks;
   archive/manifest includes every new production dependency.
7. Auto: each rule at exact ON/OFF boundaries, deadband oscillation,
   3-frame qualification, duplicate revisions, 10-second hold, tick wrap, sensor
   validity/range/faults, unsupported presets and restored mode validation.
8. Threshold editor tests: all +/- bounds, long-press/release, no wraparound,
   pair ordering/gap, invalid Save, Reset limits, Back retaining draft, X/cancel
   discarding all changes, Save committing once from either subview, distinct
   values for two devices of the same preset, supported/unsupported preset change,
   reopen/restore/schema migration, no action or callback during draft edits,
   committed limits controlling actual wire outputs (not still using defaults),
   and edits while telemetry/Auto/pending commands continue underneath.
9. Auto behavior while on every tab, in a modal and during splash; manual override,
   pending-gesture refusal, mode changes, independent devices, timeout pause,
   changed-condition Retry, late reports, disconnection/reconnect and no stale
   command replay. Test transmitted bytes and Fake Node outputs, not labels alone.
10. At least 50 navigation/modal cycles, plus sustained Auto transitions;
   report actual heap free/largest-block/fragmentation/drift and flush counts.
   Preserve >4 KiB free in tested peak states under the current profile, and do not
   hide new persistent allocations behind a higher heap budget.
11. Inspect native 800x480 screenshots of normal, missing data, command states,
    changed Y-axis scales and each Auto UI state for clipping/overlap.

Run `tools/build_sim.bat`, `tools/run_regression.bat`,
`tools/export_mcu.bat --verify`, plus the new production compile/link smoke check.
No deleted/weakened assertions or unconditional passing fixtures. Existing tests
whose old expectation was explicitly changed by specified Auto behavior must be
updated with a documented replacement assertion, not simply removed.

Follow `docs/prompts/README.md` if producing any additional handoff: no new prompt
files outside that directory and no second active prompt. Put implementation
reports/screenshots under `docs/reviews/`, not among prompt files.

Update `PLAN.md`, `DEV_LOG.md`, user integration guide, MCU baseline, relevant
design decisions and protocol integration examples. Preserve historical logs as
history, but clearly supersede obsolete current instructions. Record exact code
commands, exit status, measurements and screenshot paths in a review report.

Finish with a matrix mapping A1-A6 and each Auto requirement to changed
files, test names and evidence; list remaining target-only limitations. Do not say
"MCU verified", "all safe" or "complete" based solely on simulator/manifest PASS.
