# CURRENT — calibrate spatial alignment and layout in Tab Devices

Date: 2026-09-21. Status: **IMPLEMENTED — awaiting review**.  
Review report and evidence: [docs/reviews/2026-09-21-devices-tab-review/REVIEW.md](../reviews/2026-09-21-devices-tab-review/REVIEW.md).  
Evaluation conducted by `ui-ux-pro-max` against canonical Quiet Architecture concept mockup `docs/drafts/2026-09-17-pro-max/devices-quiet-architecture.png`.

---

## 1. Objectives & Summary

Calibrate internal geometry, alignment axes, and vertical balance across all 4 device cards in the **Devices tab** (`build_devices_page()` and `update_devices_page_widgets()` in [`ui.c`](file:///c:/Users/vanth/Documents/MyDocument/Embedded/LVGL_UI/ui.c)):
1. **Vertical Centering of Toggle Switch**: Move switch from $y = 14$ (floating high at the ceiling) down to $y = 35$ ($x = 298$), centering it vertically with the icon and 2-line text block and eliminating the 52px empty dead zone below it.
2. **Vertical Spatial Balancing in Upper Section**: Lower upper content (44px Icon to $y = 26$, Name to $y = 22$, Mode to $y = 54$, Leaf icon to $y = 56$) with divider at $y = 100$, achieving an exact 1:1 vertical symmetry (28px top ink margin vs 27px bottom ink margin to divider).
3. **16px Left & Right Grid Unification**:
   - Align switch right edge to $x = 352$ (16px right margin, matching divider and chevron).
   - Align Settings icon (`cfg_ico`) to $x = 16$ (matching upper device icon and divider left edge), label at $x = 48$.
4. **Action Bar Proportion Refinement**: Move divider to $y = 100$ and set `btn_cfg` to $(0, 101, 368, 63)$, adhering strictly to the 8-point Quiet Architecture rhythm.
5. **State Consistency (Offline / Error / Pending / Latched)**: Shift dash `—` to $(298, 31)$, Retry to $(304, 28)$, and latched switch to $(240, 35)$ in sync with the primary switch position.

---

## 2. Out of Scope

- Do not alter Dashboard or Trends layouts (already calibrated and verified).
- Do not alter device modal dialogs (`ui_dialog.c` / `open_device_dialog()`).
- Do not change packet layout, wire globals, or MCU export interfaces.
- Strictly **0 git commit, 0 git push**.

---

## 3. Detailed Coordinate Specification for `ui.c`

### In `build_devices_page()`:
- `s_dev_page_icons[i]`: `(16, 26)` (was `16, 16`).
- `s_dev_page_names[i]`: `(72, 22)`, width `210` (was `72, 16`, width `150`).
- `s_dev_page_auto_icons[i]`: `(72, 56)` (was `72, 52`).
- `s_dev_page_modes[i]`: `(72, 54)`, width `160` (was `72, 48`).
- `s_dev_page_states[i]`: `(176, 54)` (was `176, 48`).
- `s_dev_page_dashes[i]`: `(298, 31)` (was `294, 14`).
- `s_dev_page_switches[i]`: `(298, 35)` (was `294, 14`).
- `s_dev_page_retries[i]`: `(304, 28)` (was `304, 15`), size `48, 44`.
- Divider line `div`: `(16, 100)`, size `336, 1` (was `16, 96`).
- `btn_cfg`: `(0, 101)`, size `368, 63` (was `0, 97`, size `368, 67`).
- `cfg_ico`: `(16, 20)` (was `20, 22`).
- `cfg_label`: `(48, 20)` (was `56, 22`).
- `cfg_arrow`: `(332, 22)` (was `332, 24`).

### In `update_devices_page_widgets()`:
- Switch normal: `set_pos_if_changed(s_dev_page_switches[i], 298, 35);`
- Switch pending: `set_pos_if_changed(s_dev_page_switches[i], 298, 35);`
- Switch latched: `set_pos_if_changed(s_dev_page_switches[i], 240, 35);`
- Retry error: `set_pos_if_changed(s_dev_page_retries[i], 304, 28);`
- Retry latched: `set_pos_if_changed(s_dev_page_retries[i], 304, 28);`
- Mode label: `mode_x = is_auto ? 94 : 72; set_pos_if_changed(s_dev_page_modes[i], mode_x, 54);`
- State label: `set_pos_if_changed(s_dev_page_states[i], mode_x + mode_sz.x + 14, 54);`

---

## 4. Verification Suite

Run all verification tools:
```powershell
tools/build_sim.bat
tools/run_regression.bat
tools/export_mcu.bat --verify
tools/verify_production.bat
```

Success criteria:
- 33 / 33 regression checks PASS (0 glyph collision, 0 containment error).
- Min free heap > 4,096 B.
- Production smoke test PASS with isolated build.
