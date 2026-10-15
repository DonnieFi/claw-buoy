# Parts list (prototype BOM)

Quantities are for one device. Exact supplier SKUs have not been recorded; check board/module dimensions and pinout before buying substitutes. No prices are asserted.

**Affiliate disclosure:** As an Amazon Associate I earn from qualifying purchases. The amazon.ca links below are search links that carry an affiliate tag. They are not exact-SKU recommendations, so check each listing against the specification before buying.

| Qty | Part | Specification / notes |
| --- | --- | --- |
| 1 | [ESP32 development board](https://www.amazon.ca/s?k=Freenove+ESP32+WROOM+board&tag=threelayerbra-20) | Freenove classic ESP32-WROOM, 4 MB flash, no PSRAM; GPIO16 reserved for onboard RGB LED |
| 1 | [Touch display module](https://www.amazon.ca/s?k=2.8+inch+ILI9341+SPI+TFT+touch+display+XPT2046&tag=threelayerbra-20) | 2.8-inch ILI9341 SPI TFT, 320 × 240 landscape; XPT2046 resistive touch; verified onboard backlight control circuit |
| 1 | [Temperature/humidity breakout](https://www.amazon.ca/s?k=AHT20+temperature+humidity+sensor+breakout+I2C&tag=threelayerbra-20) | AHT20, I²C address 0x38 |
| 1 | [USB power/data cable](https://www.amazon.ca/s?k=USB+data+cable&tag=threelayerbra-20) | Match the actual development-board connector; data cable required for flashing |
| 1 | [Regulated USB supply](https://www.amazon.ca/s?k=5V+USB+power+adapter&tag=threelayerbra-20) | Match the development board requirements |
| As needed | [Jumper wires / connectors](https://www.amazon.ca/s?k=dupont+jumper+wires+female+to+female&tag=threelayerbra-20) | 3.3 V logic, common ground; use the root README wiring table |
| 1 set | Printed enclosure | V5 body.stl and back.stl; [PLA](https://www.amazon.ca/s?k=PLA+filament+1.75mm&tag=threelayerbra-20) suggested; print fit and dimensions unconfirmed |
| As needed | Fasteners | Select only after checking the enclosure audit and actual board; lengths and fit are not validated |

The printer is currently down. Nominal CAD dimensions are not measured/verified assembly dimensions. No successful physical fit or validated G-code is claimed. See [enclosure audit](enclosure/v5/AUDIT.md) for clearances and screw limits. Do not assume an arbitrary display module is electrically or mechanically compatible.
