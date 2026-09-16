# Smart Hub / Smart Node hardware overview

This note summarizes the two one-page schematics supplied on 2026-09-05. The
schematics are hardware references, not instructions. Pin names below are
transcribed from the drawings and must be checked against PCB/ERC outputs before
firmware pin assignment is frozen.

## Inferred system split

- **Smart Hub:** MCU carrier + 800x480-class LCD/touch shield + LoRa module.
- **Smart Node:** MCU carrier + LoRa module + four relay outputs + one shared
  RS-485 bus for environmental sensors.
- The intended Hub↔Node application protocol, number of nodes, addressing,
  retries, security, and offline rules are not defined yet.

## Smart Hub — LCD shield

### LCD and touch

| Function | MCU pin |
|---|---|
| QSPI clock | PD15 |
| QSPI data 0..3 | PD14, PD13, PD18, PD17 |
| LCD chip select | PD16 |
| LCD data/command | PD2 |
| LCD tearing-effect input | PD1 |
| LCD reset | PD0 |
| Touch reset | PD19 |
| Touch interrupt | PD25 |
| Touch I²C SCL/SDA | PD26 / PD27 |

The LCD uses a 40-pin top-contact FPC (`AFC07-S40ECA-00` in the schematic), 5 V
panel supply, and 3.3 V touch logic. The exact LCD and touch controller part
numbers are still required.

### LoRa

Both shields use a `Ra-01H` module with an SMA antenna connector:

| Function | MCU pin |
|---|---|
| NSS | ADC17 |
| MOSI / MISO / SCK | ADC14 / ADC13 / ADC12 |
| Reset | PD28 |
| DIO0 / DIO1 | ADC10 / ADC11 |

The module is powered from `3V3-AUX`. Operating frequency is confirmed as **920 MHz peer-to-peer (1 Hub, 1 Node)**. The application firmware layer handles the radio and Modbus communication, copying wire frames directly into the `extern` variables in `lora_comm.h`.

## Smart Node — relay and sensor shield

### Relay outputs

- Four `JQ1P-12V-F` changeover relays expose NC/COM/NO terminals.
- MCU pins PD16, PD15, PD14, and PD13 drive relay 1..4 respectively.
- Each coil is low-side switched by an `SI2300DS` MOSFET and has an `SS14`
  flyback diode plus a blue channel indicator.
- Safe boot polarity, contact load rating, default states, interlocks, and
  manual-versus-automatic ownership still need product decisions.
- **Actuation & Polarity Model (Decided 2026-09-09):** The Smart Node is deliberately
  dumb — it applies received GPIO levels verbatim to pins PD16..PD13 and reports the
  actual pin states back. It knows nothing about appliances, names, or polarity.
  The Smart Hub owns all appliance semantics and resolves polarity locally:
  `gpio_level = desired_on ^ active_low` and `reported_on = reported_gpio ^ active_low`.

### RS-485 sensor bus

- `SP3485EN-L/TR` 3.3 V transceiver.
- PD0 = RX, PD1 = driver/receiver enable, PD2 = TX.
- Switchable 120 Ω termination, A/B bias resistors, and `SM712` surge protection.
- Two four-pin connectors expose GND, 12 V, RS-485 A, and RS-485 B, supporting a
  shared/daisy-chain bus for the two planned sensors.

Planned sensors:

- `MXVC10S-B-P67`: RS-485 transmitter reporting equivalent CO₂ (eCO₂) and TVOC.
- `TH10S-B-IP67`: dust/water-resistant RS-485 Modbus RTU temperature/humidity
  transmitter. Reseller listings commonly state 9–30 VDC supply, 0.1-unit
  resolution, and configurable baud rates, but published temperature ranges
  conflict; treat all values as provisional.

Sensor Modbus acquisition on the RS-485 bus is implemented entirely in the Smart Node firmware by the owner, which packs the scaled readings into `lora_node_status` and broadcasts over 920 MHz LoRa. The UI layer does not query Modbus registers or require vendor datasheets.

### Power

The shield accepts 12 VDC and uses an `AP63205WU-7` buck stage to produce 5 V.
Relay coils and sensor connectors use the 12 V rail; the MCU carrier provides or
routes the 3.3 V auxiliary rail shown in the schematic.

## Candidate UI data — not requirements yet

- Per-node online state, LoRa signal/age, sensor bus health, and last update time.
- Temperature, relative humidity, eCO₂, and TVOC current values and trends.
- Four named relay states, command progress/failure, automation ownership, and
  explicit safe-state indication.
- Alarm state with threshold, hysteresis, acknowledgement, and stale-data
  semantics.

These are discovery inputs only. Screen structure should be decided with the
operator workflow before implementation.
