# ARCHIVED — historical context only

Superseded by [CURRENT](../CURRENT.md). Retained as the 2026-09-23 Trends/Auto implementation handoff.

# CURRENT — make live Trends and Auto work end to end

Date: 2026-09-23
Status: **IMPLEMENTED — awaiting review**
Scope: ONE consolidated implementation, test and integration delivery.
Owner reports live readings on the LCD while Trends Min/Max/chart remain empty,
and an OFF device remaining OFF after selecting Auto despite a threshold crossing.
Owner authorized a prompt for another coding AI; technical lead requirements
below are proposals for implementation, not claimed owner-approved behavior.

## Read and baseline

Read AGENTS.md, docs/prompts/README.md, docs/README.md,
docs/HARDWARE_OVERVIEW.md, docs/MCU_BASELINE.md,
docs/UI_DESIGN_BRIEF.md, docs/user/SMART_HUB_UI_GUIDE.md,
[2026-09-23 investigation](../../reviews/2026-09-23-trends-auto-investigation/REVIEW.md)
and [previous independent review](../../reviews/2026-09-21-g1-g4-independent/REVIEW.md).
Inspect current source/test/firmware interface, not archived prompt requirements.
Preserve dirty worktree. Do not commit, push, reset, clean, stash or delete old
evidence. Implement, self-review full final diff and close all in-scope
regressions in this ONE delivery.

Current simulator passes34 checks but tests seed Trends manually and advance
the Auto hold before evaluation; those passes do not cover the owner's live
path. Produce new tests that FAIL against the current behavior first.

## P1 — live session history, Min/Max and plot

Observed:
- update_trends_page_widgets reads Current from lora_node_status.
- Min/Max, line, markers, span and empty state read s_history.
- ui_feed_history is only called by simulator in this repository; firmware
  integration guide leaves it as a future manual step. ui_tick never builds
  history from received status frames. Thus live Current can work while the
  other fields stay empty.

Implement autonomous bounded session history in the Hub UI using the existing
16-point x4-metric storage. No Flash, another full ring, periodic heap
allocation, radio driver or packet-schema change. The live UI must work even
when firmware never calls ui_feed_history. Keep that public API usable for a
firmware-supplied series; define/test one clear precedence policy per metric
so autonomous and externally fed timestamps never interleave. Recommended:
once a metric is fed externally, that metric uses external history until
ui_init/session reset; switching ownership resets only that metric's prior
autonomous points. Document the actual policy.

Canonical default sampling proposal for this demo:
- On the first accepted fresh Node report, capture an initial point for each
  metric with independent validity. Min=Max=that reading; chart shows a real
  singleton marker, not a fabricated line. A reading of VOC=0 is valid.
- Thereafter capture one aligned sample slot per elapsed session minute;
  retain the newest16 slots. Use monotonic tick/uptime with unsigned wrap
  safety and fixed-size data. Repeated ui_tick or radio retransmissions
  without a fresh revision cannot create extra samples.
- Apply existing ui_is_sensor_available semantics: offline, relevant timeout,
  global RS485 fault, individual invalid bit or out-of-range -> invalid slot.
  Preserve earlier valid history and show gaps; do not record a fake0.
  Partial sensor failure must not invalidate independent metrics. On recovery,
  resume valid slots. If a loop skips several minutes, handle elapsed slots in
  bounded work (max16); never invent readings for minutes with no fresh packet.
- Use an honest elapsed-session time axis/tooltip for autonomous data because
  there is no RTC in this UI. Preserve wall-clock semantics for explicit
  ui_feed_history input. If a display-label adjustment is necessary, confine
  it to chart time copy and keep every visible font >=18px. Do not turn session
  uptime into an unlabeled wall-clock claim. Keep accepted chart geometry,
  colors, markers and Data unavailable/No readings yet wording.
- Render/update while Trends is open AND after navigating from other pages,
  across metric switches. Current remains direct live data, and Min/Max derive
  only from valid retained samples; explain live-vs-sampled distinction in
  docs. Redraw only on a changed point/history revision, page entry or metric
  switch. Existing identical-frame zero-redraw tests must still pass.
- Do not infer data from relay command/status. Match telemetry units:
  eCO2 ppm, VOC index unitless, temp deci-C, RH whole percent.

## P1 — Auto command missing after enabling mode

First prove which gate blocks the owner's path on a realistic fresh-frame
trace: saved mode/preset/thresholds, reported GPIO & active_low, fresh
lora_rx_revision and last RX tick, online/tx_ready, command phase,
paused_error, suspended_sensor, last confirmed transition time/hold,
qualify_count, desired GPIO, command seq, firmware TX and Node seq_echo/
readback. Keep diagnostics test-only or in review evidence, not new debug UI.

Source finding: commit_device_config_internal() calls ui_auto_rearm() AND
ui_auto_restart_hold() on Manual→Auto. ui_auto_rearm itself writes
last_transition_ms=now. This creates a new10s hold even if no relay switched.
The intended rule is10s after a REAL confirmed relay transition, then three
distinct fresh qualifying reports. Change arming/retry/config code to preserve
the timestamp of the last actual confirmed transition; clear error latch and
qualification as required without fabricating a transition. No-op Save keeps
both hold and qualification. Boot/reconnect/sensor-recovery safety gates remain
explicit and tested; do not remove them merely to force a faster demo.

