# Architecture prompt · Smart Hub UI round 5 — `lora_comm.h` and the dumb-node model

> ARCHIVED — historical context only, not the current implementation request.
> Original location: `docs/reviews/implementation-r5/FIX_PROMPT.md`.
> Use [CURRENT.md](../CURRENT.md) for the active handoff and
> [prompt rules](../README.md) before creating or updating any prompt.

This round is **not** a defect list. The owner has settled the system architecture,
and it changes the UI's input/output contract at the root. Round 4's remaining
defects (the settings overlay reverting after 3 s, retry-after-settings sending a
relay command, the silent revert, the relay-worded settings timeout) are **deleted
by this change rather than patched** — device configuration stops being a remote
command entirely.

Round 4 itself was verified good: 15/15 checks, `export_mcu --verify` PASS, splash
seam clean, and the performance work held (10 identical snapshots = 0 flushes, idle
sub-second = 0 flushes, idle 1 s = 1 flush for the uptime label only).

Copy everything below the line to the coding AI.

---

You are making an architectural change to the LVGL v8.4 Smart Hub UI in this
repository (`C:\Users\vanth\Documents\MyDocument\Embedded\LVGL_UI`, branch
`ui/smart-hub`). Read `AGENTS.md`, `docs/HARDWARE_OVERVIEW.md`,
`docs/LVGL_IMPLEMENTATION_PROMPT.md` and `docs/TECHNICAL_LEAD_REVIEW.md` first. Do
not reset, clean, stash, push or change branches.

This is the largest change since the first implementation round. Work in stages and
keep the regression suite green at every stage. Do not start by deleting things.

## 1. The system, as the owner has now defined it

Two **separate firmware projects** talk to each other over **LoRa peer-to-peer**.
Both carry a `Ra-01H` module. Neither is a gateway; there is no broker in between.

**`LCD_Shield_LoRa` — the Smart Hub.** This is where this LVGL UI is brought up.
It owns the 800×480 QSPI LCD, the capacitive touch controller and one LoRa radio.
It has **no relays and no RS-485**.

**`Relay_Shield_LoRa` — the Smart Node.** It owns four `JQ1P-12V-F` relays driven
through `SI2300DS` MOSFETs from four GPIOs, plus the `SP3485EN` RS-485 transceiver
carrying the environmental sensors. Pin assignments are already recorded in
`docs/HARDWARE_OVERVIEW.md`; do not re-derive them.

**The Node is deliberately dumb.** It receives a GPIO level per relay and writes it
to the pin. It knows nothing about appliances, names, presets, modes or semantics.

**The Hub owns all semantics.** In particular, relay polarity is per-device: with
one appliance a GPIO high means the load is ON, with another (wired to NC instead
of NO, or an inverting driver) a GPIO high means OFF. The UI resolves that and puts
a **raw 0/1 GPIO level** on the wire. The Node never has to know which is which.

## 2. Owner decisions that drive this round

1. **`lora_comm.h` at the repository root is the single source of truth** for
   everything the two projects exchange, shared verbatim by both, following the
   `motor_comm.h` convention from the `ui/motor-control` branch (read it: `git show
   ui/motor-control:motor_comm.h` and `git show ui/motor-control:UART_PROTOCOL.md`).
2. **The UI reads the wire state directly through `extern` globals**, exactly as the
   motor-control UI reads `motorStatusFast` / `motorStatusSlow` and writes
   `motorCmd`. `ui_snapshot_t` and the `ui_update_snapshot()` push API go away.
3. **Device configuration is Hub-local and never travels on the wire.** Name,
   preset/icon, relay polarity and Auto/Manual live only on the Hub. Changing them
   in the Devices dialog is **immediate**: no `Sending…`, no pending phase, no
   acknowledgement, no timeout, no confirmed overlay. It only affects what the UI
   displays and how it maps on/off to a GPIO level.
4. **Auto/Manual stays as a label with no engine behind it.** Keep it visible and
   editable and store it in the Hub-local config, but do not implement any
   sensor-to-relay automation. Thresholds and hysteresis remain undefined, and
   `AGENTS.md` still forbids inventing them.
5. **The UI never assumes command echo.** The displayed relay state follows what the
   Node reports, exactly as `UART_PROTOCOL.md` section 3.1 requires. This is what
   removes the confirmed-overlay machinery.

## 3. Build `lora_comm.h`

Put it at the repository root. Style it on `motor_comm.h`: fixed-width types from
`<stdint.h>`, one `union` per message with a `.bits` bitfield view and a `.bytes[]`
wire view, bitfields declared low-bit-first, an explicit comment stating the
little-endian assumption, scaled integers with the unit documented per field, and
**no floats anywhere**.

Two deliberate differences from `motor_comm.h`, because the transport is a radio and
not a UART — implement them and say in the header why:

