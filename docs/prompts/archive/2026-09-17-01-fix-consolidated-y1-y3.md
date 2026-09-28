ARCHIVED — historical context only. Active handoff: [CURRENT.md](../CURRENT.md).

# CURRENT — consolidated fix and acceptance, one complete delivery

Date: 2026-09-16. Status: **IMPLEMENTED — awaiting review**.
Priority: restore correct rollover behavior; protect every exported ZIP;
close bounded verification/evidence gaps. No visual redesign.
Owner requires automatic review-to-prompt handoff. Technical lead proposes the
implementation/testing details below; no new product behavior is authorized.

## Owner instruction: ONE implementation handoff, not staged deliveries

The owner explicitly requires fewer review/fix round trips. This document contains
ALL known remaining defects and acceptance gaps. Y1/Y2/Y3 are traceability IDs,
not phases, separate prompts or separate deliveries. Fix and verify the entire
scope in one implementation run; do not stop after one group and ask the owner
to submit it for review before continuing with the rest.

- Before editing, map every requirement below to existing code and a test/evidence
  item. Inspect the full affected flow, not only the lines named in the findings:
  restore -> initialization -> RX/time/reconciliation -> Auto/input -> actual TX;
  page/modal/event lifecycle -> redraw/heap; export -> copy/extract -> cleanup.
- Implement all known fixes and missing checks together. After changes, review the
  complete resulting diff and exercise those flows end-to-end. Fix regressions
  introduced by your changes in the SAME run before handing back to the owner.
- Do not tune production semantics to satisfy a flawed fixture. Check whether the
  fixture's time, RX revision, command phase and sensor state are valid first.
- Reuse existing build cache, harnesses and evidence paths. Batch related tests;
  avoid repeatedly dumping whole files, rewriting reports or re-running unchanged
  expensive checks. Run the full final suite after the last relevant code change.
- Keep accepted UI/features closed unless concrete regression evidence reopens
  them. No new aesthetics, speculative refactors or higher thresholds to extend
  this task. Optional polish is non-blocking and must not create another fix round.
- A real permission/environment/product-decision blocker may require handoff,
  but list ALL remaining items and exact reasons together. Do not claim complete
  when required checks are skipped or rely on the owner to discover omissions.

The reviewer must likewise inspect the entire submitted diff and affected flows
before issuing ONE consolidated findings list and ONE updated CURRENT, rather
than stopping at the first defect or drip-feeding separate fix groups.

## Read and scope

Read AGENTS.md, docs/prompts/README.md, this file,
docs/reviews/2026-09-16-auto-x1-x4-independent/REVIEW.md and its check.c/check-export.ps1,
docs/README.md, docs/HARDWARE_OVERVIEW.md, docs/MCU_BASELINE.md,
docs/UI_DESIGN_BRIEF.md, docs/user/SMART_HUB_UI_GUIDE.md,
docs/user/LORA_GUIDE.md and LORA_PROTOCOL.md. Inspect code/tests and Git status.
Preserve unrelated owner work and all historical evidence.

Implement/test/document when the owner hands you this coding task.
Review-only does not authorize production fixes. No commit/push, branch changes,
reset/clean/stash, radio/Modbus/Flash drivers, packet changes or unrelated refactors.

Accepted:68x44 switch targets, separated recovered switch/Retry, retained Retry,
real invalid draft in CHECK31, improved shared styles, actual pre-init restore
surviving ui_init, malformed-record matrix, all-three-sensor recovery scenarios,
recorded-packet-fed fake node, improved TX checks, primary ZIP collision refusal.
Do not re-open these repaired symptoms. Build31/31 and approved exported-production
verification pass, but a new production wrap regression and secondary ZIP overwrite
are independently reproduced.

## Freeze architecture and visuals

LVGL8.4,800x480 RGB565, one800x10 partial draw buffer,42KiB heap unchanged.
No full MCU framebuffer, per-tick allocation, float/trig hot paths or LVGL from ISR.
Use lifetime static captions/lv_label_set_text_static, fixed buffers, change guards.
Keep flat/light Study12, borderless cards, existing fonts>=18, icons/shadows,
Temperature/Humidity composition, modal(180,56,440,368),44px controls/48px steppers,
bounded dropdown and scrim/X, fixed validation slot/shared Save.
Keep uptime, Connected/Disconnected, signal bars, splash, icon-only tabs,
16-sample session-RAM Trends, zero-redraw/idempotence and truthful readback.
No new gradients, panels/pages, global recoloring or layout overhaul.

Hub owns UI/semantics/polarity/Auto; Node applies raw GPIO levels.
Preserve direct lora_comm.h/lora_hub_link.h globals,8-byte commands/20-byte status.
No snapshot layer, Node automation/watchdog, multi-node or scheduling.

## Y1 [P1] Restore wrap-safe link and Auto timing

