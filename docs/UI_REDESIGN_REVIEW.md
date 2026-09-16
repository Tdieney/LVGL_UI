# Project inspection and UI redesign · Study 03

This is a historical inspection. Current implementation reference: **study 12**
in [prototype/](prototype/README.md) and [the handoff prompt](prompts/CURRENT.md).
Latest owner changes remove Details/the notification panel, simplify captions,
use uptime, enlarge type, retain readable dark text and borderless frames, then
restore a bright near-white canvas, white bars/cards and fresh blue accents after
the owner rejected study 07's gloomy palette. Study 09 retains the approved bright
palette and adds faint card shadows while bars stay flat. Study 10 then replaces
Room Air with independent CO₂/VOC cards, removes visible Index and revises sizing
and tonal gradients. Study 11 uses 32 px bottom icons without captions, keeping
touch targets and semantic names. Study 12 then replaces every gradient with a
single flat RGB565-exact fill, changing colors only. Earlier proposals do not
override the latest request.

Historical review of study 03. Study 04 subsequently removed Details,
top bar changed to elapsed runtime and Connected/Disconnected, redundant captions
removed and typography enlarged. See [current prototype notes](prototype/README.md)
and the current decisions in [the design brief](UI_DESIGN_BRIEF.md). The findings
below describe the earlier inspection and are retained as design history.

Date: 2026-09-07. Branch: `ui/smart-hub`.

## Inspection scope and findings

Reviewed the maintained project source, build configuration, tooling, project
instructions, hardware/memory notes, design brief, plan, development log and prior
drafts. Generated build dependencies and Git object internals were excluded from
the source review. Consulted the motor-control branch only for the approved splash
asset. Existing uncommitted design notes and v1/v2 images were preserved.

| Area | Existing state | Design consequence |
|---|---|---|
| `ui.c`, `ui.h` | Neutral LVGL placeholder, no product screens | Keep MCU implementation separate from design discovery |
| `sim_pc/` | Windows GDI host, LVGL 8.4, pointer input, smoke/shot modes | Retain a working build baseline |
| `ui_mcu_profile.h` | 42 KiB heap, one 800×10 draw buffer, RGB565 | Preserve constraints for the later LVGL port |
| `tools/` | Build/regression, export, asset conversion and memory reports | Retain reusable tooling; no new build dependency for drafts |
| Hardware notes | One Hub UI; one Node with four relays and two RS-485 sensors | Room-centric navigation; connectivity affects every screen |
| Home v2 | Approved hierarchy and palette, large decorative symbols, small supporting type | Tighten visual hierarchy and make controls inspectable at native resolution |
| Remaining product UX | Trends proposals only; no Devices/Details drafts or failure states | Produce a coherent four-screen experience and explicit state previews |
| Open requirements | Sensor mapping, thresholds, radio packets, control ownership and feedback | Demonstrate with fixtures; do not define production algorithms |

## Proposed visual direction

The latest candidate is [the interactive prototype](prototype/index.html).
The last approved image remains [Home v2](drafts/home-v2-800x480.png) until review.

- Keep off-white surfaces, navy text, blue actions and restrained amber/green states.
- Use one shared 60 px top bar and 64 px bottom navigation. No brand in the shell.
- Home keeps Room Air as the primary group, comfort secondary and all four output
  tiles visible. Air-quality readings are shortcuts into the corresponding trend.
- Replace the large decorative air icon with a small supporting illustration.
  Use a short sentence to describe the overall state and an explicit category badge.
- Propose moving the persistent advisory into the Room Air card. The bell exposes
  a global non-modal alert panel. This changes the previously approved full-width
  banner treatment and requires visual review.
- Trends uses one chart and four metric selectors, with Current/Min/Max and a
  15-minute view. Missing samples create gaps. No threshold shading is assumed.
- Devices expands the four controls into a two-by-two grid. Preset/mode editing
  uses a focused dialog. Pending, confirmed and unknown output states differ.
- Details contains connection quality, sensor health and raw TVOC without adding
  multi-node management, cloud, login or an application settings hierarchy.

## Proposed interactions still needing product sign-off

| Choice | Prototype behavior | Still open |
|---|---|---|
| Quick control while Auto | Select Manual, then send requested state | Final override/return-to-Auto policy |
| Automatic control | Mode badge is selectable; no rules run | Sensor thresholds, hysteresis, safe states and ownership |
| Node loss | Hide live values; Unknown outputs; disable controls | Real stale/offline durations and firmware fail-safe |
| Partial air-sensor loss | CO₂/VOC unavailable; comfort remains visible; Room Air Unknown | Real per-sensor validity model |
| Command timeout | Unknown output and Retry | Packet acknowledgements, retries and actual load feedback |
| Alerts | Bell panel and Home inline advisory; no acknowledgement | Inline treatment versus approved banner |
| Trends | Single chart, session statistics, sample inspection | Q22–Q24 remain proposed, now directly reviewable |

## Validation and next handoff

Browser checks and native-size screenshots cover the four screens and their key
states; see [prototype notes](prototype/README.md). The unchanged simulator build
and regression pass with 25% LVGL heap use, 32,304 bytes free and 0% fragmentation.
MCU export manifest verification also passes. A dependency CMake policy warning
(CMP0177) is non-blocking.

Next: review study 03 against Home v2, settle the proposed interactions, then map
the approved screens onto LVGL. Browser layout and memory behavior do not validate
the eventual MCU implementation.
