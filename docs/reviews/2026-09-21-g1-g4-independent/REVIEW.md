# Independent review — G1–G4 delivery

Date: 2026-09-21
Disposition: **Functional repairs accepted; RAM qualification and durable-test completion remain.**
No new reproducible production UI defect found in this review. This is not an
approval of the MCU's reported 99.9% RAM utilization. One narrowly scoped
[CURRENT](../../prompts/CURRENT.md) covers the remaining work; no redesign.

## Scope and method

Reviewed the submitted changes against prior reviewer source hashes, the active
handoff, native captures, tests and the author's completion claims. Of the
production/test files recorded by the prior reviewer, changes are `ui.c`,
`ui_theme.c`, `ui_theme.h`, `sim_pc/main.c`; Auto, assets, fonts and MCU profile
hashes are unchanged. The wider dirty worktree includes earlier accepted work
and is not equivalent to this submission's diff.

Used ui-ux-pro-max to check readability, missing-data states and interaction,
not to reopen the owner's accepted appearance. No production code changed.
Reviewer additions are diagnostic files and documentation only.

## Accepted

- G1: link layout now measures updated text. Independent probe: Disconnected
  x655..777; Connected after recovery x678..777. Narrow/wide uptime, 100h,
  1000h and maximum uptime retain >=14px separator clearance.
- G2: all 34 descriptors use pinned LVGL `LV_STYLE_CONST_INIT`, with matching
  const declarations. Default theme object: text2944/data0/bss0. Assert-enabled
  complete, consistently configured LVGL/UI build and production smoke PASS.
  Sentinel aabbccdd and RGB565 canvas ef7c/swap7cef probes PASS.
- The `ui_theme.h` compatibility cast for LVGL8.4's non-const add/remove-style
  API is not itself a runtime bug: inspected pinned implementations do not
  modify attached shared const descriptors. Do not rewrite working styles just
  to obey the previous prompt's overbroad prohibition on this compatibility cast.
- Missing data: Home Temp/RH have dash/no footer; Home and Devices have no
  Unknown. Retry remains available. Trends messages are retained.
- Trends numeric/unit ownership, rebuilt range, isolated markers; no-op Save,
  callback counts, Auto hold; idempotent style attachment and identical-frame
  redraw checks pass. Check34 now contains real marker pixels/no-bridge tests,
  callback assertions, link bounds and missing-data assertions.
- All twelve identical-frame benchmarks (three pages and three Auto editors,
  valid/unavailable) report 0 flushes and 0 pixels in their controlled fixtures.

## Remaining findings — one consolidated handoff

### E1 — P2: RAM qualification incomplete and ledger claims inaccurate

References: `sim_pc/main.c:4545`, `sim_pc/main.c:4836`, author's
`../2026-09-21-const-style-delivery/REVIEW.md`, live `docs/MCU_BASELINE.md`.

CHECK31's immediate/settled samples do not observe allocation/render
intermediates; there is no measured largest request or allocation-failure
diagnostic supporting the requested candidate qualification. This is a missing
proof, not a demonstrated allocation failure. In particular, 40KiB CHECK8 Home
has 5080B total free but only 3280B contiguous; a 4KiB total-free threshold alone
does not establish suitability for the next contiguous allocation. Do not claim
the candidate fails merely because largest-block size is below total-free floor.

Fresh results (bytes; tests have different histories and must not be conflated):

| Measurement | Default 42KiB | Candidate 40KiB |
|---|---:|---:|
| Pool reservation | 43008 | 40960 |
| One 10-line draw buffer | 16000 | 16000 |
| Pool + buffer subtotal only | 59008 | 56960 |
| CHECK8 Home free / largest | 7144 / 5344 | 5080 / 3280 |
| CHECK31 minimum settled free / largest | 7592 / 6104 | 5480 / 4000 |
| CHECK31 drift cycles10→50 | 0 | +72 |
| Full regression | 34/34 PASS | 34/34 PASS |

The author's CHECK8 row uses values from CHECK31 cycle1 (7608/6512 and
5560/4456), not CHECK8. Its +72B drift is correctly within CHECK31's **256B**
limit, not within the reported 64B limit (64 belongs to CHECK9).

The ledger's '~250B additional App/Link globals' is not a measured inventory of
remaining UI state. Fresh production smoke object (test hooks disabled, host
x64, style assertions enabled) has ui.c data576+bss4064 = **4640B**, before
LVGL/display/application allocations outside that file. This is host object
section accounting, NOT target linker occupancy. The normal test-hook object
is larger again. Other static UI state cannot be omitted from a whole-UI total.
Heap usage is inside the fixed pool and must not be counted twice.

