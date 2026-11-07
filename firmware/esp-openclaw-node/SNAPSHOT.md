# OpenClaw ESP Node source snapshot

Firmware-focused snapshot of [openclaw/esp-openclaw-node](https://github.com/openclaw/esp-openclaw-node) updated through upstream commit `38c5ab2f5838940fd81d0e3235d2a28eb239fccc` (v1.3.1 plus subsequent fixes) (Apache-2.0; see `LICENSE`). It is not a complete upstream checkout and makes no claim of parity with current upstream.

Included directories: `components/esp-openclaw-node`, `components/esp-openclaw-node-provisioning`, `examples/esp32-node`, the upstream `certificates/` trusted-root bundle, and the `scripts/idf_srp_salt.py` / `patches/esp-idf/security2-srp-salt-width.patch` pair referenced by the ESP32-S3 Security2 build guard. Build caches, managed components and generated `sdkconfig` files are not included; `dependencies.lock` records the resolved component versions.

## Upstream refresh (2026-11-07)

Imported all changes affecting the included node/provisioning/example sources
through `38c5ab2f`. The main additions are the bounded handshake parser and
JSON string validation, connection-lease and transport-lifetime tracking,
scope-registry cleanup, setup-session owners, native Wi-Fi setup, Security2
phone setup frames, and their host tests. The example now uses the upstream
custom trusted-root bundle at `certificates/public-roots.pem` instead of the
full default certificate bundle.

The earlier local hello-compaction pre-pass (`esp_openclaw_node_hello_compact.h`
plus the vendored jsmn parser) was removed: upstream's
`esp_openclaw_node_handshake.c` now bounds connecting-state handshake memory and
retains the fields this node consumes. The local metadata-only protocol
diagnostics remain.

## Upstream refresh (2026-10-16)

Imported all changes affecting the included node/provisioning/example sources since
`9d905d3`: guarded teardown, bounded handshake recovery, WebSocket client 1.8.0,
saved-session reconnect behavior, and USB console nearby Wi-Fi discovery. Upstream
Talk/audio, M5Dial and watch examples are outside this firmware snapshot and are
not enabled on the no-PSRAM Buoy. The local hello-compaction patch was retained at
that time.

## Changes from upstream

- Five upstream C files are modified, each carrying a "Modified by" notice: `esp_openclaw_node_protocol.c`, `app_main.c`, `esp_openclaw_node_example_adc_node_cmd.c`, `esp_openclaw_node_example_cmd.c` and `esp_openclaw_node_example_gpio_node_cmd.c`.
- Upstream `esp_openclaw_node_handshake.c` and the JSON string validation path replace the local hello-compaction and vendored jsmn parser.
- Added to the example: Claw Buoy display, touch, UI rules and site-config sources, AHT20 and Bluetooth commands, host tests, no-PSRAM `sdkconfig.claw-box.defaults`, partition table and build configuration.
- Per-file detail is in [`docs/openclaw-integration.md`](../../docs/openclaw-integration.md); wiring and build commands are in the [root README](../../README.md).