Auto rules (inclusive):
- Humidifier ON when RH <= ON limit (default40%), OFF when RH >= OFF limit
  (default50%). RH above50% while OFF correctly stays OFF. The owner did
  not give the actual RH/limits/preset, so do not label high RH as a code bug.
- Fan ON CO2 >=1000 ppm, OFF <=800 ppm by default.
- Purifier ON VOC >=150, OFF <=100 by default; VOC=0 remains valid.
- Deadband holds current state. Invalid/offline/pending/latched inputs do
  not cause transitions. Do not create Dehumidifier Auto or reverse
  Humidifier polarity to satisfy an ambiguous example.

Test full UI contract, not just ui_auto_eval():
1. Start Hub with a valid OFF readback and no recent relay transition;
   configure supported preset Manual→Auto via the real Save path. Feed
   exactly three distinct fresh eligible reports at short intervals.
   No command on reports1/2; one seq increment and correct desired GPIO on3,
   without an unrelated10s delay. Test active_high AND active_low.
2. Node echo/readback confirms the command; switch becomes ON only from
   reported state, pending clears, then opposite threshold is suppressed
   for10s after confirmation and requires three fresh frames afterward.
3. Test humidity dry35%→ON, humid55% while OFF→remain OFF, boundary40/50,
   deadband, CO2/VOC analogs and valid VOC0.
4. Offline, stale revision, sensor faults, recovery, late ACK, pending/error,
   Retry, Manual override, reboot/reconnect, 32-bit tick wrap and threshold
   edits preserve existing safety/latch semantics. No time-only qualification.
5. Test a live-app style RX→ui_tick→TX observation→Node status echo sequence.
   Verify integration instructions publish complete status then increment
   lora_rx_revision once per real frame, update lora_last_rx_tick_ms, call
   ui_tick in the same serialized context, transmit lora_hub_cmd after it,
   and periodically send enough status reports to meet three-frame rule.
   If the owner's separate firmware source is unavailable, do not claim
   hardware fixed: deliver precise required firmware checks and measured
   simulator proof. No ISR LVGL calls or Node-side automation.

## Carry forward open E1/E2 evidence without another review cycle

Previous CURRENT remains open and is archived for traceability. Include its
bounded work in this SAME delivery after P1 functions are verified:
- E1: Honest non-overlapping RAM ledger and host-only allocation/render
  transient diagnostics for default42KiB and opt-in40KiB; largest request,
  smallest contemporaneous free/largest block, failures and50-cycle stability.
  Correct CHECK8-vs-CHECK31,256B-vs64B drift, UI_HEX example, stale gradient
  cache docs. Keep42KiB/10-line default. Do not claim MCU99.9% solved without
  target ELF/map/lv_conf/stack/DMA timing. A documented rejected40KiB
  candidate is acceptable.
- E2: Durable table-driven sensor availability on Home, Trends and all three
  Auto editors, including combined faults/partial recovery/history retention
  and unaffected metrics; full23-byte name no-op Save/config/hold invariants.
  Fold new autonomous history and Auto end-to-end tests into this suite.
Do not expand unrelated UI or redesign. If these verifications find a real
regression, fix it before final handoff; collect all genuine blockers at once.

## Frozen implementation constraints

LVGL8.4,800x480 RGB565, accepted Quiet Architecture visual, visible text>=18px,
one partial draw buffer, static captions/fixed dynamic buffers. Avoid per-tick
heap, full framebuffer, float/trig hot paths, excessive refresh and ISR LVGL.
Six LoRa globals defined once in ui.c; direct extern wire contract, command8B/
status20B, Hub active_low mapping and Node verbatim GPIO/readback unchanged.
No persistence, RF/RS485 driver, new multi-node architecture, icon/asset,
palette or typography redesign. Preserve existing Auto editor thresholds,
validation, draft Save/Cancel/Back semantics and Error/Retry behavior.

## Verification and delivery

After LAST source change: python tools/generate_assets.py --check,
tools/build_sim.bat, tools/run_regression.bat, tools/export_mcu.bat --verify,
tools/verify_production.bat, plus matched42/40KiB diagnostic regressions.
Do not weaken old assertions/headroom checks or use simulator-only injected
history as the sole Trends proof. Add native screenshots for fresh singleton,
two or more live points, invalid gap/recovery, Auto pending/ACK/error.
Report exact source/config/binary hashes, commands/logs, measured RAM,
observed first eligible→command timing and untested hardware limitations.

ONE new docs/reviews/<delivery>/ report; update docs/user/SMART_HUB_UI_GUIDE.md,
docs/UI_DESIGN_BRIEF.md only for changed product behavior, docs/README.md,
docs/MCU_BASELINE.md, PLAN.md, DEV_LOG.md and SAME CURRENT status
IMPLEMENTED — awaiting review. Self-review the entire final diff and affected
end-to-end flows before delivery. No staged prompts, no all-complete claim
without supporting tests or owner hardware evidence.

