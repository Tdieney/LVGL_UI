# ARCHIVE — Calibrate Devices Tab Icon Size (32px), Back Chevron Scale Parity (20px), Card Height (264px), and Row Labels Vertical Centering (V3)

Date: 2026-09-21. Superseded by: `2026-09-21-06-calibrate-devices-40px-settings-v4.md` / `CURRENT.md`.  
Review report and evidence: [docs/reviews/2026-09-21-settings-modal-review/REVIEW.md](../reviews/2026-09-21-settings-modal-review/REVIEW.md).  
Evaluation conducted against user feedback and canonical Quiet Architecture design intelligence (`ui-ux-pro-max`).

---

## 1. Objectives & Summary

Refine visual balance, typography alignment, and icon scale across the **Devices Tab** and **Device Settings** modal dialog in [`ui.c`](../../ui.c) based on direct owner review feedback:

1. **Devices Tab Icon Scale Calibration (44px $\rightarrow$ 32px)**:
   - Replaced oversized 44px icons in `build_devices_page()` and `update_devices_page_widgets()` with `get_preset_icon_32(cfg->preset)` positioned at $(16, 34)$.
   - Establishes a 16px left grid margin, 24px clean gap to device name ($x = 72$), and achieves **flawless 1:1 vertical symmetry** ($34\text{px}$ from top edge, $34\text{px}$ to divider at $y=100$) on the $y = 50$ optical midline.

2. **Back Button Chevron Scale Parity with Cap-Height (24px $\rightarrow$ 20px)**:
   - Generated canonical asset `ui_icon_chevron_left_20` (canvas 20x20, ink height **18px**) derived directly from the canonical Quiet Architecture master.
   - Replaced `&ui_icon_chevron_left_24` (ink height 22px) with `&ui_icon_chevron_left_20` in both `build_dialog_device_subview()` and `build_dialog_auto_subview()`.
   - The chevron ink height now matches the **exact 18px cap-height** of capital letters in `ui_font_24` (e.g. 'V' in "Ventilation", 'S' in "Settings").

3. **Form Card Height Extension (256px $\rightarrow$ 264px)**:
   - Increased card height by 8px from 256px to 264px ($y = 68..332$), dividing cleanly into 4 balanced rows of **$66\text{px}$** each ($4 \times 66 = 264\text{px}$).
   - Reclaims vertical dead space below the card, tightening the gap to the Cancel/Save action buttons ($y = 352$) to an ideal $20\text{px}$.

4. **Row Labels Vertical Centering (Moved UP)**:
   - Shifted all 4 row title labels ("Device type", "Control mode", "Auto thresholds", "Output polarity") up to relative $y = 17$ inside each 66px row:
     * Row 0: `lbl_t` at $(76, 17)$ (was 19). Top margin 17px, bottom margin to divider ($y=66$) 18px (near-perfect 1:1 balance).
     * Row 1: `lbl_m` at $(76, 83)$ (was 82). Top margin 17px, bottom margin 18px.
     * Row 2: `lbl_lim_title` at $(76, 17)$ inside limits button (was 20). Top margin 17px, bottom margin 18px.
     * Row 3: `lbl_pol` at $(76, 215)$ (was 211). Top margin 17px, bottom margin 18px.
   - Eliminates previous downward sag where top margin was 19–20px and bottom margin was only 13–14px.

5. **Collision Clearance**:
   - Positioned validation error label `s_dialog_validation_lbl` at $(24, 332)$ with size $(704, 18)$, guaranteeing 0 bounding box collision with the 264px card ($y \le 332$) and action buttons ($y \ge 352$).

---

## 2. Out of Scope

- Do not alter wire packet schemas, globals, or radio communication.
- Do not alter Dashboard or Trends layouts.
- Strictly **0 git commit, 0 git push**.

---

## 3. Verification Suite

1. `tools/build_sim.bat` (PASS, 0 warnings, 0 errors).
2. `tools/run_regression.bat` (33/33 PASS, min free heap: 4,760 B, 0 collision).
3. `tools/export_mcu.bat --verify` (PASS, 16/16 artifacts).
4. `tools/verify_production.bat` (ALL 4 PHASES PASS).
