# LoRa Protocol Specification — Smart Hub & Smart Node

> **`lora_comm.h`** (repository root) is the **single source of truth** for message framing and bitfield definitions between the Smart Hub (`LCD_Shield_LoRa`) and the Smart Node (`Relay_Shield_LoRa`). This document details how the firmware projects and the LVGL UI interact with these structs.
>
> Following the proven `motor_comm.h` pattern, there are **no intermediate mapping structs** — the UI directly accesses the wire globals `lora_hub_cmd` and `lora_node_status` (declared `extern` in `lora_comm.h`, defined in the application layer / simulator).

---

## 1. System Architecture & Responsibility Boundaries

The system consists of two separate firmware projects communicating peer-to-peer over a `Ra-01H` (Semtech SX1276) LoRa radio link operating at **920 MHz Peer-to-Peer (1 Hub, 1 Node)**:

```
+------------------------------------+        LoRa P2P 920 MHz (Ra-01H)      +------------------------------------+
|            Smart Hub               |  --------------------------------->  |            Smart Node              |
|        (LCD_Shield_LoRa)           |         lora_hub_cmd (8B)            |       (Relay_Shield_LoRa)          |
|                                    |                                      |                                    |
| - 800x480 QSPI LCD + Touch         |  <---------------------------------  | - 4x JQ1P Relays (PD16..PD13)      |
| - Hub-local Device Configuration   |       lora_node_status (20B)         | - SP3485EN RS-485 Sensor Bus       |
| - Polarity & Semantic Mapping      |                                      | - "Dumb" GPIO Actuation            |
| - LVGL v8.4 UI                     |                                      | - Sensor Sampling & Telemetry      |
+------------------------------------+                                      +------------------------------------+
```

### 1.1 The Dumb Node Model
The Smart Node is intentionally dumb:
- It receives raw GPIO levels (0 or 1) for the four relay channels and writes them directly to MCU pins (PD16, PD15, PD14, PD13).
- It has no awareness of appliance names, presets, icons, operational modes, or control rules.
- The Node samples the RS-485 bus (`MXVC10S-B-P67` and `TH10S-B-IP67`) and replies to Hub requests with telemetry and relay GPIO readback.

### 1.2 The Hub Owns All Semantics
- Appliance names, presets, Auto/Manual mode labels, and relay electrical polarity live exclusively in Hub-local memory.
- Relay polarity varies by load: one appliance may be active-high (GPIO 1 = ON), while another wired to normally-closed (NC) terminals or an inverting driver is active-low (GPIO 0 = ON).
- The Hub resolves polarity locally and places raw GPIO levels on the wire:
  $$\text{gpio\_level} = \text{desired\_on} \oplus \text{active\_low}$$
  $$\text{reported\_on} = \text{reported\_gpio} \oplus \text{active\_low}$$
- Both directions evaluate the same `active_low` flag, ensuring bidirectional consistency.
- The owner's Hub firmware receives the 920 MHz packet and copies data directly into the `extern` variables in `lora_comm.h`. The UI does not handle Modbus registers or RF drivers.

---

## 2. Transport & Framing

Communication runs at **920 MHz peer-to-peer (1 Hub to 1 Node)**. Unlike UART streaming protocols, LoRa packets are discrete datagrams handled by the SX127x modem in explicit-header mode:
1. **No SOF/EOF Envelope:** The radio physical layer already provides packet delineation, length verification, and CRC checks. Omitting start/end bytes conserves airtime on duty-cycle-limited bands.
2. **Versioned Fixed Header:** Each frame begins with `{ uint8_t version; uint8_t msg_id; uint16_t node_addr; }` allowing immediate build mismatch rejection and supporting future multi-node pairing without struct renumbering.
3. **Single Combined Status Frame:** Relay readback state and environmental telemetry are multiplexed into a single 20-byte packet to minimize packet collisions, radio duty-cycle consumption, and receiver listening windows.

---

## 3. Wire Messages

### 3.1 Hub → Node: `lora_hub_cmd_t` (8 Bytes)

