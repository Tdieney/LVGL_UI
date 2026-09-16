# Independent review — W1–W4 implementation

Date: 2026-09-16. Verdict: **changes required; partial acceptance**.
Review-only: no production UI/controller/exporter changes, commit or push.
Author submission: [REPORT.md](../2026-09-15-auto-w1-w4-fixes/REPORT.md).
Active corrective handoff: [CURRENT.md](../../prompts/CURRENT.md).

## Accepted improvements

- Rebuilt simulator and independently ran tools/run_regression.bat: smoke,
  31/31 checks, native shot size and 24 generated scenario files all pass.
- Recompiled the review harness against current sources. Late ACK and reconnect
  both retain Auto paused plus Retry on Home and Devices. The original hidden-Retry
  defect is fixed. Do not re-report it as still present.
- Native submitted Home recovery and Auto editor/invalid editor captures were
  inspected. New Devices capture [devices.png](devices.png) confirms simultaneous
  switch/Retry. Preserve the current visual direction, cards, fonts and modal.
- Shared styles improve heap: previous 3896/4024-byte failures no longer reproduce
  in the original sequence. This is an improvement, not proof of every peak.
- CHECK26 now asserts drag actually moves the dropdown and selects first/last
  options through pointer input; CHECK27 registers its callback counter before
  edits/cancel and exercises real steppers. These previous gaps are repaired.
- TX records now actually exist; pre-init fixture really runs before ui_init;
  CO2 invalid/recovery is now exercised. Remaining proof gaps are listed below.
- Production verifier uses an isolated task-owned ZIP, eliminating the old
  root-directory filename-difference ownership mechanism.
- tools/verify_production.bat passed outside sandbox with approval: actual export
  membership, separately built cached LVGL, Release production smoke exit0,
  strict negative control exit1 and matching diagnostic.
  First sandbox attempt failed at CMake Configure watchdog/access denial;
  this is an environment limitation, not a production compile failure.
  Only its matching review-owned CMake process was stopped; failed staging
  retained for diagnostics. No unrelated process or user export was removed.

## X1 [P2] Export still overwrites an existing explicit destination

Source: tools/export_mcu.bat:31-47; tools/verify_production.ps1:95-131.

The new explicit output option passes an existing destination straight to
Compress-Archive -Force. A review-owned sentinel passed as that exact destination
was replaced, with exit0. Independent disposable fixture output:

```text
EXPLICIT existing destination: exit=0 sentinel_preserved=False
DEFAULT first: exit=0 names=smart_hub_ui_.zip
DEFAULT second: exit=0 original_preserved=True names=smart_hub_ui_.zip,smart_hub_ui__1.zip
```

There is also a default filename regression: TS is assigned inside a parenthesized
batch block, but %TS% in that same block is expanded before assignment. With no
inherited TS, names omit the date/time. Sequential suffix collision avoidance does
work in this fixture; do not claim it overwrites every sequential default export.
Concurrent existence checks plus -Force remain race-prone, and exhaustion of
suffixes falls through to the original candidate.

The verifier's sentinel has a DIFFERENT name from its export destination, and
the two exports are sequential with distinct explicit paths. Thus its green
collision-safety result does not test collision refusal or concurrency.
Task-owned verifier ZIP isolation itself is an accepted improvement.

Repro: [check-export.ps1](check-export.ps1), copied exporter/inputs in a new
.codex-tmp/export-review-<GUID> fixture. Only review-owned sentinel files were
overwritten. Two generated fixture directories are retained; no user ZIP touched.

Required correction: non-clobbering default and explicit output, valid timestamp,
atomic ownership/collision handling, actual same-destination/concurrent fixture
tests, preservation on failure/timeout and honest report descriptions.

## X2 [P2] Recovered-state switches still have 30px touch height

Source: ui.c:924-930 (Home), ui.c:1547-1553 (Devices);
settled-latched placement ui.c:1082-1094 and ui.c:1614-1623.

Both switches have visual size54x30 and no extended click area. Runtime
lv_obj_get_click_area reports:

```text
HOME switch effective touch 54 x 30
RECOVERY reconnect phase=0 latch=1 Home Retry visible=1
DEVICES switch effective touch 54 x 30
RECOVERY Devices Retry visible=1
```

Late ACK produces the same geometry. Existing Retry targets meet the height
requirement; it is the switches that do not. This violates the >=44px independent
touch targets requested for the new recovery state, especially on the small LCD.
Centre-coordinate tests alone cannot detect this.

Keep current visual bounds; extend effective switch targets safely without
intersection with Retry/settings/card actions. Add boundary pointer checks on both
pages/polarities and verify correct action, fresh readback and no double activation.
This is a targeted usability correction, not a broad redesign.

## X3 [P2] Boot, TX and recovery acceptance still overstates coverage

Sources: sim_pc/test_restore_integration.c:101-176,264-356;
sim_pc/main.c:393-445,459-465,3370-3595.

- Pre-init fixture verifies a loaded config but calls ui_test_reset_device_configs
  at line174 BEFORE ui_init. The following boot therefore uses defaults, not
  the restored Auto config. It cannot prove retained config/hold/adoption through
  real initialization. The old staging-hook test remains separate partial coverage.
