> ARCHIVED — historical context only. Active handoff: [CURRENT](../CURRENT.md).
> Superseded after const-style follow-up review,2026-09-21.

# CURRENT — finish runtime/RAM work and simplify unavailable states

Date: 2026-09-21
Status: **IMPLEMENTED — awaiting review; target RAM sign-off evidence pending**
Scope: ONE implementation/fix delivery. Preserve accepted native appearance.
Target MCU RAM/timing release sign-off is pending external firmware evidence;
this does not block the scoped repo fixes. Delivery report in docs/reviews/2026-09-21-runtime-followup-delivery/REVIEW.md.

## Authority, prerequisites and boundaries

Owner requests review follow-up and:
- Dashboard Temperature/Humidity: no Offline or equivalent no-data footnote;
  em dash is sufficient.
- Dashboard device rows and Devices cards: no Unknown; use dash for unavailable
  readback.
- Trends: KEEP Data unavailable and No readings yet.
These are approved copy changes, not a redesign. Technical lead supplies the
safe layout, tests and RAM completion instructions below.

Read AGENTS.md, docs/prompts/README.md, docs/README.md, docs/HARDWARE_OVERVIEW.md,
docs/MCU_BASELINE.md, latest docs/UI_DESIGN_BRIEF.md, and
[follow-up review](../../reviews/2026-09-21-runtime-followup/REVIEW.md),
[diagnostic output](../../reviews/2026-09-21-runtime-followup/PROBE.txt),
[probe source](../../reviews/2026-09-21-runtime-followup/probe.c),
[regression](../../reviews/2026-09-21-runtime-followup/REGRESSION.txt) and
[source identity](../../reviews/2026-09-21-runtime-followup/SOURCE_IDENTITY.json).
Use ui-ux-pro-max only for applicable interaction/readability checks.
Review captures in that NEW evidence folder. Historical prompts are not active.

Preserve the dirty worktree and existing evidence/ZIPs. No reset, clean, stash,
commit or push. Do not overwrite reviewer captures by running the old probe in
its old output directory. Create one new delivery evidence directory.

## Preserve what already works

R1 style attachment growth, R2 numeric ownership, R3 chart instance range,
R4 isolated markers, R5 sensor availability, R6 custom-name no-op Save, R7
identical-frame redraw are independently repaired. Do not reimplement/reopen
them. R8 long-digit-count transitions work but same-length widening does not.
34/34 regression, asset/export and isolated production checks currently pass.
Maintain fonts>=18px, accepted colors/geometry/icons, touch feedback and targets.
Normal-state content outside the explicitly changed header should be equivalent.

## F1 — finish width-safe, stable uptime layout

ui.c:update_uptime_string tracks only strlen. update_header measures the current
narrow string during connection/signal relayout. Repro: reconnect at111:11:11,
then advance to111:20:00. Gap14px becomes-8px and time crosses separator.
See captures/uptime_after_reconnect.png in review folder.

Use a stable reserved-width time field, right-aligned text, or an equivalent
bounded layout that considers proportional glyph width. Reserve enough width
for the supported digits at each hour-count width; don't assume equal lengths
mean equal pixel widths. Preserve font, right edge, clock icon and link cluster.
The reservation/clock position must not jitter on ordinary second updates.
Maintain >=14px visible clearance to separator; a wider gap for narrow glyphs is
acceptable. Recompute only necessary header geometry, never whole-page relayout.

Test real update paths, not only direct function mocks:
- narrow-digit relayout on reconnect AND SNR/bar changes then wider same-length
  values; Connected and Disconnected states;
- normal digit transitions,99h->100h,999h->1000h, maximum uint32 uptime;
- screen containment and no overlaps with clock/room/link labels;
- header-only invalidation on a tick, stationary content, preserved zero redraw
  for identical frames at fixed time.

## F2 — exact unavailable-state rendering

This is visual-only: do not alter telemetry flags, command phases, Auto latch,
reported states, retry permission, wire data, reconnect behavior or unit policy.

Dashboard Temperature/Humidity:
- Unavailable at startup, invalid/faulted readings or stale/disconnected link:
  title + icon + existing em dash; no Offline and no No data text.
- Hide unit while value unavailable. Do not display0 as a substitute.
- Valid values/units, signed-temperature sizing and layout remain unchanged.
- Reclaim unused note objects/buffers if safely scoped, updating test hooks;
  hiding/clearing is acceptable if lifetime simplicity warrants it.

