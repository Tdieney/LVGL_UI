# Fix prompt · Smart Hub UI round 6 (revised)

> ARCHIVED — historical context only, not the current implementation request.
> Original location: `docs/reviews/implementation-r6/FIX_PROMPT.md`.
> Use [CURRENT.md](../CURRENT.md) for the active handoff and
> [prompt rules](../README.md) before creating or updating any prompt.

Round 5's architectural change landed well and is verified: `lora_comm.h` is in
place, the UI reads the wire globals, device configuration is Hub-local with no
`Sending…`, the confirmed-state overlay is gone, polarity resolves in both
directions, link staleness works, and the performance results held (10 identical
frames = 0 flushes, idle sub-second = 0, idle 1 s = 1 flush for the uptime label).
15/15 checks pass and `export_mcu --verify` passes.

**None of the round-6 review findings have been applied yet.** Verified: the
settled-idle branch is unchanged, `pad_all(tile, …)` is still absent, the fake
node's sequence variables are still `uint8_t`, `lora_comm.h` still has no static
asserts, `seq_echo` still has zero occurrences in `ui.c`, and `ui_snapshot_t` is
still in the exported `ui_types.h`. The build reports `ninja: no work to do` and
the flush metrics are byte-identical to the previous run.

This prompt merges those seven findings with three items that came out of the
owner's report of components looking misaligned.

Copy everything below the line to the coding AI.

---

You are fixing defects in the LVGL v8.4 Smart Hub UI in this repository
(`C:\Users\vanth\Documents\MyDocument\Embedded\LVGL_UI`, branch `ui/smart-hub`).
Read `AGENTS.md`, `LORA_PROTOCOL.md` and `docs/TECHNICAL_LEAD_REVIEW.md` first. Do
not reset, clean, stash, push or change branches.

Preserve round 5: the `lora_comm.h` contract, the extern-globals model, Hub-local
device configuration, the desired-versus-reported relay logic, the dirty-check
update path and its measured numbers, the study 12 palette, the test-API split and
the splash background `0xE9ECF1`.

Two of these items correct a diagnosis that was proposed earlier and is wrong.
Read items 8 and 10 before changing any dialog or tile geometry.

## P0 — a defect that only appears on real hardware

**1. Adopting a remote relay flip desynchronises the UI's desired state from the transmitted command.**

