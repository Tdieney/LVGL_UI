# Prompt Storage Conventions — Smart Hub UI

This is the single entry point for any agent writing implementation or fix prompts in this repository.
Read this document before creating or updating prompts; do not choose other directories.

## Fixed Paths

| Content | Storage Location |
|---|---|
| Active, complete, self-contained handoff prompt | [CURRENT.md](CURRENT.md) |
| File naming rules and historical catalog | [README.md](README.md) — this document |
| Superseded prompts or discarded proposals | [archive/](archive/) |
| Review results, execution reports, tests, evidence captures | `docs/reviews/<round-or-date>/` relative to repo root |
| Long-term product/UI decisions | [UI_DESIGN_BRIEF.md](../UI_DESIGN_BRIEF.md) |
| Prototypes and design assets | `docs/prototype/`, `docs/drafts/` relative to repo root |

There is only **one active prompt: `docs/prompts/CURRENT.md`**. Use this path
for handoffs between agents. Do not create additional `FIX_PROMPT.md`, `HANDOFF.md`,
or `PROMPT_NEW.md` in the repo root, `docs/`, `docs/reviews/`, or draft directories.
If image/design prompts need to be stored, place them in `docs/prompts/archive/` with
a `design` tag; images and assets remain in the design directory and do not move with prompts.

## When an Agent Writes or Updates Prompts

**Automatic review handoff (Owner instruction, 2026-09-15):** Whenever a review
finds remaining defects or issues to address, prepare/update CURRENT immediately in the same turn
and provide its link along with the review result; do not wait for the user to ask. The prompt must
cover remaining open items and acceptance tests. If an essential decision is missing, provide a
DRAFT specifying the blocked items/questions without deciding on behalf of the owner.
If a review has no remaining issues, do not create an empty fix prompt. This convention authorizes
prompt/document updates only, not code modification, committing, or pushing during a review.

1. Read `AGENTS.md`, this document, `CURRENT.md`, Git status, and the owner's latest decisions.
   Do not treat requirements in archive as active requirements.
2. Determine whether this is a draft in progress or a new handoff. Before replacing the scope/behavior
   of a previously handed-off version, preserve the old content in
   `archive/YYYY-MM-DD-NN-<kind>-<short-slug>.md`, where kind is `fix`, `implement`, `proposal`,
   or `design`; increment NN to avoid colliding with same-day archives.
   Fixing typos in an un-handed-off draft does not require creating an archive.
3. Add banner `ARCHIVED — historical context only` with link `../CURRENT.md` to the saved copy.
   Preserve historical content, updating relative links as needed.
   Never overwrite existing archives, and do not delete evidence or old prompts to make room.
4. Update **`CURRENT.md` directly**, without creating a new active filename.
   The prompt must provide complete context so the receiving agent does not need to piece together old prompts.
5. Record date and status at the top of the file (`DRAFT — needs decision`, `READY FOR HANDOFF`,
   `IMPLEMENTED — awaiting review`, or `REVIEWED`), along with scope and which decisions were made by
   the user vs proposed by the technical lead. `READY` means the prompt is ready for handoff, not that
   code has been verified.
6. Minimum required content: prerequisite reading, objectives, out of scope, architectural constraints,
   behaviors/defects to fix, data/validation, error scenarios, acceptance tests, verification commands,
   deliverables/artifacts, and untested target limitations.
7. When changing undecided behavior, highlight clearly and consult the owner; do not falsely state that
   the user approved every detail. Do not write code, commit, or push when the current task is prompt-writing only.
8. Update the archive catalog here, `PLAN.md`, `DEV_LOG.md`; record UI decisions in `docs/UI_DESIGN_BRIEF.md`.
   Introductory documents must link to CURRENT, while historical citations link to the corresponding archive.
9. Verify relative links, ensure target files exist, and avoid duplicate active prompts.
   Conclude with a link to CURRENT and a summary of changes, without pasting entire long prompts into chat.
   Execution/review reports must not overwrite prompts.

