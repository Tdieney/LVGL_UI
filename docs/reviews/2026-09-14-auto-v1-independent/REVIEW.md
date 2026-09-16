# Independent review — Auto v1 implementation

Date: 2026-09-14. Verdict: **CHANGES REQUIRED — not accepted**.

Reviewed the owner's pasted implementation transcript, current production C,
simulator/test code, integration documentation, supplied native screenshots and
`docs/prompts/CURRENT.md`. Completion claims in the implementation author's report
are evidence to verify, not a sign-off. No production code or prompt was changed
by this review. The diagnostic sources here are host-only, not MCU export inputs.

## What actually passed

- `tools/build_sim.bat`: exit 0, existing dependency cache reused.
- `tools/run_regression.bat -SkipBuild`: exit 0; 24/24 checks; 15 scenarios.
- `tools/export_mcu.bat --verify`: exit 0; manifest check only.
- `sim_pc/build/smoke_production.exe`: exit 0 in Release, but assertions were
  compiled out; see R9 before treating this as behavioral verification.
- Independent boot/config test: pre-init active_low restoration survives init,
  TX initially false; all 16 reported masks reconcile without a mask mismatch.
- Independent chart test: Y top becomes `1180`, label width 34 px; forced redraw
  changes zero Y-axis pixels. The previous stale-axis defect is fixed.
- Ten identical Trends frames plus identical history refeeds: zero flush/pixels.
- Valid temperature/humidity readings no longer show the false Ideal captions.
- Existing regression measured -8 bytes heap drift after 50 navigation cycles;
  tested dialog state had 5,504 bytes free, largest block 4,544 bytes. These are
  simulator observations, not MCU memory-map or physical LCD validation.

## Findings, in priority order

### R1 [P1] Boot/default/legacy config enables Auto without valid explicit opt-in

