# Smart Hub documentation

- **2026-09-26 Trends live-slot fix:** the chart stayed in a fixed range for up to 60 s while the live value (for example eCO₂ after blowing on the sensor) had already changed. The cause was that history kept only the first reading of each minute; all four charts were affected. The newest slot now follows the latest valid reading (owner decision, D-041) and is covered by CHECK 42. Both suites pass 42/42. See [UI_DESIGN_BRIEF.md](UI_DESIGN_BRIEF.md) and [DEV_LOG](../DEV_LOG.md).

- **2026-09-25 delivery — [REVIEWED, accepted on the host](reviews/2026-09-25-lora-v2-trends-review/REVIEW.md):** LoRa protocol v2 request/response with `GET_STATUS`, eCO₂ ppm / TVOC ppb telemetry, config schema v3, and owner-refined Trends page (hero indicator card, 15-min fixed X window, round-number Y ticks). Simulator and regression suite updated; `tools/build_sim.bat`, `tools/run_regression.bat` (41/41 checks), 40 KiB suite, MCU export and production verification pass cleanly. See [2026-09-25 delivery report](reviews/2026-09-25-lora-v2-trends-delivery/REPORT.md).

- Historical — [Final independent host-software review](reviews/2026-09-25-software-completion-final/REVIEW.md) (before the 2026-09-25 owner changes): ACCEPTED on the Windows host; [CURRENT](prompts/CURRENT.md) is REVIEWED with no active fix request. Native appearance is unchanged; target MCU/LCD/touch/LoRa sign-off still requires owner hardware measurements.

- [Historical N1/N2 correction](reviews/2026-09-24-software-completion-correction-independent/REVIEW.md): both proof gaps were closed and accepted in the final review above; the sole [CURRENT](prompts/CURRENT.md) is now REVIEWED.

- [Historical software-completion submission](prompts/archive/2026-09-24-05-implement-software-completion-submission.md): independently reviewed and corrected. The original software delivery package is in
  [reviews/2026-09-24-software-completion/REPORT.md](reviews/2026-09-24-software-completion/REPORT.md).
  The accepted native appearance is frozen; hardware sign-off remains separate.

- [Latest consolidated delivery report for final Trends (v2)](reviews/2026-09-24-trends-final-v2/REPORT.md):
  developer-submitted evidence for Stage 1, interim title, async flush, verifier,
  combined fault matrix and RAM baseline; not yet an independent final acceptance.

- [Latest independent review of final Trends delivery](reviews/2026-09-24-trends-final-independent/REVIEW.md):
  both 40-check simulator suites pass, but their Stage 1 produces zero pixels;
  the forced-refresh benchmark does not prove early LCD feedback. One
  [historical handoff](prompts/archive/2026-09-24-04-fix-trends-final-v2-submission.md) covered actual-loop timing and remaining
  verifier/E2/evidence gaps without redesigning the accepted UI.

- [Latest independent review of the 2026-09-24 delivery](reviews/2026-09-24-trends-delivery-independent/REVIEW.md):
  39/39 simulator checks pass and history rollover is repaired. Trends switch
  traffic fell about 3%, but real-LCD latency is not measured or closed;
  verifier safety and RAM/fault evidence have remaining gaps. ONE
  [historical handoff](prompts/archive/2026-09-24-03-fix-trends-final-delivery.md) covered them without changing the
  accepted appearance.

- [Latest independent Trends responsiveness review](reviews/2026-09-24-trends-responsiveness-independent/REVIEW.md):
  new 39-check suite passes and history fixes are mostly accepted, but metric-switch
  RGB565 traffic remains exactly at the previous 303–322 kB level. Verifier PID
  cleanup safety, long-session time rollover and E1/E2 RAM/fault evidence remain
  open in a [historical handoff](prompts/archive/2026-09-24-02-fix-trends-evidence-delivery.md).
  Target LCD latency was unmeasured.

- [Latest independent Trends/Auto and hardware-responsiveness review](reviews/2026-09-23-trends-auto-independent/REVIEW.md):
  live-path fixes pass 35 checks; populated Trends metric switches flush
  303–322 kB RGB565 on the host. Three elapsed/history time defects and carried
  RAM/fault proof gaps were consolidated in the [historical handoff](prompts/archive/2026-09-24-01-fix-trends-responsiveness-delivery.md).
  MCU QSPI/touch timing remains unmeasured.

