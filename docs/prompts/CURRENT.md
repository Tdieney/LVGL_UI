# CURRENT — close remaining X1–X4 after independent W1–W4 review

Date: 2026-09-16. Status: **READY FOR HANDOFF**.
Scope: bounded export safety, switch touch targets and missing acceptance proof.
Owner requires automatic corrective prompts after reviews with issues.
Technical-lead directions below implement existing W1–W4 requirements; no new
product decision, automation rule or visual redesign is introduced.
READY means ready to implement, not independently accepted.

## 1. Read first and respect scope

Read AGENTS.md, docs/prompts/README.md, this file,
docs/reviews/2026-09-16-auto-w1-w4-independent/REVIEW.md, check.c and check-export.ps1,
docs/README.md, docs/HARDWARE_OVERVIEW.md, docs/MCU_BASELINE.md,
docs/UI_DESIGN_BRIEF.md, docs/user/SMART_HUB_UI_GUIDE.md,
docs/user/LORA_GUIDE.md and LORA_PROTOCOL.md, then current implementation/tests.
Inspect Git status and preserve all unrelated work and historical review evidence.
Previous W1–W4 requirements are incorporated here; archives are historical only.

Implement, test and document X1–X4 when the owner hands you this implementation
task. No commit/push, branch changes, stash/reset/clean, unrelated refactors,
driver work, Flash persistence implementation or packet changes.
A review-only request does not authorize production fixes.

Accepted: Retry now survives reconnect/late ACK, shared styles improve heap,
popup pointer/cancel tests improved, export verification stages its own ZIP and
builds actual exported sources with separately built cached LVGL.
Do not revert those changes or claim these fixed symptoms still fail.
Build/regression31/31 and approved outside-sandbox production verification pass.
Remaining issues include actual export overwrite and touch geometry plus
specific missing proof, not a demonstrated failure of every Auto behavior.

## 2. Freeze product and architecture

- LVGL8.4,800x480 RGB565, one800x10 draw buffer, unchanged42KiB heap.
  No full MCU framebuffer, per-tick allocation, float/trig in hot paths or ISR LVGL.
  Static captions in lifetime storage and lv_label_set_text_static; fixed value
  buffers, change guards, zero-redraw/idempotence behavior.
- Preserve light flat Study12 palette, borderless surfaces, current shadows/fonts/
  icons, main card geometry, Temperature/Humidity composition and native modal
  (180,56,440,368). Visible text>=18; no global recolor, gradient or new page.
  Keep uptime, Connected/Disconnected, signal bars, splash and icon-only tabs.
- Keep44px modal controls,48px steppers, fixed validation slot, five-digit value
  width, shared Save, popup font18/44px pitch and working scrim/X lifecycle.
- Hub owns semantics/polarity/Auto; Node applies received GPIOs verbatim.
  Preserve direct lora_comm.h/lora_hub_link.h globals,8-byte command/20-byte status.
  No snapshot layer, Node automation/watchdog, multi-node, schedules or RF/Modbus.
- Keep16-sample session-RAM Trends and real readback. No stale-value or fake safe/OFF
  presentation; no forced-off guarantee during link loss.

## 3. X1 [P2] Make export genuinely non-clobbering

Evidence: tools/export_mcu.bat explicit destination still uses Compress-Archive
-Force. check-export.ps1 replaces a review-owned existing sentinel and returns0.
Normal export with TS unset produces smart_hub_ui_.zip because %TS% is expanded
before assignment inside the parenthesized block. Sequential suffixing works but
does not provide atomic concurrency safety;999-name exhaustion can fall through.

Requirements:
- Both default and explicit output must never replace an existing ZIP by default.
  Explicit existing destination: return a clear nonzero collision result and
  leave original bytes intact. No implicit overwrite feature is needed.
- Default export must include a valid date/time and choose a unique destination.
  Make ownership/collision handling atomic (e.g. exclusive reservation or unique
  staging plus atomic non-replacing publication); a Test-Path check followed by
  forced overwrite is insufficient. Handle exhaustion without fall-through.
- Return/report the exact created path. Preserve normal destination extraction
  workflow, documented arguments and --verify manifest behavior. Propagate errors.
- Preserve verifier's unique task-owned staging and actual artifact build.
  Only clean owned exact paths after canonical directory-boundary checks.
  Preserve diagnostics on failure, including collision fixture diagnostics.