Transmitted for pending output changes, with up to three attempts. The Hub sends read-only `LORA_MSG_GET_STATUS` (`0x03`) at approximately 1 Hz when idle, at boot, or after a command timeout.

| Offset | Field | Type / Bits | Description |
|---|---|---|---|
| Byte 0 | `version` | `uint8_t` | `LORA_PROTO_VERSION` (`0x02`) |
| Byte 1 | `msg_id` | `uint8_t` | `LORA_MSG_HUB_CMD` (`0x01`) to apply relays, or `LORA_MSG_GET_STATUS` (`0x03`) to leave them unchanged |
| Bytes 2..3 | `node_addr` | `uint16_t` | Target Node address (default `0x0001`, reserved for pairing) |
| Bytes 4..5 | `seq` | `uint16_t` | Command sequence number, incremented on each desired output change; ignored by Node for `GET_STATUS` |
| Byte 6:0 | `relay1_gpio` | `uint8_t : 1` | Desired level for Relay 1 (PD16): `0` = Low, `1` = High |
| Byte 6:1 | `relay2_gpio` | `uint8_t : 1` | Desired level for Relay 2 (PD15): `0` = Low, `1` = High |
| Byte 6:2 | `relay3_gpio` | `uint8_t : 1` | Desired level for Relay 3 (PD14): `0` = Low, `1` = High |
| Byte 6:3 | `relay4_gpio` | `uint8_t : 1` | Desired level for Relay 4 (PD13): `0` = Low, `1` = High |
| Byte 6:4..7 | `reserved_cmd`| `uint8_t : 4` | Reserved padding, always `0` |
| Byte 7 | `reserved_pad`| `uint8_t` | Alignment padding, always `0` |

---

### 3.2 Node → Hub: `lora_node_status_t` (20 Bytes)

Sent by the Node in response to a command or a read-only status request. Sensor values are the most recently completed RS-485 poll cycle.

| Offset | Field | Type / Bits | Description |
|---|---|---|---|
| Byte 0 | `version` | `uint8_t` | `LORA_PROTO_VERSION` (`0x02`) |
| Byte 1 | `msg_id` | `uint8_t` | `LORA_MSG_NODE_STATUS` (`0x02`) |
| Bytes 2..3 | `node_addr` | `uint16_t` | Source Node address (default `0x0001`) |
| Bytes 4..5 | `seq_echo` | `uint16_t` | Last received command sequence number applied by Node |
| Byte 6:0 | `relay1_gpio` | `uint8_t : 1` | Actual readback GPIO level for Relay 1 (PD16) |
| Byte 6:1 | `relay2_gpio` | `uint8_t : 1` | Actual readback GPIO level for Relay 2 (PD15) |
| Byte 6:2 | `relay3_gpio` | `uint8_t : 1` | Actual readback GPIO level for Relay 3 (PD14) |
| Byte 6:3 | `relay4_gpio` | `uint8_t : 1` | Actual readback GPIO level for Relay 4 (PD13) |
| Byte 6:4 | `co2_valid` | `uint8_t : 1` | `1` = Valid eCO₂ reading, `0` = Invalid / warm-up / fault |
| Byte 6:5 | `voc_valid` | `uint8_t : 1` | `1` = Valid TVOC reading, `0` = Invalid / warm-up / fault |
| Byte 6:6 | `temp_valid` | `uint8_t : 1` | `1` = Valid temperature reading, `0` = Sensor fault |
| Byte 6:7 | `humid_valid`| `uint8_t : 1` | `1` = Valid humidity reading, `0` = Sensor fault |
| Byte 7:0 | `rs485_fault` | `uint8_t : 1` | `1` = RS-485 transceiver or bus physical fault |
| Byte 7:1 | `co2_timeout` | `uint8_t : 1` | `1` = `MXVC10S-B-P67` sensor timeout |
| Byte 7:2 | `th_timeout` | `uint8_t : 1` | `1` = `TH10S-B-IP67` sensor timeout |
| Byte 7:3..7 | `reserved_faults`| `uint8_t : 5` | Reserved fault bits, always `0` |
| Bytes 8..11 | `uptime_sec` | `uint32_t` | Node uptime in seconds |
| Bytes 12..13| `co2_ppm` | `uint16_t` | Equivalent CO2 concentration in ppm (sensor range `400..60000`) |
| Bytes 14..15| `tvoc_ppb` | `uint16_t` | TVOC concentration in ppb (sensor range `0..60000`), without index conversion or clamping |
| Bytes 16..17| `temp_deci_c`| `int16_t` | Signed temperature in tenths of a degree Celsius ($0.1\ ^\circ\text{C}$, e.g. $235 = +23.5\ ^\circ\text{C}$, $-25 = -2.5\ ^\circ\text{C}$) |
| Byte 18 | `humidity_pct`| `uint8_t` | Relative humidity in whole percent (`0..100 %`) |
| Byte 19 | `sample_seq`| `uint8_t` | Increments after both sensors have been polled; repeated status replies with the same sample do not advance Auto qualification |

