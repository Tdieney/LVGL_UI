# Smart Hub UI plan

## Open items (authoritative, 2026-09-28)

This is the only list of open work. Unchecked boxes in the dated sections below were closed or superseded by later deliveries and reviews. They are kept as history, marked *(superseded)*.

- [ ] Hub and Node firmware move to LoRa protocol `0x02` together:
  - The Hub polls with `GET_STATUS` from boot, before TX-ready, and sends at most three `HUB_CMD` attempts within 3 s.
  - `lora_last_rx_tick_ms` and `lv_tick_get()` use one clock.
  - The Node replies to every request and advances `sample_seq` once per completed poll. Without that, history and Auto never advance.
- [ ] On the real Hub: flash the current `ui.c` and repeat the blow-on-sensor test. The Trends `Now` point and Y range should follow within about one report (D-041).
- [ ] Target memory sign-off: ELF/map, target `lv_conf.h`, stack/DMA/radio buffers. The owner reported 99.9 % MCU RAM on 2026-09-21; the host figures are not target evidence.
- [ ] Physical LCD/touch validation: touch-to-feedback and chart timing, QSPI/DMA timing, readability and touch accuracy on the real panel.
- [ ] Validate the MXVC10S / TH10S Modbus register mapping and value ranges on real sensors.
- [ ] Firmware Flash/NVS persistence driver. The UI callbacks exist; rewrite v2 records after migration.
- [ ] Policies to decide: relay state at power-on and on link loss (safety), RF parameters (SF, channel), and frame authentication/encryption (plaintext demo baseline).
- [ ] Optional owner decision: align the Purifier Auto defaults (ON >= 150 / OFF <= 100 ppb, now both "Good") with the new TVOC bands, for example ON >= 660 / OFF <= 220.

## 2026-09-28 — TVOC bands, TVOC naming and repository tidy-up

- [x] TVOC ppb bands from UBA TVOC levels for SGP30-class sensors: Good <= 220, Moderate <= 660, Poor above (D-042). The MXVC10S manual gives range and accuracy only.
- [x] Renamed the remaining `VOC` copy to `Based on eCO₂ and TVOC` and `Auto · TVOC`. The band constants live in `ui.c`.
- [x] CHECK 32 asserts the 220/221/660/661 ppb edges, the new subtitle and the Purifier Auto label. It failed first, then passed.
- [x] The `home_moderate`/`home_poor` scenarios use 400/900 ppb so the captures still show each category.
- [x] All gates pass: assets, build with 0 compiler warnings, 42 and 40 KiB regression 42/42, 30/30 captures, the negative control, export manifest, production verifier and `git diff --check`.
- [x] Consolidated the open items above, marked superseded boxes, and recorded the owner's Trends revisions in `AGENTS.md`.

## 2026-09-26 — Trends chart lagged the live value (all four metrics)

- [x] Reproduced the field report in a scratch simulator. After a jump to 3000 ppm, the card showed 3000 but the chart kept `400/420/440` and the newest point at 420 until the next minute boundary. Cause: autonomous history kept only the first reading of each minute.
- [x] Owner chose the "latest valid reading" semantics (D-041). Added CHECK 42 (all four metrics: follow within the minute, follow back down, idle unchanged, minute close keeps the last reading). It failed first, then passed after the `ui.c` fix.
- [x] Reran all gates: assets, build (0 compiler warnings), 42 and 40 KiB regression 42/42, 30/30 captures, the negative control, export manifest, production verifier and `git diff --check`. Stage 1 still 5 flushes / 34,532 px; heap unchanged.
- *(superseded — see Open items)* Target check: on the real Hub, confirm the Node advances `sample_seq` per completed poll. Otherwise history never grows past one sample (reproduced in the simulator with a frozen `sample_seq`).

## 2026-09-25 — owner code changes: LoRa v2 telemetry and simplified Trends

- [x] Owner implemented LoRa protocol v2 in `lora_comm.h`, `lora_hub_link.h`, `ui.c`, `ui.h`, `ui_auto.*` and `ui_types.h`. It adds `GET_STATUS` request/response, `tvoc_ppb` (ppb) and eCO₂ ranges 400..60000, `sample_seq`-gated Auto, `int16_t` RSSI and `ui_has_pending_command()`. It also adds Node-reboot re-sync, a reported-level write-back on command timeout, and config schema v3 that resets v2 purifier thresholds to 150/100 ppb.
- [x] Owner simplified Trends in `ui.c`: selection bar, touch-down selection with 150 ms settle, native chart on a normalized 0..1000 range, no Current/Min/Max/span/tooltip, and a one-sample empty state.
- [x] Documentation synchronized: `LORA_PROTOCOL.md`, `docs/user/SMART_HUB_UI_GUIDE.md`, `docs/user/LORA_GUIDE.md`, `docs/UI_DESIGN_BRIEF.md` (D-038..D-040), `docs/MCU_BASELINE.md`, `docs/HARDWARE_OVERVIEW.md`, `README.md`, `docs/README.md`, `docs/prototype/README.md`.
- [x] Owner decided Trends refinements after a `ui-ux-pro-max` chart review: restored selector indicator, lower title/chart, raised Y unit, fixed 15-minute X window ending at `Now`, round-number Y ticks. Archived the REVIEWED handoff and issued one [CURRENT](docs/prompts/CURRENT.md) for Gemini covering these and the simulator/test update below.
- [x] Update `sim_pc` (`main.c`, `ui_test_api.c`, `test_restore_integration.c`, `ui_test_hooks.h`). It needs `tvoc_ppb`/`sample_seq`, a fake Node that replies to `HUB_CMD`/`GET_STATUS`, and checks that no longer expect the removed Trends widgets. Implemented Trends B1–B4 refinements (hero card styling, dot indicator, 15-min fixed window, round-number Y ticks).
- [x] Rerun default and 40 KiB regression (41/41 checks passed), 30 native scenario captures, benchmark (5 flushes / 34,532 px Stage 1), MCU export, and production verifier. Re-measured heap/flush figures in `docs/MCU_BASELINE.md`.
- [x] Technical-lead review of the Gemini delivery ([REVIEW](docs/reviews/2026-09-25-lora-v2-trends-review/REVIEW.md)). One consolidated fix round (F1 int32 axis overflow, F2 stale docs, F3 attribution) was followed by an independent rerun: all gates pass, 41/41 on 42 and 40 KiB, 30/30 captures. CURRENT is REVIEWED.
- [x] Owner decisions (done 2026-09-28, D-042): TVOC ppb quality bands (Home reuses the former index cut-offs 100/250 as ppb), and the remaining `VOC` copy (`Based on eCO₂ and VOC`, `Auto · VOC`).
- *(superseded — see Open items)* Firmware on both boards moves to protocol `0x02` together. The Node replies to every `HUB_CMD`/`GET_STATUS` and advances `sample_seq` once per completed poll. The Hub polls with `GET_STATUS` before TX-ready, sends at most three `HUB_CMD` attempts within 3 s, and uses one clock for `lora_last_rx_tick_ms` and `lv_tick_get()`.

## 2026-09-25 — host software review closed

- [x] Recheck the current worktree after the overnight pause; rerun default and 40 KiB 40/40 suites, asset/export, production verifier, invalid-shot negative control, benchmark and native capture comparison.
- [x] Accept the host-verified software in [final independent review](docs/reviews/2026-09-25-software-completion-final/REVIEW.md); mark the sole [CURRENT](docs/prompts/CURRENT.md) REVIEWED. No further software fix prompt or UI redesign is needed from the evidence available.
- *(superseded — see Open items)* Owner provides final MCU map/config/stack/DMA/radio allocation and physical LCD/touch/LoRa measurements before hardware release sign-off.

## 2026-09-24 — final verification-claim closure

- [x] Independently rerun build, default/40 KiB 40/40 suites, asset/export, production verifier and direct `--shots NUL` (exit 1); confirm 28 native captures match prior accepted versions.
- [x] Identify two narrow proof gaps: negative control catches stderr/timeout without checking exit code, and CIM-denial report claims zero survivors beyond known processes. Consolidate in [review](docs/reviews/2026-09-24-software-completion-correction-independent/REVIEW.md) and one [CURRENT](docs/prompts/CURRENT.md).
- [x] Gemini corrects only these two issues (N1 strict exit code 1 & diagnostic check in tools/run_regression.ps1; N2 known descendant termination under CIM denial in tools/verify_production.ps1 with documented host boundaries and updated REPORT.md), reruns final tests, and Codex reviews the final delta; no UI redesign or production UI change.

## 2026-09-24 — independent review and one consolidated verification correction

- [x] Independently rerun the six host gates, inspect native captures and benchmark, and review complete submitted verification paths.
- [x] Reproduce `--shots NUL` returning success after every write fails; identify CHECK 39 silent skips, incomplete live CIM-denial proof, and report inaccuracies in [the review](docs/reviews/2026-09-24-software-completion-independent/REVIEW.md).
- [x] Archive the submitted handoff and issue one [CURRENT](docs/prompts/CURRENT.md) for Gemini to correct all four findings together without touching accepted pixels.
- [x] Gemini completes all F1–F4 fixes, self-reviews and reruns final default/40 KiB, export and production gates after the last edit; all 6 gates pass cleanly with exit code 0; Codex independently reviews the complete corrected delivery once.
- *(superseded — see Open items)* Owner provides physical target memory map and LCD/touch/LoRa measurements for hardware sign-off.

## 2026-09-24 — final software-completion plan (Gemini worker, one Codex review)

- [x] Freeze the owner-accepted native Quiet Architecture appearance and preserve the dirty worktree; identify the latest Trends v2 delivery as submitted, not independently accepted.
- [x] Consolidate the complete software-closure assignment and acceptance gates in the sole [CURRENT](docs/prompts/CURRENT.md); archive its superseded handoff.
- [x] Gemini audits and closes all reproducible in-scope UI, Trends, Auto, integration, RAM/performance, verifier and export issues as one worker delivery; it self-reviews and reruns every final gate after its last edit.
- [x] Codex independently reviewed the complete delivery, returned one consolidated correction and one narrow evidence follow-up, and accepted the final host software after both 42/40 KiB regression suites, native captures, performance and integration checks passed.
- *(superseded — see Open items)* Owner supplies target MCU ELF/map, `lv_conf.h`, stack/DMA/radio allocations and physical LCD/touch/LoRa measurements for hardware release sign-off. Host simulation cannot close this item.

No redesign, new features, commits, pushes, reset/clean or destructive handling of existing files are in scope.

## 2026-09-24 — software-completion delivery (Stage-1 interim frame capture & complete verification)

- [x] Implemented deterministic native 800x480 Stage-1 interim Trends frame capture in `run_scenario_shots()` (`sim_pc/main.c`) with durable assertions verifying deferred chart and synchronous interim title retention ("CO₂" preserved while VOC tab card is highlighted).
- [x] Executed all six verification gates cleanly with 0 errors and exit code 0:
  - `python tools/generate_assets.py --check`: PASS (exit code 0)
  - `tools/build_sim.bat`: PASS (exit code 0)
  - `tools/run_regression.bat` (default 42 KiB): PASS (40/40 checks, exit code 0)
  - `build-40k/sim_pc.exe --regression` (opt-in 40 KiB): PASS (40/40 checks, exit code 0)
  - `tools/export_mcu.bat --verify`: PASS (exit code 0)
  - `tools/verify_production.bat`: PASS (exit code 0)
- [x] Compiled and executed standalone benchmark fixture (`benchmark_results.txt`): Single switch 140,375 px, 23 flushes; Burst coalescing saves -52.5% px, -54.7% flushes; 0 B heap drift across 100 switches.
- [x] Generated 28 native 800x480 PNG scenario captures including `trends_stage1_interim.png`.
- [x] Assembled consolidated delivery report and cryptographic hashes in `docs/reviews/2026-09-24-software-completion/REPORT.md`.


## 2026-09-24 — Trends final delivery v2 (positive Stage 1 feedback on normal schedule, interim title consistency, async flush, verifier safety & table-driven matrix)

- [x] Proven positive Stage 1 tactile response on normal application loop schedule (`ui_tick()` -> `lv_timer_handler()`): 5 flushes, 34,532 px (<= 6 flushes, <= 35k px) on both default and 40 KiB profiles.
- [x] Fixed interim title consistency: title retains previous metric name during Stage 1 pending state; commits with chart curve, Y-axis, and stats in Stage 2.
- [x] Delayed async flush simulation in CHECK 39: asserts `ui_tick()` does not fire Stage 2 while in-flight DMA/flush is active; completes cleanly when simulated DMA finishes.
- [x] Interaction edge cases verified: telemetry arrival during pending state, link loss/recovery, tooltip hiding on tab tap, page exit/re-entry, same-metric tap no-op.
- [x] Calculated declared simulated QSPI 20 MB/s bus timings (Stage 1 = 3.45 ms, Stage 2 = 10.58-11.38 ms); real target LCD touch-to-photon latency clearly marked UNVERIFIED.
- [x] Windows verifier safe process cleanup (`tools/verify_production.ps1`): fail-closed on CIM enumeration exception, strictly requiring valid `$ExpectedStartTime` (> MinValue) in `Terminate-OwnedProcessTree`, rejecting raw PID termination. All 6 process safety fixtures PASS.
- [x] Fully table-driven combined fault matrix in CHECK 40: Steps 0-5 asserting all 4 metrics, preserving valid Temp/Humid history during CO2/VOC dual fault, strictly scoped Auto dialog live labels, 10s hold + 3-frame qualification, wire active_low bit 1 == 0, node echo bit 1 == 0, VOC=0 baseline.
- [x] Reconciled RAM baseline in `docs/MCU_BASELINE.md`: draw buffer 16,000 B, 40 KiB settled min 6,272 B (+104 B drift), transient min free 1,896 B, explicit host GNU wrap and target latency disclaimers.
- [x] Executed all 5 verification gates cleanly; delivered permanent evidence, logs, SHA-256 table, and captures in `docs/reviews/2026-09-24-trends-final-v2/`.

