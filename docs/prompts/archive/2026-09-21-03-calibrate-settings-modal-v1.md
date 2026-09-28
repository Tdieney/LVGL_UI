# CURRENT — calibrate Device Settings modal layout & Auto thresholds summary

Date: 2026-09-21. Status: **IMPLEMENTED — awaiting review**.  
Review report and evidence: [docs/reviews/2026-09-21-settings-modal-review/REVIEW.md](../reviews/2026-09-21-settings-modal-review/REVIEW.md).  
Evaluation conducted by `ui-ux-pro-max` against canonical Quiet Architecture concept mockup `docs/drafts/2026-09-17-pro-max/settings-quiet-architecture.png`.

---

## 1. Objectives & Summary

Calibrate visual balance, spacing rhythm, and typographic consistency across the **Device Settings** modal dialog in [`ui.c`](../../ui.c):
1. **Row 2 Auto Thresholds Summary (Single-Line Rhythm)**:
   - Replace 2-line stacked format (`"ON: %d %s\nOFF: %d %s"`) with canonical single-line Quiet Architecture format (`"%d / %d %s"`, e.g. `"1000 / 800 ppm"`).
   - Reposition `s_dialog_limits_summary_lbl` to $(440, 20)$ with size $(256, 24)$, placing it on the exact horizontal axis of the row title "Auto thresholds" ($y = 20$) and right chevron ($y = 22$), eliminating vertical crowding and restoring visual breathing room.
2. **Back Button & Contextual Title Spacing**:
   - Align contextual title labels (`s_dialog_device_title_lbl` and `lbl_auto_title`) to $x = 48$ (was $x = 56$), reducing excessive gap from back chevron (`<`) from 30px to a comfortable 14px.
   - Use concise display name (`(s_dialog_edit_preset == UI_PRESET_FAN) ? "Ventilation" : s_presets[s_dialog_edit_preset].name`) matching mockup title `"Ventilation · Settings"`.
3. **Output Polarity Unselected Text Contrast**:
   - Preserve `UI_COLOR_INK` text on both `Active high` and `Active low` segmented buttons regardless of selection, eliminating false "disabled/muted" perception.
4. **Auto Thresholds Sensor Strip Separator**:
   - Set vertical divider `strip_sep` color to `0xBCC9A6` for clear visual demarcation between sensor identity and current measurement.

---

## 2. Out of Scope

- Do not alter wire packet schemas, globals, or radio communication.
- Do not alter Dashboard or Trends layouts (already calibrated and verified).
- Strictly **0 git commit, 0 git push**.

---

## 3. Detailed Coordinate & Code Specifications for `ui.c`

### In `update_dialog_device_subview()`:
- Title update:
  ```c
  const char *disp_name = (s_dialog_edit_preset == UI_PRESET_FAN) ? "Ventilation" : s_presets[s_dialog_edit_preset].name;
  snprintf(s_dialog_title_buf, sizeof(s_dialog_title_buf), "%s · Settings", disp_name);
  ```
- Auto thresholds summary format:
  ```c
  snprintf(s_dialog_limits_summary, sizeof(s_dialog_limits_summary),
           "%d / %d %s", (int)s_dialog_edit_on_thresh,
           (int)s_dialog_edit_off_thresh, unit);
  ```
- Polarity labels text color:
  ```c
  set_text_color_if_changed(s_dialog_active_low_labels[0], UI_COLOR_INK);
  set_text_color_if_changed(s_dialog_active_low_labels[1], UI_COLOR_INK);
  ```

### In `build_dialog_device_subview()`:
- Title position: `lv_obj_set_pos(s_dialog_device_title_lbl, 48, 6);`
- Limits summary label position:
  ```c
  lv_obj_set_pos(s_dialog_limits_summary_lbl, 440, 20);
  lv_obj_set_size(s_dialog_limits_summary_lbl, 256, 24);
  ```

### In `build_dialog_auto_subview()`:
- Title position: `lv_obj_set_pos(lbl_auto_title, 48, 6);`
- Title string:
  ```c
  const char *disp_name = (s_dialog_edit_preset == UI_PRESET_FAN) ? "Ventilation" : s_presets[s_dialog_edit_preset].name;
  snprintf(s_auto_title_buf, sizeof(s_auto_title_buf), "%s · Auto thresholds", disp_name);
  ```
- Strip separator color: `lv_obj_set_style_bg_color(strip_sep, lv_color_hex(0xBCC9A6), 0);`

---

## 4. Verification Suite

1. `tools/build_sim.bat` (PASS).
2. `tools/run_regression.bat` (33/33 PASS, min free heap > 4,096 B, 0 collision).
3. `tools/export_mcu.bat --verify` (PASS).
4. `tools/verify_production.bat` (ALL 4 PHASES PASS).
