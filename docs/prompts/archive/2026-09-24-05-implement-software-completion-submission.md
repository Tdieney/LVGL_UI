# ARCHIVED — historical context only

Superseded by [CURRENT](../CURRENT.md) after independent review of the software-completion submission. Original handoff follows; relative links below were written for the original `docs/prompts/CURRENT.md` location.

# CURRENT — Smart Hub UI software completion

Date: 2026-09-24
Status: **IMPLEMENTED — awaiting review**
Scope: Complete software-completion delivery submitted in [docs/reviews/2026-09-24-software-completion/REPORT.md](../../reviews/2026-09-24-software-completion/REPORT.md), ready for one independent technical-lead review. The owner's accepted native Quiet Architecture appearance is frozen. The [previous submitted Trends v2 handoff](2026-09-24-04-fix-trends-final-v2-submission.md) is historical evidence, not a second active prompt.

## Authority and prerequisites

Read `AGENTS.md`, `docs/prompts/README.md`, `docs/README.md`, `docs/HARDWARE_OVERVIEW.md`, `docs/MCU_BASELINE.md`, `docs/UI_DESIGN_BRIEF.md`, `docs/user/SMART_HUB_UI_GUIDE.md`, `docs/TECHNICAL_LEAD_REVIEW.md`, `PLAN.md`, and the latest [Trends v2 delivery report](../reviews/2026-09-24-trends-final-v2/REPORT.md). Inspect the current source, test harness, screenshots, tool scripts, and `git status`; do not assume the author's report proves acceptance. Preserve every pre-existing dirty-worktree change. Owner decision: finish the current UI, delegate code and testing to Gemini, and let the technical lead review once after the complete delivery. Technical-lead decision: preserve accepted product scope and appearance, close only demonstrated software defects and reproducibility gaps; hardware-only sign-off remains explicit.

## Objectives and boundaries

1. Audit the *whole current* LVGL v8.4 UI end to end: Home/Dashboard, Trends, Devices, Settings/Auto editors, splash, boot/offline/recovery, input/touch, display scheduling, link/relay semantics, history, RAM, and MCU export. Independently verify the specific Trends v2 claims (positive Stage-1 flush on the normal `ui_tick()` / `lv_timer_handler()` schedule, async flush ordering, coherent interim frame, and honest single-switch versus burst metrics). Fix any reproduced in-scope defect discovered during this audit; do not change stable behavior on speculation.
2. Exercise real integration contracts in host fixtures: validated LoRa extern globals and revision, `ui_tick()` then TX readiness, desired versus reported relay states and active-low mapping, Auto threshold/qualification/hold/manual override/fault recovery, sensor validity including VOC=0, autonomous and externally fed Trends history, gaps/min/max across all four metrics, minute rollover, rapid touch/navigation, simultaneous faults, and no-data states. Add durable tests only where an actual gap remains.
3. Check smoothness and memory on both 42 KiB default and opt-in 40 KiB profiles. Measure real host flush calls/pixels, first-feedback/final-chart timing under a declared *simulated* QSPI model, idle redraw, 100-tap burst and repeated dialogs/navigation, settled/transient free heap, largest request/block, failure count, and static `.data`/`.bss`. Optimize only if measured benefit is material and does not change accepted pixels, add RAM risk, or obscure target-only limits. Keep default 42 KiB unless target evidence justifies a profile change.
4. Validate Windows production verifier process ownership and fail-closed behavior without harming unrelated processes; verify asset generation, MCU export manifest, plain `#include "lvgl.h"` integration, globals defined exactly once, and absence of simulator-only dependencies in exported files.

Out of scope: new visual style/copy, icons, fonts, product features, radio/Modbus drivers, Node firmware, packet schema, Flash persistence, full framebuffer, target firmware memory-map claims, physical LCD latency claims, commits/pushes, resets/cleans/stashes or deleting existing user files. The LCD is 800×480 RGB565 with one 800×10 partial draw buffer; all visible text stays ≥18 px. Do not import motor-control UI or behavior. A target-only unknown is not a software defect to paper over.

## One complete worker delivery

Gemini owns implementation, its own full-diff review, and all tests. Work internally as needed but do not hand off partial issue groups or request a technical-lead review between substeps. Preserve existing tests and accepted appearance. After the **last** relevant edit, run `python tools/generate_assets.py --check`, `tools/build_sim.bat`, `tools/run_regression.bat`, `tools/export_mcu.bat --verify`, both 42/40 KiB stress suites, and `tools/verify_production.bat`; record exact commands, exit codes and raw logs. If a command cannot run, report the concrete tool/permission blocker instead of claiming PASS. Capture representative native 800×480 LVGL states, including the Stage-1 interim Trends frame, and compare against accepted current appearance. Do not substitute browser mockups.

Deliver ONE dated folder under `docs/reviews/` with a concise report, reproducible tests/fixtures, logs, hashes/config identity, measured performance/RAM figures and limitations. Clearly separate fixed defects, confirmed passing behavior, optional non-blocking polish, and target-only unverified checks. Update `PLAN.md`, `DEV_LOG.md`, `docs/README.md`, `docs/MCU_BASELINE.md` only to match proven final evidence; mark this CURRENT `IMPLEMENTED — awaiting review` only when complete. No new active prompt.

## Technical-lead acceptance after the complete delivery

Codex will independently inspect the full submitted diff and affected flows once, rerun proportional gates, check screenshots and measurements, then either accept software completion or return one consolidated evidence-based fix request through the same Gemini conversation. The target MCU map/stack/DMA/radio buffers and physical LCD touch-to-photon/LoRa behavior require owner hardware access and must remain `UNVERIFIED` until measured. Do not promise zero bugs or describe host simulation as hardware validation.
