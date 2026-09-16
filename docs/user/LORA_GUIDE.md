# LoRa for Smart Hub / Smart Node — Theory to Ra-01H & STM32 HAL

Self-study reference guide for engineers new to LoRa. Goal: provide sufficient understanding to implement the radio firmware for the **two peer-to-peer projects** in this system:
- `LCD_Shield_LoRa` (Hub)
- `Relay_Shield_LoRa` (Node)

The software wire contract is specified in [`lora_comm.h`](../../lora_comm.h) and [`LORA_PROTOCOL.md`](../../LORA_PROTOCOL.md). This document covers the underlying physical layer: RF modulation, radio parameters, regulations, hardware modules, and drivers.

---

## 1. What is LoRa and How Does It Differ from WiFi/BLE?

**LoRa** (Long Range) is a physical layer modulation proprietary to Semtech based on **Chirp Spread Spectrum (CSS)**. Instead of encoding bits via amplitude or phase shifts, LoRa encodes bits into **chirps** — carrier frequency sweeps across the allocated channel bandwidth.

Key physical attribute:

> **LoRa demodulates signals situated well BELOW the noise floor.**

While WiFi requires a positive SNR (typically >= 10 dB) to decode packets, LoRa at SF12 successfully demodulates packets at **SNR = -20 dB** (signal 100 times weaker than background noise). This enables multiple kilometers of urban line-of-sight range with low transmission power (25–100 mW).

Trade-off: **Low throughput**. Data rates range from ~0.3 kbps (SF12) to ~11 kbps (SF7) at 125 kHz bandwidth:

| Metric | WiFi | BLE | LoRa |
|---|---|---|---|
| Throughput | 10⁵–10⁶ kbps | 10³ kbps | **0.3–11 kbps** |
| Range | ~30 m | ~30 m | **1–10 km** |
| Sensitivity | -90 dBm | -95 dBm | **-137 dBm** |
| Best Suited For | Video, large transfers | Peripherals, short telemetry | **Sparse telemetry, remote actuation** |

This system transmits an 8-byte command frame and a 20-byte status/sensor frame at ~1 Hz cadence — an ideal operating profile for LoRa.

---

## 2. Core RF Parameters

Both Hub and Node **must be configured with IDENTICAL parameters** across all six settings below for RF synchronization.

### 2.1 Spreading Factor (SF7 → SF12)

The number of chirps per symbol. Incrementing SF **doubles airtime** while improving receiver sensitivity by ~2.5 dB.

| SF | Demodulation SNR Limit | Bitrate (125 kHz BW) | Performance Profile |
|---|---|---|---|
| SF7 | -7.5 dB | 5.47 kbps | Lowest airtime, highest throughput |
| SF8 | -10.0 dB | 3.13 kbps | |
| SF9 | -12.5 dB | 1.76 kbps | |
| SF10 | -15.0 dB | 0.98 kbps | |
| SF11 | -17.5 dB | 0.54 kbps | |
| SF12 | -20.0 dB | 0.29 kbps | Maximum range, highest latency |

These demodulation limits are encoded in [`lora_hub_link.h`](../../lora_hub_link.h) to calculate signal strength indicator bars.

**System Baseline:** Start with **SF7**. Hub and Node are located within the same building; small frame sizes combined with SF7 minimize airtime, conserve duty cycle, and reduce packet collisions. Only increase SF if measured packet error rates warrant it. Note: **Changing SF requires updating `LORA_SPREADING_FACTOR`** in `lora_hub_link.h`.

### 2.2 Bandwidth (BW)

Ranges from 7.8 to 500 kHz. Wider bandwidth increases data throughput but degrades receiver sensitivity by 3 dB per doubling. **Use 125 kHz** — standard balanced configuration matching the SNR threshold table above.

### 2.3 Coding Rate (CR)

Forward Error Correction (FEC): 4/5, 4/6, 4/7, 4/8. CR 4/5 adds 25% redundancy; 4/8 adds 100%. **Use 4/5** for stable indoor/campus links.

