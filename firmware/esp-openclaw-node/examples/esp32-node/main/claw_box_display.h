#pragma once
#include <stdbool.h>
#include "esp_err.h"
#include "esp_openclaw_node.h"
/* Fixed Freenove WROOM board pack; never use these pins through generic GPIO/ADC. */
#define CLAW_BACKLIGHT_GPIO 17
#define CLAW_TFT_SCK 18
#define CLAW_TFT_MOSI 23
#define CLAW_TFT_MISO 19
#define CLAW_TFT_CS 27
#define CLAW_TFT_DC 26
#define CLAW_TFT_RESET 25
#define CLAW_TOUCH_CS 33
#define CLAW_TOUCH_IRQ 32
#define CLAW_TOUCH_SCK 14
#define CLAW_TOUCH_MOSI 13
#define CLAW_TOUCH_MISO 19
bool claw_box_pin_reserved(int pin);
esp_err_t claw_box_display_start(esp_openclaw_node_handle_t node);
void claw_box_display_status(const char *status);
void claw_box_display_connected(void);
void claw_box_display_sensor(double temperature, double humidity);
