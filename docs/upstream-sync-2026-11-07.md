# Upstream sync notes — 2026-11-07

Imported `openclaw/esp-openclaw-node` through upstream commit
`38c5ab2f5838940fd81d0e3235d2a28eb239fccc` (the Nov 3 snapshot) into the focused
Claw Buoy firmware snapshot, built it for ESP32, and flashed it to the
prototype.

## Scope and validation

- 153 upstream commits since `fb7bbc3`; 64 changed files in the imported
  component/example paths (+9,454 / −129).
- ESP-IDF 5.4.2, target `esp32`, 4 MB flash, 2 MiB app partition.
- Application size: 1,994,160 bytes, 5% free.
- Upstream host suites passed: handshake parsing, connection lease, scope
  registry, Wi-Fi/setup dispatcher, and Security2 guard.
- `python3 tools/public_checks.py` passed the local UI/touch/config regressions.
- Device verification: ILI9341 display and touch initialized, Wi-Fi acquired a
  LAN address, and the OpenClaw gateway handshake completed.

## Findings

### 1. Stale local `sdkconfig.claw-box` silently overrides changed defaults

This is a project build-workflow issue worth reporting.

An existing `examples/esp32-node/sdkconfig.claw-box` contained
`# CONFIG_CLAW_BOX_DISPLAY is not set` and other old values. Passing
`SDKCONFIG_DEFAULTS=sdkconfig.defaults;sdkconfig.claw-box.defaults` does not
override symbols already present in that file, so the first build had the
display UI compiled out (1,660,992 bytes and no `Calibrate`/`Actions` strings).
Removing and regenerating `sdkconfig.claw-box` produced the intended
1,994,160-byte build.

Suggested fix: document that `sdkconfig.claw-box` must be deleted or
regenerated after defaults change, or add a CI assertion that the generated
config has `CONFIG_CLAW_BOX_DISPLAY=y`. A build-directory-only `SDKCONFIG`
would avoid the trap entirely.

### 2. Focused snapshot must include new external build inputs

This is a project integration issue, not an upstream defect.

Upstream's `examples/esp32-node/sdkconfig.defaults` now selects
`certificates/public-roots.pem`, and the provisioning component's CMakeLists
conditionally uses `scripts/idf_srp_salt.py` plus
`patches/esp-idf/security2-srp-salt-width.patch` for ESP32-S3. The previous
snapshot included only `components/` and `examples/`, so a refresh that copies
only those paths leaves a dangling certificate path and a broken S3 guard.

This sync imported `certificates/`, the guard script, and the patch. Future
refresh scripts should treat them as part of the snapshot.

### 3. Local hello-compaction patch retired

No action needed beyond the removal recorded here.

Upstream's new `esp_openclaw_node_handshake.c` provides bounded handshake
parsing and JSON string validation, superseding the local
`esp_openclaw_node_hello_compact.h` pre-pass and the vendored jsmn parser. Both
were removed. The local metadata-only protocol diagnostics remain.

### 4. Upstream defects found

None reproduced. All upstream host tests pass and the merged firmware completes
the real gateway handshake.

One compatibility risk to monitor: the handshake parser intentionally retains
only `type`, `id`, `ok`, `event`, `error` and `payload` at the top level, and
only `type`, `nonce`, `ts`, `auth` and `pluginSurfaceUrls` inside `payload`. A
future gateway hello-ok/challenge field outside that allowlist would be dropped
for connecting-state frames. No current field is affected.

## Filed reports

- Claw Buoy build-config trap: https://github.com/DonnieFi/claw-buoy/issues/1
- Upstream example build-docs warning: https://github.com/openclaw/esp-openclaw-node/issues/234

## Artifacts

- Firmware: `firmware/esp-openclaw-node/examples/esp32-node/build-claw-box/esp_openclaw_node_esp32.bin`
- SHA256: `a30d9531783ed983caf6d7bf7ee53013ab1606ec260b57d40c210d59ab9413d1`
- Snapshot details: [`SNAPSHOT.md`](../firmware/esp-openclaw-node/SNAPSHOT.md)
- Change inventory: [`openclaw-integration.md`](openclaw-integration.md)