### 2.4 Carrier Frequency

**920 MHz** — Baseline frequency documented in [`HARDWARE_OVERVIEW.md`](../HARDWARE_OVERVIEW.md) (refer to Section 4 for regulatory notes).

### 2.5 Sync Word

Network identification byte. Transceivers configured with differing sync words ignore each other **even on identical carrier frequencies**.
- `0x12`: Standard default for private networks.
- `0x34`: Reserved for public LoRaWAN gateways.
**Use `0x12`** to filter out external public LoRaWAN traffic.

### 2.6 Preamble Length, Explicit Header, CRC

- **Preamble**: 8 symbols (standard).
- **Explicit Header**: ENABLED. Transmits payload length, CR, and CRC flag. This enables `lora_comm.h` to dispense with frame delimiters (SOF/EOF).
- **Payload CRC**: ENABLED. Hardware discards corrupt packets; application firmware receives only validated frames.

### 2.7 Time-on-Air (Airtime) Calculations

```
T_symbol = 2^SF / BW
T_preamble = (n_preamble + 4.25) * T_symbol

n_payload = 8 + max( ceil( (8*PL - 4*SF + 28 + 16*CRC - 20*IH) / (4*(SF - 2*DE)) ) * (CR + 4), 0 )
T_payload = n_payload * T_symbol
T_total   = T_preamble + T_payload
```
Where `PL` = payload bytes, `CRC` = 1, `IH` = 0 (explicit header), `DE` = 0 (SF7–SF10 at 125 kHz), `CR` = 1 (4/5).

For **SF7, BW 125 kHz, CR 4/5, CRC enabled**:

| Frame | Payload Size | Approximate Airtime |
|---|---|---|
| `lora_hub_cmd_t` | 8 bytes | **~31 ms** |
| `lora_node_status_t` | 20 bytes | **~51 ms** |

At **SF12** under identical settings, airtimes increase to **~830 ms** and **~1.4 s** — exceeding the 1.0 s transmission interval. Avoid high SF settings unless necessary to prevent network congestion.

---

## 3. Link Budget & Antenna Practices

```
P_rx (dBm) = P_tx (dBm) + G_tx (dBi) - L_path (dB) + G_rx (dBi) - L_cable
Link Margin = P_rx - Sensitivity
```

- **P_tx**: Ra-01H maximum output +20 dBm (100 mW). (Verify regulatory limits in Section 4).
- **Sensitivity**: SX1276 at 125 kHz BW ranges from **-123 dBm** (SF7) to **-136 dBm** (SF12).
- **Antenna Gain (G)**: Standard 920 MHz whip antennas provide 2–3 dBi.

Indoor example (SF7, +20 dBm, 2 dBi antennas, 100 dB path loss):
`P_rx = 20 + 2 - 100 + 2 = -76 dBm` -> `Margin = -76 - (-123) = 47 dB` (substantial headroom).

**Hardware Safety Rules:**
1. **NEVER power the module without an antenna connected.** Transmitting into an open circuit can damage the RF power amplifier. Always attach the SMA antenna before applying power.
2. Maintain **vertical polarization on both antennas**. Cross-polarization (90° offset) introduces ~20 dB attenuation.
3. Position antennas away from metal enclosures, ground planes, and concrete structures.

---

## 4. Regulatory Guidelines (Vietnam & Regional)

> **Notice:** This section provides technical guidelines and does not constitute formal legal counsel. Frequency allocations and power limits are subject to regulatory updates. Verify active standards with relevant telecommunications authorities.

### 4.1 Regulatory Framework

- Spectrum authority: **Authority of Radio Frequency Management (ARFM)** (`rfd.gov.vn`).
- Relevant documentation: Circulars governing license-exempt short-range radio devices (SRDs) and technical operating conditions (e.g. Circular 46/2016/TT-BTTTT, Circular 08/2021/TT-BTTTT, and subsequent revisions).

### 4.2 Operating Bands

