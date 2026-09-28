# Independent X1–X4 result review

Date: 2026-09-16. Verdict: **changes required**.
Author report: [submission](../2026-09-16-auto-x1-x4-fixes/REPORT.md).
Corrective handoff: [CURRENT](../../prompts/CURRENT.md).
Review-only: documentation/host diagnostics changed, no production fixes/commit/push.

## Accepted and independently checked

- tools/run_regression.bat: build, smoke,31/31 checks, shot size and24 scenario
  files pass. Existing CMake CMP0177 developer warning is not a build failure.
- Approved outside-sandbox tools/verify_production.bat: actual exported16-file
  membership, separately built LVGL, production smoke exit0 and strict negative
  control exit1 with expected message all pass. No sandbox retry was attempted
  this round because previous configure attempts were blocked.
- Recompiled this round's [check.c](check.c): reconnect and late ACK retain Retry,
  Home/Devices switches now have effective68x44 click areas.
  [Devices capture](devices.png) visually inspected: settled-latched switch is
  separated from Retry. Author report mistakenly lists ordinary switch x281 as
  if it were recovered-state placement; actual recovered placement is x214.
  Do NOT infer an overlap bug from that incorrect report coordinate.
- Independent heap sequence passes: Settings4640 free before tick/first render,
  settled5472, dropdown5024, Auto limits/real invalid4528. Existing CHECK31 passes
  with true invalid draft,4136 minimum and-48-byte stable drift.
  Historical probe's150/110 message describes its OLD operation, not the corrected
  current CHECK31. Do not re-report that old invalid-draft issue.
- Current pre-init config really survives ui_init; malformed-name/mode/polarity
  and all truncated lengths were added. Relevant VOC/RH recovery tests now exist.
- Simulator node now consumes recorded send packets; periodic boundary/packet
  checks improved. Accepted progress is not erased by remaining evidence gaps.
- Primary explicit ZIP collision correctly refuses exit1 and preserves sentinel.
  Unset-TS default filenames now include date/time; sequential suffixing preserves
  the first ZIP. Primary publication uses non-replacing File.Move.

## Y1 [P1] New rollover regression disables stale-link protection and Auto hold expiry

Sources: ui.c:113-117; ui_auto.c:393-402;
sim_pc/ui_test_api.c:477-481 repeats the faulty comparison in its test getter.

Both production changes replace modulo uint32 subtraction with
now >= previous ? now - previous : 0. A wrapping millisecond clock normally becomes
smaller than a recent pre-wrap timestamp (~49.7 days). That is not a zero elapsed
interval. Consequently:
- If reports stop before rollover, after rollover stale telemetry remains classified
  connected until a fresh report or the counter catches the old value, potentially
  nearly another wrap period. UI link masking/refusal/TX reconciliation uses this
  predicate, undermining the existing5s stale-link contract.
- A nonzero pre-wrap Auto hold timestamp makes elapsed stay0 after rollover:
  Auto cannot qualify despite hold having expired and fresh valid inputs.
  Explicit re-arm/recovery can restart the timestamp; it is not irreversible.

Independent production-function probe output:
```text
WRAP link: last=4294966295 now=5000 actual_age=6001 connected=1 expected=0
WRAP auto: start=4294962295 elapsed=11201 fired_after_3_frames=0 expected=1
FAIL: !link && fired
```
The wrap case exits1 when run directly. It invokes actual is_link_connected,
ui_auto_restart_hold and ui_auto_eval, not a duplicate elapsed helper.
The initial diagnostic only initialized Auto state (which intentionally leaves
last_transition_ms0); final source correctly starts a real hold before testing.

Restore wrap-safe timing without weakening boot-at-zero hold semantics. A future
1ms sentinel used during tick0 needs deliberate treatment; do not hide broken
test clocks by changing production wrap arithmetic. CHECK1 currently tests a
standalone subtraction, not these production predicates, so31/31 does not catch it.

## Y2 [P2] Export protection is bypassed by the MCU destination copy

Source: tools/export_mcu.bat:103-106, particularly copy /y at105.

Even with a new protected primary ZIP path, the exporter copies it over a same-name
ZIP already in MCU_DEST. The primary path need not collide. Disposable reproduction:
```text
EXPLICIT existing destination: exit=1 sentinel_preserved=True
SECONDARY destination ZIP: exit=0 sentinel_preserved=False
```
The second line starts with a new fixture/secondary.zip and a review-owned sentinel
fixture/dest/secondary.zip. Export succeeds and destroys the destination sentinel.
No owner ZIP was used. New [check-export.ps1](check-export.ps1) reproduces this.

