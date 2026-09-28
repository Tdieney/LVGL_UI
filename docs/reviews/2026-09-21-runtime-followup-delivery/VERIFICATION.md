# Fresh verification results — 2026-09-21 Delivery

- tools/build_sim.bat: exit 0, CMake CMP0177 developer warning.
- tools/run_regression.bat -SkipBuild: exit 0; complete output in REGRESSION.txt (34/34 checks PASS).
- python tools/generate_assets.py --check: exit 0, 25 masters / 68 runtime icons verified.
- tools/export_mcu.bat --verify: exit 0, MCU export manifest PASS.
- sim_pc/build/sim_pc.exe --shots docs/reviews/2026-09-21-runtime-followup-delivery/captures:
  exit 0, 27 raw scenario framebuffers generated deterministically.
- Host probe compiled with gcc -std=c11 -O2 -Wall -Wextra -Werror
  -DLV_CONF_INCLUDE_SIMPLE -Isim_pc -Isim_pc/build/_deps/lvgl-src -I.
  Inputs: delivery probe.c, ui_auto.c, ui_theme.c, ui_icons.c, ui_fonts.c, ui_splash_logo.c,
  sim_pc/build/lib/liblvgl.a. Exit 0; output in PROBE.txt including standard run and --theme-only.
- powershell -NoProfile -ExecutionPolicy Bypass -File tools/verify_production.ps1:
  exit 0. Phase 1 (X1/Y2/R3 collision safety), Phase 2 (16 isolated exported members with Release
  -Wall -Wextra -Werror), Phase 3 (clean production smoke test with UI_TEST_HOOKS disabled),
  Phase 4 (negative control exit 1 with expected error). ALL FOUR PHASES PASSED.
- raw2png.py converts native RGB565 buffers faithfully to PNGs in captures/; no mockups.
