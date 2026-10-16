# OpenClaw ESP Node source snapshot

Firmware-focused snapshot of [openclaw/esp-openclaw-node](https://github.com/openclaw/esp-openclaw-node) updated through upstream commit `fb7bbc3ad626018d68d6b7585744d7f574b888c0` (v1.2.0 plus subsequent fixes) (Apache-2.0; see `LICENSE`). It is not a complete upstream checkout and makes no claim of parity with current upstream.

Included directories: `components/esp-openclaw-node`, `components/esp-openclaw-node-provisioning` and `examples/esp32-node`. Build caches, managed components and generated `sdkconfig` files are not included; `dependencies.lock` records the resolved component versions.

## Upstream refresh (2026-10-16)

Imported all changes affecting the included node/provisioning/example sources since
`9d905d3`: guarded teardown, bounded handshake recovery, WebSocket client 1.8.0,
saved-session reconnect behavior, and USB console nearby Wi-Fi discovery. Upstream
Talk/audio, M5Dial and watch examples are outside this firmware snapshot and are
not enabled on the no-PSRAM Buoy. The local hello-compaction patch is retained.
Upstream Unity tests are included, but have not been run on a Unity test target.

## Changes from upstream

- Five upstream C files are modified, each carrying a "Modified by" notice: `esp_openclaw_node_protocol.c`, `app_main.c`, `esp_openclaw_node_example_adc_node_cmd.c`, `esp_openclaw_node_example_cmd.c` and `esp_openclaw_node_example_gpio_node_cmd.c`.
- Added to the node library: `esp_openclaw_node_hello_compact.h` (MIT) and the vendored jsmn parser (MIT).
- Added to the example: Claw Buoy display, touch, UI rules and site-config sources, AHT20 and Bluetooth commands, host tests, no-PSRAM `sdkconfig.claw-box.defaults`, partition table and build configuration.
- Per-file detail is in [`docs/openclaw-integration.md`](../../docs/openclaw-integration.md); wiring and build commands are in the [root README](../../README.md).
