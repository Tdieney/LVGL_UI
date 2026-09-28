# CURRENT — LoRa v2 simulator/test update and Trends axis/selector refinement

Date: 2026-09-25
Status: **REVIEWED — accepted on the host 2026-09-25** ([technical-lead review](../reviews/2026-09-25-lora-v2-trends-review/REVIEW.md); one fix round F1–F3). The assignment text below is retained for traceability.
Worker: Gemini (via Antigravity CLI), one consolidated delivery. The technical lead reviews the complete delta once.
Previous handoff: [archived](archive/2026-09-25-01-fix-final-verification-claim-closure.md) (REVIEWED, complete).

Decisions: **Owner** decided on 2026-09-25: restore the old Trends selector indicator; move the chart title down, and the chart with it; raise the Y-axis unit; use a fixed 15-minute window ending at "Now" for the X axis; use round-number Y ticks. **TL-proposed** (technical lead) items are marked below. They are implementation defaults; report any you could not meet.

## 1. Read first

`AGENTS.md`, `docs/prompts/README.md`, this file, `LORA_PROTOCOL.md`, `docs/UI_DESIGN_BRIEF.md` (section "2026-09-25 — owner changes"), `docs/user/SMART_HUB_UI_GUIDE.md` §4, §7.4, §8 and §9, and `docs/user/LORA_GUIDE.md` §9. Read only the code sections you change. Do not re-audit the whole repository.

## 2. Existing state (keep it)

- The worktree has many uncommitted owner changes; keep all of them. On 2026-09-25 the owner changed `ui.c`, `ui.h`, `ui_auto.c/.h`, `ui_types.h`, `lora_comm.h` and `lora_hub_link.h` to LoRa protocol v2. That means `GET_STATUS` request/response, `tvoc_ppb`, `sample_seq`-gated Auto, `int16_t` RSSI, `ui_has_pending_command()`, Node-reboot re-sync and config schema v3. The owner also simplified the Trends page. Build on these edits; do not revert the v2 logic.
- Pre-change reference: `smart_hub_ui_20260925_0852.zip` in the repo root holds `ui.c` from 08:52, before the Trends change. Read it read-only (for example `unzip -p smart_hub_ui_20260925_0852.zip ui.c`). Do not extract it into the repo.
- **Known failures now:**
  - `tools/build_sim.bat` fails with 12 × `no member named 'voc_index'`: `sim_pc/main.c` (10), `sim_pc/ui_test_api.c:63`, `sim_pc/test_restore_integration.c:235`. `ui.c` itself compiles.
  - With only that rename applied in a scratch copy, `--shots` still stops at `FATAL: Stage 1 desynchronized title: 'eCO₂' (expected 'CO₂')`.
  - After relaxing that too, it stops at `FATAL: home_auto_paused did not reach ERROR phase / paused_error`. Likely cause: the fake Node never advances `sample_seq`, so Auto never qualifies.
- `tools/export_mcu.bat --verify` passes (file-manifest check only).

## 3. Part A — simulator and tests for protocol v2

A1. Replace every `voc_index` use in `sim_pc/` with `tvoc_ppb`, treating values as ppb. Update title/label expectations to `eCO₂` / `TVOC`.

A2. Make the simulator's Hub/Node emulation follow `LORA_PROTOCOL.md` §3.3, in interactive mode and wherever tests emulate the radio:
- The Hub polls with `GET_STATUS` about once per second, including before `ui_is_tx_ready()`.
- The Hub sends `HUB_CMD` only while `ui_is_tx_ready() && ui_has_pending_command()`, with at most 3 attempts per `seq`.
- The fake Node replies to both message types and applies relays only for `HUB_CMD`.
- The fake Node advances `sample_seq` once per simulated sensor poll and reports an increasing `uptime_sec`.
- Keep the documented interactive behaviour: 700 ms acknowledgement delay and the `F`/`T`/`S`/`1-3`/`Esc` keys in `README.md`.

A3. Wherever a test expects Auto to qualify, advance `sample_seq` per new sample. This fixes `home_auto_paused`.