[ui.c:2042-2046](../../../ui.c#L2042), the settled-idle branch of the `ui_tick()`
sweep:

```c
} else {
    /* Settled idle: keep desired_on tracking reported_on when online so remote flips show up */
    if (online && s_desired_on[i] != reported_on) {
        s_desired_on[i] = reported_on;
        cmd_state_changed = true;
    }
}
```

It updates the UI's desired state but never writes `lora_hub_cmd`. After the UI
adopts a flip it did not command — someone forced the relay locally, or the node
rebooted into a different state — `s_desired_on[i]` holds the new value while
`lora_hub_cmd.bits.relayN_gpio` still holds the old one.

`LORA_PROTOCOL.md` line 54 says the command frame is "repeated periodically
(cadence ~1000 ms) so lost packets self-heal", and the Node "applies raw GPIO
levels verbatim". So roughly a second later the Hub retransmits the stale level,
the Node applies it, the relay snaps back, and the UI adopts that in turn. On
target this is a relay oscillating against a manual override once per second.

The simulator cannot show this, because its fake node is **edge-triggered on the
sequence number** ([sim_pc/main.c:68](../../../sim_pc/main.c#L68)):

```c
if (lora_hub_cmd.bits.seq != s_fake_node_last_seen_seq && !s_fake_node_cmd_in_flight) {
```

while the documented Node is level-triggered. That mismatch hides this entire class
of bug, so fix both halves:

- Make the UI's desired state and the transmitted command move together. Whichever
  way you resolve the conflict — adopt the remote flip and update `lora_hub_cmd` to
  match, or keep commanding the Hub's value and surface the disagreement — the two
  must never disagree silently. Say which you chose and why.
- Make the fake node apply the levels from **every** received frame, the way the
  protocol says the real Node behaves, not only when the sequence changes. Keep the
  `F` and `T` behaviours working.

If instead you conclude that the Node should be edge-triggered, then the periodic
retransmission in `LORA_PROTOCOL.md` no longer self-heals a lost frame and the
document must change. Do not leave the code and the protocol document disagreeing.

## P1 — the layout regression the owner is seeing

**2. The Home device tiles lost their padding.**

`lv_obj_set_style_pad_all(tile, 12, 0);` was dropped from the tile construction in
round 5. It belongs immediately after
[ui.c:760](../../../ui.c#L760) `lv_obj_add_style(tile, &ui_style_card_device, 0);`,
matching every other card, all of which still have theirs: CO₂/VOC `pad_all 16`
([ui.c:632](../../../ui.c#L632)), comfort `pad_hor 12 / pad_ver 10`
([ui.c:715](../../../ui.c#L715)), chart `pad_all 16` ([ui.c:1023](../../../ui.c#L1023)),
Devices cards `pad_hor 16 / pad_ver 14` ([ui.c:1277](../../../ui.c#L1277)), dialog
`pad_all 20` ([ui.c:1564](../../../ui.c#L1564)).

Measured on the rendered framebuffer, tile content rows:

| | content rows | versus prototype |
|---|---|---|
| `docs/prototype/home.png` | 309..382 | reference |
| before round 5 | 308..377 | matched |
| now | 296..365 | **13 px too high** |

Horizontally it is off by the same amount. Restore the line and re-measure against
the prototype.

**3. Add geometry assertions so this cannot recur silently.**

All fifteen checks test behaviour; none test pixel geometry, which is why a missing
padding line passed the whole suite. Add a check that renders Home, Trends, Devices
and the settings dialog and asserts a handful of anchor coordinates against
constants taken from `docs/prototype/*.png`: the Home tile content origin, the
Devices card icon-plate origin, the Trends chart card box, the settings dialog box,
and the selected navigation pill. Use a small tolerance (a couple of pixels for
antialiasing), and make the failure message print measured versus expected.

## P2 — contaminated screenshot evidence

**4. The Devices scenarios inherit state and a toast from the relay-retry scenario.**

`devices_grid.raw` ([sim_pc/main.c:1702](../../../sim_pc/main.c#L1702)) and
`devices_settings.raw` ([:1709](../../../sim_pc/main.c#L1709)) are captured after
the `home_relay_retry` scenario ([:1678](../../../sim_pc/main.c#L1678)), which
deliberately drives device 1 into `UI_CMD_PHASE_ERROR` and raises the toast
"No response from node. Output state unknown.".

Neither is cleared before the Devices captures, so the published Devices screenshot
shows "Ventilation Fan" with no switch, an `Unknown` label and a `Retry` link, and a
black toast lying across the two lower cards. The page looks broken when it is not,
and it makes the screenshots useless as layout evidence.

Reset the device command state and dismiss the toast between scenarios so each
capture shows only what that scenario is meant to show. Keep a dedicated scenario
for the retry state if you want that documented — just do not let it leak.

## P3 — protocol and code hygiene

**5. No static assertions on the wire struct sizes.**

`lora_comm.h` declares `uint8_t bytes[LORA_HUB_CMD_LEN]` (8) and
`[LORA_NODE_STATUS_LEN]` (20) inside unions whose `.bits` structs must match those
sizes exactly. I hand-checked the current layout and it is correct on ARM EABI, but
nothing enforces it: one added field and `bytes[]` silently stops covering the
struct, on both projects at once, with no diagnostic. Add

```c
_Static_assert(sizeof(lora_hub_cmd_t)    == LORA_HUB_CMD_LEN,     "...");
_Static_assert(sizeof(lora_node_status_t) == LORA_NODE_STATUS_LEN, "...");
```

or the C99-compatible equivalent if the Node toolchain needs it.

**6. Hub-local link bookkeeping is declared in the shared wire header.**

`lora_last_rx_tick_ms`, `lora_last_rssi` and `lora_rx_revision` are Hub-side
receive bookkeeping, not part of the wire contract, yet they sit in `lora_comm.h`,
which is copied verbatim into the Relay_Shield_LoRa project. Move them to a
Hub-only header, keep `lora_comm.h` to what actually crosses the radio, and update
the export manifest accordingly.

**7. `seq_echo` is transmitted but never read.**

The Node echoes the executed command sequence and `ui.c` contains zero references
to it; settling is decided purely by `reported_on == desired_on`. That is defensible
under "display follows reported state", but it means a stale in-flight status frame
that happens to match the new desired level settles the command early. Either use
`seq_echo` to confirm the report reflects the current command, or state in
`LORA_PROTOCOL.md` that the UI intentionally ignores it and why.

**8. Do not resize the settings dialog.**

An earlier diagnosis claimed the dialog had grown to 368 px, overflowed, and now
overlaps the bottom navigation by 8 px, and proposed shrinking it to 330 px. That is
wrong and shrinking it would introduce the deviation it claims to fix. Measured:

| | dialog box | gap to navigation pill |
|---|---|---|
| `docs/prototype/device-settings.png` | 440×367 at y 56..422 | 2 px |
| current build | 440×368 at y 56..423 | 1 px |

The dialog matches the reference within one pixel. The navigation pill starts at
y = 424 and nothing covers it; the 416..423 band is the navigation bar's transparent
top padding. Leave `lv_obj_set_size(box, 440, 368)` ([ui.c:1561](../../../ui.c#L1561))
alone.

**9. `ui_types.h` still ships a dead contract to firmware.**

`ui_snapshot_t`, `ui_device_snapshot_t` and `ui_metric_snapshot_t`
([ui_types.h:84](../../../ui_types.h#L84) and above) are no longer referenced by
`ui.c` or `ui.h` — only by `sim_pc/ui_test_api.c`, which synthesises a legacy view
for older assertions. `ui_types.h` is in the MCU export manifest, so firmware
carries a snapshot contract that no longer exists. Move those three types into the
simulator's test header, or drop them and rewrite the affected assertions against
the wire globals and the Hub-local config.

**10. Do not "fix" the tile label shift.**

An earlier diagnosis flagged the Home tile mode label moving between y = 50 and
y = 62 as a defect. It is the designed behaviour: when a state line (`Sending…`,
`Unknown`) appears it sits above the mode label and pushes it down. The prototype
does exactly the same — in `docs/prototype/offline.png` the first tile stacks
`Unknown` above `Auto`, with content rows 309..384 against 309..382 in the normal
state. Leave it.

**11. The timeout announcement fires once per device.**

`ui_announce()` is called inside the four-device loop in `ui_tick()`
([ui.c:2032](../../../ui.c#L2032)). Four devices timing out together produce four
announcements overwriting each other. Announce once per sweep.

**12. Fix the truncated sequence numbers in the fake node.**

[sim_pc/main.c:58](../../../sim_pc/main.c#L58) and [:62](../../../sim_pc/main.c#L62)
declare `s_fake_node_last_seen_seq` and `s_fake_node_target_seq` as `uint8_t`, while
`seq` and `seq_echo` on the wire are `uint16_t`. Past 256 commands the truncated
comparison can make the node miss a command, and the echoed value is wrong. It
passed `-Werror` only because `-Wconversion` is not enabled — consider enabling it
for the simulator target and fixing whatever else it surfaces, or at minimum correct
these two types.

## Verification required

Run and paste real output for:

```powershell
tools/build_sim.bat
tools/run_regression.bat
tools/export_mcu.bat --verify
sim_pc\build\sim_pc.exe --shots <dir>
```

Then:

1. **Geometry:** the new anchor-coordinate check passing, plus the measured Home tile
   content rows next to the prototype's 309..382.
2. **Remote flip:** a check that reports a relay level the Hub never commanded and
   asserts the UI and `lora_hub_cmd` end up agreeing, with the fake node now
   level-triggered. Show that a lost command frame still self-heals on retransmission.
3. **Clean screenshots:** regenerate all twelve and confirm `devices_grid.png` shows
   four normal cards with switches, no `Retry`, no toast. Put each shot next to its
   `docs/prototype/*.png` counterpart.
4. **Static assertions:** show the build failing if a field is added to a wire struct
   without updating its `LEN`, then show it passing again.
5. **Performance unchanged:** the flush metrics must still read 0 flushes for ten
   identical frames, 0 for idle sub-second and 1 for idle one second.
6. Heap peak, largest free block, fragmentation per screen, and the drift against its
   threshold.
7. State explicitly which of items 1–12 you completed, and for item 1 which
   resolution you chose. Do not report a check as passed unless you ran it.
