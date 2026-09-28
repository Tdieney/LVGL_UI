# Independent verification record — 2026-09-20

- `tools/build_sim.bat`: exit0; BUILD.txt contains output.
- `tools/run_regression.bat`: exit0,33/33; REGRESSION_PASS.txt contains full output.
- `tools/export_mcu.bat --verify`: exit0, `Smart Hub MCU export manifest: PASS`.
- `tools/verify_production.bat`: initial sandbox attempt exit1 at
  tools/verify_production.ps1:66, taskkill access denied. Retained diagnostic path:
  `C:/Users/vanth/AppData/Local/Temp/smart_hub_ui_x1_fixture_198006d67bcb4da4af135bcfd8b3f81a`.
  No production failure inferred from this permission denial.
- Same production command outside sandbox: exit0, all four phases passed:
  collision/tamper/race/controlled failure/watchdog checks;16 verified export
  members; isolated C99 host build; production smoke exit0; negative executable
  exit1 with `[SMOKE ERROR] Check failed: 1 == 2` on stderr.
  Full production console output was inspected in-session; this file is a
  result summary, not a fabricated full stdout transcript.
- `python tools/build_review_pack.py docs/reviews/2026-09-20-icon-color-review --test-fail`:
  helper exit0; controlled subprocess exit42 raised RuntimeError as required.
  Its generated REGRESSION.txt is the deliberate failure log; use
  REGRESSION_PASS.txt for actual successful regression evidence.
- Fresh standard simulator --shots captured27 states and RAW→PNG conversion.
- Review-only probe compiled with host GCC `-std=c11 -O2 -Wall -Wextra -Werror`
  and UI_TEST_HOOKS against current production objects/LVGL, excluding simulator
  main object. Exit0;17 captures; PROBE.txt includes genuine ERROR/latch/lateACK
  and pending-polarity setup checks. Object containment passes even in confirmed
  semantic/contrast defects; this is documented, not interpreted as acceptance.
- `analyze.py`: read-only original/capture/A4 analysis; output MEASUREMENTS.json.
  Local LVGL color packing compared against actual blank framebuffer patches.
- Host GCC size: ui_icons.c.obj text21,120B; ui_fonts.c.obj text100,336B; data/BSS0
  for both. Not MCU linker-map or total firmware Flash accounting.
- No physical target, touch, LCD optical, RF or QSPI measurements performed.

Review actions wrote only documentation/evidence and disposable diagnostic build
products. Production source/asset/executable hashes are in MEASUREMENTS.json and
checked unchanged before final handoff. No Git commit/push or owner-data cleanup.