Prompts do not grant additional authority: receiving agents must still adhere to the user's active instructions
and `AGENTS.md`; archived prompts must not be executed merely because they are discovered.

## Consolidated delivery rule (Owner, 2026-09-16)

Review the full submitted change and affected end-to-end paths, not only named
patch locations. Finish the review before issuing one consolidated findings list
and one CURRENT covering every known remaining defect and required proof gap.
Do not split fixes into separate phases/prompts or ask the owner to relay each
group for another review. Issue IDs are traceability labels, not staged handoffs.

Implementation handoffs require a complete self-review and final regression pass
after the last relevant edit; regressions introduced by the fix stay in the same
run. Keep accepted behavior/visuals and acceptance thresholds stable. Separate
verified defects, missing proof, optional non-blocking polish and target-only
limitations. Genuine blockers must be explicit and collected together. No claim
of guaranteed bug-free code or completed unexecuted tests.

## Current handoff

[CURRENT.md](CURRENT.md) — REVIEWED 2026-09-25, accepted on the host in the [technical-lead review](../reviews/2026-09-25-lora-v2-trends-review/REVIEW.md). No active fix request remains. Covers the LoRa v2 simulator/test update and the owner-decided Trends refinements: restored selector indicator, lower title/chart, fixed 15-minute X window and round-number Y ticks. Delivery package in [docs/reviews/2026-09-25-lora-v2-trends-delivery/REPORT.md](../reviews/2026-09-25-lora-v2-trends-delivery/REPORT.md).

## Handoff History Following Directory Standardization

Design exploration (not an implementation handoff):
[Quiet Architecture Settings/Auto prompts](archive/2026-09-17-06-design-quiet-settings-auto.md);
[Quiet Architecture Trends and Devices prompts](archive/2026-09-17-05-design-quiet-companions.md);
[Quiet Architecture freeform prompt](archive/2026-09-17-04-design-quiet-architecture.md);
[2026-09-17 UI UX Pro Max Dashboard image prompt](archive/2026-09-17-03-design-pro-max-dashboard.md).
The owner subsequently accepted the complete design package and authorized
implementation, then rejected the submitted native UI's fidelity. CURRENT is
now the corrective implementation handoff; the images remain visual authority.

