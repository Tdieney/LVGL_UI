# Technical-lead review — LoRa v2 simulator/test update and Trends refinements

Date: 2026-09-25 · Reviewer: technical lead · Worker: Gemini 3.7 Flash (High) via Antigravity CLI
Handoff: [CURRENT](../../prompts/CURRENT.md) · Delivery: [REPORT](../2026-09-25-lora-v2-trends-delivery/REPORT.md)

**Verdict: ACCEPTED on the Windows host.** The delivery took one consolidated fix round (F1–F3). Target MCU/LCD/touch/LoRa behaviour is not verified.

## Scope reviewed

The complete delta from the pre-run baseline snapshot to the final state:
- `ui.c` (Trends page, axis code, `UI_TEST_HOOKS` block)
- `sim_pc/main.c`, `ui_test_api.*`, `ui_test_hooks.h`, `test_restore_integration.c`
- `tools/run_regression.ps1`
- The docs and report

I also read the affected end-to-end paths (Trends switch, empty/one-sample/autonomous/external history, Auto qualification, command timeout, Node reboot) and inspected the Trends captures.

## Findings and disposition

| ID | Finding | Disposition |
|---|---|---|
| F1 | The Y-axis computation stored `lo + 2*step` in `int32_t` without clamping. That dropped the owner's 64-bit guard, so an external feed near `INT32_MAX`/`INT32_MIN` would overflow. | Fixed: the math is now `int64_t` and clamped. `format_temp_c` is widened. CHECK 10 asserts strictly increasing labels for feeds near `INT32_MAX` and `INT32_MIN`. |
| F2 | Four docs still said the simulator did not build. | Fixed in `UI_DESIGN_BRIEF.md`, `SMART_HUB_UI_GUIDE.md` (header and §12) and `MCU_BASELINE.md`. |
| F3 | `UI_DESIGN_BRIEF.md` said the owner "approved" technical-lead pixel values. | Fixed: the owner decided five directions; the pixel values and tick algorithm are attributed to the technical lead. |
| Housekeeping | Gemini had left `reg_out.txt`, an extra export ZIP and a new `sim_pc/build-40k/` in the repo. | Removed in turn 2. The 40 KiB suite uses the existing root `build-40k/`. |

Accepted as specified:
- **B1:** forest `CHECKED` card, white text and celadon dot; touch-down selection and 150 ms settle kept.
- **B2:** geometry: title (20, 14), unit (12, 96), ticks 136/191/246, chart (72, 136), time labels y 276.
- **B3:** 16-slot right-aligned window. Autonomous labels are `-15 min · -10 min · -5 min · Now`; external history shows `HH:MM` with no duplicates.
- **B4:** round-number ticks (`400/425/450`, `22.0/23.0/24.0`).
- **A1–A5:** the v2 fake Node (`GET_STATUS`/`HUB_CMD`, `sample_seq`) and CHECK 41 (a)–(h).

## Independent verification (rerun by the reviewer after the last edit)

| Command | Result |
|---|---|
| `python tools/generate_assets.py --check` | exit 0 |
| `tools/build_sim.bat` | exit 0. No compiler warnings (`-Wall -Wextra -Werror`); one upstream CMake CMP0177 developer warning. |
| `tools/run_regression.bat` (42 KiB) | exit 0 — 41/41; 30/30 captures, 768,000 B each; `--shots NUL` exits 1 with the expected diagnostic |
| `cmake --build build-40k --target sim_pc`; `build-40k\sim_pc.exe --regression` | exit 0 — 41/41 |
| `tools/export_mcu.bat --verify` | exit 0 (file-manifest check only) |
| `tools/verify_production.bat` | exit 0 — ALL PASSED |
| `git diff --check` | exit 0 (only CRLF notices) |

Benchmark (CHECK 39):
- Stage 1: 5 flushes / 34,532 px, equal to the accepted 2026-09-24 value.
- Stage 2: 14 flushes / 89,388 px.
- Heap drift over 100 switches: 0 B.

Heap figures match the reviewer's logs:
- 42 KiB: settled minimum 8,072 B, transient minimum 3,664 B.
- 40 KiB: settled minimum 5,984 B, transient minimum 1,616 B.

## Residual items (not defects of this delivery)

- **Owner-open:** TVOC ppb quality bands (the former index cut-offs 100/250 are reused as ppb), and the remaining `VOC` copy (`Based on eCO₂ and VOC`, `Auto · VOC`).
- **Optional polish:** in the restored selector, the celadon dot at (160, 14) sits only a few pixels after the word `Temperature`. This is identical to the pre-change design the owner asked to restore.
- **Target-only:** both boards need protocol `0x02` firmware, `GET_STATUS` polling before TX-ready, and one tick clock for `lora_last_rx_tick_ms`/`lv_tick_get()`. The MCU map and physical LCD latency are also unmeasured.
- **Tooling:** in turn 1 Gemini wrote a helper script in its own Antigravity scratch directory, outside the repo. Setting `allowNonWorkspaceAccess: true` remains a residual risk. An idle orphan `agy.exe` from turn 1 could not be stopped (access denied).
