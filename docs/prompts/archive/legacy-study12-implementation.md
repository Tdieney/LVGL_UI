# Implementation handoff · Smart Hub UI study 12

> ARCHIVED — historical context only, not the current implementation request.
> Original location: `docs/LVGL_IMPLEMENTATION_PROMPT.md`.
> Use [CURRENT.md](../CURRENT.md) for the active handoff and
> [prompt rules](../README.md) before creating or updating any prompt.

Copy the prompt below to the coding AI with access to this repository. This is a
handoff for implementation, not a claim that the browser prototype is MCU-ready.
The technical lead will review its diff, build evidence and simulator screenshots.

---

You are the implementation engineer for the LoRa Smart Hub UI. Implement the
current design in C with LVGL **v8.4**, inside this existing repository. Deliver
working code and evidence for a separate technical-lead review.

## Read first and preserve the workspace

Repository: `C:\Users\vanth\Documents\MyDocument\Embedded\LVGL_UI`

Read `AGENTS.md`, `docs/README.md`, `docs/HARDWARE_OVERVIEW.md`,
`docs/MCU_BASELINE.md`, `docs/UI_DESIGN_BRIEF.md`, `ui_mcu_profile.h`, the current
`ui.*`, `sim_pc/` and `tools/README.md`. Check Git status before editing. Preserve
existing uncommitted design work and unrelated changes. Work on the current
`ui/smart-hub` branch unless the owner specifies another branch. Do not reset,
clean the repository, publish, push or change branches as an incidental step.

The latest design reference is **study 12** (study 10 layout, icon-only navigation,
flat RGB565-exact surfaces), available in:

- `docs/prototype/index.html`, `styles.css`, `app.js` and `README.md`.
- Current screenshots: `home.png`, `trends.png`, `devices.png`,
  `device-settings.png`, `offline.png` and `splash.png` in that directory.

Open/render the prototype and inspect the images. Historical study 03–10 prose
and `docs/drafts/` are NOT the current UI requirements. Latest explicit owner
decisions in this prompt supersede old proposals. Implementation is now requested;
do not stop at a new design proposal or another browser mockup.

## Required UI

Implement an **800×480** English light-theme UI. Preserve the current composition,
typography hierarchy, palette, icons and generous hit areas. Use real LVGL widgets,
not a screenshot of the whole interface. No additional screens, menus or branding.

1. Shared top bar: Home icon + `Studio`; elapsed UI runtime `HH:MM:SS` with timer
   icon; `Connected` or `Disconnected` with wireless icon. No wall clock, `LoRa`
   wording, bell, notification badge or notification panel. Runtime increments
   once per second, survives tab/dialog changes and history resets, and does not
   wrap at 24 hours. Handle the underlying millisecond tick rollover.
2. Bottom navigation: exactly three **icon-only** targets, ordered Home (house),
   Trends (chart), Devices (device). Icons are 32×32 px, centered in the existing
   equal-width 48 px-high buttons within the 64 px bottom bar. No visible Home /
   Trends / Devices captions, tooltips or replacement text. Keep semantic names
   for accessibility/test identification and keyboard focus. Selected icon is
   white on the existing blue action pill; inactive icons #42546B. Do not reduce
   touch targets to icon bounds. No Details, hidden replacement debug page or
   shortcut from connection status. Titles within Trends/Devices pages remain.
3. Home: two separate, equally prominent CO₂ and VOC cards. Remove the entire Room
   Air overview, aggregate category and headline. Each metric has its own value,
   category and short note; missing data affects that metric, not its valid peer.
   The visible label is `VOC` everywhere, including Trends and accessible names;
   the number remains the existing unitless index input, NOT raw ppb or a new
   conversion. CO₂ retains ppm. Temperature/°C and humidity/% stack at the right.
   Tap any metric card to open its trend. Four device tiles occupy the bottom row,
   with names/icons/mode/switches. No `Your devices`, output/node summary or normal
   `On`/`Off` captions. Keep fault notes local to each metric/device; no new panel.
4. Trends: one large chart, four metric selectors, `Current`, `Min`, `Max`.
   Maintain the 15-minute rolling view and session-wide extrema of valid readings,
   but do not display `This session`, `Last 15 min`, suffixes beside Min/Max,
   introductory copy or persistent tooltip/history instructions. Support point
   inspection. Missing readings create gaps, never zero-valued fabricated points.
   No threshold bands or invented quality thresholds. Handle empty and constant
   data, negative temperatures and partial sensor failure. Use bounded in-RAM
   history; document capacity, sample cadence and overflow/decimation behavior.
