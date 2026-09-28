# User Manual (V1.06)

---

## I. Product Overview

The **MXVC10S-B** is a high-precision TVOC and Carbon Dioxide ($\text{CO}_{2\text{eq}}$ / equivalent carbon dioxide) transmitter. It features an aesthetic and compact appearance, simple wiring, and easy installation. 

Equipped with an imported high-precision probe, it ensures outstanding measurement performance, excellent stability, and strong anti-interference capability. The product provides high-level protective enclosure performance and Level-1 lightning protection. Through its **RS485 interface**, multiple modules can be connected together to a bus network for real-time monitoring of multiple field environments.

---

## II. Applications

Widely applied in:
- Telecommunication server rooms / Data centers
- Smart buildings
- Factory workshops
- Warehouses & Pharmaceutical storage
- Libraries & Museums
- Laboratories & Offices
- Ventilation ducts
- Greenhouses and related environments

---

## III. Product Features

- **RS485 Interface:** Communication distance up to 1,200 meters.
- **Imported High-Precision Probe:** High accuracy, wide measurement range, and excellent consistency.
- **Integrated Design:** Compact structure for easy installation.
- **Robust Performance:** High stability and strong anti-interference capability.
- **Wide Voltage Input:** DC 5V – 12V.
- **Standard Protocol:** Standard Modbus RTU protocol support.
- **Strong Protection:** Robust protective enclosure with Level-1 lightning protection.

---

## IV. Main Technical Specifications

| Parameter | Specification |
| :--- | :--- |
| **Rated Voltage** | DC 9V ~ DC 12V *(Note: Supports DC 5V–12V wide input)* |
| **Measurement Range** | **TVOC:** $0 \sim 60,000 \text{ ppb}$<br>**$\text{CO}_{2\text{eq}}$:** $400 \sim 60,000 \text{ ppm}$ |
| **Measurement Accuracy** | **TVOC:**<br>• $\pm 1 \text{ ppb}$ (@ $0 \sim 2,008 \text{ ppb}$)<br>• $\pm 6 \text{ ppb}$ (@ $2,008 \sim 11,110 \text{ ppb}$)<br>• $\pm 32 \text{ ppb}$ (@ $11,110 \sim 60,000 \text{ ppb}$)<br><br>**$\text{CO}_{2\text{eq}}$:**<br>• $\pm 1 \text{ ppm}$ (@ $400 \sim 1,479 \text{ ppm}$)<br>• $\pm 3 \text{ ppm}$ (@ $1,479 \sim 5,144 \text{ ppm}$)<br>• $\pm 9 \text{ ppm}$ (@ $5,144 \sim 17,597 \text{ ppm}$)<br>• $\pm 31 \text{ ppm}$ (@ $17,597 \sim 60,000 \text{ ppm}$) |
| **Output Interface** | RS485 |
| **Communication Protocol** | Modbus RTU |
| **Device Address** | 1 ~ 247 |
| **Baud Rate** | 1200, 2400, 4800, 9600, 19200 bit/s *(Other baud rates can be customized for bulk orders)* |
| **Average Power Consumption** | $< 0.1\text{ W}$ |
| **Installation Method** | Indoor wall-mount, Ceiling-mount |
| **Dimensions** | $65 \times 46 \times 29 \text{ mm}$ |

---

## V. Communication Protocol Details

- Conforms to standard **Modbus RTU** protocol.
- **Default Settings:** Baud rate: `9600 bps`, Address: `1`.

### 1. Master Request Frame Format

| Address | Function Code | Start Register High Byte | Start Register Low Byte | Register Count High Byte | Register Count Low Byte | CRC16 High Byte | CRC16 Low Byte |
| :---: | :---: | :---: | :---: | :---: | :---: | :---: | :---: |
| 1 Byte | `0x03` | 1 Byte | 1 Byte | 1 Byte | 1 Byte | 1 Byte | 1 Byte |

### 2. Slave Response Frame Format

| Address | Function Code | Byte Count | Data 1 High Byte | Data 1 Low Byte | Data 2 High Byte | Data 2 Low Byte | ... | CRC16 High Byte | CRC16 Low Byte |
| :---: | :---: | :---: | :---: | :---: | :---: | :---: | :---: | :---: | :---: |
| 1 Byte | `0x03` | 1 Byte | 1 Byte | 1 Byte | 1 Byte | 1 Byte | ... | 1 Byte | 1 Byte |

---

## VI. Register Information

### 1. Status Registers (Read Function Code: `0x03` / `0x04`)

| Start Address (Hex/Dec) | Content | Bytes | Unit | Remarks |
| :---: | :---: | :---: | :---: | :--- |
| `0` | None | 2 | $0.1\ ^\circ\text{C}$ | A value of `0x8000` indicates a probe read error |
| `1` | None | 2 | $0.1\ \% \text{RH}$ | A value of `0x8000` indicates a probe read error |
| `2` | None | 2 | $0.1\text{ hPa}$ | — |
| `3` | TVOC | 2 | $1\text{ ppb}$ | — |
| `4` | $\text{CO}_{2\text{eq}}$ | 2 | $1\text{ ppm}$ | — |

### 2. Parameter Registers (Read: `0x03` / `0x04`, Write: `0x10` / `0x06`)

| Start Address | Content | Bytes | Unit | Remarks / Value Mapping |
| :---: | :---: | :---: | :---: | :--- |
| `100` (`0x64`) | Communication Address | 2 | — | `1 ~ 247` |
| `101` (`0x65`) | Baud Rate | 2 | — | Options: `0 ~ 4`<br>• `0`: 1200<br>• `1`: 2400<br>• `2`: 4800<br>• `3`: 9600 (default)<br>• `4`: 19200 |

---

## VII. Command Examples

### 1. Read TVOC and $\text{CO}_{2\text{eq}}$

- **Query:**  
  `01 03 00 03 00 02 34 0B`
- **Response:**  
  `01 03 04 00 00 01 90 FB CF`  
  - `00 00` $\rightarrow$ TVOC = $0\text{ ppb}$  
  - `01 90` $\rightarrow \text{CO}_{2\text{eq}} = 400\text{ ppm}$ (Hex `0x0190` = Decimal $400$)

### 2. Read Address

- **Query:**  
  `FF 03 00 64 00 01 D0 0B`

### 3. Write / Change Address

- **When current address is known (e.g., change from `01` to `02`):**  
  `01 06 00 64 00 02 49 D4`
- **When current address is unknown (broadcast change to `02`):**  
  `FF 06 00 64 00 02 5C 0A`

---

## VIII. Quality Assurance & Warranty

- **Warranty Period:** 12 months.
- Free maintenance and repair services are provided within the warranty period under normal usage conditions, excluding man-made damage.