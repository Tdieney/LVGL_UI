# ARCHIVE — Calibrate Devices Tab Icon Size (40px), Back Chevron Scale Parity (20px), Card Height (264px), and Row Labels Vertical Centering (V4)

Date: 2026-09-21. Superseded by: `2026-09-21-07-calibrate-back-chevron-y10-devices-42px-settings-36px.md` / `CURRENT.md`.  
Review report and evidence: [docs/reviews/2026-09-21-devices-tab-review/REVIEW.md](../reviews/2026-09-21-devices-tab-review/REVIEW.md) and [docs/reviews/2026-09-21-settings-modal-review/REVIEW.md](../reviews/2026-09-21-settings-modal-review/REVIEW.md).  
Evaluation conducted against owner feedback and canonical Quiet Architecture design intelligence (`ui-ux-pro-max`).

---

## 1. Objectives & Summary

Refine visual balance, typography alignment, and icon scale across the **Devices Tab** and **Device Settings** modal dialog in [`ui.c`](../../ui.c) based on direct owner feedback ("hiện tại 32 thì nhỏ quá còn trước đó 44 thì to nên là tao nghĩ mày làm 40 đi"):

1. **Devices Tab Icon Scale Calibration (44px $\rightarrow$ 32px $\rightarrow$ 40px)**:
   - Generated canonical 40px outline preset icons (`purifier_40`, `fan_40`, `drop_40`, `light_40`, `plug_40`, `heat_40`) with 2.0px optical stroke weight, matching the Quiet Architecture line family.
   - Introduced `get_preset_icon_40(uint8_t preset)` in [`ui.c`](../../ui.c) and updated `build_devices_page()` and `update_devices_page_widgets()`.
   - Placed icons at $(16, 30)$ in the 100px upper card zone:
     * Top margin: $30\text{px}$.
     * Bottom margin to divider line ($y = 100$): $100 - (30 + 40) = 30\text{px}$.
     * Optical vertical midpoint: exact $y = 50$, perfectly aligned with toggle switch optical midline ($y = 50$).
     * Gap to device name ($x = 72$): exact $16\text{px}$ breathing room.
     * Overcomes the cramped feel of 44px while eliminating the sparseness of 32px.

2. **Back Button Chevron Scale Parity with Cap-Height (24px $\rightarrow$ 20px)**:
   - Generated canonical asset `ui_icon_chevron_left_20` (canvas 20x20, ink height **18px**) derived directly from the canonical Quiet Architecture master.
   - Replaced `&ui_icon_chevron_left_24` (ink height 22px) with `&ui_icon_chevron_left_20` in both `build_dialog_device_subview()` and `build_dialog_auto_subview()`.
   - The chevron ink height now matches the **exact 18px cap-height** of capital letters in `ui_font_24` (e.g. 'V' in "Ventilation", 'S' in "Settings").

3. **Form Card Height Extension (256px $\rightarrow$ 264px)**:
   - Increased card height by 8px from 256px to 264px ($y = 68..332$), dividing cleanly into 4 balanced rows of **$66\text{px}$** each ($4 \times 66 = 264\text{px}$).
   - Reclaims vertical dead space below the card, tightening the gap to the Cancel/Save action buttons ($y = 352$) to an ideal $20\text{px}$.

4. **Row Labels Vertical Centering (Moved UP)**:
   - Shifted all 4 row title labels ("Device type", "Control mode", "Auto thresholds", "Output polarity") up to relative $y = 17$ inside each 66px row:
     * Row 0: `lbl_t` at $(76, 17)$. Top margin 17px, bottom margin to divider ($y=66$) 18px (near-perfect 1:1 balance).
     * Row 1: `lbl_m` at $(76, 83)$. Top margin 17px, bottom margin 18px.
     * Row 2: `lbl_lim_title` at $(76, 17)$ inside limits button. Top margin 17px, bottom margin 18px.
     * Row 3: `lbl_pol` at $(76, 215)$. Top margin 17px, bottom margin 18px.
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
