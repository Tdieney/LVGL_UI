# Home v2 generation record

> ARCHIVED — historical context only, not the current implementation request.
> Original location: `docs/drafts/home-v2-prompt.md`.
> Use [CURRENT.md](../CURRENT.md) for the active handoff and
> [prompt rules](../README.md) before creating or updating any prompt.

- Mode: built-in `imagegen`, edit workflow
- Use case: `precise-object-edit`
- Edit target: `home-v1-master.png`
- Generated: 2026-09-07

## Edit pass 1 — replace branded top bar

Use case: precise-object-edit.
Asset type: embedded Smart Hub UI mockup revision.
Input image: Image 1 is the edit target.

Primary request: change only the top header bar of Image 1.

Top-bar edit:

- Completely remove the blue double-slash logo, the text “HYPHEN DEUX”, and the
  vertical divider that followed the wordmark.
- Move the existing room identity to the far left of the header.
- At the far left show one simple cobalt-blue outline home icon followed by the
  exact navy text “Studio”.
- Keep the existing 24-hour time “14:32” and the exact green status “LoRa Connected”
  aligned on the right, with their current clock and radio icons.
- Use the newly available center space as clean breathing room; do not add a new
  brand name, product name, page title, date, weather, avatar, menu, or slogan.
- Keep the top bar’s white card, height, corner radius, subtle shadow, typography
  style, spacing system, and overall light smart-home visual language.

Strict invariants:

- Change only the top header bar.
- Keep every pixel-level content decision below the top bar unchanged: the amber VOC
  advisory, Room Air card, all values and statuses, temperature and humidity cards,
  all four device tiles and toggles, and bottom navigation.
- Preserve the exact 5:3 landscape composition, colors, hierarchy, icons, readable
  English copy, and visual quality.
- Do not crop, reframe, restyle, recolor, add elements, or alter any text outside the
  top bar.
- No “Hyphen Deux” text or its blue mark anywhere in the edited interface.

## Edit pass 2 — remove redundant location icon

Use case: precise-object-edit.
Asset type: embedded Smart Hub UI mockup revision.
Input image: the most recent generated image is the edit target.

Primary request: make exactly one small correction in the top header bar.

- Remove the navy location-pin icon between the blue home icon and the word “Studio”.
- Shift the word “Studio” left so it sits directly after the blue home icon with
  clean standard spacing.
- The left header must contain exactly one blue outline home icon followed by the
  exact navy text “Studio”. No other icon, divider, logo, wordmark, subtitle, or
  brand appears on the left.

Strict invariants:

- Change only that redundant location-pin icon and the resulting spacing.
- Keep the entire rest of the image unchanged, including the top-bar card, “14:32”,
  “LoRa Connected”, all content below the header, exact values, copy, icons, colors,
  layout, cards, toggles, and bottom navigation.
- Preserve the 5:3 landscape composition and readable English text.
- Do not add “Hyphen Deux” or any brand mark anywhere.
