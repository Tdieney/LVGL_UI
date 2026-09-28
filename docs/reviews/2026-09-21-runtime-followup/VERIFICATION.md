# Fresh command results — 2026-09-21

- tools/build_sim.bat: exit0, Ninja no work; CMake CMP0177 developer warning.
- tools/run_regression.bat -SkipBuild: exit0; complete output REGRESSION.txt.
- python tools/generate_assets.py --check: exit0,25masters/68runtime icons verified.
- tools/export_mcu.bat --verify: exit0, manifest PASS.
- sim_pc/build/sim_pc.exe --shots docs/reviews/2026-09-21-runtime-followup/captures:
  exit0,27 raw frames (legacy terminal counter says24).
- Host probe compiled with gcc -std=c11 -O2 -Wall -Wextra -Werror
  -DLV_CONF_INCLUDE_SIMPLE -Isim_pc -Isim_pc/build/_deps/lvgl-src -I.
  Inputs: review probe.c,ui_auto.c,ui_theme.c,ui_icons.c,ui_fonts.c,ui_splash_logo.c,
  sim_pc/build/lib/liblvgl.a. Exit0; output PROBE.txt includes normal and --theme-only.
- powershell -NoProfile -ExecutionPolicy Bypass -File tools/verify_production.ps1:
  initial sandbox exit1 at taskkill Access denied; rerun with approved escalation
  exit0. X1/Y2/R3 collision/failure/watchdog checks PASS;16 exported members PASS;
  isolated Release -Wall -Wextra -Werror build PASS; clean production smoke exit0;
  negative --fail-check exit1 with expected error confirmed; ALL PHASES PASSED.
- raw2png.py converts native RGB565 buffers faithfully; no generated mockups.

The independent probe is observational, not an assertion suite. F1 is deliberately
reproduced despite its process exit0. No MCU hardware/map/timing tests performed.