- Do not infer ownership from before/after root filename lists.
- Test ONLY disposable fixtures: same exact existing explicit ZIP, two overlapping
  invocations competing for one explicit destination, concurrent normal exports,
  inherited/unset TS, and failure/timeout cleanup preserving unrelated sentinels.
  Concurrent explicit collision may reject one writer; it must not replace the
  winner. Default exports must produce distinct valid complete archives.
- The current verifier's different-name sentinel and sequential different-path
  exports are not collision tests; retain them if useful but rename accurately.

No destructive experiment against the owner's real exports.

## 4. X2 [P2] Complete recovered-state touch targets

Evidence: ui.c Home and Devices switches are54x30 with effective click area54x30.
Retry visibility is fixed and Retry height already meets the contract.

Keep the visual switch/card layout and extend switch hit areas to>=44px height,
with no overlap with Retry, settings or other actions and no escape from the
owning card/tile. Verify the real LVGL effective target, not a nominal constant.
If a minimal local placement adjustment is needed to separate targets, document
its coordinates; do not redesign ordinary cards.

For both Home/Devices, active_high/active_low, reconnect/late ACK:
- Reach an actual Auto-generated pending command, real timeout and asserted latch.
- Reconcile a fresh report; assert IDLE, latch retained, actual readback switch
  and visible Retry. Keep Unknown+Retry only for unresolved command ERROR.
- Check bounds and send pointer press/release at centres and extended target edges.
  Exactly the intended action must execute: switch commits Manual, Retry re-arms
  Auto. No click-through, duplicate callback or obsolete command replay.
- Retry clears latch and restarts hold/qualification; no immediate new sequence/
  output change solely from re-arm. Qualify fresh frames before a later decision.
- Offline cannot re-arm; navigation/no-op/ordinary threshold edits cannot clear
  the latch. Use fresh state for each branch and assert its preconditions.
Do not substitute ui_trigger_device_retry/toggle helpers for pointer acceptance.

## 5. X3 [P2] Finish real boot, TX and sensor recovery verification

These are missing tests, not an instruction to rewrite correct control logic.

### Boot/restore

The new pre-init fixture resets all configs before ui_init, so it never observes
restored state surviving initialization. Add isolated process fixtures that:
1. Register callback counter, load through the actual documented adapter BEFORE
   ui_init without forcing a staging flag in an already running UI.
2. Call ui_init WITHOUT resetting restored state; assert every channel's config,
   zero restore/init callbacks, initial TX blocked and proper Auto hold.
3. Publish first eligible report; reconcile then record permitted TX, asserting
   adopted GPIOs before sends (no boot zero/stale-intent pulse).
4. Include valid explicit current Auto, legacy->Manual, missing and invalid records.
   Test EVERY length0..sizeof(record)-1 for both formats, wrong kind/version,
   empty/unterminated name, invalid preset/mode/polarity and invalid threshold pairs.
   Preserve known-provenance policy; never infer record version from size alone.
Use separate fresh processes/default fixtures to protect existing regression
assumptions. Keep earlier helper tests as additional coverage.
Do not claim CRC validation unless a real existing adapter contract implements it;
no new CRC/storage driver requirement is introduced.

### Recording TX

CHECK29 now records bytes/time, but only asserts mask/sequence and >=3 periodic
sends. Its fake node also consumes shared command globals independently.

Exercise a shared/compiled documented application-loop implementation with the
recording send stub on its actual TX path: RX publication -> ui_tick/reconciliation
-> LVGL input -> gated sequence/periodic sends. A fake node must consume recorded
packets rather than a second unrecorded parallel transmit path for this test.

Assert exact expected packet bytes, timestamps/counts/order at known boundaries:
zero before first eligible report, zero while stale and before reconnect
reconciliation, first eligible reconnect TX adopts all16 masks/both polarities,
immediate new-sequence send and unchanged-sequence periodic send at1000ms.
Assert no sends before due; tolerate only the stated loop scheduling quantum.
Include negative test doubles that bypass gating, send too often, omit periodic
TX or corrupt bytes; the harness must fail those deliberate faults in Release.
No radio driver is required.

### Sensor recovery

CO2 recovery has been added; relevant VOC and RH invalid->valid paths are absent.
For EACH Fan/CO2, Purifier/VOC, Humidifier/RH:
- Start healthy/settled, invalidate the relevant sensor/fault and assert suspension.
- Recover, assert the full10s hold starts once from recovery; repeated good frames
  must not perpetually restart it.
