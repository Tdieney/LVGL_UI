> ARCHIVED — historical context only. Active handoff: [CURRENT](../CURRENT.md).
> Superseded after independent review: functional G1/G2 fixes accepted; remaining scope is RAM/test evidence and documentation.

# CURRENT — close header/style portability defects and RAM verification

Date: 2026-09-21
Status: **IMPLEMENTED — awaiting review**
Scope: ONE consolidated fix delivery; preserve accepted UI and existing repairs.
Target MCU release approval remains pending actual firmware memory/timing data.

## Read first / authority

Read AGENTS.md, docs/prompts/README.md, docs/README.md, docs/HARDWARE_OVERVIEW.md,
docs/MCU_BASELINE.md and latest docs/UI_DESIGN_BRIEF.md.
Read [independent review](../../reviews/2026-09-21-const-style-review/REVIEW.md),
[probe](../../reviews/2026-09-21-const-style-review/probe.c),
[config probes](../../reviews/2026-09-21-const-style-review/style_config_probe.c),
[default regression](../../reviews/2026-09-21-const-style-review/REGRESSION.txt),
[40KiB regression](../../reviews/2026-09-21-const-style-review/REGRESSION_40K.txt).
Inspect fresh native captures there. Historical prompts are not executable scope.
Use ui-ux-pro-max only for relevant interaction/readability checks.

Owner accepted the native appearance and requested simpler missing-data copy;
both remain authoritative. Technical lead supplies the corrections below.
No redesign, icon regeneration, font reduction, new Auto policy, packet changes,
Flash drivers, Node automation, radio/RS485 drivers, commit/push/reset/clean/stash.
Preserve dirty worktree and evidence. New results in ONE new review folder;
never rerun a probe into a previous evidence directory.

## Preserve accepted work

Old uptime narrow->wide overlap is fixed by a stable width reservation.
No-data copy is correct: Home Temp/RH dash/no footer/no unit; Home and Devices
no Unknown, single dash for unavailable/error readback, Retry retained.
Keep Trends Data unavailable / No readings yet, hero copy and Auto No live data.
Retain existing mode/Auto paused, Sending, readback switches and error-latch rearm.

Prior style-growth, Trends numeric ownership/range/isolated markers, availability,
custom-name no-op Save and identical-frame redraw fixes work; DO NOT reopen them.
Normal page body is pixel-identical to prior reviewer captures outside approved
copy changes. Preserve that baseline and all34 current checks.

## G1 — fix stale link-label width before header positioning

ui.c:update_header changes Connected/Disconnected then reads lv_obj_get_width
before LVGL resolves the text layout. Removing the earlier layout update caused:
- disconnect: Disconnected x678..800 (screen ends799; required right edge777);
- reconnect: Connected x655..754 (wrong right padding).
See reviewer home_offline.png and PROBE.txt.

Measure/update the new link text before placing the group, or use an equivalent
correct link-slot layout. Keep22px right margin, stable reserved uptime field,
>=14px minimum separator clearance and accepted font/icon arrangement.
No per-second cluster jitter, full-page invalidation or delayed display of loss.

Test actual rendered geometry after:
- boot without RX, first connect, disconnect, reconnect, repeated transitions;
- SNR/bar changes both with and without a simultaneous link change;
- narrow111:11:11->wide111:20:00, normal digit changes,100h/1000h and max uptime.
Assert link x2==777 after settled render, all labels/icons in bounds/no overlaps,
stable uptime slot on same-length ticks, and header-only dirty area. Do not
assert just uptime gap while ignoring link text.

## G2 — use pinned LVGL constant initialization correctly

ui_theme.c currently hand-writes34 mutable lv_style_t descriptors and UI_HEX
constructs native-endian RGB565 constants. Two verified conditional regressions:
- LV_USE_ASSERT_STYLE=1: sentinel is0, expectedLV_STYLE_SENTINEL_VALUE=aabbccdd.
- LV_COLOR_16_SWAP=1: canvas full=ef7c, expected7cef from lv_color_hex.

