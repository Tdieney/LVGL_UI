# LVGL Embedded UI Solutions

Welcome to the LVGL Embedded UI Repository. This repository hosts production-grade, highly optimized C UI implementations powered by LVGL v8.4 for bare-metal and RTOS microcontrollers.

---

## 1. Repository Structure & Branching Strategy

To maintain clean separation between projects and target platforms, the `main` branch serves strictly as an architectural entry point and directory index. Specific UI feature implementations, target drivers, and tools reside on dedicated feature branches:

| Branch Name | Target Display | Platform / Architecture | Description |
|---|---|---|---|
| **[`ui/motor-control`](../../tree/ui/motor-control)** | 7" 800x480 RGB565 (xSPI + GRAM) | 80 MHz MCU (128 KB SRAM, 1 MB Flash) | Industrial HMI for BLDC Motor Control with zero-alloc tick scheduler and 52 KB heap profile |

---

## 2. Accessing Project Branches

To clone and check out a specific UI implementation:

```bash
# Clone the repository:
git clone https://github.com/Tdieney/LVGL_UI.git
cd LVGL_UI

# Switch to the BLDC Motor Control HMI branch:
git checkout ui/motor-control
```

---

## 3. General Architecture Standards

All embedded UI branches in this repository follow strict bare-metal engineering constraints:

- **Strict SRAM Allocation:** Memory profiles are hard-budgeted and verified against LVGL heap limits (e.g., 52 KB heap with 16 KB partial draw buffers).
- **Zero Dynamic Allocation in Hot Paths:** Static string pointers reference Flash memory directly. Dynamic metrics use static buffer pools to prevent heap fragmentation.
- **Fixed-Point & Integer Math:** Hot-path animation, tick updates, and gauge calculations avoid soft-float operations (`float`, `sinf`, `cosf`).
- **Coalesced & Rate-Limited Teardown:** Screen transitions use single-resident SPA architecture with rate-limited clean-and-rebuild loops to eliminate OOM spikes.