| Band | Notes |
|---|---|
| **433.05 – 434.79 MHz** | Crowded ISM band; significant interference from consumer remotes |
| **920 – 925 MHz** | Recommended regional band for LoRa; aligns with **AS923** profile |

**This system utilizes 920 MHz**, situated within the 920–925 MHz band supported by the Ra-01H (803–930 MHz).

Avoid configuring for European 868 MHz or US 915 MHz bands when deploying regionally.

### 4.3 Technical Compliance Points

1. **Maximum Radiated Power**: Verify ERP/EIRP limits (typically 25 mW / +14 dBm). Exceeding configured limits requires power reduction in register configuration (`RegPaConfig`).
2. **Duty Cycle / LBT**: Verify duty cycle ceilings (e.g. 1% average). The current 1-second cadence at ~51 ms airtime yields ~5% duty cycle; if 1% is mandated, extend transmission intervals to ~5 seconds or optimize payload.
3. **Channel Spacing**: Ensure carrier frequency conforms to prescribed sub-band channel rasters.

---

## 5. Peer-to-Peer vs LoRaWAN

**LoRaWAN** introduces a complex MAC layer with gateways, network servers, join procedures (OTAA/ABP), cryptographic sessions, and adaptive data rates.

**This project implements pure LoRa P2P:** A single Hub communicates directly with a single Node over a dedicated link without network servers or gateway infrastructure.

| LoRaWAN Feature | Peer-to-Peer Implementation in this System |
|---|---|
| AES-128 Encryption | Optional application layer extension (Section 10) |
| Device Addressing | `node_addr` field (reserved in `lora_comm.h`) |
| Replay Protection | Sequential frame counters (`seq` / `seq_echo`) |
| Acknowledgments | Status frame readback serves as implicit execution ACK |
| Adaptive Data Rate | Fixed SF profile (SF7) |

---

## 6. Ra-01H Hardware Module

### 6.1 Specifications

| Parameter | Specification |
|---|---|
| Transceiver IC | **Semtech SX1276** |
| Frequency Range | 803 – 930 MHz |
| Max Output Power | +20 dBm (PA_BOOST) |
| Sensitivity | Down to -137 dBm |
| Supply Voltage | 3.3 V (**strictly 3.3 V; not 5 V tolerant**) |
| Current Consumption | TX ~120 mA @ +20 dBm, RX ~12 mA, Sleep ~1 µA |
| Host Interface | SPI Mode 0, up to 10 MHz |
| Manufacturer | Ai-Thinker |

> **Distinction:** Do not confuse Ra-01H with Ra-01SH. Ra-01SH uses the **SX1262** transceiver, which has different register sets and driver requirements (incorporating a `BUSY` pin rather than DIO lines).

### 6.2 Pin Mapping

Schematic net names (carrier board connections; mapping to target MCU GPIOs occurs during board configuration):

| Ra-01H Pin | Carrier Net Name | Functional Role |
|---|---|---|
| NSS | `ADC17` | SPI Chip Select (software GPIO controlled) |
| MOSI | `ADC14` | SPI Data Out |
| MISO | `ADC13` | SPI Data In |
| SCK | `ADC12` | SPI Clock |
| RESET | `PD28` | Active-low Reset |
| DIO0 | `ADC10` | Interrupt: TxDone / RxDone |
| DIO1 | `ADC11` | Interrupt: RxTimeout |
| 3.3V | `3V3-AUX` | Regulated 3.3 V power rail |
| ANT | `RF1` | SMA antenna interface |

### 6.3 Power Decoupling Considerations

Transmitting at +20 dBm draws transient current peaks of **~120 mA**. Inadequate decoupling causes voltage dips that reset the transceiver mid-transmission:
- Dedicated 3.3 V LDO capable of >= 200 mA output.
- Place **10 µF tantalum/ceramic + 100 nF ceramic** capacitors directly adjacent to module power pins.
- If supply brownouts persist, configure transmit power to +14 dBm.

---

## 7. P2P Architecture Implementation

Communication sequence from [`LORA_PROTOCOL.md`](../../LORA_PROTOCOL.md):

