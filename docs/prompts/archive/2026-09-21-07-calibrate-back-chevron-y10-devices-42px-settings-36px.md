# ARCHIVE — Calibrate Back Chevron Position (y=10), Devices Tab Icons (42px), and Settings Modal Icons (36px) (V5)

Date: 2026-09-21. Superseded by: `CURRENT.md` (V6).  
Review reports and evidence: [docs/reviews/2026-09-21-devices-tab-review/REVIEW.md](../reviews/2026-09-21-devices-tab-review/REVIEW.md) and [docs/reviews/2026-09-21-settings-modal-review/REVIEW.md](../reviews/2026-09-21-settings-modal-review/REVIEW.md).  
Evaluation conducted against owner feedback and canonical Quiet Architecture design intelligence (`ui-ux-pro-max`).

---

## 1. Objectives & Summary

Refine visual balance, typography alignment, and icon scale across the **Devices Tab** and **Device Settings** modal dialog in [`ui.c`](../../ui.c) based on direct owner feedback:

1. **Back Chevron Lowered by 2px ($y = 8 \rightarrow y = 10$)**:
   - In both `build_dialog_device_subview()` and `build_dialog_auto_subview()`, shifted the back button container from $(0, 8)$ to $(0, 10)$ with size $(44, 44)$.
   - Places the chevron `<` icon optical center at $y = 10 + 22 = 32$, aligning directly with the title text baseline ($y \approx 34$) and optical mid-body.

2. **Devices Tab Icon Scale Calibration (40px $\rightarrow$ 42px)**:
   - Generated canonical 42px outline preset icons (`purifier_42`, `fan_42`, `drop_42`, `light_42`, `plug_42`, `heat_42`) with 2.0px optical stroke weight.
   - Introduced `get_preset_icon_42(uint8_t preset)` in [`ui.c`](../../ui.c) and updated `build_devices_page()` and `update_devices_page_widgets()`.
   - Placed icons at $(16, 29)$ in the 100px upper card zone with 1:1 vertical symmetry ($29\text{px} : 29\text{px}$) and optical midpoint $y = 50$.

3. **Settings Modal Form Row Icons Scale Calibration (24px $\rightarrow$ 36px)**:
   - Generated canonical 36px icons (`purifier_36`, `fan_36`, `drop_36`, `light_36`, `plug_36`, `heat_36`, `sliders_36`, `leaf_36`, `gear_36`).
   - Integrated across all 4 rows in `build_dialog_device_subview()` and `update_dialog_device_subview()` at $x = 20$. In each 66px row: top margin 15px, bottom margin 15px, optical center $y_{\text{rel}} = 33$ co-linear with 46px control pills.

---

## 2. Out of Scope

- Do not alter wire packet schemas, globals, or radio communication.
- Do not alter Dashboard or Trends layouts.
- Strictly **0 git commit, 0 git push**.

---

## 3. Verification Suite

1. `python tools/generate_assets.py --check` (PASS: 25 canonical masters, 68 runtime icons verified).
2. `tools/build_sim.bat` (PASS, 0 warnings, 0 errors).
3. `tools/run_regression.bat` (33/33 PASS, min free heap: 4,760 B, 0 collision).
4. `tools/export_mcu.bat --verify` (PASS, 16/16 artifacts).
5. `tools/verify_production.bat` (ALL 4 PHASES PASS).
