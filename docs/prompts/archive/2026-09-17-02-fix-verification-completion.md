ARCHIVED — historical context only. Active handoff: [CURRENT.md](../CURRENT.md).

# CURRENT — finish verification and evidence in one delivery

Date: 2026-09-17. Status: **IMPLEMENTED — awaiting review**.
Scope: remaining R1–R4 test/verifier/evidence corrections after independent Y1–Y3 review.
Owner requires one consolidated delivery, not phases or a prompt per issue.
Technical-lead details below introduce no new product/UI decisions.

## Read first

Read AGENTS.md, docs/prompts/README.md, this CURRENT,
[latest review](../../reviews/2026-09-17-consolidated-independent/REVIEW.md),
its check.c and combined.log, docs/README.md, docs/HARDWARE_OVERVIEW.md,
docs/MCU_BASELINE.md, docs/UI_DESIGN_BRIEF.md, docs/user/SMART_HUB_UI_GUIDE.md,
docs/user/LORA_GUIDE.md and LORA_PROTOCOL.md. Inspect Git status and affected
source/tests. This file is self-contained; archived prompts are not active tasks.

The reviewer independently ran regression (31/31) and exported-production
verification successfully. An additional actual Auto timeout + open-modal probe
also passed (>4096 total free). No new production/runtime bug was confirmed.
The remaining problems are missing required coverage and overstated evidence.
Do not rewrite working production code merely to satisfy broken fixtures.

## Authority, accepted work and scope guard

When the owner gives this to a coding agent, implement/test/document all R1–R4
together, self-review the full resulting diff, fix in-scope regressions and run
the final suite after the last relevant edit. Do not stop between issue groups
for owner review. Reuse build caches; batch related checks and avoid needless
full-file dumps or repeated unchanged builds.

Preserve unrelated dirty work. No commit/push, branch change, reset/clean/stash,
new drivers, protocol change, general automation, or speculative refactor.
The review request itself authorized documents/probes, not production fixes.

Accepted and closed unless concrete regression evidence reopens them:
- Wrap-safe production link/hold arithmetic, boot sentinel handling.
- Atomic primary ZIP publication; protected secondary copy; GUID staging and TS fallback.
- Restored config surviving actual ui_init through recorded first send.
- Actual-loop gating/rate/missed-deadline/corruption fault injections.
- All-three-sensor recovery and controlled duplicate-frame qualification.
- Retry retained after recovery; separated 68x44 switches/Retry hit targets.
- Pointer-created invalid drafts, shared styles and improved heap behavior.
- Current light, borderless UI, Auto/modal spacing, Temperature/Humidity layout.
Do not add cosmetic work or higher acceptance thresholds.

## R1 — Make the existing 50-cycle stress actually exercise Auto/recovery

CHECK 30 leaves all devices Manual (sim_pc/main.c around 4269). CHECK 31's
VOC change/conditional Retry around 4434 never enables Auto or asserts that the
Auto/recovery branch ran. Its current invalid-draft pointer checks are valid.

- Explicitly configure a supported Auto channel, establish healthy/readback
  state and use eligible fresh reports/hold expiry to cause a real command.
- Include asserted pending -> timeout/latch -> recovery and explicit re-arm,
  with meaningful Auto activity during the repeated modal/navigation sequence.
  Exercise relevant modal/dropdown/invalid-draft combinations; use deliberate
  node/report behavior rather than directly setting paused_error to pass.
- Assert/count those states/actions. A Manual-only, no-command, never-latched
  or silently skipped branch must fail the test. Complete 50 cycles, retaining
  real-pointer invalid limits and validation visible/Save disabled assertions.
- Preserve discard/reopen defaults, no-op/cancel callback isolation. Pointer
  input must follow real LVGL input/timer/render cadence; don't manufacture a
  failure with invalid widget references or skipped navigation rendering.
- Track immediate/first-rendered AND stable total free, largest block, peak use,
  fragmentation, drift at the exercised states, including Auto/error activity.
  Keep >4096 total free and existing drift limit; largest block is diagnostic.
- Independent check.c passes a one-shot combination with minimum 4464 bytes.
  Reuse the scenario, not that number as a universal threshold or replacement
  for maintained coverage. Keep 42KiB heap/fonts/layout; optimize production
  only if valid expanded coverage exposes a real failure.

## R2 — Finish the already-required finite time/touch matrix

Reuse existing tests, preferably table-driven. Keep passing cases.

