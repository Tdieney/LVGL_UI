# Feature prompt · Smart Hub UI round 7 — signal strength indicator

> ARCHIVED — historical context only, not the current implementation request.
> Original location: `docs/reviews/implementation-r7/FIX_PROMPT.md`.
> Use [CURRENT.md](../CURRENT.md) for the active handoff and
> [prompt rules](../README.md) before creating or updating any prompt.

Round 6 is verified complete: 16/16 checks, export manifest PASS, Home tile
geometry back in line with the prototype (rows 308..377 against 309..382), the
desired-versus-command desync fixed on both the UI and fake-node sides, and the
performance results unchanged.

This round adds one feature the owner asked for: the `Connected` indicator in the
header should show **link strength**, not just up/down.

The protocol groundwork is already done — **do not redo it**:

- `lora_comm.h` is unchanged and stays at 20 bytes. Link quality is deliberately
  not on the wire, and the header now carries a comment block explaining why.
- `lora_hub_link.h` declares `lora_last_snr` (whole dB) next to the existing
  `lora_last_rssi`. Both are Hub-local, filled by the Hub's own radio, zero
  airtime. `sim_pc/main.c` defines them.
- Neither is read by the UI yet. That is this round's work.

Copy everything below the line to the coding AI.

---

You are adding a feature to the LVGL v8.4 Smart Hub UI in this repository
(`C:\Users\vanth\Documents\MyDocument\Embedded\LVGL_UI`, branch `ui/smart-hub`).
Read `AGENTS.md`, `LORA_PROTOCOL.md`, `lora_hub_link.h` and
`docs/TECHNICAL_LEAD_REVIEW.md` first. Do not reset, clean, stash, push or change
branches.

Preserve everything from rounds 5 and 6: the `lora_comm.h` contract at 20 bytes,
the extern-globals model, Hub-local device configuration, desired-versus-reported
relay logic, the dirty-check update path and its measured numbers, the study 12
flat palette, the test-API split, and the CHECK 16 geometry assertions.

**Do not add anything to `lora_comm.h`.** The wire protocol is settled for this
feature. Everything you need is already Hub-local.

## 1. Why SNR and not RSSI

This matters, because getting it backwards produces an indicator that lies.

RSSI and SNR are measured by whichever radio **receives** a frame. The Hub's own
SX127x reports both after each status frame it receives from the Node, which is
why no protocol field is involved.

LoRa demodulates **below the noise floor**, unlike Wi-Fi. The demodulator limit
depends on the spreading factor, at 125 kHz bandwidth:

| SF | demodulator limit |
|---|---|
| SF7 | −7.5 dB SNR |
| SF8 | −10 dB |
| SF9 | −12.5 dB |
| SF10 | −15 dB |
| SF11 | −17.5 dB |
| SF12 | −20 dB |

So a link at −120 dBm with SNR +5 dB is healthy, while −120 dBm with SNR −15 dB is
at the cliff edge. Absolute RSSI tells you almost nothing about how close to
failure you are; the **margin of SNR above the demodulator limit** does.

Drive the indicator from that margin. Leave `lora_last_rssi` for diagnostics only —
its value is separating "weak signal" from "strong signal plus strong
interference", which look identical in SNR alone. Do not put RSSI in the bar
calculation.

## 2. Design work first — update the prototype

`docs/prototype/` is the design source of truth; the LVGL implementation follows
it, not the other way round.

Replace the `radio` icon in the link status ([docs/prototype/app.js:111](../../prototype/app.js))
with a signal-strength indicator of **four bars of increasing height**, keeping the
`Connected` / `Disconnected` text beside it — the design brief requires that
wording and it carries the accessible name.

Draw the bars as **plain rectangles**, not a decorative SVG glyph. That is
deliberate: LVGL will draw the same shape with four small objects, so both sides
match exactly and no bitmap asset is involved.

States to design:

| State | Appearance |
|---|---|
| Disconnected (no status frame within the staleness bound) | all bars inactive, existing red treatment, text `Disconnected` |
| 1 bar | one active, three inactive |
| 2 bars | two active |
| 3 bars | three active |
| 4 bars | four active |

Use the study 12 palette: active bars in the existing semantic colour used for a
healthy link, inactive bars in the muted tone. Do not introduce new colours, and
keep every fill an exact RGB565 value from the study 12 sets. Keep the indicator's
overall footprint close to the 20 px icon it replaces so the header rhythm holds.

Add a control to the prototype's review panel that cycles the level, so the states
can be reviewed. Re-render the affected 800×480 screenshots and note in
`docs/prototype/README.md` which ones changed.

## 3. Mapping SNR to bars