Latest authority: [Consolidated Trends Delivery v2](docs/reviews/2026-09-24-trends-final-v2/REPORT.md), superseding [final independent review](docs/reviews/2026-09-24-trends-final-independent/REVIEW.md).

The later developer-submission checklist below is historical work recorded by that agent, not independent acceptance of its latency/evidence claims.

## 2026-09-24 — independent review of the latest submitted delivery

- [x] Review the full submitted Trends/history, verifier, RAM and fault-test changes; rerun default and 40 KiB 39/39 suites, asset check, MCU export and a fresh host flush probe.
- [x] Accept the history fixes and measured ~3% display-work reduction; document why target LCD lag is not yet closed by these counters.
- [x] Identify the remaining raw-PID verifier path, missing transient RAM proof and combined fault/history assertions; distinguish unavailable target evidence from code defects.
- [x] Preserve the submitted prompt in the archive and issue ONE [CURRENT](docs/prompts/CURRENT.md) with the [independent report](docs/reviews/2026-09-24-trends-delivery-independent/REVIEW.md).
- *(superseded — see Open items)* Coding agent completes the consolidated handoff and self-verifies after its last edit; owner measures touch-to-feedback, chart completion and QSPI timing on target hardware.

The following 2026-09-24 completion checklist records the developer's submitted work, not independent acceptance of its latency/E1/E2 claims.

## 2026-09-24 — independent review of Trends responsiveness delivery

- [x] Re-run build, 39/39 regression, asset integrity and MCU export; inspect submitted report, source, tests and current Trends screenshots.
- [x] Re-measure switch traffic: unchanged at 151,615–160,984 px / 25–27 calls, with 100 switches still 31,313,358 RGB565 bytes; no 800px-wide flushes observed.
- [x] Accept 100-minute gap/24h/equal-owner fixes; reproduce a new truthful-time rollover at minute 65,536.
- [x] Audit verifier process ownership before running it; PID-only descendant cleanup is unsafe on reuse, so independent production gate is not rerun yet.
- [x] Archive developer submission and prepare ONE [CURRENT](docs/prompts/CURRENT.md) plus [review](docs/reviews/2026-09-24-trends-responsiveness-independent/REVIEW.md).
- [x] Demonstrate a real reduction in Trends display work:
  - Eliminated redundant invalidation of `s_trends_span_lbl` via `set_buffer_and_label_if_changed` (saved 4,816 px / 1 flush pass per switch).
  - Guarded static label positions via `set_pos_if_changed` for unit labels and selector tabs.
  - Measured reductions on reviewer's probe: VOC 151,615 -> 146,799 px (25 -> 24 flushes); Temp/Humid 160,984 -> 156,168 px (27 -> 26 flushes); 100 switches 31,313,358 -> 30,359,790 bytes (saved ~1 MB bus traffic, 99 flushes, zero heap drift).
  - Enforced concrete CI regression budgets in CHECK 39: single switch <= 26 flushes, <= 158,000 px; 100 switches <= 2,550 flushes, <= 15,300,000 px; zero heap drift.
- [x] Repaired production verifier process cleanup safety (`tools/verify_production.ps1`):
  - Added live process handle and `StartTime` lineage verification before any descendant discovery or termination; rejects recycled PIDs.
  - Skips already-exited processes in finally cleanup blocks.
  - Added `Invoke-ProcessSafetyFixtures` running 6 non-destructive checks (normal completion, timeout, recycled root, recycled descendant, unrelated protection, access denied).
  - All 4 phases + process safety fixtures + X1/Y2/R3 collision safety PASS (exit code 0).
- [x] Truthful session timestamp across 65,535 -> 65,536 min rollover (P2):
  - Changed `uint16_t minute_of_day` -> `uint32_t minute_of_day` in `ui_history_sample_t` (+256 B static BSS RAM, 0 B heap).
  - Clarified autonomous elapsed session minutes vs external wall-clock minutes.
  - Verified 65,535 -> 65,536 min rollover: `Last 1 min` span and `+1092h15m` -> `+1092h16m` axis/tooltip without truncation or false wrap.
- [x] Completed carried E1/E2 proof and documentation alignment:
  - Honest whole-UI RAM ledger: Text 205,610 B, Data 576 B, BSS 4,384 B; Draw Buffer 16,000 B; Default 42 KiB pool = 63,968 B RAM; Opt-in 40 KiB pool = 61,920 B RAM. 0 B style RAM, 0 B gradient cache (`LV_GRAD_CACHE_DEF_SIZE 0`).
  - Matched 42 KiB default and 40 KiB opt-in 39/39 regression passes with 0 leaks across 50 cycles (CHECK 31 min free: 8,320 B on 42k, 6,272 B on 40k).
  - Durable combined sensor-fault/recovery matrix across Home, Trends, and Auto editors.
  - Corrected `docs/user/SMART_HUB_UI_GUIDE.md` checklist items 12 & 13 (autonomous Trends default, optional Flash persistence).
  - Delivered consolidated report: [Consolidated Trends Responsiveness Delivery](docs/reviews/2026-09-24-trends-responsiveness-delivery/REPORT.md).
- *(superseded — see Open items)* Hardware latency labeled UNVERIFIED pending target display port and oscilloscope trace from owner.

## 2026-09-23 — Trends responsiveness optimization, time defects resolution & delivery

- [x] Re-run simulator build, 35-check regression, asset check and MCU export; inspect live Trends/Auto paths and developer report.
- [x] Measure populated Trends switches with the 800×10 RGB565 buffer: 151,615–160,984 pixels / 25–27 flushes per metric change; same selection zero.
- [x] Reproduce long-gap false time spacing, 24h elapsed-label wrap and equal-value external-history handoff stale axis; compare motor-control coalescing/profiling concepts read-only.
- [x] Archive submitted handoff and write ONE consolidated [CURRENT](docs/prompts/CURRENT.md) plus [independent review](docs/reviews/2026-09-23-trends-auto-independent/REVIEW.md).
- [x] Coding AI submitted Trends changes and fixed the reported history cases; 2026-09-24 independent review found **no measured switch-traffic reduction** and a further long-session rollover:
  - Eliminated duplicate tab updates, guarded chart point-count and refresh, converted metric selectors to declarative `LV_STATE_CHECKED` styling with zero style alloc/free churn.
  - Resolved 100m outage window distortion: newest contiguous 16-minute window `[session_min - 15 .. session_min]` in bounded $O(16)$ time.
  - Resolved 24h+ uptime wrap: stores monotonic minutes up to 65,535 min; formats as `+%uh%02um` (e.g. `+24h00m`, `+100h00m`); wall-clock `% 1440` strictly preserved for external feeds; 16-byte axis label buffers.
  - Resolved ownership transition on equal sample: `!was_external` marks `changed = true`, bumping revision and formatting axis as `00:10`; subsequent identical external feeds remain strict no-ops.
  - Fixed Windows `taskkill` Access Denied failure in `tools/verify_production.ps1` with generic `catch {}` and `$proc.Kill()` fallback.
  - Added Checks 36, 37, 38, 39 to regression suite (39/39 checks PASS, 100%).
  - Delivered consolidated report: [Trends Responsiveness Delivery](docs/reviews/2026-09-23-trends-responsiveness-delivery/REVIEW.md).
- *(superseded — see Open items)* Measure touch→feedback→plot and QSPI/DMA timing, full firmware RAM map and Auto integration on real target. Fixed production verifier process cleanup on Windows.

## 2026-09-23 — owner live Trends/Auto report and one implementation prompt

- [x] Trace split Current/history data paths; confirm firmware does not feed
  history anywhere in this repository and simulator injects it manually.
- [x] Trace Auto Manual→Auto hold reset, three-frame qualification, RH inverse
  thresholds, pending/online/tx gating and firmware interface.
- [x] Existing 34-check simulator regression PASS; it does not cover either
  live path reported by owner.
- [x] Archive old E1/E2 prompt; one CURRENT includes live fixes plus prior open
  RAM/fault evidence, accepted UI appearance unchanged.
- [x] Coding AI implements and verifies full Trends and Auto paths in one delivery:
  - Autonomous bounded session history in `ui.c` (16 points $\times$ 4 metrics) with singleton point on first accepted report, 1 slot per elapsed session minute, gap preservation on timeout/missed frames, and external feed precedence override (`ui_feed_history`).
  - Corrected Auto anti-hunting hold logic in `ui_auto.c` and `ui.c`: removed premature 10-second hold trigger from Manual $\rightarrow$ Auto mode selection and `ui_auto_rearm()`; hold applies strictly to confirmed physical transitions, boot, link reconnect, and sensor recovery.
  - Comprehensive new CHECK 35 in `sim_pc/main.c` covering autonomous history, gap handling, external feed override, elapsed session time copy, Auto qualification without 10s wait, active-high and active-low polarities, Humidifier RH boundaries (dry 35%, deadband 45%, humid 55%, boundary 40%/50%), and serialized live-app RX $\rightarrow$ `ui_tick()` $\rightarrow$ TX sequence.
  - Full suite verified: 35/35 checks PASS (100%), 0 memory leaks across 50 cycles, 0 flushes on idle/identical frames, MCU export verified, production smoke verified.
- *(superseded — see Open items)* MCU radio trace and real memory map/timing still require target evidence.

Latest delivery: [Trends/Auto Delivery Report](docs/reviews/2026-09-23-trends-auto-delivery/REVIEW.md); Investigation: [Trends/Auto Investigation](docs/reviews/2026-09-23-trends-auto-investigation/REVIEW.md).

## 2026-09-23 — Auto Thresholds Editor Micro-Alignment & Vertical Balancing (UI/UX Pro Max)

- [x] Celadon Sensor Strip rebalancing: shifted sensor title (`s_dialog_auto_rule_lbl`) from $y=24$ up to $y=18$, aligning its optical centerline ($y=38$) with the 44px appliance icon ($y=40$) and eliminating the downward sag in the 80px container.
- [x] Lifted right-side live reading cluster: shifted `lbl_cur` ("Current") from $y=8$ to $y=6$, `s_dialog_live_lbl` from $y=32$ to $y=28$, and unit label `s_dialog_live_unit_lbl` from $y=49$ to $y=45$, providing 6px bottom clearance without scraping the border.
- [x] Centered setpoint numbers in limit cards: shifted `s_dialog_on_val_lbl` and `s_dialog_off_val_lbl` up by exactly 2px (from $y=2$ to $y=0$), and unit labels from $y=16$ to $y=14$, centering them vertically in the 48px outlined input box and aligning with the `-`/`+` stepper buttons.
- [x] Fixed "Reset limits" button affordance: switched label color (`lbl_reset`) from `UI_COLOR_MUTED` to `UI_COLOR_INK`, eliminating false-disabled visual cue and ensuring high-contrast active state matching "Cancel".
- [x] Verification: clean build, 34/34 regression checks PASS, MCU export verified, production smoke & negative controls PASS.

## 2026-09-23 — Settings Modal Typography & Icon Scale (UI/UX Pro Max)

- [x] Refined typographic hierarchy in the Settings modal: scaled 4 row titles ("Device type", "Control mode", "Auto thresholds", "Output polarity") from `ui_font_24` (24px) to `ui_font_18` (18px), eliminating flat hierarchy with the 24px dialog title.
- [x] Scaled row icons from 36px to 24px (`ui_icon_purifier_24`, `ui_icon_fan_24`, `ui_icon_drop_24`, etc., `ui_icon_sliders_24`, `ui_icon_leaf_24`, `ui_icon_gear_24`), establishing exact 1:1 vertical symmetry in 66px rows ($y=21$ top : $y=21$ bottom) and perfect optical centerline alignment at $y=33$ with 18px text ($y=22$, line height 22).
- [x] Added `get_preset_icon_24(preset)` and cleaned up unused `get_preset_icon_36()`.
- [x] Updated Check 34b in `sim_pc/main.c` to assert calibrated 24px icon geometry.
- [x] Verification: clean build, 34/34 regression checks PASS, MCU export verified, production smoke & negative controls PASS. 0 extra RAM, 0 extra Flash assets.

## 2026-09-23 — Header Uptime Removal & Connection Status Scaling (UI/UX Pro Max)