```
Hub  ──[ lora_hub_cmd_t, 8B ]──▶  Node     Sent on command update + periodic 1000 ms
Hub  ◀─[ lora_node_status_t, 20B ]── Node  Sent periodically every 1000 ms
```

### 7.1 Continuous RX Mode

Both nodes operate in **continuous receive (RX Continuous)** mode, entering TX mode only for the duration of packet transmission (~30–50 ms) before immediately returning to receive mode.

### 7.2 Collision Avoidance Strategies

Because both transceivers broadcast at ~1000 ms intervals, simultaneous packet transmissions could collide if clocks drift into phase synchronization.

Mitigations:
1. **Randomized Jitter**: Inject a pseudo-random delay (0–100 ms) into the transmission interval to prevent persistent phase lock.
2. **Phase Offset**: Schedule Node transmissions at offset 0 ms and Hub transmissions at offset ~500 ms relative to the reporting window.

### 7.3 Implicit State Acknowledgment

The Hub transmits desired GPIO states. The Node applies them to physical pins and **reports back actual physical states** in its periodic status frame. The UI compares *desired* vs *reported*:
- Match -> Command confirmed; idle switch rendering.
- Mismatch, < 3000 ms (`UI_CMD_TIMEOUT_MS`) -> `Sending…` pending state.
- Mismatch, >= 3000 ms -> `Unknown` error state with accessible `Retry` action.

### 7.4 Dumb Actuator Node Model

The Node performs raw GPIO level writes. All device semantics, application polarities (`active_low`), operating modes (Auto/Manual), and timing reside exclusively on the Hub.

### 7.5 Failsafe Considerations

Firmware developers must define physical safety policies for unlinked operation:
1. **Power-on State**: Initial GPIO drive level prior to receiving the first Hub command frame.
2. **Link Loss Failsafe**: Relay behavior when status reports or Hub commands cease for extended durations.

---

## 8. STM32 HAL Driver Implementation for SX1276

### 8.1 SPI Configuration (STM32CubeMX)

| Parameter | Configuration |
|---|---|
| Mode | Full-Duplex Master |
| CPOL / CPHA | **Low / 1 Edge** (SPI Mode 0) |
| Data Size | 8 bits |
| Bit Order | MSB First |
| Baud Rate | <= 10 MHz (~5 MHz recommended) |
| NSS Management | **Software** (driven via GPIO) |

Configure `NSS` as GPIO Output Push-Pull (default HIGH). Configure `RESET` as GPIO Output. Configure `DIO0` as GPIO External Interrupt (Rising edge).

### 8.2 Register Access Primitives

SX1276 address byte format: **Bit 7 = 1 for Write, Bit 7 = 0 for Read**.

```c
#define LORA_NSS_LOW()   HAL_GPIO_WritePin(LORA_NSS_PORT, LORA_NSS_PIN, GPIO_PIN_RESET)
#define LORA_NSS_HIGH()  HAL_GPIO_WritePin(LORA_NSS_PORT, LORA_NSS_PIN, GPIO_PIN_SET)

static uint8_t sx_read_reg(uint8_t addr)
{
    uint8_t tx[2] = { addr & 0x7F, 0x00 };
    uint8_t rx[2] = { 0 };
    LORA_NSS_LOW();
    HAL_SPI_TransmitReceive(&hspi1, tx, rx, 2, HAL_MAX_DELAY);
    LORA_NSS_HIGH();
    return rx[1];
}

static void sx_write_reg(uint8_t addr, uint8_t val)
{
    uint8_t tx[2] = { addr | 0x80, val };
    LORA_NSS_LOW();
    HAL_SPI_Transmit(&hspi1, tx, 2, HAL_MAX_DELAY);
    LORA_NSS_HIGH();
}

static void sx_write_fifo(const uint8_t *buf, uint8_t len)
{
    uint8_t addr = 0x00 | 0x80;          /* RegFifo write */
    LORA_NSS_LOW();
    HAL_SPI_Transmit(&hspi1, &addr, 1, HAL_MAX_DELAY);
    HAL_SPI_Transmit(&hspi1, (uint8_t *)buf, len, HAL_MAX_DELAY);
    LORA_NSS_HIGH();
}

static void sx_read_fifo(uint8_t *buf, uint8_t len)
{
    uint8_t addr = 0x00 & 0x7F;          /* RegFifo read */
    LORA_NSS_LOW();
    HAL_SPI_Transmit(&hspi1, &addr, 1, HAL_MAX_DELAY);
    HAL_SPI_Receive(&hspi1, buf, len, HAL_MAX_DELAY);
    LORA_NSS_HIGH();
}
```

