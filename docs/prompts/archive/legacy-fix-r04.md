# Fix prompt · Smart Hub UI review round 4

> ARCHIVED — historical context only, not the current implementation request.
> Original location: `docs/reviews/implementation-r4/FIX_PROMPT.md`.
> Use [CURRENT.md](../CURRENT.md) for the active handoff and
> [prompt rules](../README.md) before creating or updating any prompt.

Fourth independent review of the LVGL implementation on branch `ui/smart-hub`.

Round 3 closed all sixteen items and I verified every one of them: the splash seam
is gone (screen and logo area both sample `(239,239,247)`), retry derives its
target correctly, the acknowledgement no longer mutates `s_snapshot`, ten identical
snapshots now produce **0 flushes and 0 pixels**, `lv_obj_update_layout` is down to
two guarded call sites, the full-screen invalidate is gone, the chart handler fires
once, `ui.h` is an 11-function public API with the test surface moved to
`sim_pc/ui_test_api.*`, all forty cached pointers are reset on navigation, and the
scratch artifacts are cleaned up. `export_mcu --verify` passes and 12/12 checks pass.

What remains is one class of defect the earlier rounds never reached: **UI state
that waits on an external party is not bounded**. The owner hit it directly — after
flipping a switch in the interactive simulator the tile stays on `Sending…` forever.

Copy everything below the line to the coding AI.

---

You are continuing work on the LVGL v8.4 Smart Hub UI in this repository
(`C:\Users\vanth\Documents\MyDocument\Embedded\LVGL_UI`, branch `ui/smart-hub`).
Read `AGENTS.md`, `docs/LVGL_IMPLEMENTATION_PROMPT.md` and
`docs/TECHNICAL_LEAD_REVIEW.md` first. Do not reset, clean, stash, push or change
branches.

**Do not redesign anything and do not undo round 3.** The study 12 palette, the
dirty-check update path, the test-API split, the pointer reset and the measured
performance work are all correct — preserve them. Everything below is a defect fix
or a cleanup.

Items 1 and 2 are two faces of the same design gap, so solve them with one coherent
mechanism rather than two ad-hoc patches: **every piece of UI state that is waiting
on the application must have a bound after which the UI falls back to what the
application actually reports.**

## P0 — unbounded waiting states

**1. A pending command never times out; a lost acknowledgement freezes the tile permanently.**