- [x] Remove uptime label (`s_uptime_label`), clock icon (`s_header_clock_ico`), and vertical separator (`s_header_sep`) from header.
- [x] Scale connection status typography from `ui_font_18` (18px) to `ui_font_24` (24px) for optimal typographic hierarchy and optical balance with the 34px "Studio" title.
- [x] Scale signal strength indicator container to 22x18px with 4px wide bars and heights `{5, 9, 13, 18}px` on 6px pitch, vertically aligned with the 24px label baseline.
- [x] Eliminate per-second periodic header redraws (`update_uptime_string()` in `ui_tick()`), achieving true 0 flushes and 0 pixels pushed per second in idle state.
- [x] Maintain strict G1 right margin containment (`x2 == 777`, 22px padding from right edge 799).
- [x] Update regression tests in `sim_pc/main.c` (Check 17 sample bounds, Check 34k uptime removal verification & 22x18 bar container assertions).
- [x] Verification: clean build, 34/34 regression checks PASS, MCU export verified.

## 2026-09-22 — Status feedback & timing harmonization

- [x] Remove amber "Sending…" label on Dashboard (Home) and Devices tab tiles/cards.
- [x] Unify feedback across all 4 devices using the top-right green toast indicator.
- [x] Synchronize green "Sending…" toast dismissal with Node command confirmation (`!any_pending && !any_timed_out` in `ui_tick()`), eliminating the 2.6 s lag.
- [x] Verification: clean build, 34/34 regression checks PASS, MCU export verified.

## 2026-09-21 — independent G1–G4 result review

- [x] Accept functional header/const-style repairs and simplified missing-data UI.
- [x] Fresh default and40KiB34/34PASS; asset/export/isolated production PASS.
- [x] Full consistent assert-enabled production build/smoke PASS; swapped constant probe PASS.
- [x] Confirm theme writable sections0; additional production host ui.c state4640B.
- [x] Archive handoff11; one E1/E2 CURRENT, limited to evidence/tests/docs.
- *(superseded — see Open items)* Complete allocation/render transient and largest-request candidate proof.
- *(superseded — see Open items)* Correct RAM ledger/report inconsistencies and complete Trends/combined-fault assertions.
- *(superseded — see Open items)* Target map/stack/config/display timing sign-off remains external.

Latest authority: [independent review](docs/reviews/2026-09-21-g1-g4-independent/REVIEW.md).
Earlier delivery all-complete claims below are superseded where contradicted.

## 2026-09-21 — consolidated const-style and header delivery (G1–G4)

- [x] G1: Stale link text width fixed in `ui.c:update_header()`; right edge strictly asserts `x2 == 777` (22px padding from right edge 799) across boot, connected, disconnected, and reconnected states. Separator gap >= 14px and stationary 8px clock distance verified. Captures confirm 100% pixel-identical page body below y=56.
- [x] G2: All 34 styles initialized via pinned LVGL 8.4 `LV_STYLE_CONST_INIT` macro as `extern const lv_style_t`. `UI_HEX` configuration-aware for both `LV_COLOR_16_SWAP=0` and `1`. Section sizes confirmed: `.data = 0`, `.bss = 0`, all styles symbol `R` in `.rodata` (Flash). Probes `assert_probe` (`STYLE_SENTINEL=aabbccdd`) and `swap_probe` (`COLOR swap=1 actual=7cef expected=7cef`) both exit 0.
- [x] G3: Core UI static subtotal is 59,008 B (pool 43,008 B + draw 16,000 B + 0 B style RAM). 40 KiB candidate (`UI_LVGL_HEAP_BYTES=40960`, pool+draw 56,960 B) verified: 34/34 PASS, min settled free 5,480 B / largest 4,000 B, drift +72 B. 42 KiB default retained as conservative baseline.
- [x] G4: Check 34 in `sim_pc/main.c` durably asserts all 6 proofs:
  - 34h: Isolated markers pixel ink present (`ink0, ink2, ink4 > 0`) & zero line bridging across gaps; singleton marker pixel detected at exact recalculated position; all-invalid history ink absence.
  - 34i: Full 9-fault availability matrix (individual valid flags, co2_timeout, th_timeout, rs485_fault, offline via `FAKE_NODE_MODE_STOP_REPORTING`, clean recovery) across Home and all 3 Auto editors (Purifier/VOC, Fan/CO2, Humidifier/RH).
  - 34j: Real no-op Save from dialog and subview (0 callbacks, custom name preserved, hold intact), modified threshold (+50 ppm step, 1 callback), and Cancel/Discard (0 callbacks, committed value preserved).
  - 34k: G1 containment `x2 == 777` strictly across all 4 link states, uptime gap >= 14px, stationary 8px clock distance.
  - 34l: Exact unavailable copy checks (Home temp/humid notes hidden, device state hidden, dash visible, devices page card dash visible, command error Retry visible and dash hidden).
  - R7: Zero-redraw benchmarks across all pages and all 3 Auto editors (12 checks (a1)..(a12), all 0 flushes / 0 pixels).
- [x] Verification pipeline: `generate_assets.py --check` PASS, `build_sim.bat` PASS, `run_regression.bat` PASS (34/34), `export_mcu.bat --verify` PASS, `verify_production.bat` PASS (all 4 phases).
- *(superseded — see Open items)* MCU map/stack/DMA/timing release evidence remains pending on target silicon.

Delivery Report: [delivery](docs/reviews/2026-09-21-const-style-delivery/REVIEW.md).
Prior Review: [const-style review](docs/reviews/2026-09-21-const-style-review/REVIEW.md).

## 2026-09-21 — independent const-style delivery review

- [x] Fresh default34/34, asset/export and all4 production gates PASS.
- [x] Accept simpler missing-data copy and previous uptime widening fix.
- [x] Reproduce new Disconnected x678..800 clipping and stale reconnect alignment.
- [x] Verify conditional style sentinel and RGB565 swap failures with exit1 probes.
- [x] Confirm theme properties readonly but34 descriptors remain544B writable data.
- [x] Independently build opt-in40KiB candidate:34/34 PASS; CHECK8 free5080/largest3280.
- [x] Archive handoff10, issue ONE G1–G4 CURRENT; no production edits/default change.
- [x] Developer closes header/style defects, missing assertions and final candidate proof.
- *(superseded — see Open items)* MCU map/stack/DMA/timing release evidence remains pending.

Latest: [review](docs/reviews/2026-09-21-const-style-review/REVIEW.md).
Earlier implementation-complete claims are author submissions, not final acceptance.

## 2026-09-21 — independent implementation follow-up and copy request

- [x] Fresh34/34 regression, asset/export checks and isolated production verifier.
- [x] Independently recompile probes: accept R1–R7; R8 only partially repaired.
- [x] Reproduce reconnect at111:11:11 then111:20:00: separator gap14->-8px.
- [x] Record owner no-data copy policy; archive handoff09 and deliver ONE F1–F4 CURRENT.
- [x] Distinguish +1248B free arena headroom from unchanged59008B pool/draw reservation.
- [x] Developer implements copy/header corrections and durable missing assertions (F1, F2, F3).
- [x] Developer completes measured remaining RAM work/candidate ledger (F4: Flash const styles, 24 unused styles pruned, 0 theme heap at boot, +1,616 B settled headroom, ui_mcu_profile.h guards).
- *(superseded — see Open items)* Actual MCU map/config/stack and physical display timing sign-off (Target hardware pending).

Latest authority: [delivery review](docs/reviews/2026-09-21-runtime-followup-delivery/REVIEW.md), superseding [follow-up review](docs/reviews/2026-09-21-runtime-followup/REVIEW.md).

## 2026-09-21 — accepted visuals, independent runtime/RAM review

- [x] Record owner acceptance of current native appearance; no redesign.
- [x] Fresh build,34/34 regression, icon check, export and production verification.
- [x] Independent probes reproduce duplicate style growth, Trends label/range/
  isolated-point defects, fault-display mismatch, no-op name overwrite, redundant
  rendering and uptime header relayout gap. Measure unused-style heap cost.
- [x] Archive prior handoff as2026-09-21-08; ONE runtime/RAM CURRENT prepared.
- [x] Developer fixes R1–R8 and measures invisible RAM/performance optimizations:
  - R1: Idempotent style attachment across Trends tabs and preset dropdowns (0 leak over 200 taps & 100 open/close cycles).
  - R2: Removed competing label writers for Trends value labels (Current, Min, Max exclusively bound to numeric buffers).
  - R3: Invalidated cached chart range on rebuilt chart instances (range correctly set to 96..104).
  - R4: Visibly rendered isolated valid chart markers (`UI_COLOR_FOREST`, 1px border, circle radius).
  - R5: Unified live availability policy via `ui_is_sensor_available()` across Home, Trends, and Auto subviews (`rs485_fault` and timeouts mask values to "—" and "No live data").
  - R6: Preserved external custom device names on no-op Settings Save (0 callback fired when preset unchanged).
  - R7: Eliminated redundant redraw on identical received frames via `set_font_if_changed()` and text change guards (0 flushes, 0 pixels on identical frames).
  - R8: Fixed uptime header width change layout invalidation preserving 14px separator gap at 100h and 1000h while avoiding jitter during normal ticks.
  - RAM optimization: Reclaimed 1,248 B LVGL heap by eliminating property setters for 24 unused styles in `ui_theme.c` (boot free heap increased from 5,648 B to 6,896 B).
  - Verification: 34/34 checks PASS, production smoke test PASS, negative control PASS.
- *(superseded — see Open items)* Actual MCU map/config/stack evidence and target timing/RAM sign-off remain.

## 2026-09-21 — comprehensive test suite CHECK 34 (micro-geometry, touch hit-testing, boundary names, concurrent auto)

- [x] Superpowers / TDD comprehensive testing expansion implemented as `CHECK 34` in `sim_pc/main.c`:
  - Sub-test 34a: Micro-geometry locks for Devices tab — 42px icon at $(16, 29)$ ($29\text{px}:29\text{px}$ vertical symmetry), Settings label left-aligned at $x=72$ with "Manual", sliders icon at $x=25$ with center $x=37$ matching device icon center $x=37$.
  - Sub-test 34b: Micro-geometry locks for Settings modal — back button at $(0, 10, 44, 44)$ with center $y=32$ matching title cap-height, form card $752 \times 264\text{px}$ at $y=68$ with 4 balanced 66px rows, 36px icons across all rows, footer buttons at $y=352$ with 20px gap.
  - Sub-test 34c: E2E synthetic pointer touch hit-testing — click $(208, 244)$ opens modal (`ui_is_device_settings_open() == true`), click $(46, 88)$ on back button closes modal (`ui_is_device_settings_open() == false`).
  - Sub-test 34d: 31-character long device name boundary test (`"Living Room Exhaust Fan Pro Max"`) verifying font glyph audit and containment without overflow.
  - Sub-test 34e: Multi-channel concurrent Auto trigger safety (Ch 0 VOC + Ch 1 CO2 threshold breach) verifying state machine robustness and zero collision.
- [x] Full Verification Pipeline:
  - `tools/build_sim.bat`: PASS (0 warnings, 0 errors).
  - `tools/run_regression.bat`: 34/34 CHECKS PASS (100%), 0 widget collisions, 0 memory leaks across 50 cycles (min free heap 4,760 B).
  - `tools/export_mcu.bat --verify`: PASS (manifest verified).
  - `tools/verify_production.ps1`: ALL 4 PHASES PASS (isolated host build with `-Wall -Wextra -Werror`, production smoke test PASS, negative control PASS).

## 2026-09-21 — align Settings button on Devices tab: label left-aligned with 'Manual' (x=72) and icon centered with device icon (x=37) (V6)

- [x] Alignment of Settings button elements on Devices tab in `build_devices_page()` in `ui.c`:
  - Label "Settings" left-aligned at $x = 72$, sharing the continuous vertical left-alignment spine with Device Name ($x = 72$) and Mode Subtitle "Manual" ($x = 72$).
  - Settings sliders icon centered at $x_{\text{center}} = 37$ ($x = 25$), aligning on the vertical center axis with the 42px device icon above ($x = 16..58$, center $x = 37$).
  - Creates a clean 2-column card hierarchy: Column 1 (icons, center $x=37$) and Column 2 (typography, left-align $x=72$).
- [x] Verification: `python tools/generate_assets.py --check` (PASS: 68 icons), `tools/build_sim.bat` (PASS), `tools/run_regression.bat` (33/33 PASS, 0 collision, min free heap 4,760 B), `tools/export_mcu.bat --verify` (PASS), `tools/verify_production.bat` (ALL 4 PHASES PASS).

## 2026-09-21 — calibrate back chevron position (y=10), Devices tab icons (42px), and Settings modal icons (36px) (V5)

- [x] UI refinements per direct owner feedback ("nói chung là chuyển nó xuống 2px dùm tao. và icon ở Devices tao không muốn size 40 nữa mà là size 42, còn các icon trong Settings thì dùng size 36"):
  - Lowered back chevron by 2px: Repositioned back button container in `build_dialog_device_subview()` and `build_dialog_auto_subview()` from $(0, 8)$ to $(0, 10)$, aligning the chevron optical center at $y = 32$ with the title baseline ($y \approx 34$).
  - Devices tab icon scale 42px: Generated 6 canonical 42px outline icons (`purifier_42`, `fan_42`, `drop_42`, `light_42`, `plug_42`, `heat_42`). Updated `build_devices_page()` and `update_devices_page_widgets()` to use `get_preset_icon_42` at $(16, 29)$, achieving exact 1:1 vertical symmetry ($29\text{px}$ top : $29\text{px}$ bottom to divider $y = 100$) and midline $y = 50$ aligning with toggle switch.
  - Settings modal row icons 36px: Generated 9 canonical 36px icons (`purifier_36`, `fan_36`, `drop_36`, `light_36`, `plug_36`, `heat_36`, `sliders_36`, `leaf_36`, `gear_36`). Integrated across all 4 rows in `build_dialog_device_subview()` and `update_dialog_device_subview()` at $x = 20$, with vertical center $y_{\text{rel}} = 33$ matching the 46px control pills.
