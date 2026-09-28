# Verification and Delivery Commands — 2026-09-21

## 1. Automated Verification Suite Execution

All standard gates pass cleanly from clean baseline without regressions:
1. **Asset Integrity:**
   ```powershell
   python tools/generate_assets.py --check
   ```
   *Result:* `PASS: Icon manifest, 25 canonical masters, and 68 runtime icons verified.` (Exit 0)

2. **Simulator Build:**
   ```powershell
   tools/build_sim.bat
   ```
   *Result:* Clean compilation with Ninja, `sim_pc.exe` generated. (Exit 0)

3. **Full Regression Suite:**
   ```powershell
   tools/run_regression.bat -SkipBuild
   ```
   *Result:* `Summary: 34 / 34 checks passed. Smart Hub UI regression: PASS` (Exit 0)
   - 27 scenario framebuffers generated.
   - All 12 identical-frame redraw tests `(a1)`..`(a12)` passed with 0 flushes / 0 pixels pushed.

4. **MCU Export Verification:**
   ```powershell
   tools/export_mcu.bat --verify
   ```
   *Result:* `Smart Hub MCU export manifest: PASS` (Exit 0)

5. **Isolated Production Verification:**
   ```powershell
   tools/verify_production.bat
   ```
   *Result:* All 4 phases passed:
   - Phase 1: MCU Export Manifest & collision safety: PASS
   - Phase 2: Isolated Host Build (-Wall -Wextra -Werror, UI_TEST_HOOKS disabled): PASS
   - Phase 3: Production UI smoke test: PASS (Exit 0)
   - Phase 4: Deliberate failure negative control: PASS (Strictly Exit 1)

---

## 2. Alternate Configuration Probes (G2 Proof)

1. **Assertion-Enabled Sentinel Probe (`LV_USE_ASSERT_STYLE=1`):**
   ```powershell
   gcc -O2 -I. -Isim_pc -Isim_pc/build/_deps/lvgl-src -DLV_USE_ASSERT_STYLE=1 docs/reviews/2026-09-21-const-style-delivery/style_config_probe.c ui_theme.c ui_fonts.c sim_pc/build/lib/liblvgl.a -o docs/reviews/2026-09-21-const-style-delivery/assert_probe.exe
   docs/reviews/2026-09-21-const-style-delivery/assert_probe.exe
   ```
   *Result:* Exit 0
   ```
   STYLE_SENTINEL actual=aabbccdd expected=aabbccdd
   COLOR swap=0 actual=ef7c expected=ef7c
   ```

2. **Byte-Swapped Color Representation Probe (`LV_COLOR_16_SWAP=1`):**
   ```powershell
   gcc -O2 -I. -Isim_pc -Isim_pc/build/_deps/lvgl-src -include docs/reviews/2026-09-21-const-style-delivery/swap_conf.h docs/reviews/2026-09-21-const-style-delivery/style_config_probe.c ui_theme.c ui_fonts.c sim_pc/build/lib/liblvgl.a -o docs/reviews/2026-09-21-const-style-delivery/swap_probe.exe
   docs/reviews/2026-09-21-const-style-delivery/swap_probe.exe
   ```
   *Result:* Exit 0
   ```
   COLOR swap=1 actual=7cef expected=7cef
   ```

3. **Section Placement & Symbol Classification:**
   ```powershell
   size sim_pc/build/CMakeFiles/sim_pc.dir/C_/Users/vanth/Documents/MyDocument/Embedded/LVGL_UI/ui_theme.c.obj
   nm --defined-only sim_pc/build/CMakeFiles/sim_pc.dir/C_/Users/vanth/Documents/MyDocument/Embedded/LVGL_UI/ui_theme.c.obj
   ```
   *Result:*
   - `.text`: 2944 B, `.data`: 0 B, `.bss`: 0 B.
   - All 34 `ui_style_*` symbols are **`R`** (read-only data). All property arrays `s_*_props` are **`r`**.

---

## 3. 40 KiB Candidate Trial Execution (G3 Proof)

Candidate profile built with `-DUI_LVGL_HEAP_BYTES=40960`:
```powershell
cmake -S sim_pc -B build-40k -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_C_FLAGS="-DUI_LVGL_HEAP_BYTES=40960"
cmake --build build-40k --target sim_pc
build-40k/sim_pc.exe --regression
```
*Result:* Exit 0
- 34 / 34 checks passed.
- Min settled free: 5,480 B (threshold $> 4,096\text{ B}$).
- Largest free block: 4,000 B.
- 50-cycle navigation drift: +72 B (within existing limits).
- Peak fragmentation: 50%.

---

## 4. Native Capture Pixel Equivalence (G1 Proof)

```powershell
python tools/raw2png.py <in.raw> <out.png>
```
Byte-pair RGB565 comparison against prior review captures confirms:
- Captures below y=56 have strictly 0 diff pixels (100% pixel-identical across all pages and views).
- The only diff pixels are in the header bounding box `x=[469..799], y=[18..35]`, where the link label correctly anchors right edge at `x2 == 777` strictly across both Disconnected and Reconnected states.
