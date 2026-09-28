> **ARCHIVED — historical context only.** Superseded on 2026-09-25 by the LoRa v2 / Trends handoff in [CURRENT](../CURRENT.md). Its work was completed and REVIEWED.

# CURRENT — final verification-claim closure

Date: 2026-09-24
Status: **REVIEWED — host software accepted 2026-09-25**
Scope: One bounded follow-up to the [software-completion correction delivery](../../reviews/2026-09-24-software-completion/REPORT.md) and [independent review](../../reviews/2026-09-24-software-completion-correction-independent/REVIEW.md). The submitted correction prompt is [archived](2026-09-24-06-fix-verification-correction-submission.md).

The assignment text below is retained for traceability; its work is complete. See the final disposition at the end of this file.

## Read and preserve

Read `AGENTS.md`, `docs/prompts/README.md`, this CURRENT, the independent review, and only the affected code/report sections. Preserve the dirty worktree and owner-accepted native Quiet Architecture UI. Do not restart the full-project audit. Codex independently reran asset/build/default 40/40/40 KiB 40/40/export/production gates and directly confirmed `--shots NUL` exits 1; 28 captures match their previous native versions. These accepted results are not requests to redesign or refactor.

## Two required corrections, one delivery

1. **N1 — negative capture test catches the wrong failures.** In `tools/run_regression.ps1`, `Invoke-SimulatorWithTimeout` writes stderr via `Write-Error` while `$ErrorActionPreference='Stop'`. Thus `--shots NUL` can be counted as expected failure on stderr or timeout even if its exit code were 0. Make the negative control assert the simulator exits with the **specific expected nonzero code (1)** and expected invalid-directory diagnostic, and fail on timeout, start failure, unrelated stderr, or a different code. Keep positive 28-shot exact-manifest/size checks. Add a durable check that would fail if the simulator returned 0 while printing a FATAL message. Avoid broad catch-as-pass.
2. **N2 — verifier report overclaims what CIM denial proves.** Fixture 6c checks owned root termination and unrelated-process protection; with CIM denied before child discovery, `Invoke-ProcessWithTimeout` cannot enumerate an untracked child of `cmd.exe`. The report says “zero survivors,” which its fixture does not prove. Either add a safe controlled check covering known descendants under denial and accurately scope the claim, or explicitly document that unobserved descendants may outlive the root until the OS/command exits. Keep fail-closed nonzero status and PID+StartTime ownership guards; do not use raw PID killing, taskkill wildcards, or broad process scans to paper over this OS limitation. The accurate caveat is acceptable; no Job Object rewrite is required.

## Acceptance and limits

Gemini owns both corrections and its own affected-diff self-review. Keep this to `tools/run_regression.ps1`, verifier fixture only if needed, and report/docs accuracy; do not touch production UI pixels, text, styles, packet schema, RAM profile or other features. After the last edit run negative/positive capture checks, default and 40 KiB 40/40 suites, asset integrity, MCU export and production verifier; record exact exit codes and the upstream CMP0177 dev warning separately from compiler warnings. Reuse/update the existing dated delivery folder rather than creating an unnecessary second large evidence pack. Update `PLAN.md`, `DEV_LOG.md`, docs index and this CURRENT to `IMPLEMENTED — awaiting review` only when done. No commit/push/reset/clean/stash/deleting user work. Target hardware map and physical LCD/LoRa behavior remain UNVERIFIED. Return one concise complete result; Codex then reviews this final delta once.

## Final disposition

The [2026-09-25 independent review](../../reviews/2026-09-25-software-completion-final/REVIEW.md) accepts the host-verified software and closes N1/N2. No active fix request remains. Physical MCU/LCD/touch/LoRa validation is still owner/target-only work.