### 8.3 Essential Transceiver Registers

| Address | Register Name | Description |
|---|---|---|
| `0x00` | `RegFifo` | FIFO read/write data buffer |
| `0x01` | `RegOpMode` | Operating mode: SLEEP/STDBY/TX/RXCONTINUOUS + LoRa mode bit |
| `0x06–08` | `RegFrf` | Carrier frequency configuration (MSB/MID/LSB) |
| `0x09` | `RegPaConfig` | Power amplifier selection (PA_BOOST) and output level |
| `0x0E` | `RegFifoTxBaseAddr` | Base address for TX FIFO buffer |
| `0x0F` | `RegFifoRxBaseAddr` | Base address for RX FIFO buffer |
| `0x10` | `RegFifoRxCurrentAddr` | Start address of last received packet in FIFO |
| `0x12` | `RegIrqFlags` | Interrupt flags (write 1 to clear) |
| `0x13` | `RegRxNbBytes` | Number of payload bytes received |
| `0x19` | `RegPktSnrValue` | Signed packet SNR (divide by 4 for dB) |
| `0x1A` | `RegPktRssiValue` | Packet RSSI register |
| `0x1D` | `RegModemConfig1` | Bandwidth, Coding Rate, Explicit Header mode |
| `0x1E` | `RegModemConfig2` | Spreading Factor, CRC enable |
| `0x22` | `RegPayloadLength` | Outbound payload length |
| `0x26` | `RegModemConfig3` | LowDataRateOptimize and Auto AGC settings |
| `0x39` | `RegSyncWord` | Network synchronization word |
| `0x40` | `RegDioMapping1` | DIO0 interrupt mapping |
| `0x42` | `RegVersion` | Silicon version identifier (**must read `0x12`**) |

### 8.4 Hardware Probe & Initialization

```c
bool lora_reset_and_probe(void)
{
    HAL_GPIO_WritePin(LORA_RST_PORT, LORA_RST_PIN, GPIO_PIN_RESET);
    HAL_Delay(1);                       /* Hold reset low > 100 µs */
    HAL_GPIO_WritePin(LORA_RST_PORT, LORA_RST_PIN, GPIO_PIN_SET);
    HAL_Delay(10);                      /* Recovery delay > 5 ms */

    uint8_t ver = sx_read_reg(0x42);
    return (ver == 0x12);               /* Validates SX1276 presence */
}

#define LORA_FREQ_HZ    920000000UL     /* 920 MHz */
#define LORA_SF         7
#define LORA_SYNC_WORD  0x12
#define LORA_TX_DBM     14              /* Standard compliant level */

void lora_init(void)
{
    sx_write_reg(0x01, 0x00);           /* SLEEP mode, FSK */
    sx_write_reg(0x01, 0x80);           /* SLEEP mode, LoRa */
    sx_write_reg(0x01, 0x81);           /* STANDBY mode, LoRa */

    /* Carrier frequency: Frf = (Freq * 2^19) / 32 MHz */
    uint64_t frf = ((uint64_t)LORA_FREQ_HZ << 19) / 32000000UL;
    sx_write_reg(0x06, (uint8_t)(frf >> 16));
    sx_write_reg(0x07, (uint8_t)(frf >> 8));
    sx_write_reg(0x08, (uint8_t)(frf));

    /* PA_BOOST configuration: OutputPower = P_out - 2 */
    sx_write_reg(0x09, 0x80 | (LORA_TX_DBM - 2));
    sx_write_reg(0x0B, 0x2B);           /* Over-current protection ~120 mA */

    /* BW = 125 kHz (0x7), CR = 4/5 (0x1), Explicit Header (0) */
    sx_write_reg(0x1D, (0x7 << 4) | (0x1 << 1) | 0x00);
    /* SF = 7, CRC Enabled (0x04) */
    sx_write_reg(0x1E, (LORA_SF << 4) | 0x04);
    /* LowDataRateOptimize (required for SF11/SF12 @ 125k), AGC auto enabled */
    sx_write_reg(0x26, (LORA_SF >= 11 ? 0x08 : 0x00) | 0x04);

    sx_write_reg(0x20, 0x00);           /* Preamble length MSB */
    sx_write_reg(0x21, 0x08);           /* Preamble length = 8 symbols */
    sx_write_reg(0x39, LORA_SYNC_WORD);

    sx_write_reg(0x0E, 0x00);           /* FifoTxBaseAddr = 0 */
    sx_write_reg(0x0F, 0x00);           /* FifoRxBaseAddr = 0 */

    lora_start_rx();
}
```

