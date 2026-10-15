# OpenClaw ESP Node source snapshot

This is a firmware-focused snapshot copied from the local OpenClaw ESP Node checkout on 2026-10-07.

- Upstream repository: https://github.com/openclaw/esp-openclaw-node
- Upstream checkout commit at capture: 9d905d3
- Included directories: components/esp-openclaw-node, components/esp-openclaw-node-provisioning, and examples/esp32-node.
- The example directory includes the local, uncommitted Claw in a Box display-only firmware work. It is a working-tree snapshot, not a clean upstream release.
- Generated build caches, managed component downloads, and machine-generated sdkconfig files were excluded. The checked-in/default configuration files, dependency lock, source, project documentation, and build evidence are included. The captured firmware image and selected build logs are stored in ../builds/claw-box-display-v1-2026-10-06/.
- The upstream license and README are preserved alongside the snapshot.

The example CMake layout is preserved so its relative component paths continue to resolve inside this folder. Use the project build guide for the ESP-IDF environment and build command.

2026-10-12: example `main/` display sources, `sdkconfig.claw-box.defaults` and `esp_openclaw_node_protocol.c` updated to the "Office Buoy designer UI" build deployed 2026-10-07.

2026-10-15: V8 adds warm-dark UI tokens, contact feedback, compact activity visuals, a native 96×70 mascot, full weekday and weather faces. Public distribution uses a fresh source allowlist: historical local build/evidence links above describe the private development archive and are not included. Site-specific prompts and delivery targets are excluded; see the root README for local overrides.