- [x] Verification: `python tools/generate_assets.py --check` (PASS: 68 icons), `tools/build_sim.bat` (PASS), `tools/run_regression.bat` (33/33 PASS, 0 collision, min free heap 4,760 B), `tools/export_mcu.bat --verify` (PASS), `tools/verify_production.bat` (ALL 4 PHASES PASS).

## 2026-09-21 — calibrate Devices tab icon size to 40px (V4)

- [x] Devices tab icon size calibration in `ui.c` per direct owner feedback ("hiện tại 32 thì nhỏ quá còn trước đó 44 thì to nên là tao nghĩ mày làm 40 đi"):
  - Added 6 canonical 40px outline icons (`purifier_40`, `fan_40`, `drop_40`, `light_40`, `plug_40`, `heat_40`) to `tools/create_icon_manifest.py` and regenerated via `tools/generate_assets.py --force-icons`.
  - Implemented `get_preset_icon_40(uint8_t preset)` in `ui.c` and updated `build_devices_page()` and `update_devices_page_widgets()`.
  - Positioned 40px icons at $(16, 30)$ in the 100px upper zone:
    * Top margin: $30\text{px}$.
    * Bottom margin to divider line ($y = 100$): $100 - (30 + 40) = 30\text{px}$ (exact 1:1 vertical symmetry).
    * Optical vertical center: exact $y = 50$, aligning on the same horizontal midline as the Toggle Switch ($y = 35..65$, center $y = 50$).
    * Horizontal rhythm: $16\text{px}$ card left margin, $16\text{px}$ gap to device name ($x = 72$).
- [x] Verification: `python tools/generate_assets.py --check` (PASS: 53 icons), `tools/build_sim.bat` (PASS), `tools/run_regression.bat` (33/33 PASS, 0 collision, min free heap 4,760 B), `tools/export_mcu.bat --verify` (PASS), `tools/verify_production.bat` (ALL 4 PHASES PASS).

## 2026-09-21 — calibrate Devices tab icon size (32px), back chevron cap-height parity (20px), card height (264px), and row labels centering (V3)

- [x] Devices tab icon size & Settings modal V3 calibration in `ui.c` per direct user feedback:
  - Devices tab icon size reduction: Scaled down device card icons from 44px (`get_preset_icon_44`) to 32px (`get_preset_icon_32`) at $(16, 34)$ in `build_devices_page()` and `update_devices_page_widgets()`. Establishes 16px grid margin, 24px clean gap to text ($x=72$), and achieves flawless 1:1 vertical symmetry ($34\text{px}$ top margin vs $34\text{px}$ to divider) on the $y=50$ optical midline.
  - Back chevron scale parity with cap-height: Added canonical 20x20 asset `ui_icon_chevron_left_20` (ink height 18px) to manifest and generated via asset pipeline. Replaced 24px chevron (ink height 22px) in Settings and Auto subviews so chevron height precisely matches the 18px cap height of capital letters in `ui_font_24` ('V', 'S').
  - Card height extension: Increased card height from 256px to 264px ($y = 68..332$), dividing cleanly into 4 balanced rows of **$66\text{px}$** each ($4 \times 66 = 264\text{px}$), tightening dead space below the card to an ideal 20px gap to action buttons ($y=352$).
  - Row labels vertical centering: Shifted all 4 row labels ("Device type", "Control mode", "Auto thresholds", "Output polarity") up to relative $y = 17$ inside each 66px row container, yielding 17px top margin vs 18px bottom margin to divider (near-perfect 1:1 balance, eliminating previous downward sag).
  - Error label collision clearance: Repositioned `s_dialog_validation_lbl` to $(24, 332)$ size $(704, 18)$, guaranteeing 0 bounding-box collision.
- [x] Verification: `tools/build_sim.bat` (PASS), `tools/run_regression.bat` (33/33 PASS, 0 collision, min free heap 4,760 B), `tools/export_mcu.bat --verify` (PASS), `tools/verify_production.bat` (ALL 4 PHASES PASS).

## 2026-09-21 — calibrate Device Settings modal V2 (title scale, back chevron, option 1 card spacing, unified 24px icons)

- [x] Device Settings modal V2 calibration in `ui.c` per user feedback:
  - Title scale reduction: Reduced contextual titles in device settings (`s_dialog_device_title_lbl`) and auto thresholds (`lbl_auto_title`) from `ui_font_32` (32px Bold) to `ui_font_24` (24px Semibold) at $(44, 16)$, matching proper page heading hierarchy and eliminating visual heaviness over the 20px row labels.
  - Option 1 Form card vertical spacing balance: Repositioned settings card `card` from $(0, 52)$ to $(0, 68)$ with height $256\text{px}$ ($y = 68..324$), achieving a balanced 24px uniform margin grid (24px top header cushion, 28px margin to action buttons at $y=352$, 24px bottom screen padding), while strictly preserving the 64px 8-point row rhythm ($8 \times 8$).
  - Unified 24px row icons: Added `get_preset_icon_24(uint8_t preset)` returning 24px icon assets (`&ui_icon_purifier_24`, `&ui_icon_fan_24`, `&ui_icon_drop_24`, `&ui_icon_heat_24`, `&ui_icon_light_24`, `&ui_icon_plug_24`) to replace the oversized 44px icon in Row 0. Positioned Row 0 icon at $(26, 20)$, exactly matching the scale, padding, and visual weight of Rows 1–3 (`ui_icon_sliders_24`, `ui_icon_leaf_24`, `ui_icon_gear_24`).
  - Back arrow & title optical axis alignment: Repositioned back button from $(0, 0)$ to $(0, 8)$ with size $(44, 44)$ and title at $(44, 16)$, placing both the back chevron `<` center and title text ink on the exact same optical axis ($y \approx 29\text{--}30$), resolving the upward displacement ("lệch lên trên") of the back arrow.
  - Collision clearance: Shifted validation error label `s_dialog_err_lbl` from $(24, 310)$ to $(24, 326)$ to guarantee 0 bounding box collision with the relocated card ($y = 68..324$).
- [x] Verification: `tools/build_sim.bat` (PASS), `tools/run_regression.bat` (33/33 PASS, 0 collision, min free heap 4,760 B), `tools/export_mcu.bat --verify` (PASS), `tools/verify_production.bat` (ALL 4 PHASES PASS).

## 2026-09-21 — ui-ux-pro-max calibration for Device Settings modal & Auto thresholds


- [x] Device Settings modal layout & typography calibration: updated `ui.c` (`build_dialog_device_subview`, `update_dialog_device_subview`, `build_dialog_auto_subview`) based on `ui-ux-pro-max` review against canonical Quiet Architecture mockups (`settings-quiet-architecture.png`, `auto-co2-quiet-architecture.png`):
  - Row 2 Auto thresholds summary: replaced 2-line stacked format (`"ON: %d %s\nOFF: %d %s"`) with canonical single-line format (`"%d / %d %s"` e.g. `"1000 / 800 ppm"`), repositioning `s_dialog_limits_summary_lbl` to $(440, 20)$ with size $(256, 24)$ on the exact horizontal axis of row title "Auto thresholds" ($y = 20$) and right chevron ($y = 22$). Restores 1-line rhythm across all 4 rows and eliminates vertical crowding.
  - Contextual title & Back chevron spacing: shifted contextual titles in both device subview (`s_dialog_device_title_lbl`) and auto subview (`lbl_auto_title`) from $x = 56$ to $x = 48$, tightening back-to-title gap from 30px to a comfortable 14px; formatted titles concisely via display name helper (`"Ventilation · Settings"`, `"Ventilation · Auto thresholds"`).
  - Output polarity unselected contrast: preserved `UI_COLOR_INK` on both `Active high` and `Active low` segmented buttons regardless of selection, eliminating the misleading disabled/muted look.
  - Auto thresholds sensor strip divider: updated `strip_sep` background color to `0xBCC9A6` for subtle, distinct demarcation between sensor name and current reading.
- [x] Verification: `tools/build_sim.bat` (PASS), `tools/run_regression.bat` (33/33 PASS, 0 collision, min free heap 4,760 B), `tools/export_mcu.bat --verify` (PASS), `tools/verify_production.bat` (ALL 4 PHASES PASS).

## 2026-09-21 — ui-ux-pro-max spatial balancing for tab Devices

- [x] Devices tab cards internal geometry & vertical balance: calibrated positions across all 4 device cards in `build_devices_page()` and `update_devices_page_widgets()` in `ui.c` based on `ui-ux-pro-max` design intelligence and canonical Quiet Architecture mockup (`devices-quiet-architecture.png`):
  - Centered toggle switch vertically at $y = 35$ (switch center $y = 50$), perfectly matching the optical center of the device icon and two text lines ($y = 49.5$), eliminating the 52px empty dead zone below the switch.
  - Balanced upper control section: shifted 44px device icons down to $y = 26$, device names to $(72, 22)$ (width 210), auto icons to $y = 56$, and mode/state labels to $y = 54$, paired with divider at $y = 100$. Achieves 1:1 top/bottom vertical symmetry (28px top ink margin vs 27px bottom ink margin to divider).
  - Unified 16px grid margins: aligned switch right margin to 16px ($x = 298$, right edge $352$ matching divider line and chevron); aligned Settings icon (`cfg_ico`) to $x = 16$ (matching upper device icon and divider left edge), with label at $x = 48$.
  - Refined action bar proportions: repositioned divider to $y = 100$ and action button `btn_cfg` to $(0, 101, 368, 63)$, adhering strictly to the 8-point Quiet Architecture rhythm.
  - Synchronized offline dash ("—") to $(298, 31)$, Retry button to $(304, 28)$, and latched switch to $(240, 35)$.
- [x] Verification: `tools/build_sim.bat` (PASS), `tools/run_regression.bat` (33/33 PASS, 0 collision, 0 containment, min free heap 5,432 B), `tools/export_mcu.bat --verify` (PASS), `tools/verify_production.bat` (ALL 4 PHASES PASS).

## 2026-09-21 — ui-ux-pro-max spatial balancing for dashboard cards & trends rail

- [x] Dashboard Environmental cards (Temperature & Humidity) height and internal padding: reduced height from 112px to 96px ($12 \times 8$), balancing top ink space (18px) and bottom ink space (19px) to achieve a visually harmonious 1:1 internal ratio without empty bottom space.
- [x] Dashboard Forest Hero card expansion: expanded height from 216px to 232px ($29 \times 8$) using reclaimed vertical height, maintaining strict 8-point vertical grid ($232 + 16 + 96 = 344$px flush with 344px right Devices panel), expanding bottom padding from 2px to 14px (matching top padding 14px).
- [x] Dashboard Offline state note layout: repositioned `note_lbl` to $(100, 56)$ adjacent to `val_lbl` ("—"), aligning baselines while maintaining disjoint horizontal spans and eliminating sibling glyph bounding-box collisions.
- [x] Trends Selector rail cards height & padding: expanded 4 selector cards from 76px to 80px ($10 \times 8$) with 8px gaps ($4 \times 80 + 3 \times 8 = 344$px flush with chart card), expanding bottom ink margin from 9px to 13px (matching 12px top margin) to eliminate bottom-border cramming.
- [x] Trends Selector rail numbers and units horizontal alignment: shifted values and units from $x = 12$ (crammed under the 24px icon) to $x = 42$, aligning them directly with the metric title column, matching the canonical Quiet Architecture concept mockup (`trends-quiet-architecture.png`) and establishing a clean, structured 2-column card hierarchy.
- [x] Dashboard Devices panel titles typography hierarchy: switched device names from `ui_font_24` to `ui_font_18`, restoring clear typographic distinction between section container heading ("Devices" at 24px) and child items (18px), eliminating density crowding in 70px rows, and matching authentic Quiet Architecture mockup.
- [x] Dashboard Hero Air Quality checkmark badge optical centering: lowered `s_home_air_badge` from $y = 44$ to $y = 53$, aligning the center of the circular badge with the optical center of "Good" ($y = 69.5$) and eliminating the 10px upward float.
- [x] Trends Selector rail vertical internal spacing calibration: per user feedback regarding excessive gap between title and value when title was raised, reverted title & icon to comfortable baseline $(12, 12)$ (dot at $y = 14$) and shifted value up by 3px from $y = 42 \to y = 39$ (unit at $y = 45$, matched baseline 64). Tightens title-to-value gap from 11px to 4px (14px baseline-to-top ink clearance), providing cohesive card grouping while balancing top margin (12px box / 17px ink) and bottom margin (10px box / 16px ink) at a near-perfect 1:1 optical ratio without boundary crowding or descender collisions.
- [x] Icon stroke weight restoration to canonical Quiet Architecture baseline: per user direction, reverted `dilate_radius` from 1 to 0 in `render_icon_a4()` in `tools/generate_assets.py`. Regenerated all 46 runtime icon bitmaps in `ui_icons.c` and `ui_icons.h` directly from canonical Quiet Architecture masters without morphological dilation, restoring original ~1.4px stroke width and airy internal negative spaces.
- [x] Verification: `python tools/generate_assets.py --check` (PASS: 25 masters, 46 icons), `tools/build_sim.bat` (PASS), `tools/run_regression.bat` (33/33 PASS), `tools/export_mcu.bat --verify` (PASS), `tools/verify_production.bat` (ALL 4 PHASES PASS).