Sources: [ui_auto.c](../../../ui_auto.c#L175),
[legacy migration](../../../ui_auto.c#L215),
[config fallback](../../../ui.c#L2375).

The handoff requires Manual by default and Manual when threshold config is
missing/invalid. `ui_auto_get_defaults()` instead returns Auto for all supported
presets. Invalid config falls back through that same function. Migration preserves
legacy Auto even though the old Auto value was only a label, with no thresholds.
The full-config validator also accepts unknown schema versions.

Independent reproduction:

```text
Default modes (0=Auto,1=Manual): 0 0 0 1
No user action: commanded GPIO mask=7 (initial readback=0)
Invalid stored thresholds fallback mode=0 (expected Manual=1)
Unknown schema accepted=1
```

The mask result is a request to energize three outputs in the level-triggered wire
contract, without a user enabling Auto. Restore/validation must distinguish a
valid explicitly enabled current-schema rule from missing/legacy/malformed data.
CHECK 20 currently asserts preservation of legacy Auto, encoding the wrong
requirement into a passing test. Correct production behavior and that assertion.

### R2 [P1] Firmware main-loop example bypasses the new TX gate

Source: [SMART_HUB_UI_GUIDE.md](../../user/SMART_HUB_UI_GUIDE.md#L123).

The copyable main loop calls `lora_send()` periodically, unconditionally, BEFORE
`ui_tick()`. It does not test `ui_is_tx_ready()`. Later prose says to use the gate,
but the executable example contradicts it. Following this example transmits the
zero/unreconciled or pre-reconnect command mask, defeating the A1/A2 protection,
especially for active_low loads.

Update the actual integration example to RX -> reconcile/control -> gated TX.
Exercise periodic TX before the first report and on the first reconnect iteration;
an API existing in a header is not evidence that the integrator uses it.

### R3 [P1] Public config API bypasses control-state rules enforced by dialog Save

Source: [ui_set_device_config](../../../ui.c#L2375).

After initialization this setter still assigns config and redraws only. It neither
restarts hold/qualification on enabling Auto/new limits, nor rejects pending
polarity changes, nor uses the dialog's command arbitration/callback contract.

Independent reproduction after 20 seconds in Manual:

```text
Public API enabled Auto 300 ms ago; command phase=1 gpio=1
Public API changed polarity during pending: active_low=1 phase=1
```

The requested 10-second hold is bypassed. Pending desired/reported interpretation
can change underneath an already-transmitted GPIO command. Separate boot restore
from validated runtime commits, or explicitly reject unsupported runtime calls;
do not expose a silent second semantics for the same config fields.

### R4 [P1] Reconnect clears a latched Auto timeout without explicit re-arm

Source: [startup/reconnect branch](../../../ui.c#L2633).

Every reconnect calls `ui_auto_rearm()` for every device. That clears paused_error,
although the handoff requires explicit Retry or deliberate Manual -> Auto. A
temporary radio outage becomes an unintended way to resume a failed controller.

```text
Before disconnect: phase=2 paused_error=1
Reconnect without Retry: phase=0 paused_error=0
```

Reconcile fresh output levels/reset qualification on reconnect without silently
clearing a previously latched error. Add error -> disconnect -> reconnect tests,
not just separate tests of error and link recovery.

### R5 [P2] Sensor recovery skips the new hold and suspension is invisible

Sources: [ui_auto_eval](../../../ui_auto.c#L307),
[mode label](../../../ui.c#L989).

Invalid input resets the qualification counter but tracks no recovery transition;
the old hold timestamp survives. Three recovered frames can issue a command only
3 seconds after recovery instead of restarting the 10-second hold. Mode text is
based only on paused_error, so invalid-input suspension still displays plain Auto.

```text
Sensor invalid: displayed mode=Auto
Sensor recovered 3000 ms ago: phase=1 gpio=1
```

Track transient sensor suspension separately from the latched command error,
show Auto paused, and restart hold/qualification on recovery without latching an
ordinary sensor fault forever or resetting hold on every invalid tick.

### R6 [P2] Visible X cannot be tapped to cancel either modal subview

Sources: [full-size Device subview](../../../ui.c#L1908),
[Auto subview](../../../ui.c#L2028),
[X creation order](../../../ui.c#L2190).

The X is created before a full-size clickable subview. That transparent later
sibling covers its hit area. It remains visually visible but does not receive
pointer clicks. Real simulated pointer taps at (578,98) leave both subviews open.
Directly invoking a close callback does not test this.

Fix hit testing/stacking and add actual pointer-driven X/cancel tests from both
subviews, including invalid drafts. The user must not be forced to Save to exit.

### R7 [P2] Switching to Set limits discards the unsaved active_low change

Sources: [polarity switch](../../../ui.c#L1987),
[subview destruction](../../../ui.c#L2010).

The switch has no callback updating the shared draft. Save reads its widget state
only when that widget still exists. Set limits deletes the widget and nulls the
pointer before copying its state. The test helper writes both state and widget,
masking this bug in the supplied tests.

Pointer-driven reproduction:

```text
Physical polarity tap: widget=1 draft=0
After Set limits: auto view=1 draft polarity=0
After Save from Auto view: committed polarity=0
```

Synchronize all edited controls to one draft, preserve it across subview changes,
and verify Save/Cancel with actual clicks rather than test-only field setters.

### R8 [P2] Editor limits and default thresholds differ from the handoff

Source: [ui_auto.h](../../../ui_auto.h#L26).

| Setting | CURRENT.md | Implementation |
|---|---|---|
| CO2 editor bounds / minimum gap | 0..10000 / 50 | 300..5000 / 100 |
| VOC editor bounds / minimum gap | 1..500 / 5 | 10..500 / 15 |
| Humidity editor bounds | 0..100 | 20..90 |
| Purifier default ON | 150 | 250 |
| Humidifier default OFF | 50 | 55 |

No owner approval for these changes is present in the supplied handoff. They
change which values the LCD accepts and when default rules actuate outputs.
Restore the specified table, or obtain an explicit product decision before
updating requirements. Tests must assert the contract, not only reuse the changed
implementation constants as their expected values.

### R9 [P2] A6 remains unresolved; the new smoke test can pass without its assertions

Sources: [MCU_BASELINE.md](../../MCU_BASELINE.md#L25),
[sim config](../../../sim_pc/lv_conf.h#L24),
[smoke test](../../../sim_pc/smoke_production.c#L63),
[build target](../../../sim_pc/CMakeLists.txt#L41).

The MCU document still disables Montserrat 14 without replacing LV_FONT_DEFAULT.
The simulator/smoke target uses a different config retaining that font. Applying
the documented disable directive still produces `lv_font_montserrat_14 undeclared`
(compiler exit 1). Glyph geometry/custom font existence is not a fix for A6.

Release flags contain `-DNDEBUG`, removing the smoke test's `assert()` checks.
Compiling the SAME smoke scenario with assertions enabled yields:

```text
Assertion failed: ui_is_tx_ready(), ... smoke_production.c, line 83
Smoke assertions enabled exit: 3
```

The fixture sets last_rx_tick=1000 while LVGL tick is still zero; no eligible
fresh report is reconciled. Fix the fixture and use checks that cannot disappear
in Release. Build/link the documented exported configuration cleanly; the current
target compiles repo sources against the simulator config/library, not the staged
export with independently verified config. Clearly separate manifest, host build,
runtime checks and eventual target verification.

### R10 [P2] Pending switch shows requested output, not reported output

Sources: [Home](../../../ui.c#L998), [Devices](../../../ui.c#L1516).

Both builders explicitly use desired_on while pending and assign it to the
switch. Independent observation: pending switch checked=1 while reported GPIO=0
on an active-high channel. The prompt requires switches to remain tied to
confirmed report and show progress separately; Sending alone does not make this
optimistic switch position conform. Preserve reported state until actual readback.

## Why 24/24 is insufficient here

- CHECK 20's boot check only queries readiness in an already-running settled UI;
  it does not itself exercise cold boot + early periodic TX. Its migration check
  explicitly expects legacy Auto, contrary to the safe-default requirement.
- Dialog helpers update internal draft state directly and invoke callbacks; they
  bypass the hit-test and unsynchronized switch paths that fail with pointer taps.
- Error recovery tests do not cover timeout followed by reconnect or sensor
  recovery after a long previous hold.
- Production smoke assertions vanish in Release. Its successful linking remains
  useful evidence, but its printed PASSED is not proof of the removed checks.
- The implementation report labels A6 as custom typography verification rather
  than the actual required MCU default-font/configuration fix. It also calls the
  history 24-point while UI_HISTORY_CAPACITY remains 16. Use source/test output,
  not that report's completion matrix, as the final acceptance evidence.

## Reproduce the independent checks

From repo root, after building the current simulator dependency:

```powershell
New-Item -ItemType Directory -Path .codex-tmp/review-auto-result -Force | Out-Null
& C:/Toolchains/w64devkit/bin/gcc.exe -O2 -std=gnu11 -DLV_CONF_INCLUDE_SIMPLE -DLV_LVGL_H_INCLUDE_SIMPLE -DUI_TEST_HOOKS -I. -Isim_pc -Isim_pc/build/_deps/lvgl-src docs/reviews/2026-09-14-auto-v1-independent/check.c sim_pc/ui_test_api.c ui_auto.c ui_theme.c ui_icons.c ui_fonts.c ui_splash_logo.c sim_pc/build/lib/liblvgl.a -lgdi32 -luser32 -o .codex-tmp/review-auto-result/check.exe
foreach ($case in @('defaults','boot','chart','close','polarity_draft','api_hold','recovery','error_reconnect')) { & .codex-tmp/review-auto-result/check.exe $case }

& C:/Toolchains/w64devkit/bin/gcc.exe -O2 -UNDEBUG -std=gnu11 -DLV_CONF_INCLUDE_SIMPLE -DLV_LVGL_H_INCLUDE_SIMPLE -I. -Isim_pc -Isim_pc/build/_deps/lvgl-src docs/reviews/2026-09-14-auto-v1-independent/smoke_asserts.c ui.c ui_auto.c ui_theme.c ui_icons.c ui_fonts.c ui_splash_logo.c sim_pc/build/lib/liblvgl.a -o .codex-tmp/review-auto-result/smoke-asserts.exe
& .codex-tmp/review-auto-result/smoke-asserts.exe

& C:/Toolchains/w64devkit/bin/gcc.exe -std=gnu11 -DLV_CONF_INCLUDE_SIMPLE -DLV_LVGL_H_INCLUDE_SIMPLE -Idocs/reviews/2026-09-14-auto-v1-independent/font-config -Isim_pc/build/_deps/lvgl-src -c docs/reviews/2026-09-14-auto-v1-independent/font-check.c -o .codex-tmp/review-auto-result/font-check.o
```

The first diagnostic reports observations, not acceptance assertions; interpret
its output against R1-R10. It deliberately drives frames/ticks deterministically
without letting the Fake Node overwrite test input. Modal cases navigate to
Devices and use real simulated pointer input. It has a 5-second watchdog.
The smoke-assert and font-config checks are expected to fail on this submission.

## Acceptance disposition

A1, A3, A4 and A5 have concrete positive evidence. A2 is partial: core staging/
reconciliation works, integration and runtime config semantics remain incomplete.
A6 fails. Auto v1 needs the findings above resolved before acceptance. Keep the
visual design; focus the next implementation on correctness, safe opt-in,
interaction tests and honest build evidence. No MCU/radio/load sign-off is given.