| Archive Date | Superseded Version | Rationale |
|---|---|---|
| 2026-09-25 | [Final verification-claim closure](archive/2026-09-25-01-fix-final-verification-claim-closure.md) | Completed and REVIEWED. Owner code changes (LoRa v2, Trends) broke the simulator build, and the owner then requested Trends refinements, so a new handoff replaces it |
| 2026-09-24 | [Submitted verification correction](archive/2026-09-24-06-fix-verification-correction-submission.md) | All host gates pass; independent review found negative test catches stderr/timeout as pass and CIM-denial report overclaims zero survivors |
| 2026-09-24 | [Submitted software-completion handoff](archive/2026-09-24-05-implement-software-completion-submission.md) | Host gates and native appearance accepted provisionally; independent review found fail-open capture/test paths, unproved live CIM-denial cleanup and overclaimed evidence |
| 2026-09-24 | [Submitted Trends v2 handoff](archive/2026-09-24-04-fix-trends-final-v2-submission.md) | Final worker assignment covers complete UI software closure before one independent review; v2 claims remain submitted evidence, not acceptance |
| 2026-09-24 | [Submitted final Trends delivery](archive/2026-09-24-03-fix-trends-final-delivery.md) | 40/40 passes, but Stage 1 flushes 0 pixels in normal suite; forced-refresh benchmark and incomplete verifier/E2 evidence do not close owner-reported lag |
| 2026-09-24 | [Submitted small-improvement delivery](archive/2026-09-24-02-fix-trends-evidence-delivery.md) | History fixes accepted; ~3% host flush reduction does not close LCD lag, verifier fallback and E1/E2 proof remain open |
| 2026-09-24 | [Submitted Trends responsiveness delivery](archive/2026-09-24-01-fix-trends-responsiveness-delivery.md) | Host display traffic unchanged despite optimization claim; verifier cleanup unsafe on PID reuse, 45-day time rollover and E1/E2 evidence still open |
| 2026-09-23 | [Live Trends/Auto implementation delivery](archive/2026-09-23-02-fix-trends-auto-delivery.md) | Original live-path fixes accepted provisionally; independent review found switch cost and three time semantics defects, with E1/E2 proof still open |
| 2026-09-23 | [RAM/fault evidence handoff](archive/2026-09-23-01-fix-ram-evidence.md) | Owner reports live Trends empty and Auto failing to switch; new handoff fixes live paths and carries open E1/E2 evidence forward |
| 2026-09-21 | [Const-style verification submission](archive/2026-09-21-11-fix-const-style-verification.md) | Header/style repairs accepted; remaining transient RAM proof, inaccurate ledger and limited fault-matrix coverage consolidated as E1/E2; no redesign |
| 2026-09-21 | [Runtime follow-up submission](archive/2026-09-21-10-fix-runtime-followup.md) | Copy/uptime repairs accepted; stale link width clips header. Hand-written styles omit assert sentinel and byte-order compatibility; one G1–G4 handoff closes defects and remaining RAM/test proof |
| 2026-09-21 | [Runtime/RAM implementation handoff](archive/2026-09-21-09-fix-runtime-ram.md) | R1–R7 independently repaired; R8 same-length widening still overlaps. Consolidate owner copy cleanup, durable tests and remaining RAM proof/work |
| 2026-09-21 | [Accepted visual calibration submission](archive/2026-09-21-08-fix-accepted-visual-calibration.md) | Owner accepts appearance; superseded by one runtime, RAM and performance correction handoff |
| 2026-09-21 | [Icon recovery implementation](archive/2026-09-21-01-fix-icon-recovery.md) | Icon appearance recovered from canonical masters and verified; superseded by Devices tab review handoff |
| 2026-09-20 | [Submitted icon-appearance reconstruction](archive/2026-09-20-03-fix-submitted-icon-appearance.md) | Original design provenance confirms raster-generated icons; actual A4 masks still differ. Replace semantic SVG guessing with measured source-alpha extraction and a real single-source pipeline |
| 2026-09-20 | [Appearance handoff before clarification](archive/2026-09-20-02-fix-appearance-before-clarification.md) | Preserves submitted IMPLEMENTED status; owner clarifies shape/detail mismatch rather than size and simulator-only color comparison |
| 2026-09-20 | [Submitted final reconstruction](archive/2026-09-20-01-fix-final-reconstruction-submission.md) | Quantified remaining icon/color differences; repaired boundary flows accepted, but Trends contrast/gap/point defects and proof gaps consolidated into C1–C6 |
| 2026-09-18 | [Submitted Quiet reconstruction](archive/2026-09-18-03-fix-quiet-reconstruction-submission.md) | Composition and several repairs accepted; native probes expose overlaps, false coverage and remaining fidelity/proof gaps despite passing suites |
| 2026-09-18 | [Submitted Quiet fidelity handoff](archive/2026-09-18-02-fix-quiet-fidelity-submission.md) | Owner rejects fidelity; R2 finds clipped/overlapped data, changed shell, old icon shapes and incomplete proof claims |
| 2026-09-18 | [S1–S3 verification handoff](archive/2026-09-18-01-fix-verification-assertions.md) | Superseded old visual freeze; carried bounded proof gaps into one Quiet Architecture fidelity handoff |
| 2026-09-17 | [R1–R4 verification submission](archive/2026-09-17-02-fix-verification-completion.md) | MCU integration accepted; remaining assertions and inaccurate completion claims consolidated as S1–S3 |
| 2026-09-17 | [Consolidated Y1–Y3 submission](archive/2026-09-17-01-fix-consolidated-y1-y3.md) | Production fixes accepted; independent review found remaining coverage and evidence gaps, not a new confirmed runtime bug |
| 2026-09-16 | [Prior Y1–Y3 handoff](archive/2026-09-16-03-fix-auto-y1-y3.md) | Owner requires one complete fix delivery, full-flow review and fixed consolidated acceptance checklist |
| 2026-09-16 | [Fix Auto X1–X4](archive/2026-09-16-02-fix-auto-x1-x4.md) | Touch/heap and primary ZIP protection accepted; rollover regression, secondary ZIP overwrite and evidence gaps remain |
| 2026-09-16 | [Fix Auto W1–W4](archive/2026-09-16-01-fix-auto-w1-w4.md) | Retry and heap improved; remaining X1–X4 on export, touch, and test evidence |
| 2026-09-15 | [Fix Auto V1–V4](archive/2026-09-15-04-fix-auto-v1-v4.md) | Review confirmed UI improvements; transitioned to W1–W4 fixes on re-arm, heap, test, export |
| 2026-09-15 | [Fix Auto layout L1–L6](archive/2026-09-15-03-fix-auto-layout-l1-l6.md) | Preserved handed-off prompt; replaced with V1–V4 and missing verification from result review |
| 2026-09-15 | [Fix Auto R1–R10](archive/2026-09-15-02-fix-auto-r1-r10.md) | Re-reviewed implementation; replaced with spacing adjustments and addressing remaining L1–L6 |
| 2026-09-15 | [Implement LCD Auto v1](archive/2026-09-15-01-implement-lcd-auto-v1.md) | Preserved prior prompt; replaced with R1–R10 fix requirements following independent review |