5. Devices: four larger controls in a two-by-two grid, with preset/mode settings.
   Keep title `Devices`, names/icons, Manual/Auto and switches. No node status
   summary, `Four outputs. One connected room.`, `Node-reported output`, or normal
   `On`/`Off` captions. Settings dialog supports the eight existing presets,
   Manual/Auto, Save and Cancel/close without saving. Keep default demo load names
   Air Purifier, Ventilation Fan, Humidifier and Desk Light.
6. Preserve useful state feedback inline: Sending, Unknown and Retry. Missing
   connection disables output changes and hides stale live numbers. Sensor-only
   air failure must not hide still-valid temperature/humidity. Notifications must
   not reappear as a global panel or an acknowledgement workflow. Short transient
   bottom feedback may follow the prototype; it must not capture touch or obscure
   navigation indefinitely.
7. Splash only: use `docs/prototype/splash-logo.png`, the exact original Hyphen
   Deux logo. Background #E9ECF1; 250 ms blank, 350 ms logo fade-in/upward movement
   of about 14 px, fade-out starts at 2.1 s for 400 ms, complete at 2.5 s. Release
   splash objects/timers after completion. No brand elsewhere. No motor-control
   screens, icons, telemetry, rules or fonts imported unless independently needed
   and justified; the original splash asset is the explicit exception.

## Study 12 layout, flat surfaces and borderless cards

Study 10 replaces the predominantly white treatment with tonal color, improves
proportions and removes Room Air/visible `Index`. Study 11 changed bottom
navigation to larger icons without captions. **Study 12 changes every surface
from a two-stop vertical gradient to a single flat fill and nothing else** —
layout, proportions, typography, icons, shadows, radii, hit areas, semantic
badges and interactions are exactly as before. Use the current rendered
prototype, not earlier screenshots.

At 800×480: header 56 px, content 360 px, navigation 64 px. Content padding 12 px
vertical/20 px horizontal gives 760×336 usable space. Home columns 250/250/236 px,
12 px gaps; first row 208 px, device row 116 px. Comfort cards are 98 px each with
a 12 px gap. Prevent intrinsic text sizing from expanding them into the device row.
Device row: four 181 px tiles with 12 px gaps. Trends/Devices titles are 30 px in
a 36 px title row, followed by 12 px spacing. Trends selectors 48 px, gap 12 px,
chart 228 px; Devices two rows of 136 px cards with 12 px gap. Card radius 20 px,
small device radius 18 px, selected navigation radius 14 px. No permanent borders.

Every surface is one **flat** fill. There are no gradients anywhere in this UI:
no `bg_grad_color`, no `LV_GRAD_DIR_VER`, no `LV_GRAD_DIR_HOR`, on any object,
part or state. Set `bg_grad_dir` to `LV_GRAD_DIR_NONE` and give each surface a
single `bg_color`:

| Surface | Flat fill |
|---|---|
| Canvas (transparent header/navigation over it) | #EFEFEF |
| CO₂ | #D6E7FF |
| VOC | #E7DBFF |
| Temperature | #FFEBCE |
| Humidity | #CEEFE7 |
| Device cards, on or off | #F7F7F7 |
| Chart / settings dialog | #FFFFFF |
| Selected navigation / metric selector / primary action | #215DDE |

