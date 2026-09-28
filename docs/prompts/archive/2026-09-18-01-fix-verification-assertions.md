ARCHIVED — historical context only

Superseded on 2026-09-18 by [CURRENT](../CURRENT.md). Visual freeze clauses are historical;
bounded S1–S3 proof gaps are carried into the consolidated fidelity handoff.

# CURRENT — close remaining verification assertions; preserve MCU integration

Date: 2026-09-17. Status: **READY FOR HANDOFF**.
Owner requests review plus MCU integration matching motorCmd. The latter is
already implemented and independently host-link-tested; do not reverse it.
This ONE handoff contains all known remaining S1–S3 issues. No staged deliveries.

## Prerequisites and scope

Read AGENTS.md, docs/prompts/README.md, this CURRENT,
[latest independent review](../../reviews/2026-09-17-r1-r4-independent/REVIEW.md),
docs/README.md, docs/HARDWARE_OVERVIEW.md, docs/MCU_BASELINE.md,
docs/UI_DESIGN_BRIEF.md, docs/user/SMART_HUB_UI_GUIDE.md,
docs/user/LORA_GUIDE.md and LORA_PROTOCOL.md. Inspect the full current diff and
affected tests before editing. Historical prompts/reports are not active requirements.

Implement the bounded test/verifier/report corrections below together. Self-review
the full resulting diff; fix introduced regressions and run final checks in the
same delivery. Do not ask the owner to relay a separate prompt per item.
Preserve dirty owner work. No commit/push, branch changes, reset/clean/stash,
new CRC/sensor/radio/Flash drivers, packet changes, redesign or speculative refactor.

## Closed work / MCU integration contract

- Current regression31/31 and isolated exported-production verification pass.
  Auto stress really exercises five lifecycle sequences; both pointer actions
  run across the matrix; all six Auto wrap directions are checked.
- UI.c is the ONE storage owner for lora_hub_cmd, lora_node_status,
  lora_last_rx_tick_ms, lora_rx_revision, lora_last_snr and lora_last_rssi.
  Headers declare extern; firmware/simulator must not redefine them.
  This matches motorCmd in ui/motor-control:screens.c. Do not use weak symbols,
  header definitions or duplicate fallback globals.
- Keep plain #include "lvgl.h" in exported UI and asset generators. Firmware
  adds the directory containing lvgl.h and its target lv_conf.h configuration.
- Independent C99 host build WITHOUT LV_LVGL_H_INCLUDE_SIMPLE/UI_TEST_HOOKS
  passed; nm found each global exactly once. Owner's actual ARM project has
  NOT been built here. Retain a smoke/build check that does not reintroduce
  application-side definitions or rely on the removed include macro.
- Header comments, MCU_BASELINE (included in export) and SMART_HUB_UI_GUIDE
  were corrected by the reviewer. Keep this migration guidance: remove old
  application definitions, compile UI sources once, retain RX/TX publication
  duties and ui_is_tx_ready gating.
- Preserve accepted light borderless UI, sizes, fonts, Auto policy, heap budget,
  modular timing and non-overwriting ZIP publication. No new runtime bug was
  established by this review; alter production behavior only for a reproduced
  regression, never to satisfy an invalid fixture.

## S1 — Make artifact integrity and timeout tests prove their stated guarantees

In tools/verify_production.ps1:
1. Replace length-only 'identity' with streamed byte comparison or SHA-256 of
   each required entry against the corresponding fixture source. Verify exact
   entry names/membership and content for BOTH default concurrent ZIPs and the
   explicit-race winner, including required docs. Validate returned paths.
   Add a bounded negative control: same-length modified entry must be rejected.
2. Keep atomic primary/secondary non-overwrite tests and explicit race:
   one winner, one collision loser, complete unchanged winning artifact.
3. Test actual timeout containment, not only matching an exception message.
   Exercise both relevant worker helpers, including a concurrent worker with
   a known child process. Record task-owned PIDs/handles and assert all owned
   processes terminated within a bound; check unrelated sentinel hashes.
4. Treat failed taskkill/failed bounded waits as cleanup failure, not silent
   success. Suppress successful WaitForExit return values from result streams.
   Preserve per-worker stdout/stderr/exit/timeout diagnostics on failure.
   Keep asynchronous output draining and bounded waits.
5. Use disposable fixtures only; validate canonical task-owned cleanup paths.
   Preserve unrelated files and failure diagnostics. Do not test on owner ZIPs.
   Do not rewrite the working exporter architecture or change intended extraction.

## S2 — Complete named assertions in existing tests, not a new test project

- CHECK1: place the real LVGL clock/last RX before UINT32_MAX and cross it through
  ui_tick/integration TX. Assert exact4999/5000ms, disconnected UI masking,
  refused user action/Auto and zero stale TX, then valid recovery/readback.
  The existing ordinary-time flow and standalone predicate tests stay useful;
  add the missing combined path rather than claiming they already cover it.
  Use valid time progression/fresh-revision fixtures; never patch production
  elapsed arithmetic to accommodate a bad test clock.
