# Smart Hub interactive design study 12

Open [index.html](index.html) directly in Edge, Chrome or Firefox. No install,
build, server, external fonts, API key or internet connection is needed.

This is a browser design prototype, not MCU firmware. The actual LVGL simulator
still shows the neutral baseline. Study 12 retains study 10's layout and study
11's navigation: separate CO₂/VOC cards replace Room Air, the visible label is
now VOC, and tonal surfaces reduce the predominantly white treatment. Study 12
replaces every two-stop vertical gradient with a single flat, RGB565-exact fill
and returns the canvas, device cards and chart/dialog to plain neutral greys. Three tabs, uptime, readable text, soft shadows and
borderless frames remain. No notification panel. Bottom navigation uses 32 px
icons only, without visible captions; the 48 px-high touch targets, semantic
names and selected state remain.
The latest design proposal awaits owner review.
This browser reference is not evidence of an accepted LVGL implementation.

## Try it

1. Use the house / chart / device icons (Home / Trends / Devices), left to right.
2. Tap a Home reading to open its trend. Select metrics or focus/tap chart points.
3. Toggle an output to see Sending followed by a simulated node acknowledgement.
4. Open a device's settings to choose a load preset and Manual/Auto.
5. Use the controls below the device to preview good air, poor air, lost LoRa,
   partial sensor failure and a command timeout. For the timeout scenario, tap an
   output switch to trigger the error, then use Retry.
6. Read air/device problems inline, beside the affected reading or control. There
   is no bell, notification panel or clickable advisory opening a popup.
7. Reset session to preview empty history; choose a scenario to load sample data.
8. Replay splash to view the original Hyphen Deux asset with the approved timing.

The stage is 800×480 at desktop size. Smaller browser windows scale the whole stage
to fit; this does not redefine the embedded layout or validate physical touch targets.

## Screenshots

| Screen / state | Preview |
|---|---|
| Home | [home.png](home.png) |
| Trends | [trends.png](trends.png) |
| Devices | [devices.png](devices.png) |
| Device settings | [device-settings.png](device-settings.png) |
| Node offline | [offline.png](offline.png) |
| Degraded link (2 bars) | [home-degraded.png](home-degraded.png) |
| Splash | [splash.png](splash.png) |

## Fixture boundaries

- Every sensor reading, chart timestamp, category and output report is sample data.
  The prototype does not poll hardware or update samples on a timer.
- The top bar shows actual time since page navigation in `HH:MM:SS`, using a
  monotonic browser clock. It updates once per second, does not wrap at 24 hours,
  and does not reset on tab/scenario changes, history reset or splash replay.
  Reloading the page starts the counter again.
- Each loaded scenario has 16 one-minute sample readings from 14:17 to 14:32.
  These chart times are fixture data, independent of actual prototype uptime.
  Statistics use valid samples in the loaded session. Offline and
  air-sensor-failure fixtures have gaps in their final three affected samples.
- VOC category and number are independent fixture fields. There is no TVOC-to-index
  conversion, health threshold, Sensirion claim or automatic control algorithm.
  The visible label is VOC, but its numeric meaning is unchanged (unitless index,
  not raw ppb). No Room Air aggregate is calculated or displayed; per-metric
  categories/validity and short notes remain independently visible.
- Sample output acknowledgements take 700 ms; this is not a LoRa timeout specification.
- A quick toggle selects Manual and stays there until Auto is selected explicitly.
  This is a proposed interaction. Auto changes a badge only; rules are still open.
- Output states represent node reports, not proof a connected appliance is running.
  Normal On/Off text is omitted; switch position and color indicate the state,
  and accessible switch names/checked state remain available.
  Unavailable outputs show Unknown and no active switch. Timeout exposes Retry.
- Scenario changes clear outstanding commands/errors and replace sample history;
  load choices and mode selections survive scenario changes within the open page.
