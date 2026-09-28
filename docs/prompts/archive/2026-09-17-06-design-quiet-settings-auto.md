ARCHIVED — historical context only. Active coding handoff: [CURRENT](../CURRENT.md).

# Quiet Architecture Settings/Auto generation prompts

2026-09-17. Visual drafts for owner feedback, not implementation instructions.
Built-in image_gen, four calls. First two use the Devices concept as style
reference; last two edit the CO2 image. Source rules checked against ui_auto.h/c.
The complete proposed interaction specification is in [UI_DESIGN_BRIEF](../../UI_DESIGN_BRIEF.md).

## Settings

Output: [Settings](../../drafts/2026-09-17-pro-max/settings-quiet-architecture.png).

Use case: ui-mockup. Generate ONE high-fidelity full-screen companion settings page for the referenced QUIET ARCHITECTURE smart-home controller. Input image1 is the approved style/header reference only. Landscape1600x960, logical800x480, edge-to-edge screenshot, no bezel or collage.
Preserve the beautiful warm stone background #F1F0EA, warm porcelain #F8F7F2 panels, forest #183D34 primary actions, celadon accents, ink #202824, refined humanist sans, consistent outline icons, fine subtle edges,20px radii,16/24px spacing. Strong legible text, labels18-22logical px, headings26, key values40. No tiny labels, excessive shadows, blue accent or cartoon decoration.
Global header exactly like reference: house icon+"Studio" left, stopwatch "00:24:18" and bars+"Connected" right. This is a focused EDITING subpage: replace the Overview/Trends/Devices tab row with a back chevron and editing title. No bottom navigation or background modal.
All content and footer visible on800x480 without scrolling. Buttons at least48px logical height. No physical output toggle on an editing page. All values are drafts until "Save settings".
PAGE: DEVICE SETTINGS for Ventilation.
Editing title row at y68: back chevron in generous48px target then "Ventilation · Settings".
Main porcelain rounded panel x24 y120 width752 height264, four equal66px rows with delicate dividers. Left labels and right controls perfectly aligned:
1 "Device type", right a wide rounded dropdown showing "Ventilation fan" and downward chevron.
2 "Control mode", right two-option segmented selection "Manual" / "Auto", Auto selected forest with cream text, Manual neutral.
3 "Auto thresholds", right summary "1000 / 800 ppm" and forward chevron. This row opens the threshold editor.
4 "Output polarity", right segmented "Active high" / "Active low", Active high selected in pale celadon with dark text. This is installer-oriented wiring polarity, not an output switch. Add no misleading NC/NO explanation.
Footer outside card y408 height48: left neutral "Cancel" button; right wide forest "Save settings" button with cream text. Ample 24px outer padding. Only one Save button. No Reset here, no duplicate back text, no status banner, no keyboard.
Make the design composed and premium, a clear quiet form with large controls.

## CO2 Auto

Output: [CO2 Auto](../../drafts/2026-09-17-pro-max/auto-co2-quiet-architecture.png).