- With deterministic RX and fake periodic updates disabled, assert time alone and
  unchanged revision do not satisfy3 eligible fresh frames. Exercise the threshold
  with controlled frame arrivals after hold; check no premature decision.
- Assert unrelated faults leave healthy rules operational.
- Keep timeout, late ACK, link loss/reconnect and explicit re-arm distinct; do not
  call incremented revisions "duplicate frames".
Retain improved dropdown drag/first-last selection and draft cancel tests.

## 6. X4 [P2] Make heap stress actually cover invalid state and transitions

CHECK31 labels150/110 Purifier limits as invalid; they are valid and Save is enabled.
Create a genuinely invalid pair via pointer steppers, assert validation visible
and Save disabled BEFORE measuring. Assert Back/cancel discard and successful
reopen; never silently skip a missing widget/subview.

Retain50 complete cycles, include the previously requested Auto/recovery activity,
and record:
- immediate post-build/event-boundary and first-rendered memory before settling;
- Device modal, dropdown/scrim, selection/dismiss, Auto subview, true invalid draft,
  Back and cancellation, with named state/tick for each minimum;
- minimum total free, largest block, peak usage, fragmentation, stable drift
  separately. Keep meaningful deterministic warm-up/drift limits; do not loosen
  existing thresholds or add a delay to hide a peak.

Existing independent numbers:4104 free at first settings frame (largest3912),
4936 settled,4544 dropdown,4256 Auto limits/true invalid. All pass the existing
>4096 total-free contract. No new higher reserve/largest-block requirement.
These measurements supersede treating4296 as a universal minimum.
If corrected coverage exposes a dip, optimize within42KiB without shrinking fonts,
hiding controls or unsafe deletion; otherwise retain the working implementation.

## 7. Preserve established Auto/data contract

| Rule | ON / OFF defaults | Bounds | Step | Inclusive gap |
|---|---|---|---|---|
| Fan / CO2 ppm | >=1000 / <=800 | 0..10000 | 50 | ON>=OFF+50 |
| Purifier / unitless VOC | >=150 / <=100 | 1..500 | 5 | ON>=OFF+5 |
| Humidifier / %RH | <=40 / >=50 | 0..100 | 1 | OFF>=ON+5 |

Per-device LCD limits; imported valid integers need not be step multiples.
Clamp only edited value; invalid gap disables Save without moving its partner.
Preset change loads defaults; unsupported presets Manual-only.
Missing/invalid/legacy config defaults Manual; only complete valid current explicit
Auto opts in. Demo thresholds are not health/appliance-protection standards.
No ppb conversion/new sensor algorithm.

Preserve3 eligible fresh frames plus10-second hold after enable, committed limit
change, confirmed transition, boot/reconnect and sensor recovery; wrap-safe ticks/
revisions, relevant fault masking, pending arbitration and per-channel isolation.
No decision offline/invalid/pending/latched. Switch is explicit Manual override,
no timed return. No-op Save resets nothing and emits no callback.
Latch clears only explicit Retry or deliberate committed Manual->Auto.

## 8. Delivery and honest evidence

Run tools/build_sim.bat, tools/run_regression.bat, tools/export_mcu.bat --verify
and tools/verify_production.bat with cached dependencies disconnected.
Retain process watchdogs and strict production negative control: successful launch,
exact exit1 and expected message; crash/timeout/launch failure is not a pass.

Put NEW evidence in docs/reviews/2026-09-16-auto-x1-x4-fixes/:
- X1–X4 mapping to changed code, named tests, preconditions and actual results;
- actual stdout/stderr/exit codes, reproducible commands and negative-test outcomes;
- native800x480 unmasked late-ACK/reconnect captures on both pages, touch bounds;
- per-state heap table, valid vs invalid preconditions, stable drift;
- boot config and TX trace, all-rule recovery boundaries;
- true collision/concurrency/cleanup fixture outcomes.
Label summaries as summaries. Do not invent console transcripts or claim
CRC/multi-rule recovery/concurrency coverage that did not execute.

Preserve historical probes/reports/raw captures; adaptation writes only new paths.
Mark omissions OPEN, not ALL COMPLETE. Host pass is not real MCU/LCD/touch/radio/
relay acceptance. Distinguish sandbox failures from approved outside-sandbox runs.
Update PLAN, DEV_LOG and relevant docs/indexes with current evidence. Preserve
CURRENT as the specification; status may become IMPLEMENTED — awaiting review,
but do not replace requirements with an execution report or weaken them.
No commit/push.
