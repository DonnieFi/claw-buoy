# Parts list (prototype BOM)

Quantities are for one device. Exact supplier SKUs have not been recorded; check board/module dimensions and pinout before buying substitutes. No prices or purchase links are asserted.

| Qty | Part | Specification / notes |
| --- | --- | --- |
| 1 | ESP32 development board | Freenove classic ESP32-WROOM, 4 MB flash, no PSRAM; GPIO16 reserved for onboard RGB LED |
| 1 | Touch display module | 2.8-inch ILI9341 SPI TFT, 320 × 240 landscape; XPT2046 resistive touch; verified onboard backlight control circuit |
| 1 | Temperature/humidity breakout | AHT20, I²C address 0x38 |
| 1 | USB power/data cable | Match the actual development-board connector; data cable required for flashing |
| 1 | Regulated USB supply | Match the development board requirements |
| As needed | Jumper wires / connectors | 3.3 V logic, common ground; use the root README wiring table |
| 1 set | Printed enclosure | V5 body.stl and back.stl; PLA suggested; print fit and dimensions unconfirmed |
| As needed | Fasteners | Select only after checking the enclosure audit and actual board; lengths and fit are not validated |

The printer is currently down. Nominal CAD dimensions are not measured/verified assembly dimensions. No successful physical fit or validated G-code is claimed. See [enclosure audit](enclosure/v5/AUDIT.md) for clearances and screw limits. Do not assume an arbitrary display module is electrically or mechanically compatible.