Use the EXISTING pinned LVGL8.4 LV_STYLE_CONST_INIT macro for each immutable
descriptor. It initializes sentinel conditionally AND emits const lv_style_t.
Use matching extern const declarations in ui_theme.h and const-correct helper
parameters. Audit users; don't mutate/reset immutable styles or cast away const.
Don't turn assertions off to hide the failure.

Use LV_COLOR_MAKE-compatible constant color initialization with component
literals/extraction, or an equivalent proven configuration-aware initializer.
Keep palette tokens/value equivalence; no runtime allocation/conversion per tick.
Support the two existing RGB565 byte-order configurations correctly. Do not
invent a firmware driver/byte swap or broaden to other color depths.

Verify:
- descriptors and property arrays are read-only symbols, no style D/B symbols
  left unless explicitly justified; report section sizes rather than claiming
  that pointing to const_props automatically makes its parent descriptor const;
- all styles/properties match existing values under normal config;
- consistent full builds for LV_USE_ASSERT_STYLE=0 and1; don't mix headers
  compiled with one lv_style_t layout with a library using another;
- constant colors match lv_color_hex/LV_COLOR_MAKE with RGB565 swap0 AND swap1;
  verify decoded color parity, not raw-byte equality across swapped formats;
- ordinary and pressed/checked/disabled dropdown/switch/button behavior unchanged.
Run a bounded production smoke for assert-enabled build and full normal suite.
No production LVGL vendored edits to bypass these issues.

## G3 — finish candidate RAM proof with honest accounting

Current host object ui_theme.c: text2144,data544,bss0. All34 descriptor symbols
are D, not Flash-only; only properties are read-only. Previous theme descriptor
RAM928B -> current544B: net384B, not zero total style RAM.

43008B pool +16000B draw =59008B is a COMPONENT subtotal, not whole UI memory.
With active theme descriptors it is59552B before other UI/application state;
old equivalent subtotal59936B. Heap allocations are INSIDE the fixed pool.
Default current CHECK8 min free7144/largest5344; CHECK31 settled min7592/
largest6104, drift0. Diagnostic boot8464B. Don't sum overlapping improvements.

Reviewer has already built40KiB separately with -DUI_LVGL_HEAP_BYTES=40960:
current34/34 PASS, CHECK8 Home free5080/largest3280,
CHECK31 settled min5480/largest4000, drift+72B. Pool+draw56960B saves2048B.
This is NOT target sign-off, nor a replacement for testing after your final fixes.

After G2 and final code edits:
- Retest40KiB as an explicit opt-in candidate, keep42KiB conservative default.
- Run ALL regression/new assertions, long page/editor/dropdown/validation stress,
  construction/render transient monitoring and allocation-failure diagnostics.
- Verify total free>4096B without relaxing thresholds, stable drift and largest
  free block above measured required allocation with margin. Don't rely solely
  on lv_mem_monitor.max_used: pinned realloc accounting is incomplete.
- Provide reproducible profile build command/config/hashes/logs and a measured
  ledger of .data/.bss, readonly properties/descriptors, pool, draw buffer and
  known additional UI state. Mark host-only sizes vs target unknown.
- If40KiB fails final stricter checks, retain fallback and report actual reason;
  don't manufacture success or claim an untested arithmetic estimate is a build.
-10-line draw buffer remains default;8lines mathematically saves3200B but needs
  target DMA/transfer/latency verification. Do not state it is proven safe.
- No font/icon/history cuts, new caches/framebuffers or arbitrary stack cuts.

Collect actual MCU model, SRAM banks, map/ELF/AXF/linker script/target lv_conf,
display buffers and runtime stack/heap watermarks IF available; otherwise report
one clear external target limitation. Don't block safe repo work or invent MCU
capacity from tool defaults. No claim the owner's99.9% is fixed without evidence.

