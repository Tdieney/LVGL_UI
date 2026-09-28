# Independent consolidated review — Y1–Y3 result

Date: 2026-09-17. Verdict: **production fixes accepted; verification handoff incomplete**.

No new reproducible production/runtime defect was established in this review.
Do not reopen the accepted design or implement speculative production changes.
The remaining findings below concern incomplete required tests and completion claims.
One corrective handoff covers them all: [CURRENT](../../prompts/CURRENT.md).

## Scope and independent results

Reviewed the submitted working-tree changes in UI/Auto, shared styles, simulator
hooks/restore/TX/regression, export/production verifier, and associated documentation.
The working tree contains cumulative prior-round changes; it is not a clean
single-commit Y-only diff. Existing owner/author changes were preserved.

- `tools/run_regression.bat`: exit 0, build/smoke, 31/31 checks, 24 scenario captures.
- `tools/verify_production.bat`: exit 0, approved outside-sandbox run.
  Manifest, primary/secondary collision protection, concurrent default exports,
  exported-artifact Release build with warnings-as-errors, production smoke,
  and strict deliberate failure (exit 1 plus expected stderr) passed.
- Reviewed native author captures: Home, Auto thresholds and Manual-only settings.
  No blocking spacing/contrast/layout regression found in those views.
- Additional host-only [probe](check.c), compiled against current sources and cached
  LVGL, ran the full regression then actual Auto qualification -> pending ->
  timeout/latch while Device settings/dropdown was open. Invalid limits and an
  unsupported preset were also exercised with the error/toast present.
  Final exit 0; literal output is [combined.log](combined.log).

| Additional combined state | Total free bytes | Largest block |
|---|---:|---:|
| Auto pending + dropdown | 4904 | 4872 |
| Auto timeout + dropdown + toast | 5264 | 4816 |
| Invalid limits + Auto error + toast | 4464 | 4464 |
| Unsupported preset + dropdown + Auto error + toast | 4648 | 3424 |

Minimum for this additional sequence: **4464 > 4096**, not a universal heap minimum.
The maintained 50-cycle test measured 4872 immediate / 4376 settled, zero stable
drift. Largest block is diagnostic, not a new >4096 acceptance threshold.

Probe construction initially needed a corrected include and a normal
render/timer step after navigation; preliminary diagnostic failures were not
classified as production defects. The final source/log above are the reproducible
result. This probe uses helper-driven modal actions; it is NOT proof of 50
full pointer cycles. Build/production results above are summaries of tool
executions, not an invented complete raw transcript.

## Findings — all remaining items in one delivery

### R1 — P2: CHECK 31 does not exercise the Auto/recovery path it claims to cover

Evidence: `sim_pc/main.c:4269` restores every device to Manual before CHECK 31.
At `sim_pc/main.c:4434`, the supposed Auto activity only changes VOC, advances a
revision and conditionally retries an already-paused channel. CHECK 31 does not
enable Auto or assert any qualification, pending, timeout, recovery or re-arm.

Thus the new real-pointer invalid draft/discard checks are valid, but the
required combined Auto/recovery stress coverage remains absent. Existing heap
numbers cannot certify that omitted path. The independent one-shot combined
probe passes; this is a maintained test defect, **not a reproduced heap failure**.

Fix: add explicit, asserted real Auto transitions/recovery to the maintained
stress sequence, while preserving invalid drafts, immediate/settled sampling,
the existing >4096 reserve and drift limits. Count exercised states so a skipped
Auto branch cannot pass. Keep current UI/heap settings unless a valid test
actually exposes a failure.

### R2 — P2: reported complete boundary/touch coverage exceeds executed coverage

- `sim_pc/main.c:625-719`: wrap test says all three presets/both directions;
  Fan has ON/OFF, Purifier and Humidifier have ON only. Some first/second
  evaluations discard their results. Ordinary non-wrap rule tests do not
  replace the already-requested wrap matrix.
- CHECK 1 calls the real link predicate, which is useful, but does not drive
  the complete stale-wrap path through `ui_tick`, masked UI, action refusal
  and recorded outbound TX. Normal-time CHECK 14/29 is separate coverage.
- `sim_pc/main.c:3615`: each page/polarity/recovery combination chooses either
  Switch OR Retry by parity. It does not execute both actions in each combination;
  the Switch branch checks mode/latch but not exactly one command/action.
  Retained Retry and 68x44 targets themselves are accepted.

Fix: finish the finite existing matrix, assert intermediate states and action
counts, and report actual coverage accurately. No change to production elapsed
arithmetic, Auto policy, touch sizes or layout is requested absent regression.

### R3 — P2: export acceptance still omits requested collision/failure proofs

Evidence: `tools/verify_production.ps1:170-202` runs two default exports but
only counts two ZIP names. It does not inspect both archive contents and returned
paths. There is no same-explicit-destination concurrent test or controlled
failure/timeout sentinel test in the submitted verifier. The worker loop kills
only the timed-out parent; a first-worker failure can leave the other worker
and child processes uncollected. This is a verifier failure-path risk, not a
reproduction of the now-fixed ZIP overwrite.

Fix: verify each concurrent artifact's required membership/content and exact
returned path, add the requested same-explicit race and bounded failure/timeout
cases in disposable fixtures, and always reap all task-owned process trees.
Preserve unrelated sentinels and failure diagnostics; canonicalize/validate
task-owned cleanup targets. Do not test against user exports.

Primary atomic Move and secondary non-overwriting Copy are accepted.
Existing extraction into the requested smart_hub_ui folder is intentional,
not a new demand for non-overwriting extracted source files.

### R4 — P3: completion/evidence claims need correction

The author report line 12 says all requirements are fully resolved; R1–R3
demonstrate why that is premature. Its so-called literal transcripts abbreviate
paths with `...` and there are no separate complete raw logs. Re-running an old
review probe is author regression evidence, not independent reviewer acceptance.
`sim_pc/main.c:4566` prints `< 50 ns` without measuring elapsed CPU time.

Fix: retain raw final command/stdout/stderr/exit evidence without reconstructed
or abbreviated content; label summaries/excerpts as such. Remove or explicitly
mark unmeasured timing estimates (no new benchmarking project). Supersede current
all-complete claims and update indexes to the actual disposition. Preserve
historical reports/captures; do not run old hardcoded probes over their artifacts.

## Closed and preserved

Production modular rollover repair, secondary ZIP protection, GUID staging and
malformed inherited TS fallback; pre-init restore through recorded first send;
executed-loop TX fault injections; all-three-sensor recovery/duplicate checks;
real-pointer invalid draft creation; recovered Retry and larger switch targets;
shared-style memory improvements; current accepted visual composition.

The fault named OMITTED_PERIODIC delays a send to 2000 ms and is rejected by the
actual loop validator. Do not claim it tests every possible no-send condition,
but it is valid evidence for a missed periodic deadline.

## Limits / handoff

Physical LCD contrast/touch, MCU memory map, QSPI dirty-region timing, radio/RS485,
and actual relay behavior were not verified by these host runs. They are target
acceptance items, not a reason to redesign the simulator or add drivers here.

Only review evidence and documentation were added/updated. No production fix,
commit or push. [One complete corrective prompt](../../prompts/CURRENT.md)
contains every remaining item; no staged owner-mediated fix rounds.
