# Smart Hub UI Design Brief

## 2026-09-16 — W1–W4 Review: Retain Layout & Finalize Touch Targets

Retry availability following reconnect / late ACK and physical switch readback display behavior have been verified.
Retain active color palettes, Temperature/Humidity metric cards, and modal layouts; no broad redesign.
Newly measured switch touch footprint was 54x30: finalize the >= 44px vertical height requirement by expanding the touch hit area without clipping or overlapping adjacent Retry/Settings buttons, while preserving visual pill dimensions.
This finalizes existing requirements without introducing new Auto engine behaviors or scope changes.
Refer to the [independent review](reviews/2026-09-16-auto-w1-w4-independent/REVIEW.md) and [prompt X1–X4](prompts/CURRENT.md). Target hardware bench validation on physical LCD/touch panel remains open.

## 2026-09-15 — Preserve UI Direction; Add Re-arm for Latched Error States

The V1–V4 review confirmed that preset dropdowns and timeout metadata groupings are improved; no redesign of cards, modals, colors, or typography is requested. Refer to [W1–W4 review](reviews/2026-09-15-auto-v1-v4-independent/REVIEW.md) and [active prompt](prompts/CURRENT.md).

Technical lead specification for W1: when online, if an Auto channel remains in `paused_error` while the command phase is IDLE due to link reconnection or late ACK, the Retry/re-arm button must remain accessible. Display true reported readback rather than artificial `Unknown` states. Render Row 1 as `"Auto paused"`, Row 2 with the reported switch on the left and Retry on the right, maintaining >= 44px touch target clearance; apply corresponding layout on Devices. This represents a local state variant for the latched recovery condition. Do not silently clear the latch or alter Auto policy.

## 2026-09-15 — Automatic Prompt Delivery Post-Review; Retain Stable UI Baseline

- **Owner Directive:** Every review finding remaining defects or unresolved issues must automatically produce a corrective fix prompt in the same turn without requiring a separate request. Stored exclusively at [prompts/CURRENT.md](prompts/CURRENT.md); archive superseded versions before replacing. Review turns do not grant authorization to implement code, commit, or push.
- **Technical Lead V1–V4 Direction:** Retain primary layout, Temperature/Humidity cards, and balanced settings geometry. Finalize preset selection popup (`ui_font_18`, row pitch >= 44px, bounded within modal container, borderless) and balance status grouping during timeout/offline conditions.
- For composite states, prioritize two metadata rows: mode (`Auto paused` or `Manual`) spanning full width on the top row; state (`Unknown`) alongside Retry/dash on the bottom row, avoiding cramped three-line baseline compression. Measure against font metrics and native screenshots; retain font size >= 18px, bottom clearance >= 10px, touch targets >= 44px, and preserve control logic.
- Details here serve as technical lead specifications for the subsequent fix cycle. Retain palette, modal dimensions, Auto policies, and packet schemas. Refer to [V1–V4 review](reviews/2026-09-15-auto-layout-result-review/REVIEW.md).

## 2026-09-15 — Auto Layout Balancing & Metric Card Proportions (Technical Lead Direction)

**Owner Directive:** Review submitted fixes; re-balance Auto settings, Temperature, and Humidity card spacing; authorize technical lead to decide necessary adjustments and incorporate into new prompt. Coordinates below were selected under this authorization.