A4. Remove the tests and hook fields for widgets the owner deleted: Trends Current/Min/Max buffers and labels, the span label and the chart tooltip. That covers `ui_test_hooks.h`, `ui_test_api.*`, the `UI_TEST_HOOKS` block in `ui.c` and `main.c` checks such as CHECK 37/39/40 sections. Replace them with assertions for the current design in Part B; do not delete coverage without a replacement.

A5. Add durable regression checks for the owner's v2 behaviour:
- (a) Repeated reports with the same `sample_seq` do not advance Auto qualification; 3 distinct samples do.
- (b) A lower `uptime_sec` drops TX readiness, then all four reported GPIO levels are adopted and not replayed.
- (c) After a command timeout, `lora_hub_cmd` carries the reported level for that channel; a late matching report returns it to IDLE.
- (d) `ui_has_pending_command()` is true only while a command is PENDING.
- (e) Validity: eCO₂ 399 and 60001 are unavailable, while 400 and 60000 are valid; TVOC 0 and 60000 are valid, while 60001 is unavailable.
- (f) Schema: a 0x02 config for Air Purifier comes back as 0x03 with 150/100, other presets keep their thresholds, and boot staging fires no callback.
- (g) `lora_last_rssi = -140` round-trips as `int16_t`.
- (h) Auto max: 60000 ppm and 60000 ppb render in the Auto editor without clipping. Rename the `devices_auto_max_10000` capture to match, consistently in `main.c` and `tools/run_regression.ps1`.

## 4. Part B — Trends page (owner decisions; pixel values TL-proposed)

Card-relative coordinates in the 552×344 chart card; the left rail is unchanged.

B1. **Restore the selector indicator** exactly as in the 08:52 `ui.c` (`build_trends_page()` selector loop and `update_trends_tabs()`):
- The selected card uses `ui_style_card_hero` on `LV_STATE_CHECKED`.
- The selected card's icon, name, value and unit are white. Unselected cards have an ink icon/name/value and a muted unit.
- The selected card shows a celadon 8×8 dot (radius 4) at (160, 14); it is hidden on the other cards.
- Remove the 6×48 forest bar.

Keep the owner's touch-down selection (`LV_EVENT_PRESSED`) and the 150 ms `UI_TRENDS_SETTLE_MS` coalescing. TL-proposed: do not re-add `ui_style_pressed` to the selectors, because with touch-down selection it would add a second repaint per tap.

B2. **Title and chart lower** (TL values):

| Element | Position |
|---|---|
| Title | (20, 14), was (20, 2) |
| `Data unavailable` badge | top-right, `LV_ALIGN_TOP_RIGHT, -20, 23`, was 11 |
| Y unit label | y 96, was 92; the axis block moves +16, the unit is raised 12 relative to it |
| Y tick labels | y 136 / 191 / 246 |
| Chart | (72, 136), size and padding unchanged |
| Time labels | y 276 |
| Empty-state box | (20, 108), 512×191 |

The unit label must end at least 16 px above the top tick label box.

B3. **X axis: fixed 15-minute window.**
- The chart always has `UI_HISTORY_CAPACITY` (16) points. The newest history sample is slot 15 (right edge). With `count < 16`, history index `i` maps to slot `16 - count + i`, and leading slots are `LV_CHART_POINT_NONE`.
- Autonomous history labels are the static strings `-15 min`, `-10 min`, `-5 min`, `Now`, at slots 0/5/10/15. Use the ASCII hyphen, because the fonts contain ASCII 32..126 and no U+2212; do not regenerate fonts.
- External feed (`ui_feed_history`) uses the same right-aligned window. Its labels are `HH:MM` of `newest_minute - (15 - slot)` with midnight wrap, and the right label is also `HH:MM`, not `Now`.
- Label boxes are about 80 px wide and centred on slot x 83 / 229 / 375. The last label is right-aligned to x 532.
- Labels never duplicate. This fixes the current 3-sample `+0m +0m +1m +2m` bug.
- Empty states stay: 0 samples → `No readings yet`; 1 → `Collecting readings…` plus the hint.

