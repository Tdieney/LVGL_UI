# Smart Hub UI plan

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
- [ ] Resolve X1–X4 and obtain independent acceptance; target validation remains open.
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
- [ ] Target hardware validation on physical MCU/display/radio remains open. Strict policy: no git commit or push.

## 2026-09-15 — Independent V1–V4 result review and W1–W4 handoff

- [x] Confirm current visual improvements, build/31 baseline checks and production
  verification from actual exported artifacts, including strict negative control.
- [x] Reproduce lost Retry after reconnect/late ACK on both pages with latch still
  active; measure transient 3896/4024-byte free-heap dips after the baseline suite.
- [x] Identify missing real TX/sensor/pre-init assertions and export ZIP ownership
  risk; preserve historical evidence and prepare automatic corrective CURRENT.
- [x] Archive V1–V4 handoff as 2026-09-15-04-fix-auto-v1-v4.md.
- [ ] Resolve W1–W4 and obtain independent acceptance. UI direction is retained,
  not globally redesigned. Target validation remains separate; no commit/push.
- This review/handoff changes documentation and host diagnostics only, not UI code.

## 2026-09-15 — Automatic review-to-prompt handoff

- [x] Record owner rule in AGENTS.md and review/prompt guides: every review with
  remaining issues automatically updates CURRENT and returns its link that turn.
- [x] Archive handed-off L1–L6 prompt as 2026-09-15-03-fix-auto-layout-l1-l6.md.
- [x] Prepare self-contained V1–V4 corrective CURRENT, preserving the working main
  layout and Auto contract; include remaining test/export verification gaps.
- [ ] Implement and independently verify the V1–V4 handoff. This turn changes
  documentation/prompt only; no UI fixes, commit or push.

## 2026-09-15 independent review of L1–L6 implementation result

- [x] Rerun build, 24 regressions and targeted pointer/pending/readback/link/stress checks.
- [x] Verify separate production LVGL/UI build and strict negative control outside
  sandbox after a sandbox compiler-probe stall; no target-hardware acceptance.
- [x] Inspect native captures; reproduce open dropdown font14/32px rows and compile
  failures in the restore example. Capture a real Auto timeout independently.
- [ ] Resolve V1–V3 and review V4 optical spacing from
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
- [ ] Target hardware validation on physical MCU/display/radio remains separate. No commit or push.

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
- [ ] Close all R1–R10 acceptance requirements: a corrective implementation was
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
- [ ] Owner review of the Study 12 flat-surface appearance
- [ ] Review physical-LCD readability and remaining control/chart proposals
- [ ] Confirm the exact Hub MCU, Flash/RAM budget, LCD controller, and touch controller
- [x] LoRa RF confirmed by Owner (2026-09-09): 920 MHz, Peer-to-Peer (1 Hub, 1 Node, no complex pairing needed).
- [x] Sensor Modbus closed (2026-09-09): Owner writes Node/Hub firmware to read sensors and copy into `extern` wire globals in `lora_comm.h` directly; UI does not require sensor datasheets.
- [ ] Define relay names, safe states, manual/automatic control rules, and feedback behavior
- [ ] Define alarm thresholds, hysteresis, acknowledgement, history, and offline behavior
- [ ] Define data retention, trends, time source, language, units, and settings ownership
- [ ] Agree on the information architecture and wireframes before visual styling

## Phase 2: UI architecture

- [ ] Define Hub domain model and the MCU-to-UI snapshot interface
- [ ] Define screen hierarchy and bounded update lanes
- [ ] Prototype the highest-memory screen and measure LVGL heap/xSPI redraw cost
- [ ] Revalidate `ui_mcu_profile.h` against the complete target firmware

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
- [ ] Validate on target hardware, including touch, QSPI flush, LoRa loss, and RS-485 sensor faults