## Consolidated Versions from 2026-09-14

Historical content is preserved. The legacy paths below have been relocated and are no longer prompt storage locations.
Future entries follow the YYYY-MM-DD-NN naming rule above.

| Archive Copy | Former Path (relative to repo root) |
|---|---|
| [Study 12 implementation](archive/legacy-study12-implementation.md) | `docs/LVGL_IMPLEMENTATION_PROMPT.md` |
| [Fix round 1](archive/legacy-fix-r01.md) | `docs/reviews/implementation-r1/FIX_PROMPT.md` |
| [Fix round 2](archive/legacy-fix-r02.md) | `docs/reviews/implementation-r2/FIX_PROMPT.md` |
| [Fix round 3](archive/legacy-fix-r03.md) | `docs/reviews/implementation-r3/FIX_PROMPT.md` |
| [Fix round 4](archive/legacy-fix-r04.md) | `docs/reviews/implementation-r4/FIX_PROMPT.md` |
| [Fix round 5](archive/legacy-fix-r05.md) | `docs/reviews/implementation-r5/FIX_PROMPT.md` |
| [Fix round 6](archive/legacy-fix-r06.md) | `docs/reviews/implementation-r6/FIX_PROMPT.md` |
| [Fix round 7](archive/legacy-fix-r07.md) | `docs/reviews/implementation-r7/FIX_PROMPT.md` |
| [Fix round 8](archive/legacy-fix-r08.md) | `docs/reviews/implementation-r8/FIX_PROMPT.md` |
| [Fix round 9](archive/legacy-fix-r09.md) | `docs/reviews/implementation-r9/FIX_PROMPT.md` |
| [Fixed-threshold Auto proposal — superseded](archive/2026-09-14-fixed-threshold-auto-draft.md) | `docs/FIX_AND_AUTO_HANDOFF.md` |
| [Home v1 image prompt](archive/legacy-home-v1-design.md) | `docs/drafts/home-v1-prompt.md` |
| [Home v2 image edits](archive/legacy-home-v2-design.md) | `docs/drafts/home-v2-prompt.md` |

Paths in archived prompt contents should be interpreted relative to the original location noted in the banner if they are historical notes / bare paths. Markdown links must remain functional after relocation. Old source code line numbers are historical evidence at the time of the review, not current code line numbers.