- Across UINT32_MAX wrap, test all 3 rules in BOTH directions. Purifier OFF and
  Humidifier OFF are missing from CHECK 1; assert no decision before hold expiry
  or on fresh frames 1/2, then correct decision on frame 3. Assert outputs,
  not just invoke and discard return values. Keep boot tick0/sentinel coverage.
- Drive the actual ui_tick + integration TX path across link age 4999/5000ms:
  stopped reports -> disconnected masking -> refused user action/Auto decision
  -> no recorded stale TX, then legitimate recovery/readback adoption.
  Do not merely assert a copied predicate or unsigned subtraction. Preserve
  revision wrap, command timeout, duplicates/time-only, channel isolation tests.
- For Home/Devices x both polarities x late-ACK/reconnect, execute BOTH Switch
  and Retry from separately re-established genuine timeout/latch setups.
  The current parity-selected single action per combination is incomplete.
- Exercise centre and effective extended-edge taps where applicable; verify
  >=44px target, no overlap/click-through and exactly one intended action
  (config callback / seq / command as appropriate). Switch -> Manual, Retry ->
  hold restart with no immediate obsolete command. No accidental other-channel
  change. Offline refusal and ordinary edits/navigation must retain latch.
- Reuse valid configs and controlled fake-node timing. Do not directly set a
  latch as a substitute for the timeout under test, or weaken production rules.

## R3 — Finish export verification and failure-path containment

Keep the non-overwriting exporter design; do not rewrite it speculatively.

In disposable task-owned fixtures:
- Inspect BOTH concurrent default ZIPs: exact returned paths, distinct artifacts,
  required production file membership and source-content identity. Counting names
  is insufficient. Verify existing sentinels byte-identical.
- Run two exports racing for the SAME explicit destination. Exactly one succeeds,
  the other reports collision without modifying the winning complete archive.
  Keep existing primary/secondary collision, sequential default suffix and
  unset/unrelated TS cases.
- Exercise controlled failure and watchdog timeout paths; check nonzero outcome,
  retained diagnostics and unrelated sentinel preservation. Never use owner exports.
- Manage all started workers in try/finally, drain captured stdout/stderr safely,
  terminate/reap every remaining task-owned process tree on error/timeout, not
  only one cmd parent. Preserve useful diagnostics for each worker.
- Before recursive cleanup, resolve and validate exact task-owned paths against
  the disposable root; never broad/glob cleanup. If export cleanup is adjusted,
  keep it narrowly scoped and preserve published primary ZIP on secondary failure.
- Existing requested extraction into smart_hub_ui may update extracted source;
  ZIP protection does not imply a new prohibition on that documented extraction.

## R4 — Deliver evidence that matches executed checks

- Save complete raw final commands, stdout/stderr, exit codes and case matrix in
  docs/reviews/2026-09-17-verification-completion/. Use one report linking logs.
  Do not replace paths by ellipses inside files described as literal transcripts.
- Report requirement -> test -> result -> evidence. Separate production defects,
  coverage gaps, optional polish and hardware-only limitations.
- Remove/label the unmeasured '< 50 ns' output in sim_pc/main.c; measured redraw
  counts remain valid. Do not start a benchmarking/optimization project.
- Re-running a reviewer harness is your regression evidence, not independent
  acceptance. Adapt hardcoded output paths before reuse; never overwrite old
  review captures. Keep historical submissions, clearly superseded by new report.
- Update PLAN, DEV_LOG and relevant docs indexes; CURRENT stays this specification.
  Mark IMPLEMENTED — awaiting review only once all requested work really ran.
  Genuine permission/environment blockers must be listed together with exact
  unexecuted cases, not hidden behind ALL COMPLETE.

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

## Final delivery gate — one run, not multiple phases

After the last relevant edit run:
```powershell
tools/build_sim.bat
tools/run_regression.bat
tools/export_mcu.bat --verify
tools/verify_production.bat
```

Use cached offline dependencies, bounded watchdogs, production build without test
hooks and strict negative control (launch succeeds, exit1, expected stderr;
timeout/crash is not PASS). Save full final results, new relevant native captures,
R1 state/heap counts, R2 matrix, R3 artifact/cleanup results in the one report.
Self-review the complete resulting diff and rerun affected checks if that review
requires fixes. Do not require another owner-mediated handoff per finding.
Physical LCD/touch/MCU RAM/QSPI/radio/relay acceptance remains untested on host.