- Retain Study 12 flat light palette, 4 metric color accents, subtle elevation shadows, and borderless card frames. Do not alter outer screen bounds, re-introduce gradients, tabs, or unauthorized Auto features.
- Modal dimensions fixed at (180, 56, 440, 368), padding 20px, content area 400x328px. Header height 44px; Save button anchored at (0, 280, 400, 48) relative to content origin (200, 76), remaining stationary across error states.
- Device settings arranged in horizontal rows: y=56/112/168/224, 44px per row, 12px vertical gaps. Label/control columns at x=0/140; dropdown width 260px; Manual/Auto buttons width 124px, gap 12px. Set limits spans full content width; polarity switch maintains >= 44px touch hit target.
- Auto thresholds: context header at y=56; ON/OFF rows at y=92/152, height 48px; aligned label, minus, value, plus columns. Reset limits at y=212, height 44px; fixed two-line error slot; Save button aligned identically with Device settings view. Value 10000 does not shift layout columns. No `(index)` suffix for VOC; retain 1..500 unitless range and established threshold tables.
- Temperature and Humidity cards fixed at 236x98px, 12px horizontal gap. Two text lines centered based on true font metrics: title `ui_font_18` at y=12 (line height 25px), value `ui_font_34` at y=41 (line height 46px), text margin x=44; 24px icons at x=12, y=37 relative to card origin. Vertical margins ~12px top / 11px bottom.
- Do not re-introduce "Ideal", "Comfortable", "Healthy" or arbitrary comfort bands lacking logic. Invalid telemetry displays an em-dash `—` with inline `No data` in the value row; lost link displays `Offline`. No tertiary footer rows; do not display stale readings next to dashes. Valid readings maintain inline units and stable positions.
- Home device status retains dedicated space separate from switch/Retry controls, with text >= 18px. `Auto` / `paused` may wrap into two lines when appropriate; verify Sending, sensor pause, offline, error, and recovery states across all four tiles.
- Link loss during Auto must indicate paused state; do not alter relay policies, treat link loss as a permanent error latch, or trigger autonomous retries upon reconnect.

Refer to [latest review](reviews/2026-09-15-auto-v1-layout-review/REVIEW.md) and [active handoff](prompts/CURRENT.md).

## 2026-09-15 — Resolution of R1–R10, Preserving Product Design

The owner directed that prompts resolve all independent review defects. Active handoff is [prompts/CURRENT.md](prompts/CURRENT.md); prior implementation prompts are archived. Retain LCD threshold editing and established threshold tables. Focus areas: Manual/Auto opt-in, TX/config/recovery lifecycles, draft state isolation, touch interaction, font tables, and readback verification.

## 2026-09-14 — LCD Threshold Editing & Prompt Conventions

- **Owner Decision:** Auto thresholds must be editable on the LCD, replacing the fixed-thresholds-only proposal. All implementation and fix handoffs must use standardized storage conventions.
- **Technical Lead Specification:** Retain 3 appliance rules: Ventilation Fan/CO₂, Air Purifier/VOC, Humidifier/RH; each appliance maintains independent ON/OFF thresholds. Baseline values serve as initial defaults. Editor resides inside existing settings modal, with validation, Reset limits, shared draft state, and a single Save commit point; no new navigation tabs.
- Detailed timing, manual overrides, fault masking, recovery sequences, configuration migration, and acceptance tests are specified in [CURRENT.md](prompts/CURRENT.md).

---

## Operating Guidelines & Working Agreements

- Initial design phases produce visual artifacts only: wireframes, mockups, or native 800x480 PNG renders.
- Interactive HTML/CSS prototypes in browsers serve to validate user flows and screen states; prototyping does not imply initiating C/LVGL implementation.
- C/LVGL source code modifications and heap/xSPI profiling are deferred until user flows and screen layouts are approved.
- Physical schematics serve as hardware interface references, not design constraints.
- Implementations proceed against [CURRENT.md](prompts/CURRENT.md); independent review evaluates code, simulator captures, unit tests, and memory metrics.

## Product Context Baseline

- Smart Hub: 800x480 capacitive touch screen, QSPI display with display-side GRAM, LoRa transceiver.
- Smart Node: Four 10A relays, shared RS-485 bus supporting two environmental sensors.
- Application: Exhibition demo for smart residential or studio workspace environments.
- Target Audience: Homeowners, office staff, and technology enthusiasts.
- Network Topology: Peer-to-peer with exactly one Smart Node; no multi-node management UI required.
- Core Inquiries Addressed by UI: "Is the indoor environment healthy?" and "Which appliances are currently running or reporting faults?".
- Trends Data: In-memory session logging only; no non-volatile Flash logging.
- Language & Units: English language interface, light theme, modern Smart Home aesthetic, °C temperature. Top bar displays running session uptime (`HH:MM:SS`), not time-of-day. Real-time timestamps follow 24-hour notation.

