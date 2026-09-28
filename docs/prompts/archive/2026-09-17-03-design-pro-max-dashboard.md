ARCHIVED — historical context only. Active coding handoff: [CURRENT](../CURRENT.md).

# UI UX Pro Max Dashboard concept — 2026-09-17

Status: design exploration only, NOT approved for implementation.
Generator: built-in image_gen, not CLI. Reference: existing Home native capture.
Output: [Dashboard concept](../../drafts/2026-09-17-pro-max/dashboard-concept.png).
The bitmap is illustrative, not a pixel-accurate LVGL or interaction test.

## Final generation prompt

Use case: ui-mockup.
Create ONE polished practical redesigned Smart Home HUB DASHBOARD. Landscape 800x480 logical pixels (render at 1600x960), edge-to-edge UI screenshot, no bezel, perspective, external caption, collage or Devices page.
Reference image is CONTENT REFERENCE ONLY. Redesign layout and hierarchy; do not replicate old unequal cards. Preserve its functional scope, measurements, switches and English language.
Design direction: Swiss-grid, calm contemporary home appliance controller. Bright light gray #EFEFEF canvas, black #161616 line icons and dark #20252B text, secondary #45515A, restrained medium-light mineral hues. Flat opaque surfaces, NO outlines, gradients, glass blur, texture or heavy shadows. Modest radius 12 logical px. Crisp semibold Segoe-UI-like sans serif, all text at least 18 logical px. Strong alignment and deliberate negative space; sophisticated not playful or dark.
EXACT LAYOUT in 800x480 logical pixels:
Header x24..776 y16..48: small black home icon and "Studio" at left; stopwatch "00:00:02" and green signal bars with "Connected" at right. Nothing else.
FOUR EQUAL metric cards across one row, y64 h184, x24/216/408/600, width176, 16px gutters. CO2 and VOC flat pale mineral teal #B7D7CF and pale muted blue #BDD2E4; Temperature pale warm sand #E6D3AE; Humidity pale aqua #B7D9D8. Black icons, same 16px padding and label/value/footer baselines.
Each metric title at top, small black relevant pictogram if space permits without crowding. Huge tabular values around54–62 logical px at center. Exact content per card:
"CO₂" / "420" / "ppm" / green dot "Good"
"VOC" / "85" / green dot "Good"
"Temperature" / "23.5" / "°C"
"Humidity" / "48" / "%"
Values baseline aligned across all cards. Units clearly associated with readings, CO2 and VOC Good statuses at matching lower-left footer baseline. No invented comfort verdicts for temperature/humidity. Never add "Index", "Room air", progress meters, charts or extra descriptions.
BELOW: FOUR appliance controls in a 2-column x2-row grid. x24/408 width368; rows y264 and y340, height64, 12px rowgap. White #FAFAFA cards, borderless, modest12px rounding. Black consistent 28px outline appliance icon on left; two lines title18px and secondary "Manual"18px centered vertically beside icon. Toggle at far right, visible 54x30 with room for44px hitheight. Names: row1 "Air Purifier" and "Ventilation Fan"; row2 "Humidifier" and "Desk Light". Purifier ON, Fan OFF, Humidifier OFF, Light ON. Active toggles muted deep teal #22685F with white knob on right; inactive #8996A2 white knob on left. No On/Off text, counts, section title, settings buttons.
BOTTOM NAV y420..468: three balanced wide 48px tall touch zones, icons ONLY, no words. Home selected with a restrained light teal #C0DCD4 rounded capsule behind a BLACK 32px outline home icon. Trends black32px chart icon at center and Devices black32px appliance-grid icon at right, inactive with no background. No enormous saturated blue navigation slabs.
Show readable polished typography, equal card geometry, airy balanced spacing without wasting screen area. No additional UI or advertising text.
This is a static design concept, not a claimed exact LVGL render.
