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

## Active Version

[CURRENT.md](CURRENT.md) — 2026-09-16: addresses X1–X4 following review of W1–W4:
non-overwriting export, touch area >= 44 px for switch, boot/TX/recovery verification, and
stress testing with real invalid drafts. Retains improved UI. READY FOR HANDOFF;
review/handoff turn changes documentation and diagnostics only without editing production UI.

## Handoff History Following Directory Standardization

| Archive Date | Superseded Version | Rationale |
|---|---|---|
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