Put the mapping on the Hub side, next to the other link logic. Two things need
defining and both must be named constants, not literals buried in a function:

**The spreading factor.** It is not chosen yet — `PLAN.md` still lists LoRa
frequency, region and radio configuration as open. Declare it as a single constant
in `lora_hub_link.h` (for example `LORA_SPREADING_FACTOR`), derive the demodulator
limit from it with a small lookup table, and state in a comment that it **must match
the radio driver's actual configuration**. A mismatch here silently skews every bar.

**The thresholds.** Starting point, to be tuned once the radio is configured:

| SNR margin above the limit | bars |
|---|---|
| ≥ 10 dB | 4 |
| 5 – 10 dB | 3 |
| 2 – 5 dB | 2 |
| 0 – 2 dB | 1 |
| below the limit, or link stale | disconnected |

The limits are in half-dB steps while `lora_last_snr` is whole dB, so do the
arithmetic in tenths of a dB with integers. No floating point — `AGENTS.md` forbids
it in these paths and there is no need for it here.

**Add hysteresis.** Real per-packet SNR jitters by several dB, so a bare threshold
comparison will make the indicator flicker between two levels whenever the link
sits near a boundary. Require a margin of overshoot before moving up a level, or
smooth the input over the last few frames. Say which you chose. A flickering
indicator also defeats the dirty-check work: each change is a redraw.

## 4. Implementation notes

- Read `lora_last_snr` in the same place the header already decides
  `Connected` / `Disconnected` ([ui.c:474](../../../ui.c#L474)). If the link is
  stale you already show `Disconnected`; a valid connection implies a frame has
  arrived, so the measurement is always present when bars are shown.
- Build the bars from four small `lv_obj` rectangles created once in
  `build_header()`, and recolour them per level with `set_bg_color_if_changed()`.
  Do not create or destroy objects per update, and do not generate bitmap assets —
  this avoids the fragile headless-Edge icon pipeline entirely.
- **The header cluster is right-aligned with hardcoded widths.**
  [ui.c:498](../../../ui.c#L498) computes `link_ico_x = link_lbl_x - 7 - 20`, where
  `20` is the old icon width. If your indicator is a different width, that constant
  and the ones below it must change, or the whole cluster shifts. Re-check the
  separator and uptime positions after the change.
- Keep the update inside the existing `s_header_initialized || up_changed ||
  link_changed` guard so an unchanged level costs nothing.
- The header already redraws once per second for the uptime label. Adding bars must
  not increase that. An idle Home page with a steady link must still measure
  **0 flushes** sub-second and **1 flush** per second.

## 5. Simulator work

- Make the fake node produce a varying SNR so the indicator can actually be seen
  moving, rather than the fixed value currently in `sim_pc/main.c`.
- Add an interactive key that steps the simulated link quality through
  disconnected → 1 → 2 → 3 → 4 bars, and document it alongside the existing
  `S` / `F` / `T` / `1` / `2` / `3` keys in `sim_pc/README.md`.
- Add a scenario capture for at least one degraded level so the state is covered by
  a screenshot.
- Keep the fake node in `sim_pc/`.

## 6. Verification required

Run and paste real output for:

```powershell
tools/build_sim.bat
tools/run_regression.bat
tools/export_mcu.bat --verify
sim_pc\build\sim_pc.exe --shots <dir>
```

Then:

1. **Mapping:** a check that feeds SNR values across every threshold, including
   exactly on each boundary and below the demodulator limit, and asserts the bar
   count. Include a case proving the hysteresis prevents flicker when the value
   oscillates around a boundary.
2. **Spreading factor:** show that changing `LORA_SPREADING_FACTOR` moves the
   thresholds as expected, so the constant is genuinely wired to the calculation.
3. **Header geometry:** CHECK 16 still passes, and the right-aligned cluster —
   uptime, separator, indicator, label — is still correctly spaced. Give measured
   coordinates, and compare the header against the re-rendered prototype.
4. **Screenshots:** regenerate all scenarios plus the new degraded-link one, and put
   each next to its `docs/prototype/*.png` counterpart.
5. **Performance unchanged:** 0 flushes for ten identical frames, 0 for idle
   sub-second, 1 for idle one second, with a steady link.
6. **Colours:** sample the framebuffer and show the active and inactive bar fills
   are exactly the study 12 palette values you declared.
7. Heap peak, largest free block and fragmentation per screen — the header gains
   objects, so state the delta.
8. Update `PLAN.md`, `DEV_LOG.md`, `docs/UI_DESIGN_BRIEF.md` (as a numbered
   decision) and `docs/prototype/README.md`. State explicitly what you did not
   complete. Do not report a check as passed unless you ran it.