- [Latest Trends/Auto investigation](reviews/2026-09-23-trends-auto-investigation/REVIEW.md):
  live Current bypasses unpopulated history; Manual→Auto starts a fresh10s
  hold and requires three fresh reports. The [historical handoff](prompts/archive/2026-09-23-02-fix-trends-auto-delivery.md)
  implemented these live paths and carried previous RAM/fault evidence.

- [Previous independent G1–G4 review](reviews/2026-09-21-g1-g4-independent/REVIEW.md):
  functional header/style repairs ACCEPTED; no new reproduced UI defect.
  Default/40KiB34/34 and full assert-enabled smoke PASS. ONE CURRENT closes
  remaining RAM evidence/reporting and fault-matrix tests; target sign-off pending.

- [Previous independent const-style review](reviews/2026-09-21-const-style-review/REVIEW.md):
  default34/34 and production PASS; copy and uptime repairs accepted. Remaining
  link-label clipping, style assert/swap config defects and incomplete RAM/test
  claims consolidated in ONE CURRENT. Reviewer40KiB candidate also34/34 PASS;
  default unchanged, hardware/transient sign-off pending.

- [Previous implementation follow-up](reviews/2026-09-21-runtime-followup/REVIEW.md):
 34/34 plus production/asset/export checks PASS. R1–R7 repaired; uptime still
  overlaps after narrow-digit reconnect. ONE [CURRENT](prompts/CURRENT.md) adds
  the owner's simpler unavailable copy, durable tests and remaining measured RAM
  work. +1248B arena headroom is NOT reduced whole-MCU linked RAM.

- [Previous accepted-UI runtime/RAM review](reviews/2026-09-21-runtime-ram-review/REVIEW.md):
  current appearance ACCEPTED by owner.34/34/asset/export/production checks PASS,
  but independent probes reproduce runtime defects and in-place heap growth.
  ONE [CURRENT](prompts/CURRENT.md) covers fixes and invisible optimization.
  Reported target99.9% RAM needs actual map/config/stack evidence.

- [Historical original-icon provenance review](reviews/2026-09-20-icon-origin-review/REVIEW.md):
  CHANGES REQUIRED for icon appearance/pipeline. Original image_gen pixels, not
  a supplied vector library, govern contours. Includes18 verified source ROIs
  and a decoded-A4 comparison. ONE [CURRENT](prompts/CURRENT.md) replaces repeated
  semantic redraws with source-alpha extraction; no new full runtime acceptance.

- [Quiet Architecture C1–C6 Consolidated Delivery](reviews/2026-09-20-quiet-architecture-consolidated/COMPARE.html):
  Author submission; icon closure is contradicted by the latest review above.
  Reported consolidated delivery C1–C6 covers canonical icon
  assets (46 A4 masks), calibrated RGB565 palette & hero gradient, Trends tab unit contrast,
  Trends isolated point/domain alignment, and typography hierarchy. Full 33/33 regression PASS,
  isolated production verification PASS (all 4 phases).

- [Previous icon/color and flow review](reviews/2026-09-20-icon-color-review/REVIEW.md):
  CHANGES REQUIRED; 33/33/export/full production PASS. Quantitative RGB565/icon
  comparison plus reproduced Trends unit/gap/singleton defects in ONE C1–C6
  [historical handoff](prompts/archive/2026-09-20-03-fix-submitted-icon-appearance.md).
  Existing repaired Auto/editor flows preserved; newer runtime fixes are not
  automatically reopened by the targeted icon review.

- [Previous reconstruction review](reviews/2026-09-18-reconstruction-review/REVIEW.md):
  CHANGES REQUIRED. 32/32 and full production verification pass; independent
  boundary/recovery captures expose overlap and history-span defects. Remaining
  fidelity/proof/reporting work is consolidated in ONE Q1–Q5 CURRENT.

- [Previous R2 review](reviews/2026-09-18-fidelity-r2-review/REVIEW.md): CHANGES
  REQUIRED after owner's fidelity rejection. Original composition/icon redraw,
  clipped readings/states and remaining proof gaps in ONE current prompt.