## Telemetry Presentation Decisions

### CO₂
- **Decided:** Displayed on primary user surface with label `CO₂` to indicate ventilation quality.
- Technical term `Equivalent CO₂eq` is excluded from primary labels.
- Explanatory notes such as `estimated` or `eCO₂` are excluded from detail views.
- **Open:** Final measurement ranges and alert color bands subject to physical `MXVC10S-B-P67` sensor register specifications.

### TVOC
- **Decided:** Raw `ppb` figures are excluded from primary cards due to interpretability hurdles.
- Primary card displays unitless `VOC Index` alongside semantic status: `Good / Moderate / Poor` with color indicators.
- Dedicated `Details` tab was removed per Study 04 feedback. Raw ppb displays are omitted from primary views.
- **Open:** Sensor calibration curves and threshold boundaries for `Good / Moderate / Poor`.

**Algorithmic Note:** Sensirion VOC Index 1–500 represents a relative index computed from SGP4x raw signals via the Gas Index Algorithm, utilizing recent history as an adaptive baseline (typical air centered around 100). Do not apply linear conversions from raw TVOC ppb to "Sensirion VOC Index". Reference: [Sensirion VOC Index Note](https://sensirion.com/media/documents/02232963/6294E043/Info_Note_VOC_Index.pdf).

## Established Design Principles

- **Glance-First:** Assess room environment, node connectivity, and active relays within 5 seconds.
- Distinguish **reported physical state**, **in-flight command state**, and **automation logic**.
- Stale or severed telemetry must be clearly indicated (em-dashes and `Unknown`), never presenting frozen numbers as live data.
- Avoid color as the sole indicator; accompany with text, icons, and badges.
- Avoid technical jargon on primary operational screens.

## Information Architecture

Organized around a **room-first dashboard**:

1. `Home`: Hero air quality metrics (CO₂, VOC, temperature, humidity), active advisories, and four compact device tiles.
2. `Trends`: 16-point sliding history graphs across active session; resets upon system reboot. Displays `Current`, `Min`, and `Max` statistics.
3. `Devices`: Four relay cards displaying appliance presets, operational modes (Manual/Auto), state indicators, and configuration controls.

Navigation consists of three bottom tabs: Home, Trends, Devices. Notification panels and top-bar bell icons are omitted in favor of inline contextual advisories.

## Appliance Presets (Four Relays)

Configurable via preset selectors in the `Devices` tab:
- Air Purifier
- Ventilation Fan
- Humidifier
- Dehumidifier
- Heater
- Light
- Smart Socket
- Generic Device

Demo Configuration:
| Relay | Default Preset | Functional Role |
|---|---|---|
| 1 | Air Purifier | Automated actuation linked to VOC Index |
| 2 | Ventilation Fan | Automated actuation linked to CO₂ |
| 3 | Humidifier | Automated actuation linked to low humidity |
| 4 | Desk Light | Manual actuation for live demonstration |

Tiles render icon, appliance name, switch toggle, and `Manual/Auto` mode. Redundant `On/Off` text is omitted; switch position conveys state. Transient states display `Sending…`, `Unknown`, or `Retry`.

## Visual Studies & Design History

### Study 12 — Active Baseline: Flat Surfaces, Precise RGB565 Palettes
- Removed all CSS/LVGL gradients. Surfaces transitioned to flat fills: canvas (`#EFEFEF`), device cards (`#F7F7F7`), chart and dialog containers (`#FFFFFF`). Color accents reserved for metric tiles, active pills, and status badges.
- Colors mapped to exact 16-bit RGB565 bit patterns (`(v<<3)|(v>>2)` for R/B, `(v<<2)|(v>>4)` for G) to prevent rounding artifacts (e.g. pink tints on off-white surfaces).
- Technical rationale: LVGL v8.4 gradient caching uses keys derived from descriptor address and size without color attributes, causing color map collisions between cards of identical dimensions. Flat surfaces eliminate cache collisions and save LVGL heap.
- Splash retains `#E9ECF1` matching embedded asset bitmaps.

### Study 11 (Historical): Icon-Only Bottom Navigation
- Enlarged bottom navigation icons from 25 to 32px; removed text labels. Centered within 48px touch targets in a 64px navigation bar.
- Selected tab renders white icon on blue pill; inactive tabs use `#42546B`.

### Study 10 (Historical): Metric-First Layout & Separate Cards
- Split Room Air aggregate card into independent CO₂ and VOC hero cards.
- Screen grid: Top header 56px, content area 360px, navigation 64px; 760x336px grid with 250/250/236px columns and 12px gaps.
- Primary metric values use 62px typography; section labels 24px; comfort metrics 34/18px; status labels 18px.

### Study 09 (Historical): Elevation Shadows
- Introduced subtle drop shadow (`0 2px 10px 0 rgba(24,43,77,0.06)`) on primary cards without borders.

### Study 08 (Historical): High-Contrast Light Theme
- Near-white canvas (`#F7F8FA`), crisp white cards, high-contrast typography (`#182B4D` primary, `#42546B` secondary).

---

## Architectural Decision Log

| Date | ID | Decision |
|---|---|---|
| 2026-09-06 | D-001 | Discovery phase first; interactive drafts before C/LVGL implementation. |
| 2026-09-06 | D-002 | Primary label for eCO₂ is `CO₂`; disclosure notes omitted from primary view. |
| 2026-09-06 | D-003 | TVOC displays as unitless index; raw ppb excluded from hero cards. |
| 2026-09-07 | D-004 | Target application is residential/studio smart workplace demonstration. |
| 2026-09-07 | D-005 | P2P topology with a single Smart Node. |
| 2026-09-07 | D-006 | Home screen must communicate air status and relay states within 5 seconds. |
| 2026-09-07 | D-007 | Trends history maintained in RAM during session; no Flash logging. |
| 2026-09-07 | D-008 | English language UI, light aesthetic, °C temperature, 24-hour time. |
| 2026-09-07 | D-011 | Splash retains motion language from motor-control UI; uses Hyphen Deux mark. |
| 2026-09-07 | D-012 | Four default relays: Air Purifier, Ventilation Fan, Humidifier, Desk Light. |
| 2026-09-07 | D-013 | Home features quick toggle; detailed preset and Auto settings on Devices tab. |
| 2026-09-07 | D-014 | Three-tab navigation (Home, Trends, Devices); inline contextual advisories. |
| 2026-09-07 | D-021 | Top bar displays session uptime `HH:MM:SS`; link status displays Connected/Disconnected. |
| 2026-09-07 | D-026 | Enhanced typographic contrast; eliminated outer borders. |
| 2026-09-07 | D-033 | Eliminated gradients: all surfaces render flat RGB565 fills. |
| 2026-09-07 | D-034 | Pure neutral grey/white surfaces (`#EFEFEF`, `#F7F7F7`, `#FFFFFF`) replacing blue tints. |
| 2026-09-09 | D-035 | Adopted `motor_comm` direct global variable wire model (`lora_comm.h`). Eliminated `ui_snapshot_t` and push APIs. Node functions as dumb GPIO actuator; Hub resolves polarities (`desired_on ^ active_low`) and maintains local configs. |
| 2026-09-09 | D-036 | Replaced top-bar radio icon with 4-bar stepped indicator driven by SNR margin over demodulation limit (SF7..SF12: -7.5 dB to -20 dB). 1.0 dB hysteresis prevents border flicker. `Disconnected` indicated strictly upon packet timeout (5000 ms). Active color: `UI_COLOR_GREEN` (`#197047`), inactive: `UI_COLOR_SWITCH_OFF` (`#BDC7D6`). |
| 2026-09-14 | D-037 | Hub-side Auto v1 automation module supporting 3 appliances (Fan/CO₂, Purifier/VOC, Humidifier/RH) with LCD-editable thresholds in a 440x368 modal subview. Features 3-frame qualification, 10s anti-hunting hold, sensor fault masking, instant manual override, error latching (`Auto paused`), and safe re-arm. |