Dashboard device rows AND Devices cards:
- Offline/no-readback: ONE existing dash; Unknown label absent/hidden; switch
  remains hidden and cannot transmit. Preserve Manual/Auto paused where applicable.
- Online command error: currently Unknown+Retry, with dash hidden. Replace the
  state text with ONE dash while keeping Retry visible, readable and tappable.
  Prefer existing state-label space for the dash so it doesn't overlap Retry.
  Do not simply unhide the offline dash at its switch position underneath Retry.
- Pending: retain Sending… and disabled switch reflecting reported GPIO.
- Confirmed idle: retain reported-state switch, no redundant On/Off text.
- Recovered readback with Auto error latch: preserve reported switch + Retry
  and explicit rearm policy. Do not force this state into unavailable.
- Keep >=18px and existing touch-hitbox clearances, no layout shrink/re-theme.

Trends: keep Data unavailable and No readings yet exactly as appropriate;
existing valid history remains inspectable on link loss.
Leave global Connected/Disconnected, hero verdict/copy and Auto No live data
unchanged. Do not globally delete Unknown from internal quality enums/caches.
Update stale tests that specifically require the removed visible device copy;
don't delete their safety assertions.

Capture startup with no RX, link loss, individual sensor invalid/fault and
recovery, pending, error+Retry, late readback and latched Auto on BOTH pages.
Assert actual visible labels/flags, single-dash count and tap behavior. Confirm
zero redundant invalidations for repeated identical unavailable frames.

## F3 — close known regression blind spots in the same delivery

Keep all34 current checks (may add checks) and every existing headroom/behavior
threshold. Tests must fail by assertion/exit code on the broken behavior, not
merely print metrics. Reviewer probe is diagnostic, not an acceptance test.

1. R2: test ACTUAL numeric Current/Min/Max labels and separate unit labels, NOT
   only stat_cur_buf. Existing main.c:5661 checks a legacy100% buffer that was
   already correct before the fix. Cover equal visible numeric representations
   across ordered metric pairs, unchanged extrema, invalid/empty/recovery and
   page rebuild. No duplicate units or overlapping glyphs.
2. R4: render [400,invalid,450,invalid,500], assert all THREE marker ROIs contain
   expected ink and invalid gaps aren't bridged. Also first/interior/last,
   singleton and all-invalid histories; preserve last/inspected emphasis.
3. R5: Home, Trends Current/selectors and each Auto editor agree under individual
   valid bits, bus fault, each timeout, combinations, loss/recovery. Preserve
   unrelated readings and old valid history. VOC telemetry0 remains allowed.
4. R6: real unchanged Save from Settings AND Auto subview retains full config,
   name (including23bytes+NUL), callback count0 and Auto hold/qualification.
   Changed thresholds/preset and Cancel still work; no new name editor.
5. R7: freeze fake-node/timers suitably; after warmup assert0flush/0pixels for
   identical fresh frames on Home/Trends/Devices and all THREE Auto editors,
   valid and unavailable. Do not bypass fresh-frame Auto qualification merely
   because displayed numbers don't change. Test timeouts/recovery still render.
6. R1: retain200 same-selected selections and100 open/close cycles in the SAME
   persistent object tree. Assert style counts and no monotonic heap loss after
   warmup. Include real pointer/event cycles, not only API calls. Keep existing
   long navigation/editor/validation stress.
7. F1/F2: add the concrete sequences and actual widget/hitbox checks above.

## F4 — complete honest, measured RAM optimization

Current independently measured host baseline:
- 42KiB fixed LVGL pool43008B + ONE800x10 RGB565 draw buffer16000B =59008B.
- Diagnostic Home boot free6896B vs5648B previously (+1248B inside pool).
- Custom theme-only allocations1560B vs2808B previously.
- CHECK8 minimum free5560B/largest3760B.
- CHECK31 settled minimum free5992B/largest4504B,10->50cycle drift+40B.
These are scenario-dependent host measurements, NOT whole-MCU memory usage.

The submitted theme removes unused property allocations but keeps24 unused
mutable descriptors/initializations; remaining shared styles still allocate.
No smaller pool or draw-buffer profile was delivered. Freeing pool allocations
does NOT reduce linked RAM when the pool size remains43008B.

