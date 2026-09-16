# Smart Hub — corrective implementation + Auto v1 handoff

> ARCHIVED — historical context only, not the current implementation request.
> Original location: `docs/FIX_AND_AUTO_HANDOFF.md`.
> Use [CURRENT.md](../CURRENT.md) for the active handoff and
> [prompt rules](../README.md) before creating or updating any prompt.

Date: 2026-09-14.

Status: The six corrective items below are requested by the owner. **Section B
(Auto v1) is a technical-lead proposal awaiting owner approval. Do not implement
B merely because this document exists.** Once the owner explicitly approves
Auto v1, implement A and B together and record that approval; otherwise A can
proceed independently. This is an implementation prompt, not a completion report.

---

You are the implementation engineer in:
`C:\Users\vanth\Documents\MyDocument\Embedded\LVGL_UI`.

Repair the current implementation; do not rebuild the UI or reproduce an older
design. Read the actual current source, `AGENTS.md`, `docs/README.md`,
`docs/HARDWARE_OVERVIEW.md`, `docs/MCU_BASELINE.md`, `docs/UI_DESIGN_BRIEF.md`,
`docs/user/SMART_HUB_UI_GUIDE.md`, `LORA_PROTOCOL.md`, and this complete prompt.
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
- If Auto v1 is approved, the owner request is a narrow expansion of the old
  AGENTS.md exclusion of automation. Update that boundary explicitly: only the
  three fixed Hub-side demo rules below, not a general automation engine.

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

## B. Proposed Auto v1 — IMPLEMENT ONLY AFTER OWNER APPROVAL

### B1. Deliberately small product scope

Auto is a Hub-local per-device rule, evaluated against fresh received telemetry,
independent of the active tab, splash or settings dialog. Only these presets are
supported in v1:

| Preset | Input | Request ON | Request OFF |
|---|---|---|---|
| Ventilation Fan | displayed CO2/eCO2, ppm | >= 1000 | <= 800 |
| Air Purifier | received unitless VOC index | >= 150 | <= 100 |
| Humidifier | humidity, integer %RH | <= 40 | >= 50 |

Between thresholds, retain the reconciled state/current legitimate intent. These
are configurable-in-code demo defaults, NOT certified health, comfort or appliance
protection limits. Do not convert ppb to an invented Sensirion index. VOC input
must already be valid on the wire; its upstream derivation remains firmware scope.

Light, Plug, Generic, Heater and Dehumidifier remain Manual-only. Do not infer
rules from their names. Default all devices to Manual unless valid restored config
explicitly enables Auto on a supported preset. No schedule, scenes, global mode,
LCD threshold editor, compressor control or heater automation in this release.
Demonstrate with appropriate low-voltage indicator/demo loads, not a claim of
safe unattended appliance control.

### B2. Timing and noise rejection

- Evaluate on new received revisions, not every UI tick as if it were a new sample.
- An ON or OFF threshold must hold for 3 consecutive eligible fresh frames.
  A nonqualifying/invalid frame, lost link or changed rule clears that counter.
  Frames in the deadband do not count toward either transition.
- A new automatic transition also requires a 10-second minimum hold since the
  previous confirmed output transition. On enabling Auto, startup reconciliation,
  reconnection or recovery from sensor failure, start a new 10-second hold.
- No threshold transition fires merely because a timer elapsed without another
  eligible sample. Use wrap-safe integer timestamps and bounded fixed storage.
- A pending output command cannot be replaced/reversed by Auto. Keep the current
  desired/reported distinction and wait for confirmation or explicit error flow.
- Issue a command only when the requested logical output actually differs; do not
  increment seq or flood announcements on stable readings. Track confirmations
  and reset hysteresis/counters consistently after a change.
- External unrequested output changes while Auto is settled are adopted first;
  restart the hold/counters, then resume the same rule. Do not immediately fight
  the reported change on the same frame.

### B3. Manual interaction has clear ownership

- An accepted user switch gesture while Auto is active atomically commits Manual
  for that device and requests the opposite of its last fresh reported output.
  No timed automatic return to Auto. Notify local config change once.
- Reject switch gestures when disconnected or command-pending as before; a
  rejected gesture must not silently change mode. The user can deliberately
  re-enable Auto in settings after the pending operation settles.
- Entering Auto preserves current physical output initially and starts the
  stability/hold logic. Saving unchanged settings must not restart it.
- Saving Manual stops subsequent automatic decisions; it does not invent an
  output toggle. A command already transmitted can still complete and must remain
  tracked. Do not falsely claim it was cancelled remotely.
