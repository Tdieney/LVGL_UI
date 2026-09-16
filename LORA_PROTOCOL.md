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
- The owner's Node firmware samples the RS-485 bus (`MXVC10S-B-P67` and `TH10S-B-IP67`), packs telemetry and readback GPIO levels, and broadcasts `lora_node_status_t` over 920 MHz LoRa.

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

Transmitted whenever desired output states change, and repeated periodically (cadence ~1000 ms) so lost packets self-heal.

| Offset | Field | Type / Bits | Description |
|---|---|---|---|
| Byte 0 | `version` | `uint8_t` | `LORA_PROTO_VERSION` (`0x01`) |
| Byte 1 | `msg_id` | `uint8_t` | `LORA_MSG_HUB_CMD` (`0x01`) |
| Bytes 2..3 | `node_addr` | `uint16_t` | Target Node address (default `0x0001`, reserved for pairing) |
| Bytes 4..5 | `seq` | `uint16_t` | Command sequence number, incremented on each desired output change |
| Byte 6:0 | `relay1_gpio` | `uint8_t : 1` | Desired level for Relay 1 (PD16): `0` = Low, `1` = High |
| Byte 6:1 | `relay2_gpio` | `uint8_t : 1` | Desired level for Relay 2 (PD15): `0` = Low, `1` = High |
| Byte 6:2 | `relay3_gpio` | `uint8_t : 1` | Desired level for Relay 3 (PD14): `0` = Low, `1` = High |
| Byte 6:3 | `relay4_gpio` | `uint8_t : 1` | Desired level for Relay 4 (PD13): `0` = Low, `1` = High |
| Byte 6:4..7 | `reserved_cmd`| `uint8_t : 4` | Reserved padding, always `0` |
| Byte 7 | `reserved_pad`| `uint8_t` | Alignment padding, always `0` |

---

### 3.2 Node → Hub: `lora_node_status_t` (20 Bytes)

Broadcast periodically by the Smart Node (cadence ~1000 ms) over LoRa.

| Offset | Field | Type / Bits | Description |
|---|---|---|---|
| Byte 0 | `version` | `uint8_t` | `LORA_PROTO_VERSION` (`0x01`) |
| Byte 1 | `msg_id` | `uint8_t` | `LORA_MSG_NODE_STATUS` (`0x02`) |
| Bytes 2..3 | `node_addr` | `uint16_t` | Source Node address (default `0x0001`) |
| Bytes 4..5 | `seq_echo` | `uint16_t` | Last received command sequence number applied by Node |
| Byte 6:0 | `relay1_gpio` | `uint8_t : 1` | Actual readback GPIO level for Relay 1 (PD16) |
| Byte 6:1 | `relay2_gpio` | `uint8_t : 1` | Actual readback GPIO level for Relay 2 (PD15) |
| Byte 6:2 | `relay3_gpio` | `uint8_t : 1` | Actual readback GPIO level for Relay 3 (PD14) |
| Byte 6:3 | `relay4_gpio` | `uint8_t : 1` | Actual readback GPIO level for Relay 4 (PD13) |
| Byte 6:4 | `co2_valid` | `uint8_t : 1` | `1` = Valid CO₂ reading, `0` = Invalid / warm-up / fault |
| Byte 6:5 | `voc_valid` | `uint8_t : 1` | `1` = Valid VOC reading, `0` = Invalid / warm-up / fault |
| Byte 6:6 | `temp_valid` | `uint8_t : 1` | `1` = Valid temperature reading, `0` = Sensor fault |
| Byte 6:7 | `humid_valid`| `uint8_t : 1` | `1` = Valid humidity reading, `0` = Sensor fault |
| Byte 7:0 | `rs485_fault` | `uint8_t : 1` | `1` = RS-485 transceiver or bus physical fault |
| Byte 7:1 | `co2_timeout` | `uint8_t : 1` | `1` = `MXVC10S-B-P67` sensor timeout |
| Byte 7:2 | `th_timeout` | `uint8_t : 1` | `1` = `TH10S-B-IP67` sensor timeout |
| Byte 7:3..7 | `reserved_faults`| `uint8_t : 5` | Reserved fault bits, always `0` |
| Bytes 8..11 | `uptime_sec` | `uint32_t` | Node uptime in seconds |
| Bytes 12..13| `co2_ppm` | `uint16_t` | CO₂ concentration in ppm (`0..10000`) |
| Bytes 14..15| `voc_index` | `uint16_t` | Unitless VOC index (`0..500`) |
| Bytes 16..17| `temp_deci_c`| `int16_t` | Signed temperature in tenths of a degree Celsius ($0.1\ ^\circ\text{C}$, e.g. $235 = +23.5\ ^\circ\text{C}$, $-25 = -2.5\ ^\circ\text{C}$) |
| Byte 18 | `humidity_pct`| `uint8_t` | Relative humidity in whole percent (`0..100 %`) |
| Byte 19 | `reserved_pad`| `uint8_t` | Alignment padding, always `0` |

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
   - If a delayed report eventually arrives with `reported_on == desired_on`, the device recovers to IDLE.