### 8.5 Transmission and Reception

```c
void lora_send(const uint8_t *buf, uint8_t len)
{
    sx_write_reg(0x01, 0x81);           /* STANDBY mode */
    sx_write_reg(0x40, 0x40);           /* Map DIO0 to TxDone */
    sx_write_reg(0x0E, 0x00);
    sx_write_reg(0x0D, 0x00);           /* FifoAddrPtr = 0 */
    sx_write_fifo(buf, len);
    sx_write_reg(0x22, len);            /* PayloadLength */
    sx_write_reg(0x12, 0xFF);           /* Clear all IRQ flags */
    sx_write_reg(0x01, 0x83);           /* Enter TX mode */
}

void lora_start_rx(void)
{
    sx_write_reg(0x40, 0x00);           /* Map DIO0 to RxDone */
    sx_write_reg(0x12, 0xFF);           /* Clear all IRQ flags */
    sx_write_reg(0x0D, 0x00);           /* FifoAddrPtr = 0 */
    sx_write_reg(0x01, 0x85);           /* Enter RXCONTINUOUS mode */
}

bool lora_read_packet(uint8_t *buf, uint8_t *len, int8_t *snr_db, int8_t *rssi_dbm)
{
    uint8_t flags = sx_read_reg(0x12);
    sx_write_reg(0x12, 0xFF);                   /* Clear IRQ flags */

    if ((flags & 0x40) == 0) return false;      /* RxDone not asserted */
    if (flags & 0x20)        return false;      /* PayloadCrcError: discard frame */

    uint8_t n = sx_read_reg(0x13);              /* RegRxNbBytes */
    uint8_t cur = sx_read_reg(0x10);            /* RegFifoRxCurrentAddr */
    sx_write_reg(0x0D, cur);
    sx_read_fifo(buf, n);
    *len = n;

    int8_t raw_snr = (int8_t)sx_read_reg(0x19);
    *snr_db = (int8_t)(raw_snr / 4);            /* Register unit: 0.25 dB */

    uint8_t raw_rssi = sx_read_reg(0x1A);
    /* High band (> 779 MHz): RSSI = -157 + raw_rssi + (snr < 0 ? snr : 0) */
    int16_t rssi = -157 + (int16_t)raw_rssi;
    if (*snr_db < 0) rssi += *snr_db;
    *rssi_dbm = (int8_t)rssi;

    return true;
}
```

---

## 9. Integration with Smart Hub Firmware Loop

### 9.1 Handling Received Frames

```c
uint8_t buf[64]; uint8_t len; int8_t snr, rssi;

if (lora_read_packet(buf, &len, &snr, &rssi)) {
    if (len == LORA_NODE_STATUS_LEN &&
        buf[0] == LORA_PROTO_VERSION &&
        buf[1] == LORA_MSG_NODE_STATUS) {

        memcpy(lora_node_status.bytes, buf, LORA_NODE_STATUS_LEN);

        lora_last_snr        = snr;
        lora_last_rssi       = rssi;
        lora_last_rx_tick_ms = lv_tick_get();
        lora_rx_revision++;          /* Triggers UI refresh */
    }
}
```