- Unsupported presets disable the Auto option with concise `Manual only` copy.
  Changing from a supported preset to an unsupported one commits Manual in the
  same save. Preserve current desired physical semantics/polarity consistency.

### B4. Failure and recovery behavior — no fake safety guarantees

- Require a fresh link and valid, in-range input for the selected rule. Relevant
  sensor/bus fault bits also suspend that rule. A failure of humidity must not
  suspend a valid CO2 rule unless the common bus fault actually applies to both.
- On missing/invalid input, stop NEW automatic output decisions and reset the
  qualification state. Keep the last commanded level; do not label it a safe
  state. Previously transmitted commands may still complete. Continue reporting
  actual fresh relay readback and command outcomes when available.
- On disconnection, display unknown outputs and block user/automatic changes.
  The Hub cannot guarantee OFF without a functioning link. Do not implement or
  claim a Node-side failsafe in this UI change.
- On reconnect, reconcile fresh reports before permitting periodic TX; do not
  blindly replay a pre-disconnect Auto intent. Invalidate abandoned tracking so a
  late stale report cannot revive it. Reset Auto hold/qualification before acting.
- On automatic command timeout, latch Auto in a paused-error state for that
  device. Do not issue repeated new automatic sequences. Existing transport
  retransmission/late completion is possible and must be represented honestly.
- `Retry` in paused Auto mode means re-arm the rule from a fresh current report,
  resetting hold/counters; it must NOT blindly resend an obsolete ON demand after
  readings changed. Existing Manual retry retains its manual requested target.
  Switching to Manual remains available through settings when appropriate.
- Do not silently clear the paused-error latch merely because a late report
  matches the old target; update reported output but await explicit re-arm.

### B5. Minimal UI changes and implementation structure

- Keep Home/Devices mode text `Auto` / `Manual`. Show `Auto paused` for suspended
  automatic control, using existing mode/status areas without overlap. Existing
  Sending/Unknown/Retry status takes precedence in its own status area.
- Settings may show one short rule explanation for a supported preset, e.g.
  `On >=1000 / Off <=800 ppm`, with supported font glyphs. Do not reduce fonts,
  add a page, exceed the current modal bounds, or bury Save below the screen.
- Keep switch position tied to confirmed reported hardware; changing to Auto is
  not evidence that an output switched. Show command progress separately.
- Factor a small testable control module if useful, with no LVGL widget ownership.
  Do not add a general scheduler/event framework or duplicate the wire snapshot.
  Manual/Auto/config polarity actions must share one command arbitration path.
- Update export/build manifests for any new production module and keep all test
  fixture/hook code simulator-only. Flash persistence remains a callback contract.

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
7. If Auto approved: each rule at exact ON/OFF boundaries, deadband oscillation,
   3-frame qualification, duplicate revisions, 10-second hold, tick wrap, sensor
   validity/range/faults, unsupported presets and restored mode validation.
8. Auto behavior while on every tab, in a modal and during splash; manual override,
   pending-gesture refusal, mode changes, independent devices, timeout pause,
   changed-condition Retry, late reports, disconnection/reconnect and no stale
   command replay. Test transmitted bytes and Fake Node outputs, not labels alone.
9. At least 50 navigation/modal cycles, plus sustained Auto transitions if enabled;
   report actual heap free/largest-block/fragmentation/drift and flush counts.
   Preserve >4 KiB free in tested peak states under the current profile, and do not
   hide new persistent allocations behind a higher heap budget.
10. Inspect native 800x480 screenshots of normal, missing data, command states,
    changed Y-axis scales and each approved Auto UI state for clipping/overlap.

Run `tools/build_sim.bat`, `tools/run_regression.bat`,
`tools/export_mcu.bat --verify`, plus the new production compile/link smoke check.
No deleted/weakened assertions or unconditional passing fixtures. Existing tests
whose old expectation was explicitly changed by approved Auto behavior must be
updated with a documented replacement assertion, not simply removed.

Update `PLAN.md`, `DEV_LOG.md`, user integration guide, MCU baseline, relevant
design decisions and protocol integration examples. Preserve historical logs as
history, but clearly supersede obsolete current instructions. Record exact code
commands, exit status, measurements and screenshot paths in a review report.

Finish with a matrix mapping A1-A6 and each approved Auto requirement to changed
files, test names and evidence; list remaining target-only limitations. Do not say
"MCU verified", "all safe" or "complete" based solely on simulator/manifest PASS.