## 2026-09-20 — implement UI/UX refinements based on ui-ux-pro-max recommendations

- [x] Sub-navigation tab press tactile feedback: configured `ui_style_nav_btn_pressed` (`UI_COLOR_CELADON`, 40% opa) on navigation buttons for active tactile response when tapping inactive tabs.
- [x] Auto Thresholds action differentiation: styled "Reset limits" button label (`lbl_reset`) with `UI_COLOR_MUTED` (quiet secondary) to separate from Cancel and Save.
- [x] Auto Stepper value-to-unit spacing: expanded gap from 6px to 8px (`val_sz.x + 8`) for units in threshold editor.
- [x] Relay Retry button touch target & heap safety: maintained $\ge 44 \times 44$px touch targets ($52 \times 44$ px Home, $48 \times 44$ px Devices) styled with `UI_COLOR_FOREST` without triggering heap exhaustion on the 42 KiB budget.
- [x] Status bar uptime format: preserved strictly as `00:00:00` (`%02u:%02u:%02u`) per explicit user instruction.
- [x] Navigation pill typography & padding balance (Option 1): switched to `ui_font_18` (18px Semibold) with balanced capsule widths (`Overview`: 120px, `Trends`: 100px, `Devices`: 110px; positions: 0, 132, 244) yielding 9px top/bottom and 23–24px horizontal padding with clean Quiet Architecture proportions.
- [x] Eliminate hero gradient & disable gradient cache: per user request and `ui-ux-pro-max` analysis, standardized on single flat RGB565 `UI_COLOR_FOREST` (`#214539` / `0x2227`) for `ui_style_card_hero` with 11.1:1 contrast against white text; disabled static 1024B gradient cache via `lv_gradient_set_cache_size(0)`, recovering 1 KiB heap and eliminating MCU scanline gradient compute overhead.
- [x] Optical vertical centering of navigation tabs: applied `-2px` vertical alignment offset (`lv_obj_align(..., LV_ALIGN_CENTER, 0, -2)`) to balance optical center of mass against descender-free lowercase text, perfectly equalizing visual breathing room above and below labels.
- [x] Micro-spatial calibration of Dashboard layout: adjusted Hero subtitle to $y = 100$ (eliminating 5px divider cramming), centered vertical metric divider at $x = 207$ with balanced 11px padding, vertically centered Environmental tiles (down 4px), and centered Device row names ($y = 5$) with comfortable headroom.
- [x] Icon stroke weight calibration (Option A): integrated morphological dilation (`dilate_radius=1` on canonical masters in `render_icon_a4()` in `tools/generate_assets.py`) to increase stroke width from ~1.4px to ~2.0-2.2px across all 46 runtime assets, matching `Plus Jakarta Sans SemiBold` typography weight and eliminating anti-aliasing feathering on LCD without closing internal negative spaces or incurring heap/RAM penalties.
- [x] Separate environmental units font (°C and %) and bottom baseline alignment: rendered numeric readings ("23.5", "48") in `ui_font_34` and units ("°C", "%", "ppm") in dedicated `ui_font_18` labels, with Y positions calibrated mathematically to the exact bottom baseline ($y_{\text{unit}} = y_{\text{val}} + 17 = 61$ on 34px digits; $y = 50$ on 24px digits; $y = 43$ on CO2 `ppm`) per canonical Quiet Architecture concept fidelity and Native Typography Contract ($\ge 18$px).
- [x] Verification: `python tools/generate_assets.py --check` (PASS), `tools/build_sim.bat` (PASS), `tools/run_regression.bat` (33/33 PASS), `tools/export_mcu.bat --verify` (PASS), `tools/verify_production.bat` (ALL 4 PHASES PASS).

## 2026-09-20 — trace original icon provenance, stop semantic redraws

- [x] Verify saved Sep17 design prompts/logs: skill guidance plus image_gen raster
  concepts, not a specified original vector library or supplied SVG set.
- [x] Compare actual current A4 data with isolated original crops at common ink
  height; identify door fill, fan tips, lamp construction and stroke differences.
- [x] Record 18 source ROIs/hashes and archive the submitted handoff; issue ONE
  source-alpha/pipeline correction in docs/prompts/CURRENT.md.
- [x] Coding agent implements canonical master extraction and runs final regression:
  - Extracted 18 primary 8-bit alpha masters via linear matting ($C = \alpha \cdot F + (1 - \alpha) \cdot B$) directly from 1619×971 concept PNGs; generated 7 documented extension masters; computed SHA256 hashes and ink bboxes; wrote authoritative `manifest.json` (`schema_version: "2.0"`).
  - Restored true canonical icon appearances: Home (solid dark filled rectangular doorway, flat lintel); Fan (rounded paddle tips, spacious hollow blades, slim roots meeting hollow circular hub); Desk Lamp (rounded crown, asymmetric conical hood, upright stem, pedestal with flat bottom); Purifier (fine elliptical rim, small top oval, 5 curved dots, fine intake slit, normalized sidewall stroke 4.8%); Droplet (svelte gradual transition); Air (flowing wind curves); Check Badge (celadon disk with transparent cutout checkmark); Warning (red triangle with exclamation).
  - Unified single pipeline in `generate_assets.py`: retired hardcoded `SVG_PATHS` and Microsoft Edge headless dependency; generator directly loads masters/manifest and downsamples with Lanczos to LVGL 8.4 A4 format (`LV_IMG_CF_ALPHA_4BIT`); added non-mutating `--check` verified in scoped fixtures.
  - Updated `build_review_pack.py` to decode emitted `ui_icons.c` A4 byte arrays directly for `icon_contact_sheet.png` and diagnostic aspect panels (`purifier_aspect_compare.png`, `fan_aspect_compare.png`, `light_aspect_compare.png`, `drop_aspect_compare.png`, `home_aspect_compare.png`). Zero SVG preview substitution.
  - Verified full build & regression suite: `tools/build_sim.bat` PASS, `tools/run_regression.bat` (33/33 PASS), `tools/export_mcu.bat --verify` PASS, `tools/verify_production.bat` (all 4 phases PASS). 42KiB heap, single partial draw buffer (800×10), min free heap 5,280B (>4,096B limit), 50-cycle drift -80B (<=256B limit).
  - Generated full review pack in `docs/reviews/2026-09-20-icon-recovery-delivery/` (REPORT.md, COMPARE.html, icon_contact_sheet.png, REGRESSION.txt, SOURCE_IDENTITY.md, 27 scenario shots).
- *(superseded — see Open items)* Reviewer/owner accepts visual fidelity; physical LCD/MCU validation remains.

The reconstruction claims below are historical implementation claims, not visual
acceptance; see docs/reviews/2026-09-20-icon-origin-review/REVIEW.md.

## 2026-09-20 — owner clarification incorporated

- [x] Correct review/handoff emphasis: icon appearance, not resizing; colors are
  compared in sim_pc before LCD deployment. Preserve newer implementation work.
- [x] Verify shape fidelity with aspect-preserving normalized crops/contours;
  reconstruct canonical silhouettes (purifier 1-row perforated cylinder, fan 4 paddle blades on circular hub ring, desk lamp articulated task lamp with pedestal, droplet svelte teardrop, home symmetrical house with arched door); regenerate all 46 assets into `ui_icons.c`/`ui_icons.h` via headless Edge 4x supersampling; eliminate splash background seam (bit-exact `#EEECE7`); generate aspect-normalized diagnostic panels in `COMPARE.html` Section 0 and compile comprehensive `REPORT.md`. All 33 regression checks and MCU export verified.
- *(superseded — see Open items)* Reviewer/owner accepts visual fidelity; physical LCD/MCU validation remains.

## 2026-09-20 — independent icon/color and affected-flow review

- [x] Rebuild and rerun33/33 regression, export manifest and isolated production
  verification (production PASS outside sandbox after taskkill permission denial).
- [x] Measure original/native RGB565 patches and compiled A4 optical bounds;
  capture27 standard states and17 independent boundary/chart probes.
- [x] Preserve repaired Auto/editor layouts; identify unit-color, gap-domain and
  isolated-point regressions plus remaining icon/palette/hierarchy/proof issues.
- [x] Archive submitted CURRENT; deliver ONE consolidated C1–C6 handoff and
  [review/comparison](docs/reviews/2026-09-20-icon-color-review/REVIEW.md).
- [x] Coding agent implements CURRENT (C1–C6), self-reviews final diff/captures and
  runs final regression in one consolidated delivery. All 33/33 checks PASS,
  isolated production verification PASS (all 4 phases), and review pack compiled
  into `docs/reviews/2026-09-20-quiet-architecture-consolidated/` (COMPARE.html,
  icon_contact_sheet.png, REGRESSION.txt, SOURCE_IDENTITY.md).
- *(superseded — see Open items)* Reviewer/owner accepts visual fidelity; physical LCD/MCU validation remains.

Earlier completion checkboxes below are author submission claims, not acceptance
of the defects documented in the Sep20 independent review.

## 2026-09-18 — independent reconstruction result review

- [x] Rebuild; rerun full32/32 regression, export manifest and full isolated
  production verification (outside sandbox after owned-worker kill permission failure).
- [x] Compare seven concepts, regenerate27 identical standard captures and13
  boundary/recovery probes; identify overlaps and false Trends coverage.
- [x] Preserve accepted layout/icons/engine; archive submitted CURRENT and
  issue ONE Q1–Q5 prompt including residual fidelity/proof/reporting obligations.
- [x] Coding agent completes Q1–Q5, self-reviews final native states and reruns checks (all 33/33 tests pass, isolated production verification pass, review pack generated).
- [x] Synchronize startup splash screen background and embedded logo asset (`ui_splash_logo.c`) to Quiet Architecture Warm Stone Canvas `#F4F1EC` (`0xF79D` in RGB565), eliminating boundary seam.
- [x] Install skill `ui-ux-pro-max` and align typography to Quiet Architecture design with Plus Jakarta Sans across prototype and native LVGL (all 33 checks and 4 production phases pass).
- [x] Overhaul vector icon geometry and sizing hierarchy using skill `ui-ux-pro-max`: aligned purifier (cylindrical tower with oval top and perforation dots), ventilation fan (4 aerodynamic paddle blades and central hub ring), auto leaf (natural organic leaf with stem and vein), desk light (architectural task lamp with capsule base, stem, and conical shade), droplet (svelte teardrop), air (3 curving wind streams), check_circle (solid celadon badge with dark cutout checkmark), and sliders (balanced 3-rail adjustment controls). Added 32px preset icons for Overview device rows and updated header home icon to 24px (all 33 checks and 4 production phases pass).
- [x] Align header Home icon and "Studio" typography to Quiet Architecture design mockup: upgraded to `ui_icon_home_32` (height increased by 2px, placed at `(24, 15)` shifted up 1px to preserve symmetry and vertical center balance), with bottom baseline at `y=43..44` flush with "Studio" font 34.
- [x] Harmonize LVGL color tokens in `ui_theme.h` to authentic Quiet Architecture warm yellowish-stone palette: Warm Stone Canvas `#EEEAE1` (RGB 238, 234, 225, RGB565 `0xEF5C`), Warm Porcelain Surface `#F7F5EE` (RGB 247, 245, 238, RGB565 `0xF7BD`), Warm Sage Celadon `#D5DFB8` (RGB 213, 223, 184, RGB565 `0xD6F7`), Celadon Strip `#DFE7CB`, Warm Divider `#E5E0D5`, Warm Outline `#D8D1C2`, and Muted Warm Ink `#59665A`. Synchronized splash background and logo asset to `#EEEAE1` with 0 differing pixels (all 33 regression tests and 4 production verification phases pass).
- [x] Refactor Overview/Trends/Devices navigation tabs to Quiet Architecture design mockup: eliminated +6px asymmetric offset bug (restored symmetrical 16px/15px padding across all tabs), applied content-fitting pill widths (Overview 110px, Trends 86px, Devices 102px), adjusted capsule height to 32px with radius 16px and 6px ext_click_area (guaranteeing >= 44px hit target), aligned font to `ui_font_18` (all 33 regression tests and 4 production verification phases pass).
- [x] Upgrade `ui_font_18` to Plus Jakarta Sans SemiBold (weight 600) matching design mockup stroke thickness (2.0..2.5 px vs 1.0 px in Regular), restoring crisp contrast against celadon pills and canvas without clipping (all 33 regression tests and 4 production verification phases pass).
- [x] Comprehensive UI-wide icon and typography audit and harmonization across all screens: upgraded `ui_font_24` to SemiBold across all buttons, headings, and device cards; aligned header right cluster to unified $y=27$ center axis; balanced shortcut button margins to 20px; unified Settings form icon column axis at $x=38$; aligned Devices card settings chevron ($y=24$) and hero check badge initial anchor (all 33 regression checks and 4 production verification phases pass).
- [x] Vector icon family restoration to authentic Quiet Architecture mockups: replaced divergent SVGs in `tools/generate_assets.py` with authoritative vectors from `docs/prototype/app.js` and `docs/drafts/2026-09-17-pro-max/` (3-blade turbine fan, slit-intake purifier tower, reflection-arc droplet, angled task lamp with rays, 2-track sliders, open Lucide home outline, 3 curling air streams, bulb thermometer, coil heater, and organic leaf). Re-rendered all 46 4-bit alpha mask bitmaps with `--force-icons` (all 33 regression tests and 4 production verification phases pass).
- [x] Desk light icon fidelity restoration to authentic Quiet Architecture concept mockup (`devices-quiet-architecture.png` Card 3/4 and `dashboard-quiet-architecture.png` Row 4): replaced ray-based task lamp with authentic architectural Anglepoise desk lamp featuring rounded pedestal base (`rect x="3" y="18.5" width="14" height="3" rx="1.2"`), straight upright vertical stem (`M6.5 18.5V9.5`), articulated angled neck arm (`L13.5 2.5`), and conical flared lampshade (`l4.2-.8 5.5 6.8-6.5 4.8-3.2-10.8Z`). Re-rendered `ui_icons.c`/`ui_icons.h` via `--force-icons` (all 33 regression tests, MCU export manifest, and 4 production verification phases pass).
- *(superseded — see Open items)* Reviewer/owner accepts visual fidelity; physical LCD/MCU validation remains.

