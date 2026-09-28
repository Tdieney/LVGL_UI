# R2 review — changes required

Date: 2026-09-18. Owner rejects the submitted fidelity correction and requests
resemblance to the original images, explicitly including regenerated icons.
Review/prompt delivery only; no production source edits.

[Consolidated prompt](../../prompts/CURRENT.md) · [Comparison](COMPARE.html) ·
[Fresh regression log](REGRESSION.txt)

[Unchanged source identities](SOURCE_IDENTITY.md)

## Evidence-backed findings

### R1 — P1: clipped/overlapped information and smaller touch targets

- Auto live readings are cut off in [CO2](devices_auto_thresholds.png),
  [RH](devices_auto_humidifier.png) and [VOC](devices_auto_invalid_draft.png).
  ui.c:2580 gives the strip56px height; :2609 positions the34px label at y22.
  Actual font line height is46px (ui_fonts.c:3621):22+46 exceeds56.
  Current and Now duplicate the same meaning.
- [Poor](home_poor.png):1420 collides with ppm,340 with index;
  [Moderate](home_moderate.png):160 collides with index.
  ui.c:834/:856 fixes unit offsets at92/65 irrespective of number width.
- [Sensor pause](home_auto_paused_switch.png) and [offline](home_offline.png):
  Auto paused wraps inside the98px lane and the second line is clipped.
  ui.c:1109 constrains mode width without reserving a second text line.
- The offline header's navigation obscures the timer as Disconnected expands.
  Longer uptimes are another required boundary case.
- [Retry toast](home_relay_retry.png) covers the device shortcut/header area
  after page content moved upward.
- Navigation is90x30 with no expanded hit area (ui.c:716); shortcut36x36 (:904).
  Both regress below the established44px touch requirement.

### R2 — P1: composition was redesigned, not reproduced

Reference: separate Studio/status row, navigation row, then content.
Actual: tabs merged into header; content y56 instead of roughly112; equal368px
Overview columns; tall176x152 environmental tiles; Devices cards stretched to192px.
These changes cause cramped header and excessive empty middle areas in device
cards. They are not necessary18px adaptations. Restore the original topology,
wider left Overview column, shallow environmental tiles and separate tabs.

### R3 — P1: new icon sizes still use the wrong old silhouettes

tools/generate_assets.py:27–35 retains a rectangular calculator-like purifier,
three-blade pinwheel, diagonal lamp and marked droplet.44px variants reuse these
paths. The reference uses cylindrical purifier/elliptical rim, four rounded fan
blades, connected upright desk lamp, clean droplet and three slider tracks.
Text chevrons remain despite generated icon assets.

Redraw vector sources, not just size/stroke; regenerate native alpha masks,
inspect a contact sheet and all page uses. This is now explicitly owner-requested.

### R4 — P1: Trends remains structurally different and has false time copy

[Trends](trends_data.png), [empty history](trends_empty.png); ui.c:1180–1320:
240px rail instead of roughly184px steals chart width; missing selector icons;
18px selector readings; long heading; full-size units crowd stats; all-point dots,
no subtle area/endpoint hierarchy. Last15minutes is hardcoded even with no history.
VOC index was reintroduced. Use real coverage, short names and separate units.

### R5 — P2: Settings/Auto scale and controls still miss the reference

Dropdown still border0 in ui_theme.c despite report claiming an outline;
18px value/tiny chevron, uneven row-icon scale, missing ON/OFF association in
summary, long Manual-only copy. Back/stepper symbols are undersized. Auto strip
is56px instead of about80px; live value clipping is therefore structural.
Keep genuine improvements: contextual titles, segmented polarity, outlined
value fields, stable error borders, equal Auto footer and actionable validation.

### R6 — P2: unsupported copy and completion claims

Overview introduces “CO2 and VOC are within healthy range” (ui.c:807/:980).
Remove this unsupported health assurance; retain neutral existing status semantics.
Remove index suffix and VOC index title as requested by owner.

The submission report labels all seven views MATCH / Exact Visual Parity and
claims zero clipping; its own screenshots contradict this. Settings/Home modes
are not matched to the references. It also falsely calls the dirty worktree clean,
reports C source sizes as Flash cost and calls alpha-mask icons RGB565.
MCU_BASELINE still contains obsolete modal/font/heap claims.

### R7 — P2: inherited proof work improved but is not closed

Preserve ZIP entry SHA256/same-length negative control and the added ordinary-time
boundary path. Remaining obligations are source-backed proof gaps, not newly
established Auto/radio runtime failures:

- Assert-ZipEntriesSha256 collects entryNames but accepts unexpected/duplicate
  entries, selecting the first match.
- Both watchdog tests only match timeout exception text; no task-owned child
  PID death assertions. Cleanup still ignores taskkill/wait failures/catches.
- CHECK1 integration uses current lv_tick_get, not actual near-UINT32_MAX clock
  crossing. Separate wrap predicates do not replace the combined UI/TX path.
- CHECK28 second retry setup does not assert PENDING then timeout/latch before
  recovery. It just advances through the sequence.
- CHECK31 increments sent count on IDLE (main.c:4642), then only requires
  nonzero counters (:4681), not all five scheduled lifecycle outcomes.
- CHECK32 finds label objects/fonts but does not verify glyph containment;
  clipped Current and Auto paused can still pass.

## Verification

- Viewed all seven approved concepts and new pages; inspected constructors,
  theme, generated metrics/declarations, SVG generator and affected tests/tools.
  Generated bitmap bytes were validated through render/metadata, not manually
  inspected byte by byte. Existing unrelated dirty changes were preserved.
- tools/build_sim.bat: exit0; cached/no work, existing CMP0177 developer warning.
- sim_pc/build/sim_pc.exe --regression: exit0,32/32.
  Settled minimum free5928; largest4392; drift0. Unchanged10 frames0 flush;
  changing CO2:18 flushes/77840 pixels.
- Fresh --shots plus existing raw2png conversion:24 RAW/24 PNG in this folder.
- tools/export_mcu.bat --verify: exit0, manifest PASS, not ZIP hashing proof.
- Full wrapper and isolated production verifier NOT rerun this review.
- Actual target build/LCD/QSPI/touch/RF/relay acceptance remains untested.

ui-ux-pro-max built-in professional rules informed hierarchy/stroke/feedback
review. Icon searches found no relevant match; no database-derived icon
prescription is claimed. No new aesthetic, mobile OS or framework work.

One CURRENT covers R1–R7; no staged handoffs, UI edits, commits or pushes.