Related incomplete X1 items reproduced:
- An inherited unrelated TS is accepted verbatim:
  INHERITED TS: exit=0 invalid_name_exists=True
  with smart_hub_ui_UNRELATED_VALUE.zip. Generate the timestamp independently or
  validate any explicitly supported override; ambient TS must not corrupt naming.
- Two simultaneous default exports in this run returned0 and1 respectively;
  loser output was ERROR: temporary directory collision. Random batch staging
  names collided. This is a safe rejection, not proven corruption, but does not
  meet the requested distinct-artifact success for concurrent normal exports.
  Use robust per-invocation staging ownership; retain non-replacing publication.

Only review-owned fixture sentinel files were overwritten. Copied fixtures remain
under .codex-tmp/export-review-<GUID>; no cleanup of unrelated files occurred.
The author report's directory-refusal code snippets do not match the real exporter:
MCU_DEST is still reused and extraction uses -Force. Do not claim directory refusal.

## Y3 [P2] Some requested verification is still missing; report overstates completion

These are remaining proof/documentation issues, not new production Auto defects:

- Boot fixture now verifies restored config across real init but still checks
  outbound readiness/mask rather than recording the first send through the TX loop.
- The four CHECK29 negative cases mutate records or pass false to validate_tx_log.
  They test the log validator, not fault-injected execution of the integration
  loop. A gates-bypassed sender or omitted periodic send must be caught in an
  actual executed loop scenario, including initial boot.
- CHECK28 still tests one configuration and selected page/actions, not the full
  Home/Devices x polarity x reconnect/late-ACK pointer matrix. Its second supposed
  timeout lacks an asserted error/latch precondition before manual override.
- CHECK31 now uses real invalid values but still uses direct threshold helpers,
  checks invalid preconditions only on cycle1, and performs no Auto/recovery
  activity inside50cycles. These specified paths should execute or remain OPEN.
- Export verifier tests sequential default exports, not concurrent contention or
  failure/timeout preservation. Its collision fixture is unconditionally removed
  in finally, losing diagnostics on failure.
- Submission claims corrupted CRC testing, but adapter contains no CRC validation.
  Report includes nonexistent exporter code and incorrect Devices placement;
  no new screenshots/raw logs were submitted in its review folder (REPORT.md only).
  Re-running an older reviewer probe is useful but not independent acceptance,
  and overwrites its hardcoded old captures. New runs must use new artifact paths.

Do not expand these into an unrelated test framework or redesign. Complete the
specific previously requested checks; generate literal logs/new captures, and
label any missing cases OPEN instead of ALL COMPLETE.

## Reproduction

From repository root:
```powershell
tools/run_regression.bat
tools/verify_production.bat
C:/Toolchains/w64devkit/bin/gcc.exe -O2 -std=gnu11 -DLV_CONF_INCLUDE_SIMPLE -DLV_LVGL_H_INCLUDE_SIMPLE -DUI_TEST_HOOKS -I. -Isim_pc -Isim_pc/build/_deps/lvgl-src docs/reviews/2026-09-16-auto-x1-x4-independent/check.c sim_pc/ui_test_api.c sim_pc/test_restore_integration.c ui_auto.c ui_theme.c ui_icons.c ui_fonts.c ui_splash_logo.c sim_pc/build/lib/liblvgl.a -lgdi32 -luser32 -o .codex-tmp/x1x4-review.exe
.codex-tmp/x1x4-review.exe wrap
.codex-tmp/x1x4-review.exe late_ack
.codex-tmp/x1x4-review.exe reconnect
.codex-tmp/x1x4-review.exe heap_after_suite
powershell -NoProfile -ExecutionPolicy Bypass -File docs/reviews/2026-09-16-auto-x1-x4-independent/check-export.ps1
```
check.c has a15s watchdog and writes only this round's captures.
Export diagnostics print outcomes; process exit0 does not mean all predicates pass.
Concurrency collision is schedule-dependent; sentinel overwrite and timestamp
inheritance are deterministic in the recorded fixture setup.
Host checks do not establish physical MCU/LCD/touch/radio/load acceptance.