- **Drop the `SOF`/`EOF` envelope.** The SX127x in explicit-header mode already
  delivers a whole packet with its own length field and CRC, so a start/end byte
  pair buys nothing and costs airtime on a duty-cycle-limited link. Keep a small
  header of `{ protocol version, message id }` so both sides can reject a mismatched
  build, and keep the fixed-length-per-id rule.
- **Prefer one combined Node→Hub status frame over two.** Every extra frame is extra
  airtime and another chance to lose something. Unless you can show a reason to split
  them, carry relay state and sensor telemetry in a single periodic frame.

Messages to define, at minimum:

**Hub → Node, relay command.** A per-relay GPIO level for all four outputs (four
bits is enough) plus a sequence number the Hub increments whenever the desired
levels change. The Node applies the levels and echoes the sequence. Send it on
change and re-send periodically so a lost frame self-heals.

**Node → Hub, status + telemetry.** The four **actual** GPIO output levels read
back, the echoed command sequence, and the sensor readings: CO₂ in ppm, VOC as the
existing unitless index, temperature in 0.1 °C signed, relative humidity in whole
percent. Include a per-metric validity bit — the UI already renders each metric's
validity independently and must keep doing so. Include fault flags for the RS-485
bus and per-sensor timeouts, and a node uptime counter.

Do not invent Modbus register maps, LoRa frequency/region settings, spreading
factor, addressing or pairing. Those remain open in `PLAN.md`. Define only the
payload contract; leave a clearly marked place for an address/pairing field so it
can be added without renumbering.

## 4. Rework the UI contract

### 4.1 What replaces the snapshot

Follow the motor-control pattern: the wire structs are the state. Declare the
globals `extern` in a header the UI includes, define them in the application layer,
and have the UI read them. The simulator plays the role of that application layer.

Because there is no longer a push call to hang updates off, **poll the wire state
from `ui_tick()`**. That is safe here only because round 3's dirty-check layer
already makes an unchanged update free — keep `set_buffer_and_label_if_changed()`,
`set_pos_if_changed()`, `set_bg_color_if_changed()`, `set_text_color_if_changed()`,
`set_flag_if_changed()` and `set_state_if_changed()` exactly as they are. To avoid
re-formatting two dozen strings every tick when nothing changed, gate the whole
refresh on a cheap change test — a revision counter the application bumps on each
received frame, or a `memcmp` against a cached copy of the wire struct. Measure it:
an idle Home page must still cost **0 flushes** per tick and must not regress the
1-flush-per-second uptime figure.

### 4.2 Relay output, with polarity resolved on the Hub

The UI writes the desired GPIO levels into the command global — it does not call a
command callback with `on/off` any more. Compute the level from the Hub-local
config:

```
gpio_level  = desired_on ^ cfg[i].active_low
reported_on = reported_level ^ cfg[i].active_low
```

Both directions must use the same `active_low`, or the tile will read back inverted.
Expose `active_low` (name it for what it is) in the device settings dialog so the
installer can flip it per output, and default it to a documented value.

### 4.3 Delete the settings command path

`ui_settings_command_cb_t`, `has_pending_settings`, `has_confirmed_preset`,
`confirmed_preset` and every pending/overlay branch that exists for presets go away.
Saving the dialog writes Hub-local config and the tile updates on the spot. If the
application needs to persist the configuration to flash, give it a plain
notification callback with no request id, no pending state and no timeout — the UI
must not wait for anything.

This is the fix for the defect the owner reported: changing a device's type showed
the new value for about 3.7 seconds and then silently reverted, because the preset
was being treated as a remote command whose confirmation overlay expired against a
snapshot that never carried the new value.

### 4.4 Simplify relay state to "desired versus reported"

With the display following reported state, the whole confirmed-overlay mechanism
(`has_confirmed_state`, `confirmed_on`, `confirmed_mode`, `confirmed_tick_ms`, the
reconciliation block in the old `ui_update_snapshot()`, and the overlay sweep in
`ui_tick()`) is no longer needed. Replace it with the straightforward rule:

- reported level equals desired level → **settled**, show the switch normally;
- they differ and the command is younger than `UI_CMD_TIMEOUT_MS` → **`Sending…`**;
- they differ and it is older → **`Unknown` + `Retry`**, switch usable again.

Keep `UI_CMD_TIMEOUT_MS` and its documented rationale in `docs/MCU_BASELINE.md`; it
now measures "no confirming status frame arrived", which is the same failure it
always meant. Note in the doc that a spreading factor above SF10 will need the value
raised, since the current 3000 ms budget assumes SF7–SF10 airtime.

Verify that this really does remove the round-4 defects rather than relocating them:
a Node that reports a level the Hub did not ask for (someone forced the relay, or
the node rebooted) must show up on screen, not be masked.