The Hub treats a sensor as unavailable when its `*_valid` bit is `0`, a related fault bit is set, or the value is outside the sensor range: eCO₂ below `400` or above `60000` ppm, or TVOC above `60000` ppb.

### 3.3 Request/Response Exchange (v2)

```
Hub  ──[ GET_STATUS, 8B ]────────▶  Node   ~1 Hz when idle, at boot, after a command timeout
Hub  ──[ HUB_CMD, 8B ]───────────▶  Node   Only while a UI command is pending, up to 3 attempts
Hub  ◀─[ lora_node_status_t, 20B ]── Node  Reply to either request
```

- The Node applies relay levels only for `LORA_MSG_HUB_CMD`. For `LORA_MSG_GET_STATUS` it leaves the outputs unchanged and replies with the current readback and latest sensor sample.
- The Node never transmits unsolicited. Because of this, the Hub must send `GET_STATUS` **before** `ui_is_tx_ready()` becomes `true`. Otherwise no first report arrives and the UI never becomes TX-ready.
- The Hub sends `HUB_CMD` only when `ui_is_tx_ready()` and `ui_has_pending_command()` are both `true`. A command counts as pending until the Node reports it (settled) or `UI_CMD_TIMEOUT_MS` (3000 ms) expires. Retries must fit inside that window.
- Version `0x02` is not compatible with `0x01`. Each side rejects frames whose `version` byte differs, so the Hub and Node firmware must be updated together.

---

## 4. UI State Machine: Desired vs Reported

The UI **never assumes command echo**. The display strictly reflects what the Node reports back:

```
[ User Interaction ] ---> desired_on updated ---> gpio_level = desired_on ^ active_low ---> lora_hub_cmd updated
                                                                                                  |
                                                                                           (LoRa TX / RX)
                                                                                                  v
[ UI Display ] <--------- reported_on = reported_gpio ^ active_low <----------------- lora_node_status received
```

For each relay channel $i \in \{0..3\}$:
1. **Settled State (`cmd->phase == UI_CMD_PHASE_IDLE`):**
   - The relay switch displays `reported_on`.
   - The switch is enabled for user interaction.
   - Status text is hidden (single-line mode display).
2. **In-Flight Command (`cmd->phase == UI_CMD_PHASE_PENDING` and $\text{age} < \text{UI\_CMD\_TIMEOUT\_MS}$):**
   - Tile displays `"Sending…"`.
   - Switch is disabled in the `desired_on` position.
   - Settling condition: Command transitions to settled IDLE when $\mathbf{online} \land (\text{reported\_on} == \text{desired\_on}) \land ((\text{int16\_t})(\text{seq\_echo} - \text{cmd->seq}) \ge 0)$.
     Requiring `seq_echo >= cmd->seq` guarantees that the incoming report confirms execution of the current command, preventing stale in-flight status frames from settling a new command prematurely.
3. **Timeout / Failed Command (`cmd->phase == UI_CMD_PHASE_ERROR` and $\text{age} \ge \text{UI\_CMD\_TIMEOUT\_MS}$):**
   - Tile displays `"Unknown"` with the `"Retry"` button.
   - Switch is usable again to allow re-trying.
   - On timeout the UI writes the channel's `reported_gpio` back into `lora_hub_cmd`, so a later command frame cannot re-apply the failed level by accident.
   - If a delayed report eventually arrives with `reported_on == desired_on`, the device recovers to IDLE and the wire bit is synchronized to the report again.
