# Runtime/RAM follow-up review — 2026-09-21

Disposition: **CHANGES REQUIRED**, one consolidated handoff in
[CURRENT](../../prompts/CURRENT.md). Production UI sources were not edited by this
review. Owner-requested no-data copy cleanup is specified, not implemented.

## Scope and verification

Reviewed submitted ui.c/theme changes and affected Dashboard, Trends, Devices,
Settings/Auto, link/fault/recovery, command/readback and memory paths. Compared
source identity to the previous review: ui_auto.c, fonts, icons, splash, lv_conf.h
and MCU profile are unchanged; ui.c, ui_theme.c and simulator tests changed.
The worktree contains substantial older uncommitted work; HEAD is not a clean
previous-delivery baseline. Do not attribute that entire diff to this submission.

Used ui-ux-pro-max's applicable interaction/readability checks, preserving the
accepted native appearance and >=18px text. No web/mobile redesign rules imposed.

Fresh local checks:
- Simulator build PASS (no rebuild needed; CMake CMP0177 developer warning).
- Regression/smoke/scenario runner PASS: **34/34**, 27 scenario captures.
- Asset integrity PASS: 25 masters, 68 runtime icons.
- MCU export manifest PASS.
- Isolated production verifier ALL FOUR PHASES PASS, including negative control.
  Initial sandbox attempt failed at owned-fixture taskkill with Access denied;
  rerun with approved escalation passed. This is not a production code failure.
- Independently recompiled host-only [probe.c](probe.c); observations in
  [PROBE.txt](PROBE.txt). It logs diagnostic observations and returns 0 even when
  a defect is reproduced; it is NOT a pass/fail acceptance suite.
- [Regression log](REGRESSION.txt), [verification record](VERIFICATION.md),
  [source identity](SOURCE_IDENTITY.json), [captures](captures/).

No target MCU map/ELF/AXF/linker script/target lv_conf.h or physical LCD was
available. No claim of target RAM percentage, frame rate or bus timing validation.
Prior capture directories are not treated as immutable pixel baselines; the
previous probe writes into its original evidence path if rerun. This review uses
a new directory and does not claim a verified 100% old-vs-new visual identity.

## Accepted repairs — do not reopen them

| Previous issue | Fresh observation |
|---|---|
| R1 duplicate styles | 200 same-metric selections: free11560->11560, style count3->3. 100 dropdown reopens in ONE editor: free15584->15584, styles16->16. |
| R2 competing label owners | Actual Current/Min/Max labels remain numeric100 on CO2->RH; unit is separately %. Next RH99 updates correctly. |
| R3 rebuilt range | Actual chart96..104 before AND after Trends->Home->Trends. |
| R4 hidden isolated points | [400,invalid,450,invalid,500] now draws all three: forest pixels13/13/29 in the same9x9 probes. |
| R5 availability | Bus fault masks all readings; co2_timeout masks only CO2/VOC, th_timeout masks only Temp/RH. Home, Trends selectors and all Auto editors agree. VOC0 is displayed; VOC501/CO210001/RH101 are masked. |
| R6 no-op name loss | Studio Fan retained, actual config callback count0. |
| R7 redundant redraw | Home10 identical RX frames0flush/0pixels. All THREE Auto editors with valid live data likewise0/0; invalid Auto reading also0/0. |
| R8 digit-count boundary | 100h and1000h transitions repaired, BUT same-length widening after relayout remains broken (F1). |

The new regression exercises style stress, chart rebuild, bus fault and custom
name. Existing Auto/TX/polarity/recovery and typography checks pass. These results
do not prove all possible input sequences.

## Remaining findings

### F1 — P2: header overlap after reconnect or signal-bar relayout