- Truncation matrices use selected lengths, not every supported truncated length.
  Added schema/kind/preset/threshold cases are useful, but malformed names,
  active_low and invalid modes are still absent in this adapter test despite its
  comments/report. No CRC verification exists in this fixture; do not claim it.
- CHECK29 stores full bytes/timestamps but never asserts them. Periodic check only
  requires >=3 sends in3100ms; excessive sends or incorrect intervals can pass.
  Its recorder loop is an extra host stub while fake_node_tick also independently
  consumes the shared command. Thus it is not evidence that the node received
  only the recorded sends or that the documented integration loop is shared.
  The no-first-report check occurs in an already initialized long-running suite.
- CHECK30 invalidates/recovers CO2, but Purifier only sees unrelated CO2/Temp
  invalidity and Humidifier starts with valid RH. Neither relevant VOC nor RH
  invalid->valid recovery is tested. The fake node continues creating periodic
  fresh revisions; the comment "Duplicate frames" even accompanies explicit
  lora_rx_revision increments, so duplicate/time-only qualification is unproven.
- New CHECK28 pointer Retry is on Devices after late ACK for one channel/polarity.
  The second timeout setup lacks an assertion that it actually re-entered ERROR
  and latched before the Manual override tap. Original review probes establish
  visibility but not the full requested pointer/re-arm matrix.
- Author REPORT section3.1 is not a literal current test transcript: e.g. it labels
  CHECK1 splash, whereas actual executable labels CHECK1 uptime. Future evidence
  must capture real output, not reconstructed log-shaped summaries.

These are verification gaps, not demonstrated production boot/Auto algorithm bugs.
Finish the previously requested acceptance, retain repaired tests, and do not
rewrite working controller behavior simply to make testing easier.

## X4 [P2] Heap stress never reaches the claimed invalid draft

Source: sim_pc/main.c:3633-3684.

CHECK31 opens default Purifier (ON150/OFF100), increments OFF twice by5, then calls
that checkpoint "Validation error visible". The pair150/110 remains valid and
Save is enabled. The diagnostic reproduces its exact operations:

```text
CHECK31 equivalent draft: on=150 off=110 validation='' save_disabled=0
Actual invalid draft: on=150 off=150 validation='ON must exceed OFF by at least 5' save_disabled=1
```

A separate true-invalid sample passed memory reserve. Thus this is a false
coverage claim, NOT a newly demonstrated below4096 failure.

Measured after full suite in [check.c](check.c):

| State | Free bytes | Largest block | Fragmentation |
|---|---:|---:|---:|
| Devices | 11136 | 9248 | 17% |
| Settings immediately built / first rendered | 4104 | 3912 | 5% |
| Settings another40ms later | 4936 | 3912 | 21% |
| Dropdown before tick / rendered | 4544 | 3912 | 14% |
| Auto limits before tick / rendered | 4256 | 4256 | 0% |
| True invalid limits | 4256 | 4256 | 0% |

The maintained stress reports4296 minimum, not the lowest independent sample4104.
The latter passes >4096 by8 bytes; largest block need not exceed4096 under the
existing contract. No leak/OOM was observed, and no higher reserve target is imposed.

Assert invalid preconditions/disabled Save before recording that state; retain all
state sampling, capture immediate/first-rendered peaks, stable drift and frag
separately, include requested Auto/recovery activity. Report measured scope honestly.

## Reproduction and limitations

From repo root (Windows):
```powershell
tools/build_sim.bat
tools/run_regression.bat
tools/verify_production.bat
C:/Toolchains/w64devkit/bin/gcc.exe -O2 -std=gnu11 -DLV_CONF_INCLUDE_SIMPLE -DLV_LVGL_H_INCLUDE_SIMPLE -DUI_TEST_HOOKS -I. -Isim_pc -Isim_pc/build/_deps/lvgl-src docs/reviews/2026-09-16-auto-w1-w4-independent/check.c sim_pc/ui_test_api.c sim_pc/test_restore_integration.c ui_auto.c ui_theme.c ui_icons.c ui_fonts.c ui_splash_logo.c sim_pc/build/lib/liblvgl.a -lgdi32 -luser32 -o .codex-tmp/w1w4-review.exe
.codex-tmp/w1w4-review.exe reconnect
.codex-tmp/w1w4-review.exe late_ack
.codex-tmp/w1w4-review.exe heap_after_suite
powershell -NoProfile -ExecutionPolicy Bypass -File docs/reviews/2026-09-16-auto-w1-w4-independent/check-export.ps1
```

check.c is a new host-only adaptation of the previous independent harness, writes
only this round's captures, and has a15s watchdog. Exit0 covers the original
Retry/heap assertions; printed touch/draft diagnostics expose X2/X4 rather than
being counted as passed acceptance tests. check-export.ps1 is a diagnostic, not
a green acceptance assertion: inspect sentinel_preserved=False.

No physical LCD/touch/MCU/radio/relay verification. No new sensor/health claims.
Historical reports are retained, but their all-complete language is superseded
by this independent disposition. See CURRENT for the bounded corrective handoff.

