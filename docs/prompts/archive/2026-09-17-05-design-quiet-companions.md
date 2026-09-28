ARCHIVED — historical context only. Active coding handoff: [CURRENT](../CURRENT.md).

# Quiet Architecture — Trends and Devices image prompts

Dashboard direction accepted by owner; companion pages are visual drafts only.
Generated with built-in image_gen, one call per page, using the
[Dashboard reference](../../drafts/2026-09-17-pro-max/dashboard-quiet-architecture.png).
Not an implementation prompt or proof of native geometry/accessibility.

## Trends

Output: [Trends](../../drafts/2026-09-17-pro-max/trends-quiet-architecture.png).

Use case: ui-mockup. Input image 1 is the APPROVED visual style and header/navigation reference (Overview screen), not a screen to reproduce. Create ONE new companion page, full straight-on landscape 800:480 screen, render approximately1600x960. No device bezel, collage, external captions or perspective.
QUIET ARCHITECTURE design system: warm stone #F1F0EA canvas, porcelain #F8F7F2 cards, deep forest #183D34, ink #202824, pale celadon #D8E8B7 selected accents, subtle thin warm gray rules, delicate surface edges, generous 16/24px rhythms and 18-20px rounded corners. Match reference's refined humanist sans, icons, optical scale and surface treatment exactly. Text remains clear and contrasting, never tiny administrative UI. 800x480 logical touchscreen: labels~18px, headings24, important numbers34-48, controls generous44px hit areas.
Header/nav invariant: upper-left outline house and "Studio"; upper-right stopwatch "00:24:18", separator, four signal bars "Connected". Same positions and dimensions as reference. Below: horizontal text tabs "Overview", "Trends", "Devices", selected tab light celadon rounded capsule. No bottom nav. Page content below y110, outer margins24, no scrolling.
PAGE: TRENDS. Select Trends in top navigation; Overview and Devices unselected.
Two-column content: left narrow vertical metric selector stack width160 and main wide porcelain chart panel width576, separated16px. Content y112..456.
Left column four consistent rounded selector tiles stacked with12px gaps. Each ~77px high, title18px and value26px. CO₂ is selected: forest background, cream text, tiny celadon selection dot. The remaining cards are warm porcelain with dark ink. Exact labels/readings: "CO₂" "420 ppm"; "VOC" "85"; "Temperature" "23.5°C"; "Humidity" "48%". Use modest matching outline icons only if not crowded.
Main chart panel generous24px inner padding. Header "CO₂" left and small non-interactive text "Last 15 min" right. Summary row: prominent "Current" with "420 ppm", then secondary "Min" with "420", and "Max" with "780". Min/max use same ppm unit inherited from clearly labelled axis. Align values.
Below summaries a LARGE clean LINE CHART, accurate axes and values, no decorative random shapes. Thin muted horizontal gridlines with Y labels "800", "600", "400", unit "ppm". X labels "-15m", "-10m", "-5m", "Now". Time left to right; series starts620, rises modestly to maximum780 about one third through, then slopes downward through600 to final420 just above400 line. Never below420 or above780. Use straight-connected small line segments with rounded joins, not large smoothed overshoot. Forest green3px stroke, very subtle green area tint beneath, final endpoint dark dot with light ring. Keep axes readable18px and inside frame. No chart animations, crosshair or tooltip in this default view. Do not add invented status callouts, dates, forecasts, download, multi-series legend, calendars, trend arrows, cumulative health scores or help copy.
The overall page is quiet, crafted and readable, with the graph as focus and one dark selected metric tile anchoring the reference color system.

## Devices

Output: [Devices](../../drafts/2026-09-17-pro-max/devices-quiet-architecture.png).

Use case: ui-mockup. Input image 1 is the APPROVED visual style and header/navigation reference (Overview screen), not a screen to reproduce. Create ONE new companion page, full straight-on landscape 800:480 screen, render approximately1600x960. No device bezel, collage, external captions or perspective.
QUIET ARCHITECTURE design system: warm stone #F1F0EA canvas, porcelain #F8F7F2 cards, deep forest #183D34, ink #202824, pale celadon #D8E8B7 selected accents, subtle thin warm gray rules, delicate surface edges, generous 16/24px rhythms and 18-20px rounded corners. Match reference's refined humanist sans, icons, optical scale and surface treatment exactly. Text remains clear and contrasting, never tiny administrative UI. 800x480 logical touchscreen: labels~18px, headings24, important numbers34-48, controls generous44px hit areas.
Header/nav invariant: upper-left outline house and "Studio"; upper-right stopwatch "00:24:18", separator, four signal bars "Connected". Same positions and dimensions as reference. Below: horizontal text tabs "Overview", "Trends", "Devices", selected tab light celadon rounded capsule. No bottom nav. Page content below y110, outer margins24, no scrolling.
PAGE: DEVICES. Select Devices in top navigation; Overview and Trends unselected.
Below the existing nav, use four generous appliance cards in a TWO BY TWO grid, logical x24/408 width368, y112/292 height164,16px gutters. All four cards same warm porcelain fill, fine warmgray surface edge, corner20. Do NOT color entire cards based on relay state. Do NOT add a second Devices page title, node metadata, counts, chart, metric hero, bottom nav or modal.
Each card has identical internal hierarchy:
TOP: at24px inset left, a refined32px forest outline appliance pictogram (cylindrical purifier, fan, droplet, desk lamp). Beside it full device name in strong24px humanist sans. Far right a clear54x30 toggle, vertically aligned and with ample hitarea, with no overlap. Generous spacing, names must fit; if necessary name22px but never tiny.
MIDDLE: under the name, readable18px mode description. First two cards show a tiny leaf and "Auto", with a subtly separated associated sensor text: respectively "VOC" and "CO₂". Bottom two show "Manual". Dark muted green text, small pale celadon Auto badge allowed, not a second toggle. No duplicate On/Off text and no sensor numbers or thresholds.
BOTTOM: subtle full-width divider and a clear wide settings action row, left small sliders icon then "Settings", right chevron. At least44 logical px high. This is one action to open that appliance's settings for mode, thresholds and preset, visually separated from the output switch. Do not show threshold values in these overview cards.
EXACT devices/states in row-major order:
1 "Air purifier" / Auto / VOC / ON: deep forest switch track, ivory knob on RIGHT.
2 "Ventilation" / Auto / CO₂ / OFF: warm neutral gray switch track, ivory knob on LEFT.
3 "Humidifier" / Manual / OFF: warm neutral gray track, ivory knob on LEFT.
4 "Desk light" / Manual / ON: deep forest track, ivory knob on RIGHT.
Switch direction, mode and Settings action must be unambiguous. Treat Auto as mode and switch as actual output, not as automation enable/disable. No "Active"/"Paused"/"Running" invented copy. No numerical energy, schedules, scenes or new appliances.
This is the companion screen to the exact reference design. Preserve its calm premium architectural personality and natural warm background, high contrast blackgreen text, delicate outlines, uniform icon strokes, deliberate consistent spacing. Avoid dashboard admin look, rainbow tiles and tiny text.
