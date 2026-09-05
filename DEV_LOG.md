# Smart Hub development log

Material changes are recorded newest first. The complete motor-control history is
preserved on branch `ui/motor-control`.

---

## 2026-09-05

- **Repository · Smart Hub baseline** — Created `ui/smart-hub` from the archived
  motor-control branch, removed all motor-specific implementation/design material,
  retained only reusable LVGL simulator and MCU utilities, added a neutral buildable
  placeholder, and documented the supplied LCD/relay schematics plus open requirements.
  Clean build, strict warnings, smoke render, RGB565 shot-size check, and MCU export
  manifest passed; the placeholder uses 25% of the constrained 42 KiB LVGL heap
  (32,304 bytes free, 0% fragmentation).
  File: repository-wide cleanup, `ui.*`, `sim_pc/*`, `tools/*`, `docs/*`, `PLAN.md`.