B4. **Y axis: round-number ticks.**
- Exactly three labels `lo`, `lo+s`, `lo+2s`, where `s ∈ {1, 2, 2.5, 5}×10^k` in display units and is an integer in raw units (temperature raw = 0.1 °C).
- The chart range is `[lo, lo+2s]`, so the three grid lines coincide with the labels.
- TL algorithm: expand spans below the existing minimum (10 raw for temperature, 20 otherwise) symmetrically. Pad by `max(1, 5% of span)` on each side. Choose the smallest `s` with `lo = floor(pmin/s)*s` and `lo+2s ≥ pmax`, using floor for negatives and 64-bit math.
- Required examples:
  - eCO₂ 410..440 → `400 / 425 / 450`.
  - Temperature 22.8..23.6 °C → `22.0 / 23.0 / 24.0`.
  - Temperature −2.5..−0.5 °C has correct negative labels.
  - TVOC 59000..60000 fits right-aligned in the 56 px column.
- Labels keep real units and use the existing integer formatters; no float formatting.

B5. Keep everything else the owner set: native 2 px line and 4 px markers, normalized 0..1000 plotting, surface fill, non-clickable chart, the title/badge row, and ≥ 18 px text. Keep static captions in const storage via `lv_label_set_text_static()`, fixed buffers, and no per-tick allocation.

## 5. Out of scope

Packet layout/schema, `ui_auto` thresholds and rules, Home/Devices/Settings visuals, fonts/icons regeneration, and firmware/radio drivers. Two items stay owner-open and must not be decided by the worker: the TVOC ppb quality bands (100/250), and the remaining `VOC` copy (`Based on eCO₂ and VOC`, `Auto · VOC`). No commit, push, branch, stage, stash, reset, checkout or clean. Do not delete user files.

## 6. Acceptance

1. `python tools/generate_assets.py --check` exits 0.
2. `tools/build_sim.bat` exits 0 with `-Wall -Wextra -Werror`.
3. `tools/run_regression.bat` (42 KiB) passes all checks, including A5 and B1–B4 assertions, the exact capture manifest and the `--shots NUL` negative control.
4. `cmake --build build-40k --target sim_pc` then `build-40k\sim_pc.exe --regression` passes all checks.
5. `tools/export_mcu.bat --verify` and `tools/verify_production.bat` exit 0. If the sandbox blocks the verifier's CIM calls, say so; do not weaken it.
6. Captures include every Trends state as PNG:
   - `eCO₂`, `TVOC`, temperature and humidity with 16 external samples.
   - Autonomous history with fewer than 16 samples (right-aligned, `-15 min … Now`).
   - The empty state and the one-sample state.
   - A Stage-1 interim frame (restored forest card, old chart still showing).
7. Benchmark rerun reports Stage 1 and Stage 2 flush pixels, compared with the accepted 2026-09-24 values (Stage 1 5 flushes / 34,532 px). Also rerun heap figures (42/40 KiB), and 0 B drift over 100 switches.
8. `git diff --check` is clean.

## 7. Deliverables

- Code and test changes.
- `docs/reviews/2026-09-25-lora-v2-trends-delivery/REPORT.md` with captures, command results (exact exit codes; list upstream CMake dev warnings separately from compiler warnings) and SHA-256 of the captures.
- Update `docs/UI_DESIGN_BRIEF.md`: in the 2026-09-25 Trends subsection record the restored indicator and new axes, and amend D-040.
- Update `docs/user/SMART_HUB_UI_GUIDE.md` §7.4.
- Update `docs/MCU_BASELINE.md`: replace the "pre-change" note with re-measured figures.
- Update the status lines in `README.md`/`docs/README.md`, `PLAN.md`, `DEV_LOG.md` and `docs/prompts/README.md`.
- Set this file's status to `IMPLEMENTED — awaiting review`.

Physical MCU/LCD/touch/LoRa behaviour remains **UNVERIFIED** (host only).
