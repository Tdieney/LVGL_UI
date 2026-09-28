# Verification provenance — 2026-09-18

Read-only diagnostics against the current dirty workspace; no production or test
source changes in this review. Evidence capture creates files in this folder.

| Command | Result |
|---|---|
| `tools/build_sim.bat` | Exit0; cached CMake configure succeeds, Ninja reports no work; CMP0177 developer warning |
| `sim_pc/build/sim_pc.exe --regression` | Exit0;32/32; full output in [REGRESSION.txt](REGRESSION.txt) |
| `sim_pc/build/sim_pc.exe --shots docs/reviews/2026-09-18-design-fidelity` |24 scenario RAW captures generated |
| `python tools/raw2png.py <capture.raw> <capture.png> 800 480` |24 matching PNGs generated using existing converter |
| `tools/export_mcu.bat --verify` | Exit0; Smart Hub MCU export manifest: PASS |
| `git diff --check` | Exit0; existing CRLF normalization warnings only |

The capture run preceded the final no-op rebuild; the executable SHA256 was
unchanged. Fresh direct regression was run again after that rebuild. The full
regression wrapper and isolated production verifier were not rerun this turn.
No physical MCU/LCD, RF, sensor or relay checks were performed.

Source/executable SHA256 values sampled during review and checked again after
document edits; all five were unchanged:

```text
ui.c
6785339A99415B4F48593C9AC7FA6A84160F480C7F619691A6AFC0476AA28B4C
ui_theme.c
3D8B8BAEFFAFE71BAC3DBA11671F13E37C8F4C03B7C5284FB0D1CCC22FD95C89
ui_theme.h
44E04A97E3D1DDB3AC1A7DDA8DEBD666D804DB5F8AB94BF5A62647BD307F7B79
sim_pc/main.c
3F38B643EC612F709BE8C21D09C271608CFCF0C9E72B2DD3160C398AD1DB8780
sim_pc/build/sim_pc.exe
92C446593C902C783A0531F4632946E1AE428B9DA045F226C88FFFB38C151E0F
```

These hashes record review provenance, not a new firmware CRC feature or an
assertion that the outstanding ZIP source-identity verifier gap is fixed.
