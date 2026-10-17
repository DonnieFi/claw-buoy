# V9 touch evaluation

## Pre-V9 baseline

1. Custom driver samples XPT2046 on SPI3 every 10 ms at 1 MHz.
2. Z1 pressure must reach 100; three samples per coordinate are median filtered.
3. Three invalid polls complete a stroke; actions previously waited for release.
4. Two diagonal raw points map screen coordinates and persist in NVS/Gateway corners.
5. First wake gesture is consumed; contact halo/control tint provide visual feedback.

## Library decision

The registry package is **atanisoft/esp_lcd_touch_xpt2046**, not an
`espressif/esp_lcd_touch_xpt2046` package. It uses Espressif's `esp_lcd_touch`
interface and can feed an LVGL `lv_indev`. Reviewed upstream source from
https://github.com/atanisoft/esp_lcd_touch_xpt2046 and registry metadata at
https://components.espressif.com/components/atanisoft/esp_lcd_touch_xpt2046 .

Source comparison: the alternative adds allocated touch state, LCD panel IO,
Z1+Z2 pressure and configurable averaging/IRQ gating. Our reader uses Z1 plus
median filtering, diagnostic-only IRQ (contacts with IRQ high still work), and
exports per-cause rejection counters. Z thresholds are not numerically equivalent.
The alternative source is roughly 400 lines versus our roughly 80-line reader;
this source count is not a firmware-size measurement. A matched ESP-IDF 5.4.2 build measured object text of 719 bytes for our reader, 3,505 bytes for the alternative reader, plus 4,649 bytes for the shared esp_lcd_touch object. These are unlinked object sections, not whole-image or RAM/latency comparisons; linker garbage collection and configuration affect final cost. End-to-end latency
has not been measured for either implementation. Swapping libraries alone cannot
remove the application's former release-dispatch delay. It would require adapting
raw-coordinate calibration, counters and gesture policy, not merely registering
an LVGL input device. An isolated ESP-IDF 5.4.2 component-manager build of Atanisoft 1.0.6, Espressif esp_lcd_touch and LVGL 9.2.0 succeeded, including SPI3 pin setup and an lv_indev read callback. It was never flashed. Calibration was not adapted in this probe. No physical A/B test or end-to-end latency benchmark was performed.

**Recommendation: keep the tested custom driver for this update.** Retain 1 MHz
until signal quality on the shared-MISO wiring is established. Existing pins:
SPI3 CLK14/MOSI13/MISO19/CS33/IRQ32; TFT wiring unchanged.

## Changes and limitations

Pressure minimum is a named constant of 50. Low-pressure counters remain exposed.
Initial contact and debounced release are distinct queued events. Main controls
act once on initial contact; their release is consumed even after movement.
Other gestures stay release-classified. A drag beginning on a ping button now
sends that action immediately: it cannot be retrospectively cancelled. Existing
15-second ping throttling and no-send mode remain.

Wake happens on initial contact with visual receipt; it does not activate a hidden
button. Main tiles/back are already >=44 px high; the alarm toggle is now 44 px.
Self-test suppresses actions and shows the current reported contact as a dot with
raw coordinates and pressure. Start/stop via the existing display.update command.

Three-point calibration targets are (40,40), (280,190), (280,40), with a determinant
check rejecting near-collinear raw points. Affine coefficients are derived from
three raw contacts and their raw values persist in NVS. Legacy saved corners are
loaded unchanged; writing calibrationCorners resets mapping to the legacy mode.
No invented third measurement is generated for the attached unit.

Host tests cover pressure just below/at threshold, IRQ high/low contacts, bad raw
readings and SPI failures; initial press/hold/dropout/release events, affine target
and midpoint mapping, degenerate fits, hit-region separation and existing UI rules.
Firmware/live verification is reported separately. See the hands-on result below.

If resistive touch remains unsatisfactory, a **compatible capacitive 2.8-inch
panel** is the better hardware direction (e.g. a suitable CST816-based module).
Match controller, display interface, supply, pinout and enclosure dimensions first;
this is not a drop-in wiring claim or a tested replacement recommendation.

## Hands-on result — 2026-10-17

The author confirmed that light taps, edge navigation and first-contact wake
feedback work well on the flashed V9 prototype, then reported that taps work
very well but swipes feel stuttery. Tap/wake behavior is hands-on confirmed on
this unit; swipe smoothness remains a known limitation, not a completed fix.
No physical library A/B test, 2 MHz SPI test or affine-calibration accuracy
measurement is implied by this feedback. Existing two-point calibration remains
active until a new three-point calibration is performed.
