# Smart Hub UI Simulator (PC / Windows)

The PC simulator runs the LVGL v8.4 Smart Hub UI natively on Windows using GDI rendering, enforcing the exact MCU profile constraints: 800x480 RGB565 display, 42 KiB LVGL heap, and a single 800x10 partial draw buffer.

## Running the Interactive Simulator

Run without arguments from the repository root:

```powershell
sim_pc\build\sim_pc.exe
```

On startup, the simulator automatically initiates the authentic 2.5-second splash animation sequence before loading the Home page.

## Deterministic Fake Node Fixture

The interactive simulator includes a deterministic fake node that responds to device relay toggles and settings dialog saves:

- **Nominal Flow:** When a relay switch is clicked or settings are saved, the device enters the `Sending…` state. The fake node acknowledges success after a realistic 700 ms turnaround delay (reusing the browser prototype baseline), whereupon the node confirmation is displayed and the new state is reflected.
- **Interactive Unhappy Path Testing:** Keyboard shortcuts allow operators to simulate network faults, lost packets, and negative acknowledgements on the fly.

## Keyboard Shortcuts

| Key | Action | Description |
|---|---|---|
| `S` | **Replay Splash** | Replays the 2.5s Hyphen Deux splash sequence (#E9ECF1 fade-in, rise +14 px, hold, fade-out). |
| `F` | **Force Next Command to Fail** | Arms the fake node to return a negative acknowledgement (`success = false`) after 700 ms. The affected device transitions to `Unknown` with an active `Retry` button. |
| `T` | **Force Next Command to Timeout** | Arms the fake node to drop the next command and never send an ACK (simulating packet loss over LoRa). After `UI_CMD_TIMEOUT_MS` (3000 ms), the device automatically transitions to `Unknown` + `Retry`. |
| `L` | **Cycle Link Quality / SNR** | Steps simulated link quality through disconnected (0 bars) → 1 bar (-7 dB) → 2 bars (-4 dB) → 3 bars (0 dB) → 4 bars (+6 dB). |
| `1` | **Go to Home** | Direct page navigation shortcut to Home screen. |
| `2` | **Go to Trends** | Direct page navigation shortcut to Trends chart screen. |
| `3` | **Go to Devices** | Direct page navigation shortcut to 2x2 Devices grid screen. |
| `Esc` | **Exit** | Closes the simulator. |

## Headless and Automated Modes

```powershell
sim_pc\build\sim_pc.exe --smoke           # Quick smoke test verifying draw buffer flushes
sim_pc\build\sim_pc.exe --regression      # Complete 14-check regression suite & benchmarks
sim_pc\build\sim_pc.exe --shot <file.raw> # Renders a single headless 800x480 RGB565 shot
sim_pc\build\sim_pc.exe --shots <dir>     # Generates 12 deterministic scenario framebuffers
```