Use case: ui-mockup. Generate ONE high-fidelity full-screen companion settings page for the referenced QUIET ARCHITECTURE smart-home controller. Input image1 is the approved style/header reference only. Landscape1600x960, logical800x480, edge-to-edge screenshot, no bezel or collage.
Preserve the beautiful warm stone background #F1F0EA, warm porcelain #F8F7F2 panels, forest #183D34 primary actions, celadon accents, ink #202824, refined humanist sans, consistent outline icons, fine subtle edges,20px radii,16/24px spacing. Strong legible text, labels18-22logical px, headings26, key values40. No tiny labels, excessive shadows, blue accent or cartoon decoration.
Global header exactly like reference: house icon+"Studio" left, stopwatch "00:24:18" and bars+"Connected" right. This is a focused EDITING subpage: replace the Overview/Trends/Devices tab row with a back chevron and editing title. No bottom navigation or background modal.
All content and footer visible on800x480 without scrolling. Buttons at least48px logical height. No physical output toggle on an editing page. All values are drafts until "Save settings".PAGE: AUTO THRESHOLDS editor. Back chevron plus title at y68; this back action returns to parent Settings keeping draft. At y120 a56px-high pale celadon strip shows appliance/sensor at left and current reading at right.
Two equal porcelain cards x24/408 y192 w368 h144 with20px padding. LEFT "Turn ON", RIGHT "Turn OFF". Under each heading a plainly worded inclusive comparison label. Under comparison, a horizontal stepper with square48px MINUS button, large centered threshold with unit, square48px PLUS button. NO sliders and no numeric keyboard. Both cards align perfectly.
Below at y352..392 a reserved two-line helper/validation region with18px text, no shifting footer.
Footer y408h48: neutral "Cancel" left; quiet secondary "Reset limits" center; forest primary "Save settings" right. Buttons not crowded. Save commits all device settings, not just thresholds. Do not invent extra buttons or control outputs. All text readable.
VALID CO₂ EXAMPLE. Title "Ventilation · Auto thresholds". Strip: airflow icon "CO₂" and right "Current 420 ppm".
Left Turn ON comparator "CO₂ at or above"; stepper threshold "1000 ppm".
Right Turn OFF comparator "CO₂ at or below"; stepper threshold "800 ppm".
Helper exact two lines: "ON must be at least 50 ppm above OFF." and "Between limits, the device keeps its state."
Save settings enabled forest. Plus/minus darkforest outlines with clear icons. Card numbers~36px. Do not use strict > or <; labels explicitly include equality.

## Humidity Auto

Output: [Humidity Auto](../../drafts/2026-09-17-pro-max/auto-humidity-quiet-architecture.png).

Use case: precise-object-edit. Input image is the Auto thresholds CO2 screen edit target. Preserve its entire exact composition, typography, header, warm stone/forest/porcelain palette, all controls, spacing, numbers scale and footer. Produce a matching full screenshot same landscape dimensions. Change ONLY content for a humidifier rule:
title "Humidifier · Auto thresholds".
Sensor strip: replace fan icon with consistent outline droplet, label "Humidity", current reading "48 %RH".
Left card heading "Turn ON", comparator "Humidity at or below", value "40 %RH".
Right card heading "Turn OFF", comparator "Humidity at or above", value "50 %RH".
The comparison directions MUST reverse relative to CO2: ON at/below40, OFF at/above50.
Helper lines "OFF must be at least 5 %RH above ON." and "Between limits, the device keeps its state."
Keep minus and plus buttons, Cancel, Reset limits, and enabled forest Save settings.
No new UI, no additional panels, no threshold arrows, no warning. Strong text contrast.

## Invalid VOC

Output: [Invalid VOC](../../drafts/2026-09-17-pro-max/auto-voc-invalid-quiet-architecture.png).

Use case: precise-object-edit. Input image is the exact CO2 Auto thresholds screen to edit. Preserve exact dimensions, composition, geometry, style, typography, background, header and button locations. Create one matching full-screen screenshot of INVALID VOC THRESHOLD DRAFT.
Change title to "Air purifier · Auto thresholds".
Sensor strip: cylindrical purifier outline icon, label "VOC", current "85" WITHOUT any unit.
LEFT "Turn ON", comparator "VOC at or above", value "150" unitless.
RIGHT "Turn OFF", comparator "VOC at or below", value "150" unitless.
Both threshold values150 form invalid gap0. Mark both value boxes with thin muted brick-red outlines, retaining neutral background and black numbers. Minus/plus remain enabled to allow fixing.
Replace helper area with brick-red warning triangle and text on TWO readable lines:
"ON must be at least 5 above OFF."
"Raise ON or lower OFF to continue."
Do NOT move any controls when showing error.
Footer Cancel and Reset limits remain enabled. "Save settings" DISABLED: flat desaturated gray-beige background with clear darkgray label and no green fill. No new confirmation modal, toast or popup.
Keep all other pixels/layout as close as possible to reference. No ppb, ppm, Index, new titles or extra text.