Production ui.c:is_link_connected and ui_auto.c:ui_auto_eval now use
now>=previous ? now-previous : 0. This breaks the normal32-bit millisecond wrap.
The test getter ui_is_auto_hold_active repeats the defect.

Independent case:
- last RX=4294966295, now=5000 -> real age6001ms, but connected=true.
- Hold restarted at4294962295;3 valid Fan evaluations at6000/6100/6200 after wrap
  -> elapsed11201ms, but no decision.
Thus stale telemetry may remain online and Auto hold may remain stuck until a new
event resets its timestamp. This is a new regression, not an old UI layout issue.

Requirements:
- Use correct modular elapsed-time semantics for bounded timeouts/holds.
  Align production predicates and test observation; do not merely fix a getter.
- Preserve real boot tick0, sentinel/uninitialized handling and the full10s hold.
  Explicitly handle any future1ms timestamp currently used as the tick0 sentinel.
  Do not blindly revert into immediate hold expiry at0, and do not clamp all
  post-wrap differences to0. Fix invalid simulator time setups in the simulator,
  rather than weakening production rollover behavior.
- No packet schema change is needed. Preserve fresh-revision qualification and
  pending/error/reconnect behavior.
- Add maintained tests of ACTUAL production paths, not just a standalone unsigned
  subtraction: fresh/stale boundary4999/5000ms across wrap, stopped reports across
  wrap, UI disconnected masking/action refusal and outbound TX blocked.
- Auto: boot at0 and starts just before UINT32_MAX; hold9999/10000ms; no early
  transition, then3 eligible fresh frames after expiry; both rule directions and
  all3 presets. Duplicate revisions/time alone cannot qualify.
- Test reconnect/re-arm, pending timeout, and revision rollover remain correct.
  Integrate wrap probe into regression with Release assertions/watchdog.
  Existing31 checks must stay meaningful; no disabling/relabeling failed tests.

## Y2 [P2] Protect secondary ZIP and complete export ownership

Primary publication now correctly uses non-replacing File.Move. Retain it.
Remaining production bug: copy /y OUT_ZIP MCU_DEST overwrites a same-name ZIP
inside MCU_DEST even when OUT_ZIP is new. Independent sentinel outcome:
exit0, sentinel_preserved=False.

- Protect EVERY archive written/copied by the export workflow, not just primary.
  Existing secondary ZIP must remain byte-identical; either refuse clearly with
  nonzero status or choose/report an unambiguous unique owned output.
  Avoid partial misleading success; report/retain any already created primary
  artifact on failure. Same-source/destination case must be handled deliberately.
- Keep normal MCU extraction usability, but document what an existing extracted
  source tree may update. Do not claim directory refusal that is not implemented.
  This handoff does not require deleting/reorganizing an owner's destination tree.
- Default names must contain a generated valid date/time independent of an
  unrelated inherited TS environment variable; current code trusts it verbatim.
  If a test-only override is retained, validate it and clearly scope it.
- Replace collision-prone RANDOM staging with robust unique per-invocation
  ownership. Two simultaneous default exports in review returned0 and1 with
  ERROR: temporary directory collision. This failed safely but does not satisfy
  successful distinct default artifacts. Keep atomic non-replacing publication,
  and exhaustion/failure must not fall through into overwriting.
- Validate canonical exact paths and directory boundaries before recursive cleanup.
  Preserve diagnostics on failure, including verifier collision fixtures.
- Disposable-fixture tests only: primary existing destination, secondary same-name
  ZIP, simultaneous same explicit destination (one may reject), simultaneous
  normal exports (both distinct complete artifacts), unset/unrelated TS,
  failure/timeout preserving unrelated files. Verify valid archive contents and
  exact returned paths, not just filenames/counts.
- Never reproduce collisions against user exports. Keep --verify manifest behavior,
  isolated task-owned production verification and existing documented arguments.

## Y3 [P2] Finish the remaining requested proof and correct the report

Do not rewrite correct control logic merely to simplify tests.

1. Extend the actual pre-init/post-init fixture through a RECORDED first send.
   No pre-init config reset. Assert restored configs/defaults,0 callbacks, TX
   blocked before eligible report and first packet adopts current readback.
2. Existing four negative cases mutate log arrays/validator parameters; they prove
   only the log validator. Add executed-loop fault injection for gating bypass,
   too-fast sends, missing periodic sends and corrupted bytes. The real harness
   must reject each fault in Release. Keep proper packet/timing boundary checks.
3. Complete Home/Devices x both polarities x reconnect/late-ACK pointer coverage.
   Assert actual pending->timeout/latch before every recovery branch.
   Check centres/extended edges, non-overlap, exactly one intended action,
   switch->Manual, Retry hold restart/no immediate obsolete command, offline
   refusal and ordinary edits/navigation preserving latch.