Locations: [ui.c:687](../../../ui.c#L687),
[ui.c:832](../../../ui.c#L832); simulator test near main.c:5719.

update_uptime_string dirties header geometry only when strlen changes.
update_header positions the label from its CURRENT proportional-font width when
link/bar state changes. If that moment has narrow digits, later wider digits of
the SAME length run into the separator.

Reproduction (no production source edits):
1. Connected Home, set uptime111:11:11.
2. Disconnect then reconnect, forcing header layout. Measured separator gap14px.
3. Advance displayed uptime to111:20:00 without changing link or string length.
4. Gap becomes **-8px**: label crosses the separator.
See [native capture](captures/uptime_after_reconnect.png) and PROBE.txt.

This is the unresolved part of R8, not a new visual-polish demand. Use a stable
reserved-width time field/right-aligned text or equivalent width-safe layout.
Do not shift the cluster every second, change accepted font, or redraw the page.
Test reconnect and SNR relayout at narrow digits, same-length widening, normal
second changes and long-uptime boundaries. Require >=14px minimum clear gap,
not a false guarantee that a proportional string always fills its reservation.

### F2 — owner-requested no-data copy reduction (not a prior implementation bug)

Current locations ui.c:1270/1293,1335..1340,2130..2136.
- Home Temp/RH: remove visible Offline and equivalent No data footer text; leave
  the existing em dash, hide unit when unavailable. Keep valid numeric layout.
- Home device rows and Devices cards: remove visible Unknown. Offline/no-readback
  uses the existing dash. Keep mode/Auto-paused, genuine Sending and Retry.
- Command-error state currently hides its dash and shows Unknown + Retry
  ([capture](captures/devices_error.png)). Simply clearing Unknown would leave
  no readback marker. Render ONE dash alongside Retry with separated hit areas;
  preserve error latch/retry behavior and reported-vs-desired semantics.
- Trends Data unavailable and No readings yet are explicitly retained.
- Do not remove global Connected/Disconnected, change hero copy or Auto editor
  No live data: those were not requested. No mass Unknown-string replacement;
  legacy/internal quality labels are separate from visible device state.

### F3 — P2 verification gap: some added tests don't protect the actual repair

main.c:5661 checks stat_cur_buf ==100%, the legacy formatted buffer that also
passed while the ACTUAL numeric widget incorrectly contained100%. Replace/add
checks of lv_label_get_text on Current/Min/Max and separate unit widgets across
equal-number transitions. Do not undo the fixed production code.

R4 still lacks a permanent assertion of all isolated-marker pixels/no bridging.
New R8 tests cover only 100h/1000h, missing F1. R6 checks the name but not actual
no-op callbacks/config identity/hold preservation. Auto editor zero-redraw and
fault matrix are independently observed here but need durable assertions.
Promote these focused cases into the normal regression runner; assertions must
fail on the old behavior, not merely print measurements. Keep all existing checks
and acceptance floors. Same-page pointer interactions must complement API stress.

### F4 — RAM delivery remains partial; do not equate free heap with linked RAM

Fresh measurements:
- Diagnostic Home boot free5648->6896: +1248B inside the arena.
- Theme-only free40408->38848: current1560B consumption, previous2808B;
  +1248B improvement in this identical host setup.
- CHECK8 minimum observed free5560B/largest3760B (previous4216/2416).
- CHECK31 settled minimum free5992/largest4504, cycles10->50 drift+40B; passes.
- UI_LVGL_HEAP_BYTES still43008 and single draw buffer still16000:
  **59008B reservation unchanged**, before other RAM/stack/firmware allocations.
- Unused style property allocations are gone; their mutable global descriptors
  and zero-property initialization remain. Shared styles are still mutable.

Thus no demonstrated reduction of the target's reported99.9% linked RAM usage.
Previous handoff's const-style/dead-descriptor work, measured smaller profile,
transient/largest-allocation evidence and non-double-counted RAM ledger were not
completed. Finish safe invisible work, or explicitly document each rejected
candidate with measurements/rationale. Keep42KiB/10lines as conservative default
until a candidate passes unchanged headroom/regression tests; target approval
still requires actual firmware evidence. Do not arbitrarily cut stacks or fonts.

walkthrough.md's blanket R1-R8 completion is overstated. MCU_BASELINE still has
a current top summary reporting the old4216B/nonzero-redraw result and executable
guidance mentioning gradient1024/old font names. Refresh live guidance/status,
keep historical reports intact and clearly labeled. Mark CURRENT's lifecycle.

## Required next delivery

ONE CURRENT covers F1–F4: minimal visible copy edits, remaining header fix,
targeted durable tests, measured remaining RAM work and honest documentation.
No icon regeneration, redesign, Auto policy change, firmware drivers or Flash
feature work. Preserve accepted repairs; complete final self-review/tests after
the last edit rather than asking the owner to relay issue groups separately.
