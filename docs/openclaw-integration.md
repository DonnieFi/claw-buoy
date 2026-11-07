# OpenClaw integration and upstream changes

## Architecture

Claw Buoy is an ESP32 **OpenClaw node**, not an OpenClaw Gateway fork. It keeps the upstream node connection, provisioning and command mechanism. Local firmware owns LCD/touch rendering, calibrated navigation, clock, alarm, idle/overnight backlight control and cached content. Buttons submit `agent.request` through the existing `node.event` Gateway request path; the Gateway agent performs configured weather/calendar/delivery work. A request being accepted is not confirmation of message delivery. Display commands (`display.text`, `display.update`, `display.state`) provide the return/control path. AHT20 commands provide room sensor readings. BLE/Classic discovery source is included; it is not proof of presence detection.

Site-specific calendar/location/delivery rules are intentionally excluded and supplied through local private overrides and Gateway setup. This repo does not ship a configured personal Gateway or modify/install Gateway code.

## Current upstream refresh

As of 2026-11-07 the included node/provisioning sources are updated through
`38c5ab2f5838940fd81d0e3235d2a28eb239fccc`. See the [snapshot](../firmware/esp-openclaw-node/SNAPSHOT.md)
for scope. The initial V8 inventory below is historical; V9 additionally includes
pressure tuning, press-down controls, affine calibration and a no-send touch test.

The 2026-11-07 refresh imports upstream's bounded handshake parser
(`esp_openclaw_node_handshake.c`), connection-lease and setup-session work, and
the custom trusted-root bundle at
`firmware/esp-openclaw-node/certificates/public-roots.pem` selected by
`examples/esp32-node/sdkconfig.defaults`. It also includes the ESP32-S3
Security2 guard script/patch referenced by the provisioning component; the ESP32
build does not execute that guard. The local hello-compaction patch and vendored
jsmn parser were removed because the upstream parser supersedes them; the local
metadata-only protocol diagnostics remain.

## Initial V8 snapshot comparison baseline

Compared included C/H files byte-for-byte against upstream `openclaw/esp-openclaw-node` commit **9d905d30e5499e7d0d42a566b0f6bbf5c08c42f6**, using the local upstream Git object. This is the captured baseline, **not a claim of parity with current upstream**. This is a focused source snapshot, not a complete upstream checkout. Counts below describe the initial V8 publication, before the portability and modification-notice cleanup. Current source also has configurable time/dark hours, labels and action routing; no additional node protocol behavior was changed. Counts below are line-diff counts; generated logo arrays make raw line counts misleading.

## Node library boundary

At the initial V8 baseline, only **one existing component C/H file** differed: `esp_openclaw_node_protocol.c` (+26 / −4). It added metadata-only heap/parse diagnostics and compacted successful hello responses before allocating the cJSON tree while connecting. Added support: `esp_openclaw_node_hello_compact.h` (94 lines) and the MIT-licensed vendored jsmn parser (471 lines). Authentication and plugin URL fields consumed by the node were retained. This was a local compatibility/memory patch, not a new wire protocol.

The 2026-11-07 refresh removes that local pre-pass and vendored parser:
upstream's `esp_openclaw_node_handshake.c` now provides bounded handshake
parsing and JSON string validation. The protocol diagnostics remain. The rest of
this section describes the historical V8 inventory, not the current snapshot.

At the V8 baseline, all other included existing component C/H files, including
runtime, public node API and provisioning sources, matched that baseline. This
statement applies to the published snapshot, not other historical working
checkouts.

## Application boundary

Most customization lives in the ESP32 example: display/touch UI, PNG-derived logo arrays, AHT20 and Bluetooth commands, site-config defaults, UI rules, app/command registration and GPIO/ADC setup. Build/dependency/configuration files also add LVGL/LCD support and the no-PSRAM board partition/settings. Hardware and documentation are project additions, outside the node library.

### C/H inventory

| Path (under firmware/esp-openclaw-node) | Status | Added lines | Removed lines |
| --- | --- | ---: | ---: |
| `components/esp-openclaw-node/private_include/esp_openclaw_node_hello_compact.h` | added | 94 | 0 |
| `components/esp-openclaw-node/private_include/vendor/jsmn.h` | added | 471 | 0 |
| `components/esp-openclaw-node/src/esp_openclaw_node_protocol.c` | modified | 26 | 4 |
| `examples/esp32-node/main/app_main.c` | modified | 14 | 1 |
| `examples/esp32-node/main/buoy_site_config.h` | added | 23 | 0 |
| `examples/esp32-node/main/buoy_ui_rules.h` | added | 131 | 0 |
| `examples/esp32-node/main/claw_box_display.c` | added | 952 | 0 |
| `examples/esp32-node/main/claw_box_display.h` | added | 22 | 0 |
| `examples/esp32-node/main/claw_box_touch.c` | added | 77 | 0 |
| `examples/esp32-node/main/claw_box_touch.h` | added | 11 | 0 |
| `examples/esp32-node/main/claw_logo.h` | added | 3529 | 0 |
| `examples/esp32-node/main/claw_logo_small.h` | added | 287 | 0 |
| `examples/esp32-node/main/esp_openclaw_node_example_adc_node_cmd.c` | modified | 7 | 0 |
| `examples/esp32-node/main/esp_openclaw_node_example_aht20_node_cmd.c` | added | 199 | 0 |
| `examples/esp32-node/main/esp_openclaw_node_example_aht20_node_cmd.h` | added | 13 | 0 |
| `examples/esp32-node/main/esp_openclaw_node_example_ble_node_cmd.c` | added | 419 | 0 |
| `examples/esp32-node/main/esp_openclaw_node_example_ble_node_cmd.h` | added | 9 | 0 |
| `examples/esp32-node/main/esp_openclaw_node_example_cmd.c` | modified | 10 | 0 |
| `examples/esp32-node/main/esp_openclaw_node_example_gpio_node_cmd.c` | modified | 2 | 1 |
| `examples/esp32-node/tests/stubs/driver/gpio.h` | added | 8 | 0 |
| `examples/esp32-node/tests/stubs/driver/spi_master.h` | added | 15 | 0 |
| `examples/esp32-node/tests/stubs/esp_err.h` | added | 3 | 0 |
| `examples/esp32-node/tests/stubs/esp_openclaw_node.h` | added | 2 | 0 |
| `examples/esp32-node/tests/stubs/freertos/FreeRTOS.h` | added | 5 | 0 |
| `examples/esp32-node/tests/touch_regression.c` | added | 28 | 0 |
| `examples/esp32-node/tests/ui_regression.c` | added | 92 | 0 |

The `esp_openclaw_node_hello_compact.h` and `vendor/jsmn.h` rows record the
historical V8 publication; both files were removed in the 2026-11-07 refresh.

This inventory excludes non-C/H build files, enclosure files and documentation. No claim is made that local patches have been upstreamed.
