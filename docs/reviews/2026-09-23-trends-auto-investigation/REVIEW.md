# Trends and Auto live-path investigation

Date: 2026-09-23. Scope: code/test/document review and one implementation handoff.
No production source was edited. The owner reported live telemetry on LCD with
empty Trends history and an Auto output remaining OFF. No target firmware
trace, actual RH reading, thresholds or polarity snapshot was supplied.

## Verified code paths

1. `ui.c:update_trends_page_widgets()` reads Current and selector values from
   `lora_node_status`, while Min/Max/plot/span read `s_history`. The only way to
   populate that array is `ui_feed_history()`. Repository search found call
   sites in `sim_pc/main.c` only. `docs/user/SMART_HUB_UI_GUIDE.md` explicitly
   leaves the history connection as integration checklist item13 and says the
   UI does not accumulate history. Thus the owner's Current-with-empty-history
   symptom is an expected integration gap, independent of sensor validity.
2. `ui.c:commit_device_config_internal()` invokes `ui_auto_rearm()` followed by
   `ui_auto_restart_hold()` for Manual→Auto, although no relay transition occurs
   from a settings Save. `ui_auto_rearm()` also sets `last_transition_ms=now`.
   `ui_auto_eval()` refuses transitions until 10s elapse, then requires three
   fresh eligible revisions. This can explain a delayed apparent failure.
   It does not prove a persistent failure on the owner's MCU.
3. `ui.c:ui_tick()` evaluates Auto only on changed `lora_rx_revision`, online,
   `s_tx_ready`, Auto mode and idle command phase. Firmware must publish and
   send in the documented serialized order. If report cadence does not supply
   three fresh frames, Auto cannot command. Actual TX/ACK/readback still need
   target evidence; simulator's command tests alone cannot prove them.
4. `ui_auto.c:ui_auto_eval()` implements Humidifier ON at RH<=ON (40% default),
   OFF at RH>=OFF (50% default). The owner's phrase “humidity crossed threshold”
   does not establish direction; e.g.55% while OFF correctly remains OFF.

## Regression coverage gap

The existing runner passes34 checks, but `sim_pc/main.c:feed_standard_history()`
injects all four historical series, and several Auto tests advance 10s before
qualifying frames. The tests verify their seeded paths, not automatic live
history or immediate Manual→Auto arming on a stable OFF relay. Fresh
`tools/run_regression.bat -SkipBuild` on this workspace: PASS (34 checks;
27 scenario shots). The owner's physical symptom remains unverified without
firmware trace.

## Design and scope decision for handoff

The accepted Quiet Architecture chart should show a true singleton after the
first valid frame, then truthful time-ordered session samples. This follows
the focused ui-ux-pro-max chart guidance for a time series; no new chart type,
icons or page layout are needed. The [single CURRENT](../../prompts/CURRENT.md)
requires a bounded UI-owned session history because firmware is absent from
this repository; its explicit external-history API remains supported.

The handoff also separates Auto's confirmed relay-transition hold from mere
mode selection, tests all gate conditions and carries previously open RAM/fault
verification into the same delivery. Target MCU acceptance still needs the
owner's firmware RX/TX/ACK and memory evidence.