`toggle_device()` sets `cmd->phase = UI_CMD_PHASE_PENDING` ([ui.c:1437](../../../ui.c#L1437))
and `on_dialog_save_click()` does the same ([ui.c:1510](../../../ui.c#L1510)). The
only code that leaves `PENDING` is `ui_handle_command_result()`, which requires a
matching `req_id` ([ui.c:1849](../../../ui.c#L1849) / [ui.c:1867](../../../ui.c#L1867)).
There is no timeout anywhere: `grep -n "timeout" ui.c` returns nothing, and
`ui_tick()` only advances the uptime counter.

While `PENDING`, the device tile renders `Sending…`, **disables** the switch and
**hides** the Retry affordance, and `toggle_device()` early-returns on
`cmd->phase == UI_CMD_PHASE_PENDING`. A new application snapshot does not clear it
either — `ui_update_snapshot()` never touches `cmd->phase`.

So if the acknowledgement never arrives, that device is frozen for the rest of the
session with no user-reachable way out. On a LoRa link, a dropped ack is a normal
event, not an exotic failure. This is the single worst remaining defect: the control
the operator needs most is the one that becomes permanently dead.

Implement a bounded pending state:

- Record the tick when the command was issued (`lv_tick_get()`), and sweep the four
  devices from `ui_tick()`. Handle `lv_tick_get()` wraparound with unsigned
  subtraction, the way `ui_tick()` already does for the uptime.
- On expiry, move to `UI_CMD_PHASE_ERROR` — the state that already shows `Unknown`
  plus a working `Retry` — and announce the timeout once, not repeatedly.
- Pick the timeout from the application's perspective and say why you chose it; the
  transport round-trip is not specified yet, so make it a single named constant that
  firmware can retune, and document it in `docs/MCU_BASELINE.md`.
- A late acknowledgement arriving after expiry must be ignored: it no longer matches
  a `PENDING` request, which the existing `req_id` correlation already handles —
  confirm that with a test rather than assuming it.
- The sweep must stay bounded and must not invalidate anything when no command is in
  flight. The "ten identical snapshots produce 0 flushes" result must still hold, and
  an idle Home page must still produce 0 flushes per second while a timer is armed.

**2. The confirmed-state overlay never expires when the application disagrees.**

Round 3 added the overlay to stop the tile flipping back when a snapshot arrives that
has not caught up yet. That part works. The reconciliation, though, clears it only on
**agreement** ([ui.c:1798-1806](../../../ui.c#L1798)):

```c
if (cmd->has_confirmed_state) {
    if (d->valid && d->on == cmd->confirmed_on && d->mode == cmd->confirmed_mode) {
        cmd->has_confirmed_state = false;
    }
}
```

and the render path always prefers the overlay ([ui.c:905](../../../ui.c#L905)):

```c
bool target_on = (cmd->phase == UI_CMD_PHASE_PENDING) ? cmd->requested_on :
                 (cmd->has_confirmed_state ? cmd->confirmed_on : d->on);
```

Failure: the user turns a device on, the node confirms, the overlay records `on`.
The relay then trips or someone switches it off at the node, and the application
reports `on = false, valid = true`. The clear condition is not met, so the overlay
survives — and it keeps winning in the render path on **every** subsequent snapshot.
The UI shows `On` indefinitely while the application says `Off`. Only a manual toggle
clears it ([ui.c:1436](../../../ui.c#L1436)). `has_confirmed_preset` is stuck the same
way, so the name and icon freeze too.

For a relay UI this inverts the contract: "a reported relay state does not prove an
appliance is powered" — here the UI contradicts the only party that actually knows.

Bound the overlay the same way as item 1: keep "clear when the application agrees",
and add an expiry after which the application's value wins regardless. Do not simply
clear it on the next snapshot — that reintroduces the flip-flop round 3 fixed. Add a
check that acknowledges a command, then pushes a *contradicting* snapshot repeatedly,
and asserts the tile follows the application once the bound has elapsed.

## P1 — the interactive simulator is missing its fake node

**3. `run_interactive()` never registers the command callbacks and never plays the splash.**

[sim_pc/main.c:1695](../../../sim_pc/main.c#L1695) calls `lvgl_setup()`, `ui_init()`,
pushes a baseline snapshot and history, then enters the message loop. It does **not**
call `ui_set_relay_command_cb()` / `ui_set_settings_command_cb()`, and it does **not**
call `ui_replay_splash()`. Both are done in `run_scenario_shots()`
([sim_pc/main.c:1471](../../../sim_pc/main.c#L1471), [:1478](../../../sim_pc/main.c#L1478))
and in the regression path, but not in the one mode a human actually runs.

Consequences when someone launches `sim_pc.exe` with no arguments:

- **No splash.** The animation is implemented and correct — the `--shots` output
  proves it — it is simply never triggered.
- **Every switch sticks on `Sending…`.** `toggle_device()` sets `PENDING` and reaches
  `if (s_relay_cmd_cb != NULL)` with a null pointer, so no command is emitted and
  nothing will ever call `ui_handle_command_result()`. Combined with item 1 the tile
  is dead for the rest of the session. The same applies to Save in the settings dialog.

Fix the simulator, not the UI:

- Register both callbacks in `run_interactive()`.
- Give the simulator a small deterministic fake node: on a relay or settings command,
  acknowledge after a realistic delay using an `lv_timer` (the browser prototype used
  700 ms — reuse that), calling `ui_handle_command_result(req_id, true)`. Keep the
  fixture in `sim_pc/`, never in `ui.c`.
- Provide a way to exercise the unhappy paths interactively — a key binding that makes
  the next command fail, and one that makes it never answer at all so item 1's timeout
  can be seen on screen. Document the keys in `sim_pc/README` or the repository README.
- Call `ui_replay_splash()` on startup so the interactive app matches the product
  sequence, and keep a key binding to replay it.

Item 1 still has to be fixed independently: the frozen tile is a UI defect that would
occur on real hardware whenever an ack is lost. Fixing only the simulator would hide it.

## P2 — cleanups

**4. `ui_device_cmd_state_t` is defined twice with nothing keeping the copies in sync.**

`ui.c` defines it in the `#else` branch of the `UI_TEST_HOOKS` guard
([ui.c:59-81](../../../ui.c#L59)) and `sim_pc/ui_test_hooks.h` declares a byte-for-byte
"mirror" of the same struct. Only one is compiled per configuration, so if the two ever
diverge there is **no compile error** — the simulator would read a different memory
layout than the firmware ships, and the tests would be validating something the target
does not run. There is no `_Static_assert` guarding it either.

Define the struct once — a private `ui_internal.h` included unconditionally by `ui.c`
and by `ui_test_hooks.h` is the straightforward option — and delete the duplicate. If
you add a header, add it to the `tools/export_mcu.bat` manifest and re-run
`tools/export_mcu.bat --verify`.

**5. CHECK 9 reports "Zero heap leak" while printing a non-zero drift.**

[sim_pc/main.c:906](../../../sim_pc/main.c#L906) prints
`"PASS: Zero heap leak (%ld byte drift)"`, which produced
`Zero heap leak (40 byte drift)` on this build. The claim and the number contradict
each other, and the drift moved from −16 bytes in round 3 to +40 bytes now. Neither
value is dangerous with 8.8 KiB free, but a test that says "zero" while measuring
non-zero is a test you cannot trust later.

Assert an explicit threshold, print the measured value plainly, and word the PASS line
so it matches what was measured. While you are there, say in the output what the
threshold is, so a future regression is obvious.

## Verification required

Run and paste real output for:

```powershell
tools/build_sim.bat
tools/run_regression.bat
tools/export_mcu.bat --verify
sim_pc\build\sim_pc.exe --shots <dir>
```

Then:

1. **Timeout behaviour:** a check that issues a command, never acknowledges it, and
   asserts the tile reaches `Unknown` + `Retry` within the bound, that the switch is
   usable again afterwards, and that a late acknowledgement for the expired request is
   ignored.
2. **Overlay bound:** a check that acknowledges a command and then pushes a
   contradicting snapshot repeatedly, asserting the UI follows the application once the
   bound elapses — and a second check proving the original flip-flop fix still holds
   for a snapshot that simply has not caught up yet.
3. **Idle cost unchanged:** re-run the flush instrumentation and show that ten identical
   snapshots still push **0 pixels** with the new timers armed, and report the per-second
   cost of an idle Home page.
4. **Interactive mode, by hand:** launch `sim_pc.exe` with no arguments and confirm the
   splash plays, a switch completes to its new state after the fake-node delay, the
   forced-failure key produces `Unknown` + `Retry`, and the no-answer key produces the
   timeout. Say explicitly that you ran it interactively; do not infer it from headless
   output.
5. Heap peak, largest free block and fragmentation per screen, plus the drift figure
   against its new threshold.
6. Update `PLAN.md`, `DEV_LOG.md` and `docs/MCU_BASELINE.md` (including the timeout
   constant). List explicitly anything you did not complete. Do not report a check as
   passed unless you ran it.
