#pragma once
#include <stdbool.h>
#include <stdint.h>
#include "esp_err.h"

esp_err_t claw_box_touch_start(void);
#define CLAW_TOUCH_PRESSURE_THRESHOLD 50
#define CLAW_TOUCH_SPI_HZ 1000000
/* Uncalibrated 12-bit coordinates; a false result means no valid contact. */
bool claw_box_touch_read(uint16_t *x, uint16_t *y);

typedef struct { uint32_t polls,penSamples,validSamples,lowPressureSamples,badRawSamples,spiErrors,irqHighSamples,irqLowSamples; bool initialized; int irqLevel; uint16_t pressure,rawX,rawY; } claw_box_touch_diagnostics_t;
void claw_box_touch_get_diagnostics(claw_box_touch_diagnostics_t *out);
