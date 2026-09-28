# Reviewer commands and limits — 2026-09-21

- Normal tools/build_sim.bat PASS; CMake CMP0177 developer warning only.
- tools/run_regression.bat -SkipBuild PASS34/34; full log REGRESSION.txt.
- Asset --check PASS25masters/68icons; export --verify PASS.
- Isolated production verifier: ALL4 phases PASS with approved escalation.
- New native --shots produced27 scenario frames; reviewer probe freshly compiled.
- Probe command: gcc -std=c11 -O2 -Wall -Wextra -Werror -DLV_CONF_INCLUDE_SIMPLE
  -Isim_pc -Isim_pc/build/_deps/lvgl-src -I. probe.c ui_auto.c ui_theme.c
  ui_icons.c ui_fonts.c ui_splash_logo.c sim_pc/build/lib/liblvgl.a -o probe.exe.
  Probe is observational; printed findings are not assertions.
- style_config_probe.c compiled with ui_theme.c/ui_fonts.c and normal LVGL
  archive for unused symbols. No LVGL runtime/style API invoked by this tiny
  probe. -DLV_USE_ASSERT_STYLE=1 checks descriptor.sentinel; exit1.
  Swap variant uses -DLV_CONF_PATH=<absolute review folder>/swap_conf.h; exit1.
  These are data-contract checks, not alternate-config full integration runs.
- Host ui_theme object GNU size: text2144,data544,bss0; nm all34ui_style_* D.
- RGB565 image comparison via byte-pair comparison, header y<56 excluded from
  normal body equivalence. Normal captures body0 changed pixels; offline/error
  body differences confined to requested state copy. No generated mockups.
-40KiB separate configure:
  cmake -S sim_pc -B docs/reviews/2026-09-21-const-style-review/build-40k
  -G Ninja -DCMAKE_BUILD_TYPE=Release
  -DCMAKE_C_COMPILER=C:/Toolchains/w64devkit/bin/gcc.exe
  -DFETCHCONTENT_SOURCE_DIR_LVGL=<repo>/sim_pc/build/_deps/lvgl-src
  -DFETCHCONTENT_FULLY_DISCONNECTED=ON -DCMAKE_C_FLAGS=-DUI_LVGL_HEAP_BYTES=40960
  cmake --build <that build directory> --target sim_pc -j8
  <that build directory>/sim_pc.exe --regression
  Initial sandbox configure stalled at ABI detection and was interrupted;
  approved escalated retry completed. Build PASS; regression exit0,34/34.
  Profile build is diagnostic only; no production/default file changed.
- No hardware/map/stack-watermark or DMA latency tests.40KiB transient allocation
  instrumentation and full alternate-config smoke remain developer deliverables.