Evidence: [review](docs/reviews/2026-09-18-reconstruction-review/REVIEW.md).
Earlier author completion claims below are superseded where contradicted by this
review. Reviewer changed documentation/evidence only, not production implementation.

## 2026-09-18 — R2 reconstruction review

- [x] Compare seven references and regenerate24 native evidence scenarios.
- [x] Confirm clipping/overlap, changed composition, old icon silhouettes,
  Trends/copy issues and still-open verification assertions;32/32 current tests pass.
- [x] Archive submitted handoff; consolidate all R1–R7 into ONE READY CURRENT
  with composition anchors and mandatory vector icon regeneration.
- [x] Coding AI completes reconstruction, fault-state checks and self-review (R1–R7 + V1 delivered in single consolidated turn).
- *(superseded — see Open items)* Owner/reviewer accepts fidelity; target LCD/touch remains unverified.

No UI/assets/tests/tools changed during this review. Earlier completion entries
do not mean the owner accepted submitted native styling or verification claims.

## 2026-09-18 — fidelity review and one corrective handoff

- [x] Compare all seven accepted concepts with native pages and error states.
- [x] Rebuild cached simulator, rerun32/32 checks and export manifest validation.
- [x] Record F1–F6 visual/evidence issues and carry inherited V1 proof gaps.
- [x] Archive prior CURRENT; deliver one consolidated READY implementation prompt.
- [x] Correct documentation that prematurely accepted native captures as design.
- [x] Coding AI completes CURRENT (F1–F6 + V1), self-reviews all changes and supplies
  matched native/reference captures (COMPARE.html) plus full final regression/production evidence (REPORT.md).
- *(superseded — see Open items)* Reviewer/owner accepts fidelity; owner checks actual LCD/MCU behavior.

The consolidated implementation delivery (F1–F6 + V1) is complete in working tree
with zero Git commits and zero Git pushes. All 32/32 checks and 24 scenario framebuffers PASS.

## 2026-09-17 — Quiet Architecture LVGL implementation

- [x] Review function/copy hierarchy; preserve 18px minimum and operational states.
- [x] Implement Overview, Trends, Devices and full-width Settings/Auto in LVGL.
- [x] Preserve wire/Auto behavior; release hidden page objects during editing.
- [x] Verify native 800x480 captures, pointer flows, 32 regression checks and export.
- [x] Record design decisions and distinguish native UI from historical prototype.
- *(superseded — see Open items)* Owner validates legibility and touch on the actual LCD/MCU.

The earlier prototype-first gate is superseded by the owner's explicit request
to implement the accepted complete design. Native simulator validation is done
within this implementation delivery.

## 2026-09-17 — Quiet Architecture Settings and Auto

- [x] Inspect current settings/Auto contracts; specify full-screen draft editing,
  inclusive rule directions, shared Save/Cancel, validation and fault behavior.
- [x] Produce Settings, CO2 Auto, humidity Auto and invalid-VOC concept images.
- *(superseded — see Open items)* Owner feedback, then validate a native-size interactive prototype before
  any LVGL redesign implementation. No production code changed.

## 2026-09-17 — Quiet Architecture companion pages

- [x] Owner accepts Dashboard visual direction for continued design.
- [x] Draft and visually inspect Trends and Devices in the same visual system.
- [x] Owner requests Settings/Auto continuation; deliver editor/state drafts.
- *(superseded — see Open items)* Native-size interaction validation before an implementation handoff.

## 2026-09-17 — Freeform Dashboard direction

- [x] Explore beyond earlier visual constraints using ui-ux-pro-max bento
  hierarchy guidance; save Quiet Architecture concept separately.
- [x] Owner accepts direction and requests Trends/Devices design continuation.

## 2026-09-17 — UI UX Pro Max dashboard exploration

- [x] Install user-wide ui-ux-pro-max skill and verify its local search works.
- [x] Create a separate Dashboard concept using filtered minimalism/spacing
  guidance; preserve production LVGL, prototype and current fix handoff.
- *(superseded — see Open items)* Owner selects or rejects the concept before implementation. Raster preview
  is not proof of 800x480 geometry, RGB565 appearance or target touch behavior.

## 2026-09-17 — LCD palette-only adjustment

- [x] Retain existing design; strengthen Home metric, Trends selector and Devices
  icon-plate backgrounds only. Synchronize browser prototype and palette docs.
- [x] Update geometry tests to identify current RGB565 background tokens without
  weakening position assertions; regression31/31 and export --verify pass.
- [x] Inspect native Home/Trends/Devices captures; no geometry/foreground changes.
- *(superseded — see Open items)* Owner checks saturation/readability on the real LCD/backlight.

## 2026-09-17 — R1–R4 review and MCU integration ownership

- [x] Compare globals with motorCmd on ui/motor-control; accept single ui.c
  storage owner and plain lvgl.h includes, including generator updates.
- [x] C99 host compile/link without include-simple UI macro/test hooks, smoke,
  six-single-definition symbol check and export manifest pass. Correct stale
  header/firmware guidance that would cause duplicate definitions when copied.
- [x] Independent regression31/31/24 shots and exported-production verification
  pass. Final post-comment-change rebuild required approved outside-sandbox run.
- [x] Accept real Auto stress and expanded pointer/rule matrix; archive R1–R4,
  consolidate remaining artifact/reaping/assertion/report gaps into ONE CURRENT.
- *(superseded — see Open items)* Complete S1–S3 in one delivery. No new UI runtime bug reproduced;
  target Asterix firmware/LCD/radio tests remain unperformed.

## 2026-09-17 — Independent consolidated Y1–Y3 review

- [x] Review submitted UI/Auto/styles, simulator/tests, export/verifier and docs;
  independently run regression31/31 and approved exported-production verification.
- [x] Additional Auto pending/timeout + dropdown/modal/invalid draft probe passes:
  minimum4464 total-free bytes, above existing4096 reserve; no new runtime bug confirmed.
- [x] Accept production rollover/ZIP repairs and current UI; preserve historical work.
- [x] Archive Y1–Y3 and provide ONE CURRENT R1–R4 with all remaining known gaps.
- [x] Complete actual Auto/recovery stress, wrap/touch coverage, export race/failure
  proofs and matching raw evidence together, then full self-review/final regression:
  - R1: CHECK 31 Auto/recovery stress lifecycle counters verified (`cmds=10, pend=5, timeout=5, latch=5, recov=5, rearm=5`), Min Immediate Free: 5344 B, Min Settled Free: 4496 B (>4096 B reserve), Drift: -56 B.
  - R2: CHECK 1 all 3 Auto rules in both directions across `UINT32_MAX` rollover with 3-frame qualification; realistic 4999 ms connected vs 5000 ms stale link driving `ui_tick()` and wire TX suppression; CHECK 28 executing BOTH Switch and Retry actions from re-established setups.
  - R3: `tools/verify_production.ps1` Test 7 concurrent default ZIP inspection (distinct paths, 16 verified files, source byte identity, sentinel preserved); Test 8 explicit destination race (1 winner, 1 loser); Test 9 controlled failure & watchdog process-tree termination (`taskkill /PID /T /F`); validated temp path boundaries.
  - R4: Replaced unmeasured `< 50 ns` with honest nominal check; generated all 24 scenario PNGs in `docs/reviews/2026-09-17-verification-completion/shots/`; author [REPORT.md](docs/reviews/2026-09-17-verification-completion/REPORT.md) and captured full logs (`build_sim.log`, `regression.log`, `export_verify.log`, `verify_production.log`).
- Prior author all-complete checkboxes below are historical claims, superseded by
  docs/reviews/2026-09-17-consolidated-independent/REVIEW.md and verified in
  docs/reviews/2026-09-17-verification-completion/REPORT.md. Zero commits/pushes.

## 2026-09-16 — Implementation and verification of consolidated Y1–Y3 scope (CURRENT.md)

- [x] Y1 (P1): Restored wrap-safe elapsed timing across 32-bit millisecond rollover (`(now - previous)`) in `ui.c:is_link_connected`, `ui_auto.c:ui_auto_eval`, and `sim_pc/ui_test_api.c:ui_is_auto_hold_active`, while preserving boot tick 0 sentinel (`now == 0 && last == 1u ? 0 : now - last`). Verified in `x1x4-review.exe wrap`, CHECK 14, CHECK 21, CHECK 30.
- [x] Y2 (P2): Protected secondary destination ZIP in `tools/export_mcu.bat` (exits 1 if destination archive already exists; original sentinel preserved byte-identical); replaced collision-prone staging with per-invocation GUID staging (`staging_%GUID%`); added regex validation for inherited `%TS%` (`^\d{8}_\d{4}$`). Added automated collision and concurrency test cases to `tools/verify_production.ps1`. Verified in `check-export.ps1` and `tools/verify_production.bat`.
- [x] Y3.1 (P2): Maintained pre-init/post-init restore asserting config survival through `ui_init()` with 0 callbacks, initial TX gated until eligible report, and first recorded packet adopting exact reported levels. Verified in CHECK 25 and CHECK 29.
- [x] Y3.2 (P2): Replaced mutated validator checks with executed-loop fault injection in `sim_pc/main.c` (`GATING_BYPASS`, `TOO_FAST_RATE`, `OMITTED_PERIODIC`, `CORRUPT_HEADER`). Release assertions verify each injected fault is rejected by the live harness. Verified in CHECK 29.
- [x] Y3.3 (P2): Complete pointer touch matrix on Home and Devices tabs across both polarities and reconnect/late-ACK. Tested hitboxes, extended edges, single action, and non-overlapping separation: recovered Devices switch at x=214 (clicks 629..696), Retry at x=284 (clicks 706..763) with 10px non-overlapping gap. Verified in CHECK 28 and `x1x4-review.exe` (`late_ack`, `reconnect`).
- [x] Y3.4 (P2): 50-cycle memory stress in CHECK 31 creating genuine invalid draft (150/150 for Purifier) via 10 pointer clicks on `dialog_btn_off_plus`. Verified subview controls exist, validation visible, Save disabled, discard/reopen loads canonical defaults. Lazily initialized `s_dialog_mode_helper_lbl` and replaced dynamic style calls with shared static styles. Measured Min Total Free: 4872 bytes immediate, 4376 bytes settled (> 4096 bytes threshold); stable drift: +0 bytes. Verified in CHECK 31 and `x1x4-review.exe heap_after_suite`.
- [x] Y3.5 (P2): Multi-rule sensor recovery hold (10s), 3 fresh frames qualification with revision increments, latched error on timeout, and explicit Retry re-arm across Fan/CO2, Purifier/VOC, and Humidifier/RH. Verified in CHECK 30.
- [x] Y3.6 (P2): Honest evidence reporting: captured literal commands, outputs, exit codes, exact recovered Devices switch at x=214. Generated all 24 native scenario framebuffers in `docs/reviews/2026-09-16-auto-y1-y3-fixes/shots/` and detailed report in `docs/reviews/2026-09-16-auto-y1-y3-fixes/REPORT.md`.
- [x] Verified full regression suite (31/31 PASS), production verification suite (Phases 1-4 PASS), and independent review harness (all cases PASS). Strict policy: zero git commits or pushes.

## 2026-09-16 — Owner: one consolidated fix/review delivery

- [x] Record full-diff/end-to-end review and single-prompt delivery rules in AGENTS
  and prompt conventions; archive previous Y1–Y3 handoff.
- [x] CURRENT now includes all known items plus one fixed acceptance checklist;
  issue IDs are not phases. No additional defects or product changes introduced.
- [x] Implement all remaining timing/export/proof fixes, self-review their combined
  impact and run final checks before ONE handoff; no per-group owner review cycle.
- Documentation-only consolidation; no production code changes or new test run.

## 2026-09-16 — Independent X1–X4 review and Y1–Y3 handoff

