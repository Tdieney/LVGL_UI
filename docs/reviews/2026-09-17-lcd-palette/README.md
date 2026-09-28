# LCD palette adjustment — 2026-09-17

Owner-requested implementation, not a new general review/fix round.
Keep the existing UI; strengthen only the previously washed-out surfaces.

| Surface | Previous | New RGB888 input |
|---|---|---|
| Home CO2 | #D6E7FF | #ADCEFF |
| Home VOC | #E7DBFF | #CEB5F7 |
| Home temperature | #FFEBCE | #FFD394 |
| Home humidity | #CEEFE7 | #94D7C6 |
| Trends inactive selectors | #DEEBF7 | #BDD3EF |
| Devices icon plates | #DEEBFF | #B5CEF7 |

Selected Trends stays #215DDE; canvas, foregrounds, status badges, switch colors,
geometry, shadows and all behavior stay unchanged. Flat fills, no borders added.
LVGL quantizes inputs to RGB565. No new heap allocations or rendering effects.

Verification: tools/run_regression.bat (approved outside-sandbox build) exit0,
31/31 checks and24 scenario captures; tools/export_mcu.bat --verify exit0.
Geometry tests now exclude the correct palette tokens instead of obsolete
hardcoded colors; position assertions are unchanged.

Native simulator screenshots visually inspected:
[Home](home_good.png), [Trends](trends_data.png), [Devices](devices_grid.png).
Generated with sim_pc.exe --shots and tools/raw2png.py; not AI mockups.
LCD saturation depends on panel/backlight/viewing angle and needs owner testing.
The earlier S1–S3 handoff is untouched; this change does not claim to close it.