- All state is in page memory. Reload resets defaults. Nothing is saved to browser
  storage, Flash or an external service.

## Files and asset provenance

- `index.html`: device shell and review controls outside the device boundary.
- `styles.css`: shared visual tokens, fixed-screen layout and focus states.
- `app.js`: three screens, uptime, fixture state, chart and review interactions.
- `splash-logo.png`: copied unchanged from
  `ui/motor-control:tools/assets/ui_logo.png`, the specifically requested splash
  exception. No motor screens, icons, telemetry or behavior were imported.
- All application icons and charts are local SVG markup. No generated bitmap is
  used as the screen background; the prototype is editable HTML/CSS.

## Study 12 layout and color roles

- Header 56 px, content 360 px, navigation 64 px. Content padding 12/20 px gives
  a 760×336 grid: columns 250/250/236 with 12 px gaps, rows 208/116 with 12 px gap.
  CO₂ and VOC have equal emphasis; temperature/humidity stack in 98 px cards.
- Navigation: 32×32 px icons centered in equal-width 48 px-high buttons, no visible
  captions. Selected white icon on the blue action fill, inactive #42546B. Accessible
  names Home/Trends/Devices and keyboard focus remain; other layout is unchanged.
- Sensor labels 24 px, readings 62 px; comfort labels 18 px, readings 34 px;
  notes/status 16 px. Four 181×116 device tiles share consistent names/control rows.
- Trends/Devices: title 30 px, title row 36 px with 12 px after it; Trends selectors
  48 px plus 12 px gap, chart 228 px. Devices cards 136 px tall with 12 px gaps.
- Flat fills, no gradients anywhere. Neutral surfaces are plain grey-to-white:
  canvas #EFEFEF, device cards #F7F7F7 regardless of output state, chart/dialog
  #FFFFFF. Header/navigation are transparent over the canvas; the lightness order
  canvas < device < chart is deliberate.
- Metric hues carry the only surface color: CO₂ #D6E7FF; VOC #E7DBFF;
  temperature #FFEBCE; humidity #CEEFE7. Selected nav/metric and primary button
  #215DDE with white text; inactive metric selector #DEEBF7.
- Every fill is an exact RGB565 value (red/blue `(v<<3)|(v>>2)`, green
  `(v<<2)|(v>>4)`), so the declared color is what a 16-bit panel shows. The old
  near-white stops such as #F8FAFF rounded red up to 255 and green down, tinting
  large surfaces magenta on the LCD.
- Metric colors identify metrics, not air quality. Badge words/colors remain the
  quality indicators. CO₂ foreground #153E79, VOC #483277; temperature/humidity
  icons #985116/#17665D. All colors are reference CSS, not physical-LCD validation.

- Text: primary #182B4D; secondary/units/axes #42546B; blue text #1552B0.
  Supporting captions use weight 600; no low-opacity labels.
- Card elevation: `0 2px 10px 0 rgba(24,43,77,0.06)` on sensor/comfort cards, Home
  device tiles, the chart card and larger Devices cards. One shadow, no spread;
  no extra shadow on header/navigation, selectors, icons or mode buttons. Existing
  modal/knob shadows remain unchanged. Card radius 20 px, small tiles 18 px.
- Switch/chart #1865E7; off switch #BDC7D6, white knob. Selected mode fill #EFF3FF. Blue text has its own
  token; icon/plot stroke weights are unchanged.
- Device/home icons #2470F0; Devices icon plates #DEEBFF.
  Settings/inactive navigation/timer icons use #42546B. Connected text,
  Comfortable and the moderate advisory use dark secondary text.
- Good #197047 on #E7F7EF; Moderate #8A5000 on #FFF3DE; Poor #A12B25 on #FFEFEF.
  Quality badges retain words plus color, the connection icon retains semantic
  color, and actual faults stay explicit. Unknown uses neutral #EFF3F7.