Default host theme writable storage decreases 544B→0 relative to last review;
40KiB would save a further fixed 2048B if qualified. These are separate effects.
Neither establishes the Cortex-M build's actual gain without a map. Keep42KiB
default and10lines.8lines saves3200B arithmetically but target timing is untested.

Related documentation corrections in the same task: author's UI_HEX example is
not the implemented `.full` initializer; live MCU baseline still describes a
1024B hero-gradient cache, while ui_theme_init sets cache size0. Correct docs,
not accepted source/palette. Historical submitted reports can remain with an
explicit erratum pointing to measured evidence.

### E2 — P2: fault-matrix regression coverage still short of requested scope

Reference: `sim_pc/main.c:5812` through the end of34i. There are eight single
fault/offline cases plus clean recovery, not nine simultaneous fault
combinations. They check Home and the relevant Auto editor(s), not Trends
Current/selectors across the matrix. Coexisting faults, partial recovery,
preservation of unaffected live/history values and positive assertions on
unaffected Auto editors are not comprehensively guarded by this block.

The reviewer's separate observational probe shows correct Home/Trends/Auto
values for its cases; no production defect reproduced. However its exit0 is
not a durable pass/fail test. Add the previously requested missing assertions
to standard regression rather than calling the existing matrix complete.
Existing equal-number RH and no-op Save tests are accepted; fold remaining
metric/unit and maximum-name boundary cases into the same bounded completion.

## Fresh verification

- `tools/build_sim.bat`: PASS (cached source; no dependency updates).
- `python tools/generate_assets.py --check`: PASS,25 masters/68 runtime icons.
- `tools/run_regression.bat -SkipBuild`:34/34 PASS,27 scenario captures.
- `tools/export_mcu.bat --verify`: PASS.
- `tools/verify_production.bat`: ALL phases PASS, isolated exported build,
  production smoke and deliberate exit1 negative control included.
- Rebuilt root build-40k target and reran `sim_pc.exe --regression`:34/34 PASS.
  Its cached compiler is WinLibs; default/assert build uses w64devkit. These
  are independent host runs, not a controlled same-toolchain target comparison.
- Full LV_USE_ASSERT_STYLE=1 production smoke: PASS. Swap probe tests constant
  data, not an end-to-end swapped display driver/asset rendering configuration.
- Fresh diagnostic probe logs sensor/link/save/chart/idle behavior. It is an
  observational supplement, not a replacement for assertive regression.

Native pixel comparison: all37 paired frames except `home_offline` have zero
body pixel differences below y56. Offline has1054 changed body pixels in the
Ventilation `Auto paused` label placement (old centered vs current left-aligned);
it is not a palette/icon change. Do not repeat the author's blanket assertion
that every body is pixel-identical. Header differences are expected G1 repairs.
Reviewed offline Home/Devices images; required simplified copy is correct.

## Evidence and reproduction

- [REGRESSION.txt](REGRESSION.txt), [REGRESSION_40K.txt](REGRESSION_40K.txt)
- [PROBE.txt](PROBE.txt), [probe.c](probe.c)
- [CONFIG_AND_SECTIONS.txt](CONFIG_AND_SECTIONS.txt), [style_config_probe.c](style_config_probe.c)
- [VISUAL_DIFF.txt](VISUAL_DIFF.txt), [captures](captures/)
- [SOURCE_IDENTITY.json](SOURCE_IDENTITY.json)

Assert build used `cmake -S sim_pc -B <this-review>/build-assert -G Ninja`,
Release, `C:/Toolchains/w64devkit/bin/gcc.exe`,
`-DFETCHCONTENT_SOURCE_DIR_LVGL=<repo>/sim_pc/build/_deps/lvgl-src`,
`-DFETCHCONTENT_FULLY_DISCONNECTED=ON`, `-DCMAKE_C_FLAGS=-DLV_USE_ASSERT_STYLE=1`;
then `cmake --build <build> --target smoke_production -j8` and run executable.
No configuration changed in the default build.

## Limitations / decision

Functional repairs accepted. Finish E1/E2 in ONE verification/documentation
delivery, preserving working production code unless a new test reproduces an
actual bug. No need to regenerate icons, redesign screens or alter Auto policy.
Actual target map/ELF, lv_conf, memory banks, stacks, DMA and display timing were
not supplied; MCU99.9% RAM resolution and target performance remain unverified.
Do not turn that external limitation into repeated speculative UI fix rounds.
