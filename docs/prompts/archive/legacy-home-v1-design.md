# Home v1 generation record

> ARCHIVED — historical context only, not the current implementation request.
> Original location: `docs/drafts/home-v1-prompt.md`.
> Use [CURRENT.md](../CURRENT.md) for the active handoff and
> [prompt rules](../README.md) before creating or updating any prompt.

- Mode: built-in `imagegen`
- Use case: `ui-mockup`
- Generated: 2026-09-07

## Final prompt

Use case: ui-mockup.

Create one polished first-draft UI mockup for an embedded LoRa Smart Hub touchscreen.
Show only the flat screen artwork, straight-on, with no device bezel, no hands, no
room scene, no browser chrome, and no presentation board. Compose precisely for a
5:3 landscape 800x480 display.

Product and audience: a technology-event demo for a modern smart-home or workplace
studio. The screen must let a visitor understand within five seconds: “Is the room
air OK?” and “Which devices are running or warning?”

Visual direction: modern premium smart-home interface, light theme, warm off-white
canvas, crisp white cards, deep navy typography, restrained cobalt blue primary
color, mint/green for good, amber for moderate, coral only for genuine faults.
Rounded 14 px cards, very subtle shadows, generous spacing, strong hierarchy, large
readable typography suitable for an embedded touchscreen. Mostly flat colors and
clean vector-like line icons. Avoid gradients, glassmorphism, neon, dark theme, 3D
gauges, decorative charts, excessive tiny labels, industrial styling, and
motor-control imagery.

Exact screen content and hierarchy:

- Compact top header with a simple text wordmark “HYPHEN DEUX” at left, room name
  “Studio”, 24-hour time “14:32”, and a small green LoRa-connected indicator at right.
- A slim but visible amber advisory banner: “VOC level is elevated — consider fresh
  air”. It is informational and non-modal.
- Main hero card titled “ROOM AIR”. Make “MODERATE” the dominant state, paired with
  a calm amber circular air-quality symbol and the supportive line “Air could be better”.
- Inside or immediately beside the hero, show two primary air metrics: “CO₂ 780 ppm”
  with status “Good”, and “VOC INDEX 120” with status “Moderate”. Use “CO₂”, never
  “eCO₂” or “estimated”.
- Two compact comfort cards: “TEMPERATURE 24.6 °C Comfortable” and “HUMIDITY 48%
  Comfortable”. They must be visually secondary to Room Air.
- A section titled “DEVICES” with four compact device tiles in one row, each with a
  recognizable outline icon, state, small mode badge, and touch-friendly toggle:
  1. “Air Purifier” — ON — AUTO
  2. “Ventilation Fan” — OFF — AUTO
  3. “Humidifier” — OFF — AUTO
  4. “Desk Light” — ON — MANUAL
  ON tiles should be subtly blue-highlighted, not filled with loud color.
- A bottom navigation bar with four destinations: Home, Trends, Devices, Details.
  Home is selected in blue. Use clear icons plus labels.

All visible UI copy must be English and correctly spelled. Do not add other metrics,
rooms, weather, dates, marketing slogans, or fake paragraphs. Ensure no overlaps or
clipped text. The composition should look feasible at 800x480 and remain legible when
viewed at that size.