- CHECK28: before the second (Retry) recovery branch, assert a genuine PENDING
  command, then timeout/latch, before late ACK/reconnect. Keep both actions on
  both pages/polarities/recovery types, edge/centre behavior and exactly-once
  checks. No layout/touch-target changes.
- CHECK31: count actual command/seq/TX/readback transitions, not mere IDLE phase.
  Assert all existing five scheduled pending/timeout/latch/recovery/re-arm
  outcomes and both directions; a skipped scheduled lifecycle must fail.
  Keep50 cycles, invalid-draft pointers/discard, immediate/stable heap sampling,
  >4096 total free and existing drift criterion. Largest-block/fragmentation
  remain diagnostic, not new pass thresholds. Current5344/4496/-56 results
  are accepted observations, not fixed expected numbers.
- Keep restore, packet fault injection, boot0, revision/hold boundaries,
  isolation and all existing passing checks. No extra product features.

## S3 — Correct evidence and document the actual result

Save one final report and raw logs under
docs/reviews/2026-09-17-assertion-completion/. Include exact command, exit status,
stdout/stderr, case counts, named results and evidence paths. Use relative links.
Supersede the previous completion report; never overwrite historical artifacts.

Explicitly correct: VOC is unitless (not ppm/ppb); config has no implemented CRC
guarantee; use the real export manifest/font files; the race is for one explicit
ZIP path, not a --dest directory; no invented heap/fragmentation criteria or
mathematical proof claims. Do not add CRC or alter firmware to justify prose.

Report source-identity rejection and actual timeout/reaping outcomes separately
from ordinary export PASS. Distinguish confirmed defects, proof gaps and target
limitations. Mark genuine skipped checks OPEN with reasons, never ALL COMPLETE.
Update PLAN, DEV_LOG and indexes. CURRENT stays the spec; change only its status
to IMPLEMENTED — awaiting review when the complete scope has actually run.

## Frozen product/data contract

LVGL8.4,800x480 RGB565, one800x10 partial draw buffer,42KiB heap; no MCU framebuffer.
Static lifetime captions/lv_label_set_text_static, fixed dynamic text buffers,
bounded redraw; no per-tick allocation, float/trig hot paths or LVGL from ISR.
Preserve Study12 flat/light borderless cards, fonts>=18, current icons/shadows,
modal(180,56,440,368),44px controls/48px steppers, bounded dropdown/scrim/X,
validation slot/shared Save. No new gradient/recolor/page/layout changes.
Keep uptime, Connected/Disconnected, signal bars, splash, icon-only tabs,
16-sample session-RAM history, readback-driven switches, redraw/idempotence.

Hub owns Auto/appliance semantics/polarity; Node applies raw GPIO levels.
Direct lora_comm/lora_hub_link globals; unchanged8-byte command/20-byte status.
No new snapshot layer, radio/Modbus/Flash drivers, Node automation or multi-node.

| Rule | ON / OFF defaults | Bounds | Step | Inclusive gap |
|---|---|---|---|---|
| Fan / CO2 ppm | >=1000 / <=800 | 0..10000 | 50 | ON>=OFF+50 |
| Purifier / unitless VOC | >=150 / <=100 | 1..500 | 5 | ON>=OFF+5 |
| Humidifier / %RH | <=40 / >=50 | 0..100 | 1 | OFF>=ON+5 |

Independent LCD limits; imported valid integers need not be step multiples.
Clamp edited value only; invalid gap disables Save without shifting partner.
Preset change loads defaults; unsupported types Manual-only. Missing/invalid/
legacy config defaults Manual; valid current explicit Auto opts in.
These demo thresholds are not health/protection guarantees; no ppb algorithm.

Three eligible fresh frames plus10s hold after enable, committed limit change,
confirmed transition, boot/reconnect and sensor recovery. No Auto decision
offline/invalid/pending/latched. Manual switch is explicit override, no timed
Auto return. No-op Save emits no callback/reset. Retry or deliberate Manual->Auto
re-arms; ordinary edits/reconnect/late ACK do not. No forced-OFF on link loss.

## Final verification and single handoff

Run after the last relevant edit, using cached/offline dependencies:
```powershell
tools/build_sim.bat
tools/run_regression.bat
tools/export_mcu.bat --verify
tools/verify_production.bat
```

Keep watchdogs and production strict negative control (successful launch, exact
exit1 and expected stderr; timeout/crash is not PASS). Retain C99 include/link
coverage without LV_LVGL_H_INCLUDE_SIMPLE and test hooks; no simulator globals
in the firmware smoke runner. Run the complete fixed suite and self-review in
one delivery; do not hand off one issue group at a time. Physical MCU/LCD/touch/
QSPI/radio/relay acceptance is still untested here. No commit/push.