Finish the least invasive useful remaining work:
- Audit consumers and remove truly dead style descriptors/exports/initialization
  and dead display-only buffers. Do not break externally documented APIs silently.
- Evaluate immutable shared styles using pinned LVGL8.4 const-style tables in
  read-only storage. Preserve exact properties, selectors, inheritance/order,
  pressed/checked/disabled/default behavior; never mutate/reset const styles.
  Avoid a new theme framework. If a candidate is rejected, provide measured or
  code-specific rationale; don't claim it is already completed.
- Trial a smaller explicitly selectable heap profile AFTER reductions, with
  conservative42KiB fallback. Report a tested candidate or show why trials fail;
  don't lower the existing >4096B free floor to manufacture success.
- Measure construction/render/dropdown/validation transients, total free,
  largest free block vs largest allocation, fragmentation and long-cycle drift.
  lv_mem_monitor.max_used in pinned LVGL does not reliably account for realloc;
  don't size solely from that field or settled screenshots. Host-only bounded
  instrumentation is allowed; no allocator replacement or vendored production
  patches purely for flattering measurements.
- Keep10-line draw buffer default. An OPTIONAL8-line host candidate reserves
 12800B (3200B less); label target DMA/xSPI latency/throughput approval pending.
  No second buffer/full framebuffer/new cache/premature flush-ready.
- Keep icons/fonts,16 history samples per metric and all approved features.
  Assets are const; shrinking them generally addresses Flash, not proven RAM.
  Gradient cache is already0, so don't invent a1024B saving.
- Ledger must distinguish .data/.bss/descriptors vs fixed pools/buffers and
  allocations INSIDE pools. Report independent deltas and net total without
  double counting. Keep target RAM99.9% unresolved unless real evidence arrives.

No target map/ELF/AXF/linker script/target lv_conf or stack declarations were
found in this repo. Collect these plus exact MCU/SRAM banks and meaning of99.9%
if supplied; otherwise one explicit target limitation. Do not infer capacity
from tools/mcu_size.ps1 defaults. Do not fabricate target savings/FPS or cut
application/RTOS stacks. HOST-VERIFIED CANDIDATE is not target release approval.

## Frozen architecture and Auto behavior

LVGL8.4/800x480/RGB565; one partial buffer. Static captions with lifetime-safe
storage; bounded dynamic buffers; no per-tick heap churn/float/trig hot path.
UI/LVGL/config access serialized in application context, never ISR.
Six LoRa globals defined once in ui.c, existing8B command/20B status, Hub polarity,
Node raw GPIO, reported-state switches, ui_is_tx_ready startup/reconnect gating.

Fan CO2 ON>=1000/OFF<=800,bounds0..10000,step50,gap50.
Purifier VOC ON>=150/OFF<=100,threshold bounds1..500,step5,gap5.
Humidifier ON<=40/OFF>=50,bounds0..100,step1,gap5 opposite direction.
Three fresh qualifying frames after10s hold; no decisions offline/invalid/pending/
latched. No forced-off on loss or timed return from Manual. Retry/Manual->Auto
explicit rearm; edits/reconnect/late ACK do not silently clear error latch.
Valid non-step-multiple imported limits remain valid; Cancel discards draft and
Auto Back retains draft. No persistence drivers, packet changes, Node automation,
radio/RS485 code, health claims or additional product features.

## Final verification and handoff

After LAST relevant edit run:
- python tools/generate_assets.py --check
- tools/build_sim.bat
- tools/run_regression.bat
- tools/export_mcu.bat --verify
- tools/verify_production.bat
- New focused assertions, candidate-profile builds, visual/memory measurements.

One docs/reviews/<new-delivery>/ report with actual logs, fresh native captures,
source/executable/config hashes, F1–F4 disposition, RAM ledger + candidate
results/limitations, measured redraw cost and a full self-review of final diff.
Record skipped hardware tests honestly. Fix regressions introduced by this work
before delivery; no staged prompts or owner-mediated per-group fix rounds.

Refresh live README/docs indexes, MCU_BASELINE, PLAN, DEV_LOG and test guidance.
Remove obsolete CURRENT guidance about gradient1024/old fonts/nonzero identical
redraw/old free4216; historical reports remain historical, not rewritten.
Record approved copy rules in UI_DESIGN_BRIEF. Mark this SAME CURRENT
IMPLEMENTED — awaiting review when delivered, with target sign-off pending.
Do not claim all work complete while required repo proof/optimizations are absent.