4. In50-cycle heap test create invalid limits via real pointers; assert subview/
   controls exist, validation visible and Save disabled each exercised cycle,
   plus discard/reopen. Include Auto/recovery activity rather than only Manual
   navigation. Capture immediate/first-rendered and stable measurements separately:
   total free, largest block, peak usage, fragmentation and drift.
   Existing >4096 total-free reserve remains the requirement; no higher threshold,
   extra settling delay, font shrink or heap enlargement. Current measurements
   pass; optimize only if correct expanded coverage exposes a failure.
5. Keep all-three-sensor recovery tests; assert no decision on frame1/frame2 for
   VOC/RH as for Fan, and controlled duplicate/time-only input.
6. Capture literal commands/stdout/stderr/exit codes, not reconstructed transcripts.
   Remove unsupported current all-complete/CRC/directory-refusal claims.
   Report actual recovered Devices switch x214, not ordinary x281.
   Re-running a reviewer probe is not independent acceptance; never overwrite its
   hardcoded historical captures. Adapt paths into a new evidence folder.
   Mark unexecuted cases OPEN, not ALL COMPLETE.

## Preserve Auto/data contract

| Rule | ON / OFF defaults | Bounds | Step | Inclusive gap |
|---|---|---|---|---|
| Fan / CO2 ppm | >=1000 / <=800 | 0..10000 | 50 | ON>=OFF+50 |
| Purifier / unitless VOC | >=150 / <=100 | 1..500 | 5 | ON>=OFF+5 |
| Humidifier / %RH | <=40 / >=50 | 0..100 | 1 | OFF>=ON+5 |

Independent LCD limits; valid imported integers need not be step multiples.
Clamp only edited value; invalid gap disables Save without moving its partner.
Preset changes load defaults; unsupported presets Manual-only.
Missing/invalid/legacy config defaults Manual; only valid current explicit Auto
opts in. These are demo thresholds, not health/appliance protection standards.
No ppb conversion or new sensor algorithm.

Keep3 eligible fresh frames plus10s hold after enable, committed limit change,
confirmed transition, boot/reconnect and sensor recovery; wrap-safe tick/revision,
relevant faults, pending arbitration and per-channel isolation.
No decisions offline/invalid/pending/latched. Explicit switch commits Manual,
no timed Auto return. No-op Save emits no callback/resets nothing.
Explicit Retry or deliberate Manual->Auto re-arms; ordinary edits/reconnect/
late ACK must not re-arm implicitly. No forced-OFF guarantee during link loss.

## Single delivery: fixed acceptance checklist

Complete all rows below before reporting the implementation finished. These are
the existing requirements summarized, not extra scope or sequential phases.

| Acceptance area | Required result in the same delivery |
|---|---|
| Time and link | Real production paths pass tick0/wrap, 4999/5000ms link and 9999/10000ms hold boundaries; stale TX/actions blocked |
| Auto and recovery | All3 rules/both directions, 3 fresh frames, duplicate/time-only rejection, timeout/re-arm and channel isolation preserved |
| Restore and TX | Config survives real init; first recorded packet adopts readback; executed faulty senders are rejected by the harness |
| Touch and lifecycle | Both pages/polarities/recovery branches, actual edge taps, correct single action, no overlaps/click-through/stale widget references |
| Heap and visuals | 50 real-pointer cycles with invalid drafts/Auto recovery; >4096 total free at tested peaks; stable drift, no accepted visual/redraw regression |
| Export and cleanup | Every ZIP protected, correct names, concurrent success/isolation, truthful partial-failure outcomes and unrelated sentinels preserved |
| Evidence | Literal logs/exit codes, new native captures, no invented CRC/directory/independent-acceptance claims; report matches current code |

Use one final report with columns requirement -> implementation -> test -> actual
result -> evidence path. Distinguish verified defects, missing proof, optional
polish and target-only limitations; do not conflate them. If any row is incomplete,
state it explicitly and continue in-scope work where possible instead of declaring
ALL COMPLETE. Host acceptance does not imply hardware acceptance.

## Delivery commands and artifacts

Run tools/build_sim.bat, tools/run_regression.bat, tools/export_mcu.bat --verify
and tools/verify_production.bat, cached dependencies disconnected.
Retain process watchdogs and production strict negative control: successful launch,
exact exit1/expected stderr. Timeout/crash/launch failure cannot count as pass.
Label approved outside-sandbox runs and environment limitations honestly.

New evidence goes in docs/reviews/2026-09-16-auto-y1-y3-fixes/:
Y1–Y3 mapping, actual logs, wrap/boot/TX traces, concurrent/secondary export
sentinels, native800x480 unmasked recovery captures on both pages, real pointer
bounds and per-state heap table. Preserve old reports and artifacts.
Update PLAN, DEV_LOG and relevant documentation indexes/product notes.
CURRENT remains the specification, with IMPLEMENTED — awaiting review status
only after implementation; never replace requirements with an execution report.
Physical MCU/LCD/touch/radio/relay acceptance remains untested by host checks.
No commit/push.