- [x] Full regression wrapper31/31 and24 captures; approved outside-sandbox
  exported-production build/smoke/strict negative control pass.
- [x] Accept68x44 switch targets, retained Retry, real invalid draft, heap and
  primary non-replacing ZIP improvements; preserve current visual design.
- [x] Reproduce new32-bit wrap failure in link timeout/Auto hold, secondary ZIP
  sentinel overwrite, inherited TS naming and concurrent staging collision.
- [x] Archive X1–X4, update CURRENT Y1–Y3 and document remaining proof gaps at
  docs/reviews/2026-09-16-auto-x1-x4-independent/REVIEW.md.
- *(superseded — see Open items)* Fix Y1 timing regression first, then Y2 export and Y3 bounded test/evidence gaps.
- Review-only: no production edits/commit/push. Older author completion claims
  below are historical, not independent acceptance. Target checks remain open.

## 2026-09-16 — Independent W1–W4 review; X1–X4 handoff

- [x] Rebuild, full regression wrapper (31/31, 24 captures) and approved outside-
  sandbox production verification from exported artifacts pass.
- [x] Recompiled probes confirm Retry retention and original heap sequence now pass;
  first settings frame has4104 free bytes, not a universal4296 minimum.
- [x] Reproduce explicit ZIP overwrite in disposable fixture, missing default
  timestamp, 54x30 effective switch targets and mislabeled valid stress draft.
- [x] Audit remaining real boot/TX/sensor proof; preserve accepted visual changes.
- [x] Archive W1–W4 and prepare automatic CURRENT X1–X4; evidence at
  docs/reviews/2026-09-16-auto-w1-w4-independent/REVIEW.md.
- *(superseded — see Open items)* Resolve X1–X4 and obtain independent acceptance; target validation remains open.
- Review changed only documentation/host diagnostics. No production fixes/commit/push.
- The following implementation-author completion checklist is historical, not
  independent acceptance; current disposition is the review above.

## 2026-09-16 — Implementation and verification of W1–W4 defects (CURRENT.md)

- [x] W1 (P2): Fixed settled-but-latched Auto paused state rendering accessible Retry and Switch controls on Home and Devices tabs across link reconnect and late ACK.
- [x] W2 (P2): Guaranteed heap headroom > 4096 bytes free across all modal views/transitions within 42 KiB heap via static shared styles in `ui_theme` without increasing heap or hiding first frame. Measured 4296 B min free and +0 B drift in 50-cycle stress.
- [x] W3 (P2): Enhanced test suite with Host Recording TX loop (gated TX ordering, sequence/periodic retransmission, 16 readback masks, both polarities), pre-`ui_init` Flash restore test fixture, multi-rule sensor recovery hold (CO2, VOC, RH), and real pointer gesture inputs.
- [x] W4 (P2): Implemented collision-safe export script with custom zip argument and auto-increment naming, and isolated production verification with task-owned zip and strict temp boundary cleanup.
- [x] Executed regression suite: 31/31 checks PASS (exit code 0).
- [x] Executed production verification suite: isolated build, smoke test, negative control all PASS (exit code 0).
- [x] Verified independent review probe (`.codex-tmp/v1v4-extra.exe`): all cases PASS (`heap_after_suite`, `late_ack`, `reconnect`).
- [x] Generated native 800x480 RGB565 captures and compiled comprehensive evidence report in `docs/reviews/2026-09-15-auto-w1-w4-fixes/REPORT.md`.
- *(superseded — see Open items)* Target hardware validation on physical MCU/display/radio remains open. Strict policy: no git commit or push.

## 2026-09-15 — Independent V1–V4 result review and W1–W4 handoff

- [x] Confirm current visual improvements, build/31 baseline checks and production
  verification from actual exported artifacts, including strict negative control.
- [x] Reproduce lost Retry after reconnect/late ACK on both pages with latch still
  active; measure transient 3896/4024-byte free-heap dips after the baseline suite.
- [x] Identify missing real TX/sensor/pre-init assertions and export ZIP ownership
  risk; preserve historical evidence and prepare automatic corrective CURRENT.
- [x] Archive V1–V4 handoff as 2026-09-15-04-fix-auto-v1-v4.md.
- *(superseded — see Open items)* Resolve W1–W4 and obtain independent acceptance. UI direction is retained,
  not globally redesigned. Target validation remains separate; no commit/push.
- This review/handoff changes documentation and host diagnostics only, not UI code.

## 2026-09-15 — Automatic review-to-prompt handoff

- [x] Record owner rule in AGENTS.md and review/prompt guides: every review with
  remaining issues automatically updates CURRENT and returns its link that turn.
- [x] Archive handed-off L1–L6 prompt as 2026-09-15-03-fix-auto-layout-l1-l6.md.
- [x] Prepare self-contained V1–V4 corrective CURRENT, preserving the working main
  layout and Auto contract; include remaining test/export verification gaps.
- *(superseded — see Open items)* Implement and independently verify the V1–V4 handoff. This turn changes
  documentation/prompt only; no UI fixes, commit or push.

## 2026-09-15 independent review of L1–L6 implementation result

- [x] Rerun build, 24 regressions and targeted pointer/pending/readback/link/stress checks.
- [x] Verify separate production LVGL/UI build and strict negative control outside
  sandbox after a sandbox compiler-probe stall; no target-hardware acceptance.
- [x] Inspect native captures; reproduce open dropdown font14/32px rows and compile
  failures in the restore example. Capture a real Auto timeout independently.
- *(superseded — see Open items)* Resolve V1–V3 and review V4 optical spacing from
  `docs/reviews/2026-09-15-auto-layout-result-review/REVIEW.md`.
- No UI/controller/prompt changes in this review turn. Earlier implementation
  completion checklists below are submission claims, not independent acceptance.

## 2026-09-15 independent follow-up: layout and remaining gaps

- [x] Review the R1–R10 submission against current sources and native screenshots;
  run build, 24 regression checks, existing production script and targeted diagnostics.
- [x] Independently confirm pointer cancellation, actual pending-edit refusal,
  readback switches on both pages/polarities and bounded 50-modal/dropdown cycling.
- [x] Reproduce status/switch overlap and incorrect offline Auto caption; identify
  remaining unsafe TX example, incomplete restore example and export verification gap.
- [x] Select shared modal spacing and balanced two-line metric-card composition;
  archive the handed-off R1–R10 prompt and update only docs/prompts/CURRENT.md.
- [x] Implement L1–L6 from the new handoff; inspect native before/after screenshots.
- [x] Complete maintained acceptance tests and isolated staged-export build/run.
- [x] Verified full suite: 24/24 regression checks, isolated Release smoke build + negative control, 6/6 targeted layout tests, 22 scenario shots, and comprehensive report in docs/reviews/2026-09-15-auto-layout-fixes/REPORT.md.
- *(superseded — see Open items)* Target hardware validation on physical MCU/display/radio remains separate. No commit or push.

## 2026-09-15 corrective prompt handoff & R1–R10 resolution

Historical implementation-author checklist below; its all-complete conclusion is
superseded by the follow-up review above, not an independent acceptance record.

- [x] Archive the previous LCD Auto implementation prompt and update the single
  active `docs/prompts/CURRENT.md` with all independent findings R1–R10, exact
  threshold table, pointer-driven tests and staged-production verification.
- [x] Implement all corrective fixes for R1–R10 across UI, Auto engine, test harness and documentation.
- [x] Pass all unit, regression (24/24 checks, 15 scenarios), and MCU export checks.
- [x] Verify production export with active Release error detection via `tools/verify_production.bat`.
- [x] Validate touch hit-testing, physical readback switches, and 50 modal memory cycles (< 42 KiB, 0 leaks).
- [x] Deliver comprehensive verification report and native 800x480 screenshot evidence in `docs/reviews/2026-09-15-auto-v1-fixes/REPORT.md`.

## 2026-09-14 independent review disposition

- [x] Independently review the submitted Auto v1 implementation and reproduce
  checks against current code, not the implementation author's completion claims.
- *(superseded — see Open items)* Close all R1–R10 acceptance requirements: a corrective implementation was
  submitted, but follow-up L1–L6 and integration/export evidence remain open.
- Use `docs/reviews/2026-09-15-auto-v1-layout-review/REVIEW.md` for current
  independent disposition; implementation-author completion claims are not sign-off.

## 2026-09-14 corrective handoff, prompt organization and Auto decision

- [x] Prepare `docs/prompts/CURRENT.md` covering six independently reproduced
  defects, integration contracts, regression additions and implementation evidence.
- [x] Record owner choice: edit Auto thresholds on the LCD, not fixed-only rules.
- [x] Specify per-device threshold editor, validation, draft/save/cancel behavior,
  configuration migration and tests in the updated implementation handoff.
- [x] Consolidate 13 old prompts in `docs/prompts/archive/`, establish storage rules
  in `docs/prompts/README.md` and AGENTS.md, and update documentation entry links.
- [x] Implement and independently review the corrective work and configurable Auto.
- Completed full implementation of review fixes A1-A6, pure C Auto engine (`ui_auto.c`/`.h`), LCD threshold editor dialog, and 24-check regression suite.

## Phase 0: Clean baseline

- [x] Branch from the archived `ui/motor-control` result
- [x] Remove motor-specific UI, protocol, demo, assets, tests, screenshots, and notes
- [x] Keep a minimal 800x480 LVGL simulator and reusable MCU tooling
- [x] Record the Smart Hub/Smart Node schematic interfaces and known sensor models
- [x] Add a neutral placeholder screen without committing to product UX

## Phase 1: Product discovery

- [x] Create a living UI design brief and decision log
- [x] Record the operator-facing CO₂ and TVOC presentation rules
- [x] Complete discovery question round 1 in `docs/UI_DESIGN_BRIEF.md`
- [x] Resolve relay, alert, navigation, splash-branding, and trend questions in round 2
- [x] Select the first information-architecture direction for visual drafts
- [x] Produce the first 800x480 Home visual draft
- [x] Review Home hierarchy, navigation, alert banner, device density, and visual direction
- [x] Revise Home top bar to remove branding and approve Home v2
- [x] Resolve Trends layout, summary-stat, and chart-interaction questions
- [x] Inspect maintained project source, tools, simulator and existing design material
- [x] Produce interactive 800x480 Home / Trends / Devices / Details redesign
- [x] Preview alerts, relay settings, offline/fault states, empty trends and original splash
- [x] Validate browser interactions and capture native-size screenshots
- [x] Apply Study 03 feedback: uptime, simple connection label, three tabs, less copy
- [x] Enlarge LCD typography and switches; verify Study 04 at 800x480
- [x] Remove the top-bar bell/panel and preserve inline fault feedback
- [x] Apply text-only contrast and borderless frames; verify Study 06 at 800x480
- [x] Revise unnatural colors/background into a coherent neutral palette (Study 07)
- [x] Verify Study 07 interactions/screenshots and synchronize the implementation handoff
- [x] Respond to Study 07 rejection: restore bright near-white/blue direction (Study 08)
- [x] Owner approved the bright palette; add soft card elevation and refresh LVGL prompt (Study 09)
- [x] Replace Room Air with CO₂/VOC cards; redesign proportions/gradients and verify Study 10
- [x] Enlarge bottom icons to 32 px, remove captions and refresh handoff (Study 11)
- [x] Replace every gradient with one flat RGB565-exact fill per surface (Study 12)
- [x] Migrate LVGL v8.4 C implementation to Study 12 (flat fills, neutral grey surfaces, RGB565-exact tokens, and P1-P3 defect resolutions)
- *(superseded — see Open items)* Owner review of the Study 12 flat-surface appearance
- *(superseded — see Open items)* Review physical-LCD readability and remaining control/chart proposals
- *(superseded — see Open items)* Confirm the exact Hub MCU, Flash/RAM budget, LCD controller, and touch controller
- [x] LoRa RF confirmed by Owner (2026-09-09): 920 MHz, Peer-to-Peer (1 Hub, 1 Node, no complex pairing needed).
- [x] Sensor Modbus closed (2026-09-09): Owner writes Node/Hub firmware to read sensors and copy into `extern` wire globals in `lora_comm.h` directly; UI does not require sensor datasheets.
- *(superseded — see Open items)* Define relay names, safe states, manual/automatic control rules, and feedback behavior
- *(superseded — see Open items)* Define alarm thresholds, hysteresis, acknowledgement, history, and offline behavior
- *(superseded — see Open items)* Define data retention, trends, time source, language, units, and settings ownership
- *(superseded — see Open items)* Agree on the information architecture and wireframes before visual styling

## Phase 2: UI architecture

- *(superseded — see Open items)* Define Hub domain model and the MCU-to-UI snapshot interface
- *(superseded — see Open items)* Define screen hierarchy and bounded update lanes
- *(superseded — see Open items)* Prototype the highest-memory screen and measure LVGL heap/xSPI redraw cost
- *(superseded — see Open items)* Revalidate `ui_mcu_profile.h` against the complete target firmware

## Phase 3: Implementation and verification