## G4 — complete the already-requested durable checks; truthful report

The last report claims tests which aren't implemented as assertions.
Do not label an observational probe exit0 as proof. Minimal required closure:
- Trends numeric Current/Min/Max AND unit widgets: keep actual RH assertions;
  cover equal-number metric/unit transitions, empty/recovery and rebuild.
- Three isolated markers [400,invalid,450,invalid,500]: assert real pixels for
  all three and absence of bridging, plus singleton/all-invalid. Series values
  alone pass even with the original transparent-marker defect.
- Fault matrix: individual valid bits,rs485_fault,co2_timeout,th_timeout,
  combinations/recovery; Home, Trends Current/selectors and all Auto editors.
  Preserve unrelated sensor values, existing history and valid VOC telemetry0.
- Real no-op Save from Settings AND Auto subview: callback0, full unchanged
  config/custom name (max23bytes+NUL), no Auto hold/qualification reset.
  Changed thresholds/preset and Cancel still follow existing semantics.
- Fixed-time identical frames: assert0flush/0pixels on all pages and ALL THREE
  Auto editors, valid and unavailable, after warmup. Keep fresh-frame Auto
  qualification/timeouts working; don't hide traffic to fake an idle result.
- Keep200 same-selected tabs /100 same-editor dropdown cycles; include pointer
  events, style counts and no monotonic heap loss after warmup. One allocator
  metadata adjustment is not automatically a leak; record convergence.
- G1 link-label containment and G2 config matrix must fail before their fixes.
- Keep copy-state tests for BOTH pages, pending/error/Retry/recovery/latch and
  single dash; test actual pointer Retry hitbox and unchanged safety behavior.

Self-review full final diff and run every gate after LAST relevant edit.
Do not respond with staged fixes, partially completed required tests, or
all-complete claims contradicted by source. Fix introduced regressions in the
same delivery. Optional polish is not a new blocker.

## Frozen architecture / Auto contract

LVGL8.4,800x480 RGB565,visible text>=18px,one partial draw buffer,accepted assets.
Lifetime-safe static captions/fixed dynamic buffers; no per-tick allocation,
float/trig hot paths or ISR LVGL. UI/config/data access serialized in app context.
Six LoRa globals defined once in ui.c;8B command/20B status, Hub polarity,
Node raw GPIO and reported-state switches, startup/reconnect ui_is_tx_ready gating.

FanCO2 ON>=1000/OFF<=800,bounds0..10000,step50,gap50.
PurifierVOC ON>=150/OFF<=100,threshold1..500,step5,gap5.
Humidifier ON<=40/OFF>=50,bounds0..100,step1,inverse gap5.
Three fresh qualifying frames after10s hold. No action offline/invalid/pending/
latched; no forced-off on loss or timed Manual->Auto. Retry/Manual->Auto explicit
rearm; reconnect/late ACK/threshold edits do not silently clear error latch.
Keep valid non-step-multiple imports, Cancel discard, Auto Back draft retention.

## Commands, evidence and handoff

After final code edits:
- python tools/generate_assets.py --check
- tools/build_sim.bat
- tools/run_regression.bat
- tools/export_mcu.bat --verify
- tools/verify_production.bat
- configuration matrix,40KiB candidate and focused tests above.

ONE new docs/reviews/<delivery>/ report: G1–G4 disposition, real logs, source/
executable/config hashes, native captures/pixel diffs, section-size/RAM ledger,
candidate transient/largest-block data, measured redraw and explicit skipped
hardware tests. Preserve prior evidence; reviewer probes must target NEW paths.

Correct live MCU_BASELINE and delivery claims about all styles in Flash,
full matrix/callback/pixel assertions, candidate testing and8-line safety.
Update README/docs indexes, PLAN, DEV_LOG/test guidance. Product copy/appearance
stays unchanged; do not replay historical designs. Mark SAME CURRENT
IMPLEMENTED — awaiting review, target release evidence pending as applicable.