The three neutral surfaces are **pure neutral greys** (#EFEFEF < #F7F7F7 <
#FFFFFF), not blue-tinted whites. Do not reintroduce a blue cast on the canvas,
device cards, chart card or dialog. Color belongs to the four metric hues, the
blue action fill, the icon plates and the semantic badges only.

Every value above is an **exact RGB565 color**: red and blue are of the form
`(v<<3)|(v>>2)` and green of the form `(v<<2)|(v>>4)`, so `lv_color_hex()` on a
16-bit display renders precisely the declared color. Do not "improve" or round
these hex values, and do not substitute the old study 10/11 gradient stops: those
had red channels (for example #F8FAFF, #F0F5FF, #E0E9FA) that RGB565 rounds up to
255 while rounding green down, which tinted large near-white surfaces magenta.
If you introduce any new fill, pick it from the same exact-value sets.

Keep the lightness order canvas < device card < chart card so cards stay legible
against the ground; the neutral scale above already encodes it. Metric hues identify the metric, NOT its quality. Keep them
stable for Good/Poor/Unknown; separate labeled semantic badges convey the actual
category. No fake gauges or decorative chart data. Use native LVGL fills, not
full-screen bitmaps. Report measured draw cost; banding is no longer expected
because there is nothing to interpolate.

Two further reasons this UI must stay flat, both verified in this repository, so
do not reintroduce gradients as a "visual improvement":

- LVGL v8.4 keys its gradient cache on `descriptor address ^ size ^ (w >> 1)`
  (`lv_draw_sw_gradient.c`, `compute_key`) and the descriptor is a stack
  temporary in `lv_draw_rect_dsc_t`. Two same-sized surfaces with different
  colors drawn in one partial-buffer band therefore collide and share one color
  map: the 250×208 CO₂ and VOC cards rendered byte-identical blue.
- With the cache disabled instead, LVGL allocates a gradient map from the LVGL
  heap on every gradient draw (~1.6 KB for an 800 px-wide surface). On the 42 KiB
  budget that allocation failed and `LV_ASSERT_MALLOC` locked the UI in
  `while(1)`.

Retain exactly one soft shadow on separate Home CO₂/VOC/comfort cards, all Home device
tiles, the Trends chart card and all four Devices cards. Browser reference:
`box-shadow: 0 2px 10px 0 rgba(24,43,77,0.06)` (no spread, no stacked shadows).
Keep the specified flat fills and zero borders in every state, including disabled cards.
Do not introduce a grey halo. Header/navigation stay flat;
do not add this shadow to metric selectors, icon plates, badges or mode buttons.
The existing settings-modal shadow and switch-knob treatment remain separate.

For LVGL, start with shadow color #182B4D, opacity about 15/255, width about 10,
offset X=0/Y=2 and spread=0 on a shared card style. These are starting values,
not a promise of pixel equivalence: CSS blur radius and LVGL shadow width can
render differently. Compare native 800×480 RGB565 screenshots and tune only the
shadow to the reference's faint, soft appearance. Check clipping at container
edges and gaps. Keep shadow styles static; do not animate them or invalidate the
whole screen each tick. Measure their redraw/heap cost with the existing partial
buffer. Do not silently remove shadows, add borders or increase memory budgets
to work around a performance issue; report measured constraints for lead review.

Use the current prototype CSS as the token source. Key colors:

| Role | Foreground | Background |
|---|---|---|
| Main text | #182B4D | Light flat surfaces |
| Secondary text, units, axes | #42546B | #FFFFFF / light surfaces |
| Blue text | #1552B0 | #FFFFFF / #EDF4FF |
| Good text | #197047 | #E9F8EF |
| Moderate text | #8A5000 | #FFF3DA |
| Poor | #A12B25 | #FFF0ED |
| Unknown text | #42546B | #F0F3F8 |
| CO₂ label/reading/icon | #153E79 | CO₂ fill |
| VOC label/reading/icon | #483277 | VOC fill |

Use semibold-equivalent glyphs for supporting captions, units and axes. Do not
use thin low-opacity text or inherit LVGL disabled opacity over an entire card.
Keep disabled text readable and controls clearly non-interactive. Switch off
track is #BCC7D6, on track #1765E8, knob white. Chart is #1765E8; blue text #1552B0.
Selected navigation icons and active metric text are white on the action
action fill. Inactive metric selector fill #DEEBF7; selected settings-mode fill #EFF3FF.
Device/home icons #2470F0, Devices icon plates #DFEAFF; settings/inactive nav/timer
icons #42546B. Temperature icon #985116, humidity icon #17665D, both without plates.
Comfortable captions, Connected text and moderate advisory prose use secondary text, not extra
green/amber text. Semantic color is concentrated in quality badges, the small
connection icon and actual fault feedback. Internal separators/chart grid #DCE5F3.
Keep existing icon/plot stroke weights.
Set LVGL border widths to zero for cards, metric selectors, settings controls and
dialog frames. No persistent outlines or animated/decorative extra layers.
The original splash background remains #E9ECF1 to match its unchanged logo asset.

At native resolution: CO₂/VOC labels 24 px and values 62 px; comfort labels 18 px
and values 34 px; device names 18–24 px, supporting states/notes at least 16 px,
navigation icons 32 px with no captions, page titles 30 px, trend summary values 32 px. Use the nearest
suitable LVGL font size without shrinking to make a layout fit. Check four-digit
CO₂ plus ppm, Unknown, Sending, Retry and two-line device names.
The switch is 54×30 px inside a 58×48 px target; key actions
have at least 44 px hit height. Include glyphs for CO₂, °C, %, em dash and ellipsis.
Avoid parent opacity reducing contrast. Validate RGB565 screenshots, not only CSS.

## Architecture and boundaries

- Preserve `ui_init()` / `ui_tick()` integration or update every caller with a
  documented rationale. Keep LVGL calls on the application/UI context, never ISR.
- Adopt the `motor_comm.h` wire contract pattern: the UI directly reads and writes
  wire globals defined in `lora_comm.h` (`lora_hub_cmd`, `lora_node_status`,
  `lora_last_rx_tick_ms`, `lora_last_rssi`, `lora_rx_revision`). Use scaled integers;
  no floating-point types anywhere.
- Device configuration (name, preset, mode, `active_low` polarity) is Hub-local and
  persists immediately without remote ACKs or timeout reversion. Relay polarity is
  resolved on the Hub: `gpio_level = desired_on ^ active_low`, `reported_on = reported_gpio ^ active_low`.
- Output state follows the "desired versus reported" model: settled when equal,
  `Sending…` while younger than `UI_CMD_TIMEOUT_MS` (3000 ms), and `Unknown` + `Retry`
  past timeout. Without received status frames, link staleness marks the link `Disconnected`,
  hiding live data and refusing output changes.
- Keep deterministic sample data and fake acknowledgements in simulator-only
  code. The browser's 700 ms delay, TVOC raw numbers, category labels and chart
  timestamps are fixtures, not a LoRa protocol or sensor algorithm. Never derive
  VOC Index from ppb using an invented linear mapping. No Modbus register map,
  RF configuration, automation thresholds or real relay control in this task.
- Maintain separate UI view state so updates do not overwrite unfinished settings
  edits. Closing/cancelling must not mutate the application configuration.
- Keep one reusable shell and bound active objects/timers. Choose lazy page
  creation or another measured approach suitable for the memory profile. Do not
  rebuild all screens on every tick or leak callbacks when navigating.

## MCU constraints

- LVGL 8.4, RGB565; **one 800×10 partial draw buffer (16,000 bytes)** and starting
  **42 KiB LVGL heap**. Do not silently enlarge either to pass tests.
- No full 800×480 framebuffer on MCU. The existing Windows-only GDI framebuffer
  is allowed and must remain host-only.
- Keep fixed captions in process-lifetime const storage with
  `lv_label_set_text_static()`. Dynamic static-text buffers must outlive labels;
  use separate persistent fixed buffers per simultaneously visible field.
- No per-tick allocations, float formatting or trigonometry in render/input hot
  paths. Update only changed values/objects in bounded lanes; uptime should not
  invalidate the entire screen each second. Chart work and point inspection must
  be bounded; do not allocate an image/framebuffer to draw the chart.
- Measure peak heap, largest free block and fragmentation during page transitions,
  open settings, chart inspection, faults and splash-to-Home. Also report non-heap
  static history/buffer use and image/font Flash cost. Simulator success alone is
  not full target memory validation; exact MCU/linker map remains unavailable.
- Keep asset-generation steps reproducible and generated assets const. Update
  CMake and the explicit MCU export manifest for all new production dependencies;
  exclude demo code, Windows code and browser files from firmware export.

## Execute and verify

Briefly state the proposed file/API organization, then implement and test without
waiting for another design round. Resolve routine choices locally; surface real
conflicts with the 42 KiB profile or missing target firmware instead of faking data
or increasing budgets. Do not claim actual LCD readability or hardware validation
without testing that hardware.

Run the repository checks:

```powershell
tools/build_sim.bat
tools/run_regression.bat
tools/export_mcu.bat --verify
```

Extend verification beyond the old placeholder smoke test: deterministic shots
for all three pages, settings, good/moderate/poor/unknown, offline, partial sensor
failure, pending/ack/timeout/retry, empty/filled history and splash. Check uptime
minute/hour/24-hour boundaries and millisecond tick rollover; request/ack
correlation, no stale callbacks after a page change, cancelled settings, missing
chart points and font glyph coverage. Exercise repeated page/dialog cycles and
report memory stability. Inspect images at 800×480 for clipping/overlap/contrast.
Do not report a test passed unless you ran it.

Update `PLAN.md`, `DEV_LOG.md` and the relevant API/build documentation. Do not
mark hardware validation complete based on the host simulator.

## Deliver for technical-lead review

Finish with a concise review package:

1. Changed files and a summary of the public input/output contract.
2. Exact commands executed and their results, including failures/limitations.
3. Paths to current **LVGL simulator** screenshots, not browser screenshots reused
   as implementation evidence.
4. Heap/fragmentation and static memory/asset-size measurements with conditions.
5. Deviations from study 12, assumptions still application-owned, and any real
   blocker. Mark incomplete work honestly.

Keep changes local and reviewable. The technical lead will independently inspect
the diff, run the checks and decide whether it is ready for integration.