- Card, selector, settings-control and dialog borders are zero. Subtle existing
  internal separators use #DEE7F7; keyboard focus can show a temporary outline.
- Splash alone keeps #E9ECF1 to match its unchanged original logo asset; it is a
  flat fill already and is deliberately left at its original value.
- Study 12 supersedes study 10/11's gradient surfaces; study 09's shadow scope,
  borderless frames and removed features are unchanged. Readable text still applies.
  Desktop color contrast is not proof of physical-LCD readability.

## Verification

### 4-Bar Signal Indicator Revision (2026-09-09)

Replaced the legacy `radio` SVG glyph in the link status indicator with a 4-bar signal-strength indicator drawn as plain rectangles (widths 3 px, gap 2 px, heights 4, 7, 10, 14 px; total 18×14 px footprint) matching LVGL primitive rendering. Active bars use Study 12 `#197047` (`var(--green)`), inactive bars use `#BDC7D6` (`var(--switch-off)`), and the offline state renders all bars inactive beside the existing red `Disconnected` text.

Re-rendered affected screenshots at native 800×480 using headless Edge:
- `home.png` (updated: 4 bars active)
- `trends.png` (updated: 4 bars active)
- `devices.png` (updated: 4 bars active)
- `device-settings.png` (updated: 4 bars active)
- `offline.png` (updated: all 4 bars inactive, red Disconnected text)
- `home-degraded.png` (new: 2 bars active, 2 bars inactive)
`splash.png` remains unchanged (splash overlay covers the screen with no header).

Added a 'Signal' dropdown selector to the review controls allowing interactive review of all states (4 bars, 3 bars, 2 bars, 1 bar, Disconnected).

### Study 12 flat-surface revision (2026-09-07)

Re-rendered `home.png`, `trends.png`, `devices.png`, `device-settings.png` and
`offline.png` at native 800×480 from the updated prototype with headless Edge and
inspected them. Confirmed no `linear-gradient` remains in `styles.css`, that each
flat fill is an exact RGB565 value, and that the canvas < device < chart lightness
order holds. `splash.png` is unchanged because the splash background and logo asset
were not touched. The interaction sweep below was run against study 11 and was
**not** re-run for study 12; this revision changed only fill colors, but that is a
reasoning argument, not a fresh test result.

### Study 11 interaction sweep (2026-09-07)

Checked in Chromium with local Playwright: all three screens, removal of Details,
icon-only navigation with 32 px icons, accessible names, 48 px hit heights and active routing,
bell/panel/advisory click targets, borderless cards/settings, exact shared card-shadow
style and shadow-free bars/selectors/icon plates, flat surfaces, removal of
Room Air and visible Index, equal CO₂/VOC cards, no Home card overlap or content
overflow in normal/offline/sensor-failure/four-digit poor-air states,
and the specified redundant copy, ticking uptime and hour/24-hour boundaries,
metric switching and
point tooltips, Manual transition, pending/acknowledged/failed output commands,
preset changes, missing node/sensor readings, inline warnings, empty
history, splash timing/asset load and 390 px viewport fit. No browser script errors.
Inspected native 800×480 screenshots with enlarged typography. Main labels are
18–24 px, readings 32–62 px, supporting states 16 px and switches 54×30 px inside
58×48 px hit targets. Older Details screenshot is preserved only as a historical
reference at [study03-details.png](../drafts/study03-details.png).
The old notification panel is archived at
[study04-alerts.png](../drafts/study04-alerts.png).

The existing simulator build, smoke render, RGB565 shot-size regression and MCU
export verification passed during study 03. Study 11 changes only the browser
prototype/docs, so those unchanged C checks were not rerun. Browser validation
does not establish readiness for LVGL or hardware.

## Implementation handoff

Give the coding AI [the implementation prompt](../prompts/CURRENT.md)
with repository access. Its result will be checked against the
[technical-lead review contract](../TECHNICAL_LEAD_REVIEW.md).
