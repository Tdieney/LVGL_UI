# CURRENT — Calibrate Settings Modal V2 (Title Scale, Back Chevron Alignment, Option 1 Card Spacing, Unified 24px Row Icons)

Date: 2026-09-21. Status: **IMPLEMENTED — awaiting review**.  
Review report and evidence: [docs/reviews/2026-09-21-settings-modal-review/REVIEW.md](../reviews/2026-09-21-settings-modal-review/REVIEW.md).  
Evaluation conducted against user feedback and canonical Quiet Architecture concept mockup `docs/drafts/2026-09-17-pro-max/settings-quiet-architecture.png`.

---

## 1. Objectives & Summary

Calibrate visual balance, header hierarchy, and icon sizing across the **Device Settings** modal dialog in [`ui.c`](../../ui.c) based on owner review feedback:

1. **Title Scale Calibration**:
   - Reduced dialog contextual titles (`s_dialog_device_title_lbl` and `lbl_auto_title`) from `ui_font_32` (32px Bold) to `ui_font_24` (24px Semibold).
   - Resolves the oversized title issue while preserving crisp contrast and hierarchy over the 20px row labels.

2. **Form Card Vertical Spacing Balance (Option 1 Implemented)**:
   - Shifted form card `card` from $(0, 52)$ to $(0, 68)$ with height $256\text{px}$ ($y = 68..324$).
   - Achieves balanced spacing rhythm:
     * Header breathing room above card: $24\text{px}$ (under $y=44$ header bounds).
     * Gap between card and action buttons: $28\text{px}$ ($y=324 \rightarrow 352$).
     * Preserves strict 64px ($8 \times 8$) Quiet Architecture row rhythm and centered 46px interactive controls.

3. **Unified 24px Row Icons**:
   - Replaced oversized 44px preset icon in Row 0 (`get_preset_icon_44`) with `get_preset_icon_24(uint8_t preset)` returning the exact 24px assets (`&ui_icon_purifier_24`, `&ui_icon_fan_24`, `&ui_icon_drop_24`, `&ui_icon_heat_24`, `&ui_icon_light_24`, `&ui_icon_plug_24`).
   - Unified icon offsets across all 4 rows to $(26, 20)$ relative to each row container, matching Rows 1–3 (`ui_icon_sliders_24`, `ui_icon_leaf_24`, `ui_icon_gear_24`).

4. **Back Arrow & Title Optical Axis Alignment**:
   - Repositioned back button from $(0, 0)$ to $(0, 8)$ with size $(44, 44)$.
   - Positioned `ui_font_24` title at $(44, 16)$, placing the optical centers of both the back chevron `<` and title baseline on the exact same axis ($y \approx 29\text{--}30$).
   - Eliminates the upward displacement ("lệch lên trên") of the back chevron.

5. **Auto Thresholds Dialog Header Parity**:
   - Applied identical header geometry in `build_dialog_auto_subview()`: back button at $(0, 8, 44, 44)$ and `ui_font_24` title at $(44, 16)$.

6. **Zero Regression & Collision Clearance**:
   - Shifted `s_dialog_err_lbl` from $(24, 310)$ to $(24, 326)$ to guarantee 0 bounding box collision with the relocated card ($y = 68..324$).

---

## 2. Out of Scope

- Do not alter wire packet schemas, globals, or radio communication.
- Do not alter Dashboard or Trends layouts.
- Strictly **0 git commit, 0 git push**.

---

## 3. Verification Results

1. `tools/build_sim.bat` (PASS, 0 warning, 0 error).
2. `tools/run_regression.bat` (33/33 PASS, min free heap: 4,760 B, 0 collision).
3. `tools/export_mcu.bat --verify` (PASS, 16/16 artifacts).
4. `tools/verify_production.bat` (ALL 4 PHASES PASS).
