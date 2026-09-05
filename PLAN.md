# Smart Hub UI plan

## Phase 0: Clean baseline

- [x] Branch from the archived `ui/motor-control` result
- [x] Remove motor-specific UI, protocol, demo, assets, tests, screenshots, and notes
- [x] Keep a minimal 800x480 LVGL simulator and reusable MCU tooling
- [x] Record the Smart Hub/Smart Node schematic interfaces and known sensor models
- [x] Add a neutral placeholder screen without committing to product UX

## Phase 1: Product discovery

- [ ] Confirm the exact Hub MCU, Flash/RAM budget, LCD controller, and touch controller
- [ ] Confirm LoRa frequency/region, node capacity, pairing/provisioning flow, and packet format
- [ ] Obtain official Modbus register maps for `MXVC10S-B-P67` and `TH10S-B-IP67`
- [ ] Define relay names, safe states, manual/automatic control rules, and feedback behavior
- [ ] Define alarm thresholds, hysteresis, acknowledgement, history, and offline behavior
- [ ] Define data retention, trends, time source, language, units, and settings ownership
- [ ] Agree on the information architecture and wireframes before visual styling

## Phase 2: UI architecture

- [ ] Define Hub domain model and the MCU-to-UI snapshot interface
- [ ] Define screen hierarchy and bounded update lanes
- [ ] Prototype the highest-memory screen and measure LVGL heap/xSPI redraw cost
- [ ] Revalidate `ui_mcu_profile.h` against the complete target firmware

## Phase 3: Implementation and verification

- [ ] Implement agreed screens and interactions
- [ ] Add deterministic demo fixtures and headless behavioral tests
- [ ] Add dirty-region and long-run heap regression guardrails
- [ ] Validate on target hardware, including touch, QSPI flush, LoRa loss, and RS-485 sensor faults