- [Previous fidelity review and comparison](reviews/2026-09-18-design-fidelity/REVIEW.md):
  CHANGES REQUIRED. Seven accepted concepts versus submitted LVGL UI;32/32
  functional checks do not establish visual fidelity. No production edits.
- [Previous native implementation](reviews/2026-09-17-quiet-implementation/REPORT.md):
  author submission and captures, not an accepted replacement visual reference.

- [prompts/CURRENT.md](prompts/CURRENT.md): single active coding-AI handoff for
  final software completion and consolidated review.
- [Previous integration review](reviews/2026-09-17-r1-r4-independent/REVIEW.md):
  MCU includes/global ownership and C99 link accepted; regression31/31/export
  build pass. Remaining artifact/reaping assertions and report inaccuracies.
  Integration guidance corrected; preserve accepted UI and production behavior.
- [Verification Completion Report](reviews/2026-09-17-verification-completion/REPORT.md):
  author submission retained; full-completion/content-identity claims superseded
  by the latest independent review. Real stress/matrix improvements accepted.
- [Previous independent review](reviews/2026-09-17-consolidated-independent/REVIEW.md):
  regression/production verification and additional Auto+modal probe pass;
  no new confirmed runtime defect; prompted R1–R4 verification completion.
- [Y1–Y3 author submission](reviews/2026-09-16-auto-y1-y3-fixes/REPORT.md):
  retained historically; full-completion claims superseded by latest review.
- [Previous independent review](reviews/2026-09-16-auto-x1-x4-independent/REVIEW.md):
  touch/heap/primary export improvements accepted; production rollover regression
  and secondary ZIP overwrite reproduced. Automatic corrective prompt supplied.
- [X1–X4 author submission](reviews/2026-09-16-auto-x1-x4-fixes/REPORT.md):
  retained as submitted; all-complete claims superseded by latest independent review.
- [Previous W1–W4 result review](reviews/2026-09-16-auto-w1-w4-independent/REVIEW.md):
  historical30px targets/primary ZIP defects; these symptoms are now repaired.
- [W1–W4 author report](reviews/2026-09-15-auto-w1-w4-fixes/REPORT.md):
  submission retained; all-complete claims superseded by the independent review.
- [Previous independent review](reviews/2026-09-15-auto-v1-v4-independent/REVIEW.md):
  historical missing-Retry and heap reproductions; those symptoms now pass.
- [Auto V1–V4 verification report](reviews/2026-09-15-auto-v1-v4-fixes/REPORT.md):
  implementation-author submission and native screenshots; its full-acceptance/peak
  claims were superseded by the previous independent review.
- [Previous Auto/layout review](reviews/2026-09-15-auto-layout-result-review/REVIEW.md):
  improved main layout and confirmed tests; remaining V1–V4 findings with native
  captures and diagnostics. Reviews with issues automatically include CURRENT.
- [prompts/README.md](prompts/README.md): mandatory prompt storage/lifecycle rules
  and index of archived implementation, fix and design prompts.
- [TECHNICAL_LEAD_REVIEW.md](TECHNICAL_LEAD_REVIEW.md): independent review criteria
  and required implementation evidence.
- [Auto v1 independent review](reviews/2026-09-14-auto-v1-independent/REVIEW.md):
  2026-09-14 changes-required verdict, reproduced findings and host diagnostics.
- [prototype/index.html](prototype/index.html): interactive 800x480 redesign;
  open directly in a browser. [Usage and screenshots](prototype/README.md).
- [UI_REDESIGN_REVIEW.md](UI_REDESIGN_REVIEW.md): project inspection findings,
  design changes, validation and proposed interactions.
- [HARDWARE_OVERVIEW.md](HARDWARE_OVERVIEW.md): interfaces extracted from the
  supplied LCD and relay shield schematics, sensor context, and open questions.
- [MCU_BASELINE.md](MCU_BASELINE.md): inherited display/memory assumptions that
  must be revalidated for the Smart Hub firmware.
- [UI_DESIGN_BRIEF.md](UI_DESIGN_BRIEF.md): living product/UI decisions,
  unresolved questions, and design discussion history.
- [../PLAN.md](../PLAN.md): discovery and implementation sequence.
- [../DEV_LOG.md](../DEV_LOG.md): material repository changes.