### 4.5 Link state

Connection state now derives from the age of the last received status frame, not
from a field somebody sets by hand. Expose the last-receive tick (and RSSI if you
have it) alongside the wire globals and let the UI decide `Connected` /
`Disconnected` from a documented staleness bound. When the link is stale the UI must
keep doing what it already does correctly: hide stale live numbers, show `Unknown`,
and refuse output changes.

## 5. Rework the simulator into a real fake node

`sim_pc` becomes the application layer. Instead of calling `ui_update_snapshot()`,
it owns the wire globals and behaves like the Node:

- It reads the command global, applies the requested GPIO levels after a realistic
  delay (keep the existing 700 ms), and then **reports those levels back** in the
  status global with the echoed sequence. This is the piece that was missing: the old
  fake node acknowledged a command and then kept reporting the original state
  forever, which is why configuration appeared to revert.
- It keeps producing the deterministic sensor fixtures the scenarios already use.
- Keep the interactive key bindings from round 4 (`S` splash, `F` force failure,
  `T` force timeout, `1`/`2`/`3` pages) and keep them working against the new model:
  `F` makes the node apply nothing but stay alive, `T` makes the node stop reporting
  entirely so the staleness path is exercised too.
- The fake node lives in `sim_pc/`, never in `ui.c`.

## 6. What must not regress

Round 3 and round 4 produced verified results. Preserve them and re-prove them:

- The study 12 flat RGB565-exact palette and every screen's layout.
- The dirty-check update path and its measured numbers.
- The test API split: `ui.h` stays a small public API, the test surface stays in
  `sim_pc/ui_test_api.*`, and `ui_internal.h` remains the single definition of the
  internal command state.
- `reset_page_widget_pointers()` clearing every cached pointer on navigation.
- The pending-command timeout, in its new "no confirming report" form.
- The splash background `0xE9ECF1` and its comment.

Update the MCU export manifest in `tools/export_mcu.bat` to include `lora_comm.h`,
and re-run `tools/export_mcu.bat --verify`.

## 7. Documentation you must update

- **Write `LORA_PROTOCOL.md`** at the repository root, in the shape of the
  motor-control `UART_PROTOCOL.md`: frame layout, every message and field with units
  and scaling, who writes what, the cadence, and the explicit statement that the UI
  displays reported state and never assumes command echo.
- **`AGENTS.md` currently says to keep transport out of UI code.** The owner has
  chosen the motor-control pattern instead, where the UI reads the wire globals
  directly. Record that decision in `AGENTS.md` and in `docs/UI_DESIGN_BRIEF.md` as
  a numbered decision, with the trade-off stated honestly, so the next reviewer does
  not flag it as a violation. Do not leave the two documents contradicting each other.
- **`docs/HARDWARE_OVERVIEW.md`**: add that the Node applies GPIO levels verbatim and
  that per-device polarity is resolved on the Hub.
- **`docs/LVGL_IMPLEMENTATION_PROMPT.md`** and **`docs/TECHNICAL_LEAD_REVIEW.md`**:
  replace the snapshot-contract wording with the new one.
- **`PLAN.md`** and **`DEV_LOG.md`** as usual. Keep the open questions open —
  frequency/region, node addressing and pairing, Modbus register maps, alarm
  thresholds and configuration persistence are all still undecided.

## 8. Verification required

Run and paste real output for:

```powershell
tools/build_sim.bat
tools/run_regression.bat
tools/export_mcu.bat --verify
sim_pc\build\sim_pc.exe --shots <dir>
```

Then:

1. **The reported defect is gone:** change a device's type in the Devices dialog and
   show it persists indefinitely, with no `Sending…` at any point. State how long you
   observed it.
2. **Polarity:** a check that sets `active_low` on one output and asserts the
   transmitted GPIO level is inverted while the displayed on/off state is not, in
   both directions.
3. **Desired versus reported:** checks for settled, `Sending…` inside the timeout,
   `Unknown` + `Retry` past it, a late report arriving after the timeout, and a Node
   reporting a level the Hub never requested.
4. **Link staleness:** stop the fake node reporting and show the UI going
   `Disconnected`, hiding live values and refusing output changes; then resume and
   show it recovering.
5. **Performance unchanged:** re-run the flush instrumentation and show idle Home is
   still 0 flushes per tick and 1 flush per second, with the polling model in place.
   Report the per-tick cost of the change test you chose.
6. **Screenshots:** regenerate all twelve and compare against `docs/prototype/*.png`.
7. Heap peak, largest free block, fragmentation per screen, and the drift against its
   threshold.
8. List explicitly what you did not complete, and every question this raised that
   only the owner can answer. Do not report a check as passed unless you ran it.
