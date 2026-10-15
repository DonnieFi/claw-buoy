# Contributing

Build with ESP-IDF 5.4.2 and the root README command. Run `python3 tools/public_checks.py` before proposing a change. UI work should preserve touch target sizes, first-tap wake behavior, calibration, stored settings, and GPIO17 backlight sleep/wake. Include mock screens with synthetic content and clearly label renders, host tests, and device observations.

Do not commit generated SDK configuration, managed components, logs, release archives, NVS dumps, credentials, real notifications/calendars, or device/network identities. Never use `git add .` on a working device checkout; review the exact staged file list and diff.

Keep upstream license notices and identify changes to vendored upstream files. Source under `firmware/esp-openclaw-node/` retains its existing upstream licensing; do not relabel it as project-wide MIT.

The enclosure is a fit draft. Include measurements and slicer evidence with mechanical changes rather than claiming a guaranteed fit.
