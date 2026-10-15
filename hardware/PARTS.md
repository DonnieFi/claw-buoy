# Parts list (prototype BOM)

Quantities are for one device. Exact supplier SKUs have not been recorded; check board/module dimensions and pinout before buying substitutes. No prices are asserted.

**Affiliate disclosure:** As an Amazon Associate I earn from qualifying purchases. The Amazon.ca and Amazon.com links below are search links that carry an affiliate tag for each store. They are not exact-SKU recommendations, so check each listing against the specification before buying.

| Qty | Part | Buy (affiliate) | Specification / notes |
| --- | --- | --- | --- |
| 1 | ESP32 development board | [Amazon.ca](https://www.amazon.ca/s?k=Freenove+ESP32+WROOM+board&tag=threelayerbra-20) / [Amazon.com](https://www.amazon.com/s?k=Freenove+ESP32+WROOM+board&tag=threelayerb0f-20) | Freenove classic ESP32-WROOM, 4 MB flash, no PSRAM; GPIO16 reserved for onboard RGB LED |
| 1 | Touch display module | [Amazon.ca](https://www.amazon.ca/s?k=2.8+inch+ILI9341+SPI+TFT+touch+display+XPT2046&tag=threelayerbra-20) / [Amazon.com](https://www.amazon.com/s?k=2.8+inch+ILI9341+SPI+TFT+touch+display+XPT2046&tag=threelayerb0f-20) | 2.8-inch ILI9341 SPI TFT, 320 × 240 landscape; XPT2046 resistive touch; verified onboard backlight control circuit |
| 1 | Temperature/humidity breakout | [Amazon.ca](https://www.amazon.ca/s?k=AHT20+temperature+humidity+sensor+breakout+I2C&tag=threelayerbra-20) / [Amazon.com](https://www.amazon.com/s?k=AHT20+temperature+humidity+sensor+breakout+I2C&tag=threelayerb0f-20) | AHT20, I²C address 0x38 |
| 1 | USB power/data cable | [Amazon.ca](https://www.amazon.ca/s?k=USB+data+cable&tag=threelayerbra-20) / [Amazon.com](https://www.amazon.com/s?k=USB+data+cable&tag=threelayerb0f-20) | Match the actual development-board connector; data cable required for flashing |
| 1 | Regulated USB supply | [Amazon.ca](https://www.amazon.ca/s?k=5V+USB+power+adapter&tag=threelayerbra-20) / [Amazon.com](https://www.amazon.com/s?k=5V+USB+power+adapter&tag=threelayerb0f-20) | Match the development board requirements |
| As needed | Jumper wires / connectors | [Amazon.ca](https://www.amazon.ca/s?k=dupont+jumper+wires+female+to+female&tag=threelayerbra-20) / [Amazon.com](https://www.amazon.com/s?k=dupont+jumper+wires+female+to+female&tag=threelayerb0f-20) | 3.3 V logic, common ground; use the root README wiring table |
| 1 set | Printed enclosure | PLA filament: [Amazon.ca](https://www.amazon.ca/s?k=PLA+filament+1.75mm&tag=threelayerbra-20) / [Amazon.com](https://www.amazon.com/s?k=PLA+filament+1.75mm&tag=threelayerb0f-20) | V5 body.stl and back.stl; PLA suggested; print fit and dimensions unconfirmed |
| As needed | Fasteners | — | Select only after checking the enclosure audit and actual board; lengths and fit are not validated |

The enclosure has not been test-fit. Nominal CAD dimensions are not measured/verified assembly dimensions. No successful physical fit or validated G-code is claimed. See [enclosure audit](enclosure/v5/AUDIT.md) for clearances and screw limits. Do not assume an arbitrary display module is electrically or mechanically compatible.