### 9.2 Outbound Transmission Loop (Gated TX)

All transmission branches (periodic 1000 ms heartbeat and instant dispatch upon sequence changes) must be gated by `ui_is_tx_ready()`:

```c
static uint16_t s_last_sent_seq = 0;
static uint32_t s_last_tx_ms = 0;

uint32_t now = lv_tick_get();
bool seq_changed = (lora_hub_cmd.bits.seq != s_last_sent_seq);
bool periodic_due = (now - s_last_tx_ms >= 1000u);

if (ui_is_tx_ready() && (seq_changed || periodic_due)) {
    lora_send(lora_hub_cmd.bytes, LORA_HUB_CMD_LEN);
    s_last_sent_seq = lora_hub_cmd.bits.seq;
    s_last_tx_ms = now;
}
```

---

## 10. Security Considerations

Frames transmit in plaintext without cryptographic authentication. Any device operating on 920 MHz with sync word `0x12` can sniff telemetry or forge relay commands.

Future enhancements for commercial production:
1. **Message Integrity Code (MIC/CMAC)**: Compute a 4-byte CMAC over frame data and sequence counter using a pre-shared key.
2. **Payload Encryption (AES-128-CTR)**: Encrypt telemetry and command payloads.
3. **Key Exchange & Pairing**: Secure device pairing using the reserved `node_addr` field.

---

## 11. Bring-up Procedure

Execute in strict sequence:

1. [ ] Connect SMA antenna **before** powering board.
2. [ ] Verify 3.3 V rail stability and presence of 10 µF decoupling capacitor.
3. [ ] Confirm `sx_read_reg(0x42)` returns **`0x12`**.
4. [ ] Map carrier net names to MCU hardware pins in CubeMX.
5. [ ] Configure identical RF parameters on both boards: frequency, SF, BW, CR, sync word, CRC.
6. [ ] Transmit fixed test strings and verify reception via UART debugging before integrating `lora_comm.h`.
7. [ ] Validate received SNR and RSSI telemetry.
8. [ ] Transition to native `lora_hub_cmd_t` and `lora_node_status_t` frames.
9. [ ] Validate frame lengths and protocol versions before memory copying.
10. [ ] Link frames to global state variables, confirming `lora_rx_revision++`.
11. [ ] Test antenna disconnection: Hub must indicate `Disconnected` after 5 seconds and recover upon reconnection.
12. [ ] Measure operational duty cycle to verify compliance with Section 4.3.

---

## 12. Troubleshooting Reference

| Symptom | Probable Cause |
|---|---|
| `RegVersion` != 0x12 | SPI pin configuration error, hardware NSS conflict, transceiver in reset |
| Transceivers cannot communicate | Parameter mismatch in sync word, SF, BW, CR, or carrier frequency |
| Corrupted packets received | Payload CRC disabled, or failing to check `PayloadCrcError` |
| Packet loss during high-power TX | Power supply brownout during transmission; add bulk capacitance or reduce power |
| High RSSI but failed packet decode | Receiver front-end saturation (boards too close) or co-channel interference |
| Only first packet received | Failure to return transceiver to `RXCONTINUOUS` or failure to clear IRQ flags |
| Severely negative SNR at short distance | Antenna cross-polarization (90° mismatch) or missing antenna |

---

## 13. Documentation References

- **Semtech SX1276/77/78/79 Datasheet**: Complete register map and LoRa modem operating theory.
- **Semtech AN1200.22 LoRa Modulation Basics**: Mathematical derivations for CSS, SF, and airtime.
- **Ai-Thinker Ra-01H Datasheet**: Pinouts, electrical ratings, module dimensions.
- **Authority of Radio Frequency Management (ARFM)** (`rfd.gov.vn`): Active frequency allocations and license-exempt standards.
