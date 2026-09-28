# Independent review — verification completion and MCU integration

Date: 2026-09-17. Scope: submitted R1–R4 completion plus the owner's pasted MCU
include/link errors and request to follow motorCmd storage ownership.

**Verdict:** MCU include/global-storage changes accepted. Host regression and
exported production build pass. No new UI runtime defect reproduced. Remaining
findings are verifier/test/evidence gaps; one [CURRENT](../../prompts/CURRENT.md)
covers all of them. No UI redesign requested.

## MCU integration — accepted; stale guidance corrected in this review

- ui/motor-control:screens.c defines motorCmd and status storage; other files
  consume header extern declarations. Current ui.c:21-26 applies the same model
  to all six LoRa globals. Simulator and smoke runner no longer redefine them.
- The earlier undefined-reference errors were consistent with declarations
  without a linked storage definition. Current ui.c supplies that storage.
- Exported UI headers/assets and generators use plain lvgl.h. No nested
  lvgl/lvgl.h include remains in production UI source.
- Independent host C99 -Wall -Wextra -Werror build WITHOUT
  LV_LVGL_H_INCLUDE_SIMPLE and WITHOUT UI_TEST_HOOKS passed. Smoke passed; nm
  shows exactly one definition of each of the six globals.
- Found stale instructions in lora_comm.h and SMART_HUB_UI_GUIDE section 7.1
  telling firmware to define the globals. Following them with the new ui.c
  would cause multiple-definition errors. Corrected those comments/instructions,
  clarified lora_hub_link.h and added the integration contract to MCU_BASELINE,
  which is already bundled by export_mcu.bat.
- Firmware must include the headers and publish data, not define the six globals
  again. Remove definitions added by an older integration. Compile each UI
  source once and add the LVGL header directory to the compiler include path.
- No access to the owner's Cortex-M33/Asterix project was available here.
  Host linking is not a claim that firmware.elf now builds on that target.

## Executed checks

- tools/run_regression.bat: PASS,31/31,24 native scenario captures.
  After documentation/header-comment corrections, sandbox rebuild stalled;
  interrupted it and confirmed no compiler/build processes remained. Approved
  outside-sandbox final rebuild/regression passed; [literal log](regression.log).
- tools/verify_production.bat: approved outside-sandbox run, exit0. Collision,
  explicit race, controlled failure/watchdog exception, isolated exported
  production build, clean smoke and exact exit1 negative control passed.
  This statement summarizes actual execution; it does not endorse overstated
  byte-identity/process-reaping claims discussed below.
- Host C99 compile/link, smoke, six-symbol check and export --verify: exit0;
  [command/output evidence](integration.log).
- CHECK31 now really runs Auto: commands counter10, pending/timeouts/latches/
  recoveries/rearms5 each. Measured free5344 immediate /4496 settled, drift-56.
  These are tested-state measurements, not a universal transient guarantee.
- All six Auto wrap directions now assert frames1/2/3. Both Switch and Retry
  run across page/polarity/recovery combinations. These improvements are accepted.

## Remaining findings, consolidated

### S1 [P2] Export integrity/reaping claims still exceed assertions

tools/verify_production.ps1:274-279 calls a length comparison source identity.
Same-length modified content passes it; no entry data is read/hash-compared.
The explicit race winner at328 checks only entry count, not names/content.
Thus REPORT.md's byte-for-byte/complete-artifact claim is not established.

The watchdog test at364-378 only checks that an exception string contains
Watchdog timeout. It does not assert child exit, sentinel integrity under timeout,
or exercise timeout of Invoke-ConcurrentWorkers. Helpers ignore taskkill failure;
the concurrent helper returns an unchecked WaitForExit result to the pipeline
and can discard diagnostics on exceptions. No actual orphan was observed here:
this is an unverified failure path, not evidence that normal export is broken.

Fix the named checks using actual entry bytes/hashes and bounded task-owned
process-tree observation. Add a same-length corruption negative control so
the integrity assertion demonstrably detects the old false-positive condition.
Keep current non-overwriting exporter and existing happy-path tests.

### S2 [P2] Small remaining holes in claimed complete flow assertions

- sim_pc/main.c:624 takes the current ordinary LVGL tick for the end-to-end
  4999/5000ms flow. It never positions that flow across UINT32_MAX. Separate
  predicate wrap tests are useful but do not meet the already-requested
  integrated rollover test; asserted UI masking is also absent from this block.
- CHECK28 Retry re-setup at3804-3843 now executes real traffic, but does not
  assert PENDING and timed-out/latch state before injecting recovery, unlike
  the first setup. Add those preconditions; keep the completed action matrix.
- CHECK31 counts a 'command sent' at4736 merely because phase is IDLE after
  800ms. IDLE can mean nothing was sent. At4775 all counts only need be nonzero:
  missing four of the five scheduled lifecycles still passes. Assert observed
  seq/TX/readback and the existing five scheduled outcomes, not a new stress size.

No production timing/Auto/touch bug is inferred from these missing assertions.
The current measured stress behavior and layout remain accepted.

### S3 [P3] Correct report/contract inaccuracies, not product behavior

Author REPORT.md says VOC thresholds are ppm/ppb (26,62-63), although voc_index
is unitless; claims CRC-protected config despite no such implementation (176);
lists nine font files and16 required export files rather than the actual
manifest; describes an explicit ZIP race as a --dest directory collision;
adds unapproved largest-block/fragmentation thresholds and calls the host suite
mathematically rigorous. The raw log files now exist, which is an improvement,
but these interpretations remain inaccurate. Supersede with a factual report
and portable relative links; preserve historical evidence.

Do not implement CRC, change sensor scaling, add thresholds or radio drivers
to make the prose true. Keep the existing packet/storage/Auto contract.

## Changed by this reviewer / handoff

Only integration header comments/docs, review evidence, prompt lifecycle and
tracking documents changed. Submitted UI/global definitions were retained,
not reimplemented. No production behavior changes, commit or push.
Physical LCD/touch, firmware map/stack, QSPI and radio/relay tests remain target
limitations. [One bounded corrective handoff](../../prompts/CURRENT.md) contains
all outstanding S1–S3 work; completed UI/Auto behavior is not reopened.