4. **Unsolicited State Change (e.g. Node reboot, local override):**
   - When settled idle and a remote flip is reported ($\text{reported\_on} \ne \text{desired\_on}$), the UI adopts the remote state:
     $$\text{s\_desired\_on}[i] = \text{reported\_on}$$
     $$\text{lora\_hub\_cmd\_set\_relay\_gpio}(\&lora\_hub\_cmd, i, \text{reported\_gpio})$$
   - Updating `lora_hub_cmd` keeps the next command consistent with the Node's reported state. Idle traffic uses `GET_STATUS`, so it cannot change relay outputs.
5. **Node Reboot (`uptime_sec` decreases):**
   - When a fresh report shows a lower `uptime_sec` than the previous one, the Hub drops TX readiness and re-runs startup synchronization: it adopts all four reported GPIO levels at once and does not replay older desired states.
   - The Auto sample tracker is reset, so the first report after the reboot counts as a new sample.

---

## 5. Offline Link Detection

Connection health is determined by the staleness of the last received status frame. `ui.c` compares `lora_last_rx_tick_ms` with `lv_tick_get()`, so firmware must write that timestamp from the same clock LVGL uses. If firmware writes `HAL_GetTick()`, configure LVGL with `LV_TICK_CUSTOM 1` and `LV_TICK_CUSTOM_SYS_TIME_EXPR (HAL_GetTick())` rather than a separate `lv_tick_inc()` counter. Mixing two clocks can make the unsigned age wrap, and the link then appears disconnected permanently.
- $\text{now} - \text{lora\_last\_rx\_tick\_ms} < \text{LORA\_LINK\_TIMEOUT\_MS}\ (5000\text{ ms}) \implies \mathbf{Connected}$.
- $\text{now} - \text{lora\_last\_rx\_tick\_ms} \ge \text{LORA\_LINK\_TIMEOUT\_MS}\ (5000\text{ ms}) \implies \mathbf{Disconnected}$.

When $\mathbf{Disconnected}$:
- Header displays `Disconnected` with secondary text color.
- All live sensor numbers are masked with em-dashes. The Home eCO₂ and TVOC units stay visible (`— ppm`, `— ppb`); temperature and humidity hide their units.
- Quality badges display `· Unknown`.
- Device tiles display `Unknown` with em-dash `—`, switches are hidden, and user toggle clicks are rejected.

---

## 6. Hub-Local Automation & Wire Contract

Auto does not add any fields to the wire contract. Protocol v2 (2026-09-25) changes message semantics but keeps the frame sizes:
- `lora_hub_cmd_t` remains 8 bytes; `lora_node_status_t` remains 20 bytes. v2 adds `LORA_MSG_GET_STATUS`, redefines byte 14..15 as `tvoc_ppb` and byte 19 as `sample_seq`.
- The Node remains a completely dumb GPIO actuator applying received levels verbatim and echoing readbacks.
- **Hub-Local Execution:** Automation evaluation (`ui_auto.c`) executes strictly on the Hub MCU using telemetry already present in `lora_node_status`. Thresholds, qualification counters, hold timers, and control modes live exclusively in Hub RAM/NVS.
- **Fresh-Sample Qualification:** Auto counts a report toward its 3-frame qualification only when `sample_seq` differs from the last evaluated sample (or the first sample after boot, reconnect or Node reboot). Command replies and repeated `GET_STATUS` replies that carry the same sample do not speed up switching.
- **Boot TX Gating:** Outbound radio commands require `ui_is_tx_ready()` and a pending UI command. Read-only status requests establish the initial relay state without changing outputs.
- **Settled Idle Wire Sync:** In settled idle, the Hub maintains $cmd\_gpio == reported\_gpio$ unconditionally across all 4 channels. Idle polling uses `GET_STATUS`, so the stored command frame is not retransmitted in idle.
