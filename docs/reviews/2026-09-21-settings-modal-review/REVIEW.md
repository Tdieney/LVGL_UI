# Review Report: Devices Tab & Settings Modal Calibration (V3)

Date: 2026-09-21. Evaluator: `ui-ux-pro-max` design intelligence.  
Mockup Reference: [docs/drafts/2026-09-17-pro-max/settings-quiet-architecture.png](../../drafts/2026-09-17-pro-max/settings-quiet-architecture.png), [docs/drafts/2026-09-17-pro-max/devices-quiet-architecture.png](../../drafts/2026-09-17-pro-max/devices-quiet-architecture.png).  
Active handoff prompt: [docs/prompts/CURRENT.md](../../prompts/CURRENT.md).

---

## 1. Visual Evidence

- **Settings V3 3-Way Comparison**: [settings_v3_3way_comparison.png](settings_v3_3way_comparison.png)  
  *(Previous V2 vs Calibrated V3 [20px Chevron = Cap Height + 264px Card + Centered Row Labels] vs Canonical Concept Mockup)*
- **Header Chevron Zoom Parity**: [settings_header_chevron_zoom.png](settings_header_chevron_zoom.png)  
  *(3x pixel-level comparison demonstrating chevron height parity with capital letter 'V')*
- **Devices Tab 2-Way Comparison**: [devices_v3_comparison.png](devices_v3_comparison.png)  
  *(Previous 44px oversized icons vs Calibrated 32px icons with 1:1 vertical symmetry)*
- **Calibrated Settings Capture (V3)**: [devices_settings_v3.png](devices_settings_v3.png)
- **Calibrated Devices Grid Capture (V3)**: [devices_grid_v3.png](devices_grid_v3.png)
- **Calibrated Auto Thresholds Capture (V3)**: [devices_auto_thresholds_v3.png](devices_auto_thresholds_v3.png)

---

## 2. User Feedback Issues & Implemented V3 Resolutions

| Item | Problem Reported | Resolution Implemented | Impact |
| :--- | :--- | :--- | :--- |
| **1. Devices Tab Icon Size** | "icon ở tab Devices hơi to, cần giảm size một xíu lại". | Replaced 44px icons with `get_preset_icon_32()` at $(16, 34)$ in `build_devices_page()` and `update_devices_page_widgets()`. | Reduced visual heaviness; achieved 1:1 vertical symmetry ($34\text{px}$ top margin vs $34\text{px}$ to divider), perfectly centered on $y=50$ midline with 24px gap to text. |
| **2. Back Chevron Height Parity** | "mũi tên Back về trên cùng cũng cần chỉnh, giảm size nó xuống 1 chút để nó cao bằng chữ hoa đầu tiên". | Added canonical `ui_icon_chevron_left_20` (canvas 20x20, ink height **18px**). Replaced 24px chevron (ink height 22px) in Settings and Auto subviews. | Chevron ink height matches the exact 18px cap height of capital letters in `ui_font_24` ('V' in "Ventilation", 'S' in "Settings"). |
| **3. Card Height Extension** | "card ở chính giữa cần tăng height lên 1 chút vì tao thấy bên dưới còn trống (tăng vài px thôi)". | Increased card height from 256px to 264px ($y = 68..332$), dividing cleanly into 4 rows of **$66\text{px}$** each ($4 \times 66 = 264$). | Tightened empty dead space below card to an ideal 20px gap to action buttons ($y=352$), without crowding. |
| **4. Row Labels Vertical Centering** | "các chữ 'Device type', 'Control mode', 'Auto thresholds', 'Output polarity' 4 chữ này cần move lên 1 xíu để nhìn spacing top vs bottom đều hơn". | Shifted all 4 row labels up to relative $y = 17$ inside each 66px row container. | Top margin is 17px, bottom margin to divider is 18px (flawless 1:1 symmetry, eliminating previous 19px vs 13px downward sag). |
| **5. Collision Clearance** | Relocated error label `s_dialog_validation_lbl` to $(24, 332)$ size $(704, 18)$. | Fits precisely in the 20px gap between card ($y=332$) and buttons ($y=352$). | 0 bounding box collisions; regression tests pass 33/33. |

---

## 3. Verification Suite Results

- **`tools/build_sim.bat`**: **PASS** (Release build, 0 warning, 0 error).
- **`tools/run_regression.bat`**: **33 / 33 checks PASS**
  * 0 bounding box collisions.
  * 0 containment errors.
  * Heap settled: **4,760 B** free (threshold $> 4,096\text{ B}$).
  * Auto lifecycle & font audits ($\ge 18\text{px}$) verified.
- **`tools/export_mcu.bat --verify`**: **PASS** (16/16 artifacts verified).
- **`tools/verify_production.bat`**: **ALL 4 PHASES PASS**
  * Phase 1: MCU Export Manifest & collision safety PASS.
  * Phase 2: Isolated host build with strict `-Werror` PASS.
  * Phase 3: Production smoke test clean run PASS.
  * Phase 4: Negative control (`--fail-check`) strictly verified PASS.
- **Repository constraint**: **0 git commit, 0 git push**.

---

## 4. V5 Adjustments & Visual Evidence (y=10 Back Button, 42px Devices Icons, 36px Settings Icons)

### Owner Directive (2026-09-21)
> *"nói chung là chuyển nó xuống 2px dùm tao. và icon ở Devices tao không muốn size 40 nữa mà là size 42, còn các icon trong Settings thì dùng size 36"*

### Visual Evidence & Measurements
- **Settings V3 vs V4 Comparison**: [settings_v3_vs_v4_comparison.png](settings_v3_vs_v4_comparison.png)  
  *(V3 [24px row icons, back y=8] vs V4 [36px row icons, back y=10])*
- **Header Chevron Zoom V4**: [settings_header_chevron_zoom_v4.png](settings_header_chevron_zoom_v4.png)  
  *(Demonstrating back button lowered to y=10 with center y=32 co-linear with title baseline)*
- **Devices Tab Progression (44px vs 40px vs 42px)**: [devices_progression_44_40_42.png](devices_progression_44_40_42.png)
- **Calibrated Settings Capture (V4)**: [devices_settings_v4.png](devices_settings_v4.png)
- **Calibrated Devices Grid Capture (42px)**: [devices_grid_42px.png](devices_grid_42px.png)

### Summary of Geometric Calibration
1. **Back Chevron Lowered by 2px**: `lv_obj_set_pos(back, 0, 10)` in `build_dialog_device_subview()` and `build_dialog_auto_subview()`. Center $y = 32$.
2. **Devices Tab 42px Icons**: `get_preset_icon_42` at $(16, 29)$ in 100px upper zone. 1:1 symmetry ($29\text{px}$ top, $29\text{px}$ to divider), optical center $y = 50$.
3. **Settings Modal 36px Icons**: Integrated 36px icons across all 4 rows at $x = 20$. In each 66px row: top margin 15px, bottom margin 15px, optical center $y_{\text{rel}} = 33$ co-linear with 46px control pills.