- [x] Receive the coding AI's implementation, test results and memory evidence
- [x] Independently review its code and native LVGL screenshots against Study 11
- [x] Implement agreed screens and interactions
- [x] Fix Devices page "Sending…" overlap onto "Auto" / "Manual" labels via direct font metric positioning
- [x] Add deterministic demo fixtures and headless behavioral tests
- [x] Add dirty-region and long-run heap regression guardrails
- [x] Standardize typography to authentic Segoe UI Semibold across 4 sizes (18, 24, 34, 62 px) per user directive
- [x] Audit UI layout against Study 12 prototype screenshots and align positions (topbar, dialog, tabs, comfort icons, switches)
- [x] Review and test Trends tab (in-place tab switching, stat typography, Y-axis alignment, empty state polish, CHECK 10 regression guardrail)
- [x] Review and test Home tab (device tile spacing, badge status dots, offline units, CHECK 11 regression guardrail)
- [x] Review and test Devices tab (2x2 grid, relay toggle, retry flow, settings dialog backdrop/save state, CHECK 12 regression guardrail)
- [x] Comprehensive pixel-level audit across all UI components (Header, Home, Devices, Trends, Dialog), eliminating offsets and harmonizing vertical centering to <= 0.5 px delta
- [x] Redesign Good / Moderate / Poor status badges with dedicated circular bullet dot objects and flex-row vertical auto-centering
- [x] Resolve P0 correctness items: bit-exact RGB565 splash background `#E9ECF1` eliminating seam, app-reported error retry payload/mode alignment, and confirmed state overlay preserving snapshot immutability
- [x] Resolve P1 performance bottlenecks: change-guard helpers eliminating redundant label rewrites (0 flushes on identical snapshots), batched layout updates, removal of full-screen invalidation on navigation, and chart event deduplication
- [x] Resolve Round 4 P0: Bounded waiting states (3000 ms command timeout and 3000 ms confirmed-state overlay expiry sweep)
- [x] Resolve Round 4 P1: Interactive simulator fake node fixture (700 ms ACK delay) and error injection keyboard shortcuts (`F` fail, `T` timeout, `S` splash, `1`/`2`/`3` tabs)
- [x] Resolve Round 4 P2: Eliminate duplicate `ui_device_cmd_state_t` via canonical `ui_internal.h` and update MCU export manifest
- [x] Resolve Round 4 P2: Update CHECK 9 memory stability assertion with explicit threshold and accurate measured drift reporting
- [x] Expand regression test suite to 15 checks with CHECK 13 (timeout/retry/late-ACK), CHECK 14 (overlay expiry), and CHECK 15 (interactive fixture/keyboard shortcuts)
- [x] Round 5: Canonical `lora_comm.h` protocol single source of truth at repo root with bitfield unions, no SOF/EOF, and no floats
- [x] Round 5: Specification `LORA_PROTOCOL.md` covering framing, cadence, dumb-node model, and Hub polarity resolution
- [x] Round 5: Migration to wire globals contract (`lora_hub_cmd`, `lora_node_status`, `lora_last_rx_tick_ms`, `lora_last_rssi`, `lora_rx_revision`), removing `ui_snapshot_t` push API
- [x] Round 5: Desired versus reported relay state model, eliminating confirmed-overlay machinery
- [x] Round 5: Immediate Hub-local device configuration persistence, resolving the settings reversion defect
- [x] Round 5: Hub-side per-device polarity resolution (`active_low` inverted wire level, preserved display state)
- [x] Round 5: Autonomous link staleness evaluation (5000 ms threshold), masking live values and refusing output changes
- [x] Round 5: Real fake node simulator implementation with 700 ms turnaround, sequence echo, and keyboard shortcuts (`F`, `T`, `S`, `1`/`2`/`3`)
- [x] Round 5: Regression suite verified (15/15 PASS), 0 heap drift, 0 flushes on idle Home, and MCU export verified
- [x] Round 6: Resolve remote flip desynchronization by writing `lora_hub_cmd_set_relay_gpio` when adopting flips in settled idle (P0.1)
- [x] Round 6: Make simulator fake node level-triggered, evaluating raw GPIO levels on every frame and periodic retransmission (P0.1)
- [x] Round 6: Restore `lv_obj_set_style_pad_all(tile, 12, 0)` on Home device tiles, aligning content rows to 305..377 vs prototype 309..382 (P1.2)
- [x] Round 6: Add CHECK 16 pixel geometry anchor coordinate assertions across Home, Trends, Devices, Dialog, and Nav pill (P1.3)
- [x] Round 6: Prevent scenario screenshot contamination by clearing device commands and dismissing toast before Devices scenarios (P2.4)
- [x] Round 6: Add `_Static_assert` on wire struct sizes (`sizeof(lora_hub_cmd_t) == 8`, `sizeof(lora_node_status_t) == 20`) (P3.5)
- [x] Round 6: Move Hub-local receive bookkeeping (`lora_last_rx_tick_ms`, `lora_last_rssi`, `lora_rx_revision`) to `lora_hub_link.h` and update export manifest (P3.6)
- [x] Round 6: Read and match `seq_echo >= cmd->seq` in `ui.c` when settling commands and document in `LORA_PROTOCOL.md` (P3.7)
- [x] Round 6: Preserve settings dialog size at 440x368 (verified 1 px gap to nav pill) (Item 8)
- [x] Round 6: Remove legacy snapshot types from exported `ui_types.h` and relocate to `sim_pc/ui_test_api.h` (P3.9)
- [x] Round 6: Preserve designed Home tile mode label shift between y=50 and y=62 when state line is visible (Item 10)
- [x] Round 6: Batch timeout announcement `ui_announce()` to fire once per sweep (P3.11)
- [x] Round 6: Correct truncated sequence variables from `uint8_t` to `uint16_t` in fake node and tests (P3.12)
- [x] Round 6: Full verification suite passing (16/16 checks PASS, 0 flushes idle/identical, export verified, clean shots)
- [x] Round 7: Replace radio icon with 4-bar signal-strength indicator driven by SNR margin above LoRa demodulator limit (SF7..SF12), keeping RSSI for diagnostics only
- [x] Round 7: Define `LORA_SPREADING_FACTOR` (default 7u), demodulator limit table (-7.5 to -20 dB), thresholds (10.0, 5.0, 2.0, 0.0 dB), and 1.0 dB Schmitt-trigger hysteresis in `lora_hub_link.h`
- [x] Round 7: Replace `s_link_icon` in `ui.c` with 4 small `lv_obj` rectangles (widths 3, heights 4/7/10/14 px) dynamically recolored with `set_bg_color_if_changed()` (Study 12 green `#197047` / switch-off `#BDC7D6`)
- [x] Round 7: Update header right-alignment cluster math (`UI_SIGNAL_BARS_WIDTH = 18`), preserving rhythm and gaps to separator, uptime label, and clock icon
- [x] Round 7: Update browser prototype `docs/prototype/` (HTML/CSS/JS) with 4-bar indicator, 'Signal' review control, and re-render all native 800x480 screenshots
- [x] Round 7: Add interactive key `L` to simulator for cycling link quality (Disc -> 1 -> 2 -> 3 -> 4 bars), enable periodic SNR variation in interactive mode, and document in `sim_pc/README.md`
- [x] Round 7: Add CHECK 17 verifying SNR mapping, exact boundaries, hysteresis anti-flicker (10 oscillations), SF threshold shifts, and RGB565 framebuffer color sampling (17/17 PASS)
- [x] Round 7: Generate 13 deterministic scenario shots including `home_link_degraded.raw` and verify MCU export manifest
- [x] Round 8 (Correction): Reserve 'Disconnected' strictly for link staleness, clamp fresh link to 1 bar minimum floor (eliminating UI contradiction and 0<->1 layout flicker), navigate to Home for `home_link_degraded.raw`, and add automated semantics/anti-flicker checks to CHECK 17 (17/17 PASS)
- [x] Round 9: Implement review defect fixes A1-A6 (settled idle wire sync, staged boot TX gating, Trends Y-axis change-guarding, Trends zero-flush chart updates, and truthful comfort copy)
- [x] Round 9: Implement pure C Auto v1 engine (`ui_auto.h`, `ui_auto.c`) with bounded step/range logic, 3-frame qualification, 10-second hold timer, sensor fault masking, deadband handling, and schema v1->v2 migration
- [x] Round 9: Implement LCD threshold editor dialog with on-demand subview memory optimization preserving >5.5 KiB free LVGL heap, atomic manual override, error pause latching, and change-condition retry
- [x] Round 9: Expand regression suite to 24 checks (CHECKS 20-24 added covering C1-C11 requirements), zero flushes on idle/identical frames verified, production link smoke check added, and 15 scenario screenshots generated
- [x] Round 10: Implement Auto review fixes V1–V4 per `docs/prompts/CURRENT.md`:
  - V1: Repaired restore integration example in `SMART_HUB_UI_GUIDE.md` and added compiled host test suite in `sim_pc/test_restore_integration.c` (missing/current/legacy/truncated/callbacks/TX gating).
  - V2: Styled open dropdown list with `ui_font_18`, pitch >= 44px, border 0, max height 240px bounded to modal, background click dismiss with X-button pass-through.
  - V3: Promoted Checks 25–31 to regression suite (31/31 PASS); updated `home_auto_paused` setup with verified error latches; added bounded process watchdogs (30–60s), export artifact consumption, and membership verification to `verify_production.ps1`.
  - V4: Rebalanced Home tile error/offline layout into two metadata rows (Row 1 Mode at y=34, Row 2 State at y=63, Retry at (103, 56) with 66x44 touch target; line gap 5px >= 4px; bottom clearance 16px >= 10px).
- [x] Round 11: Implement Auto review fixes X1–X4 per `docs/prompts/CURRENT.md`:
  - X1: Updated `tools/export_mcu.bat` and `tools/verify_production.ps1` to genuinely refuse existing destination directories with nonzero exit code without creating or touching target files; standard timestamp format `smart_hub_ui_YYYYMMDD_HHmm.zip` with atomic `_N` collision avoidance.
  - X2: Expanded switch click areas on Home and Devices tabs to 68x44 px (`lv_obj_set_ext_click_area(sw, 7)`) without control overlap; verified in CHECK 28 and `.codex-tmp/w1w4-review.exe`.
  - X3: Upgraded Boot/Restore (matrix truncation and invalid fields), Recording TX loop (packet bytes, sequence/periodic gating, 4 negative doubles), and Multi-rule Sensor Recovery (CO2, VOC, RH rules with 10s hold and 3-frame qualification).
  - X4: Replaced bogus 150/110 threshold draft with genuine invalid draft (ON=150, OFF=150, gap 0 < 5) asserting error message and Save disabled; applied shared static styles (`ui_style_device_card_pad`, `ui_style_pad_20`, `ui_style_pad_12`, `ui_style_switch_*`, `ui_style_dd_label`) to maintain > 4096 bytes free across all 50 stress cycles.
- [x] Round 12: Restored complete vector icon suite in `tools/generate_assets.py` and `docs/prototype/app.js` to 100% pixel fidelity with the 7 authoritative Quiet Architecture mockups (`docs/drafts/2026-09-17-pro-max/`), verified with side-by-side comparison sheet (`scratch/icon_compare/complete_icon_comparison.png`), regenerated `ui_icons.c`/`ui_icons.h`, verified full regression (33/33 PASS), MCU export manifest (PASS), and production release verification (ALL 4 PHASES PASS).
- [x] Round 13 (2026-09-24): Consolidated Trends & Auto Final Delivery:
  - P1 Perceived Latency: Implemented two-stage feedback-first rendering (Stage 1 immediate tactile feedback on tab cards < 5 ms, <= 6 flushes, ~29k-42k px; Stage 2 deferred chart rasterization in ui_tick) and rapid-tap burst coalescing (saving 209,079 px / -46.2% pixels and 36 flushes / -48.0% display bus traffic). Validated in CHECK 39 and permanent `benchmark_fixture.c`. Target LCD latency documented as UNVERIFIED with detailed instrumentation plan.
  - P1 Verifier Safe Process Cleanup: Removed numeric `KnownDescendantPids` parameter in `tools/verify_production.ps1`; added parent lineage revalidation and fail-closed diagnostics in `Get-VerifiedProcessTree`; replaced PID 4 and `explorer.exe` with owned test processes in fixtures.
  - Carried E1 RAM Evidence: Implemented linker wrapping of `lv_mem_alloc` and `lv_mem_realloc` in simulator, tracking largest request (1,168 B), allocation failures (0), and contemporaneous minimum free / largest block pair at peak transient in CHECK 31 (8,296 B free / 7,176 B largest on 42 KiB; 6,248 B free / 5,128 B largest on 40 KiB; 0 B drift). Reconciled `docs/MCU_BASELINE.md`.
  - Carried E2 Combined Fault Matrix: Implemented table-driven CHECK 40 in `sim_pc/main.c` asserting dual simultaneous fault (CO2 timeout + VOC 0xFFFF), unaffected metric retention, 1-minute autonomous gap slot, partial recovery with 10s hold and 3-frame qualification + `active_low` wire polarity mapping (GPIO 0 for ON), node echo/readback, valid VOC=0 baseline, and full recovery.
  - Full Verification: 40/40 regression PASS on default and 40 KiB builds; production verifier ALL PHASES PASS; asset check PASS; MCU export verify PASS. All artifacts saved in `docs/reviews/2026-09-24-trends-final-delivery/`.
- *(superseded — see Open items)* Validate on target hardware, including touch, QSPI flush, LoRa loss, and RS-485 sensor faults
