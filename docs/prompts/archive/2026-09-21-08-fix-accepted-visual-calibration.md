> ARCHIVED — historical context only. Active handoff: [CURRENT](../CURRENT.md).
> Owner accepted the appearance on 2026-09-21; superseded by runtime/RAM review.

# CURRENT — Align Settings Button on Devices Tab: Text 'Settings' Left-Aligned with 'Manual' (x=72) and Icon Centered with Device Icon (x=37) (V6)

Date: 2026-09-21. Status: **IMPLEMENTED — verified**.  
Review report and evidence: [docs/reviews/2026-09-21-devices-tab-review/REVIEW.md](../../reviews/2026-09-21-devices-tab-review/REVIEW.md) and [docs/reviews/2026-09-21-settings-modal-review/REVIEW.md](../../reviews/2026-09-21-settings-modal-review/REVIEW.md).  
Evaluation conducted against owner feedback ("4 chữ 'Settings' ở tab Devices cần align left với chữ 'Manual' bên trên cúng, và icon setting cần align center với icon device bên trên chúng") and canonical Quiet Architecture design intelligence (`ui-ux-pro-max`).

---

## 1. Objectives & Summary

Refine visual alignment across the **Devices Tab** cards in [`ui.c`](../../../ui.c) based on direct owner feedback:

1. **Text "Settings" Left-Aligned with "Manual" ($x = 72$)**:
   - In `build_devices_page()`, updated `cfg_label` ("Settings") position from $(48, 20)$ to $(72, 20)$.
   - Establishes a strict vertical left-alignment axis at **$x = 72$** across all 3 text elements on the card:
     * Line 1: Device Name at $x = 72$ (e.g. "Ventilation Fan")
     * Line 2: Mode subtitle at $x = 72$ (e.g. "Manual")
     * Action Bar: "Settings" label at $x = 72$
   - Leaves a comfortable $196\text{px}$ clearance to the right chevron arrow at $x = 332$.

2. **Settings Icon Centered with Device Icon ($x_{\text{center}} = 37$)**:
   - In `build_devices_page()`, updated `cfg_ico` (`&ui_icon_sliders_24`, size $24 \times 24$) from $(16, 20)$ to $(25, 20)$.
   - The device icon above is $42 \times 42\text{px}$ placed at $(16, 29)$, spanning $x \in [16, 58]$ with optical center at $x = 16 + 21 = \mathbf{37\text{px}}$.
   - With `cfg_ico` starting at $x = 25$, it spans $x \in [25, 49]$ with optical center at $x = 25 + 12 = \mathbf{37\text{px}}$.
   - **Both icons share the exact same vertical center axis $x = 37$**, creating structured, elegant 2-column card architecture:
     * Column 1 (Icons): Optical centerline at $x = 37$.
     * Column 2 (Typography): Left-alignment spine at $x = 72$.

---

## 2. Out of Scope

- Do not alter wire packet schemas, globals, or radio communication.
- Strictly **0 git commit, 0 git push**.

---

## 3. Verification Suite

1. `python tools/generate_assets.py --check` (PASS: 25 canonical masters, 68 runtime icons verified).
2. `tools/build_sim.bat` (PASS, 0 warnings, 0 errors).
3. `tools/run_regression.bat` (33/33 PASS, min free heap: 4,760 B, 0 collision).
4. `tools/export_mcu.bat --verify` (PASS, 16/16 artifacts).
5. `tools/verify_production.bat` (ALL 4 PHASES PASS).