4. **Unsolicited State Change (e.g. Node reboot, local override):**
   - When settled idle and a remote flip is reported ($\text{reported\_on} \ne \text{desired\_on}$), the UI adopts the remote state:
     $$\text{s\_desired\_on}[i] = \text{reported\_on}$$
     $$\text{lora\_hub\_cmd\_set\_relay\_gpio}(\&lora\_hub\_cmd, i, \text{reported\_gpio})$$
   - Updating `lora_hub_cmd` immediately ensures that the UI's desired state and the periodically transmitted wire frame move together. When the Hub retransmits its periodic command frame (~1000 ms cadence), it retransmits the adopted state rather than fighting against the local override, preventing 1 Hz relay oscillation on real hardware.

---

## 5. Offline Link Detection

Connection health is determined by the staleness of the last received status frame:
- $\text{now} - \text{lora\_last\_rx\_tick\_ms} < \text{LORA\_LINK\_TIMEOUT\_MS}\ (5000\text{ ms}) \implies \mathbf{Connected}$.
- $\text{now} - \text{lora\_last\_rx\_tick\_ms} \ge \text{LORA\_LINK\_TIMEOUT\_MS}\ (5000\text{ ms}) \implies \mathbf{Disconnected}$.

When $\mathbf{Disconnected}$:
- Header displays `Disconnected` with secondary text color.
- All live sensor numbers are masked with em-dashes (`—°C`, `—%`, `— ppm`, `—`).
- Quality badges display `· Unknown`.
- Device tiles display `Unknown` with em-dash `—`, switches are hidden, and user toggle clicks are rejected.

---

## 6. Hub-Local Automation & Wire Contract Invariance

The wire contract between Smart Hub and Smart Node is **completely invariant** under Auto v1:
- `lora_hub_cmd_t` remains 8 bytes; `lora_node_status_t` remains 20 bytes.
- The Node remains a completely dumb GPIO actuator applying received levels verbatim and echoing readbacks.
- **Hub-Local Execution:** Automation evaluation (`ui_auto.c`) executes strictly on the Hub MCU using telemetry already present in `lora_node_status`. Thresholds, qualification counters, hold timers, and control modes live exclusively in Hub RAM/NVS.
- **Boot TX Gating:** Outbound radio command transmission is gated by `ui_is_tx_ready()`. The Hub suppresses periodic command packets at boot until the first valid status frame from the Node is reconciled, preventing unwanted relay chatter or toggling physical loads upon power-up.
- **Settled Idle Wire Sync:** In settled idle, the Hub maintains $cmd\_gpio == reported\_gpio$ unconditionally across all 4 channels, preventing spurious 1 Hz oscillation across retransmissions.
