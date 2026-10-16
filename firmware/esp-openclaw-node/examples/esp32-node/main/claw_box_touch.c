#include "claw_box_touch.h"
#include <stddef.h>
#include "claw_box_display.h"
#include "driver/gpio.h"
#include "driver/spi_master.h"
#include "freertos/FreeRTOS.h"
static portMUX_TYPE diagnostic_mux=portMUX_INITIALIZER_UNLOCKED;

static spi_device_handle_t touch;
static claw_box_touch_diagnostics_t diagnostics;
void claw_box_touch_get_diagnostics(claw_box_touch_diagnostics_t *out) {
    portENTER_CRITICAL(&diagnostic_mux); *out=diagnostics; portEXIT_CRITICAL(&diagnostic_mux);
}
static bool publish(claw_box_touch_diagnostics_t *d, bool valid) {
    portENTER_CRITICAL(&diagnostic_mux); diagnostics=*d; portEXIT_CRITICAL(&diagnostic_mux); return valid;
}

esp_err_t claw_box_touch_start(void)
{
    gpio_config_t irq = {.pin_bit_mask = 1ULL << CLAW_TOUCH_IRQ,
                         .mode = GPIO_MODE_INPUT, .pull_up_en = GPIO_PULLUP_ENABLE};
    esp_err_t err = gpio_config(&irq);
    if (err != ESP_OK) return err;
    spi_bus_config_t bus = {.mosi_io_num = CLAW_TOUCH_MOSI,
        .miso_io_num = CLAW_TOUCH_MISO, .sclk_io_num = CLAW_TOUCH_SCK,
        .quadwp_io_num = -1, .quadhd_io_num = -1, .max_transfer_sz = 3};
    err = spi_bus_initialize(SPI3_HOST, &bus, SPI_DMA_DISABLED);
    if (err != ESP_OK) return err;
    spi_device_interface_config_t device = {.clock_speed_hz = CLAW_TOUCH_SPI_HZ,
        .mode = 0, .spics_io_num = CLAW_TOUCH_CS, .queue_size = 1};
    err = spi_bus_add_device(SPI3_HOST, &device, &touch);
    if (err != ESP_OK) spi_bus_free(SPI3_HOST);
    return err;
}

static bool sample(uint8_t command, uint16_t *value)
{
    /* XPT2046: control byte, then 12-bit result with one leading null bit.
       PD1/PD0=0 restores PENIRQ after each conversion. */
    spi_transaction_t transaction = {.length = 24,
        .flags = SPI_TRANS_USE_TXDATA | SPI_TRANS_USE_RXDATA,
        .tx_data = {command, 0, 0}};
    if (spi_device_polling_transmit(touch, &transaction) != ESP_OK) return false;
    *value = (((uint16_t)transaction.rx_data[1] << 8) | transaction.rx_data[2]) >> 3;
    return true;
}

static uint16_t median(uint16_t a, uint16_t b, uint16_t c)
{
    if (a > b) { uint16_t t = a; a = b; b = t; }
    if (b > c) { uint16_t t = b; b = c; c = t; }
    return a > b ? a : b;
}

bool claw_box_touch_read(uint16_t *x, uint16_t *y)
{
    claw_box_touch_diagnostics_t d;claw_box_touch_get_diagnostics(&d);
    ++d.polls;d.initialized=touch!=NULL;d.irqLevel=gpio_get_level(CLAW_TOUCH_IRQ);
    if(d.irqLevel)++d.irqHighSamples;else{++d.irqLowSamples;++d.penSamples;}
    if(!touch)return publish(&d,false);
    /* PENIRQ is diagnostic, not the sole acquisition gate. Read Z1 even when
       IRQ is high so an unarmed/stuck IRQ cannot suppress genuine contacts.
       Pressure plus valid X/Y still reject idle/floating data. Each conversion
       powers down the controller (PD=00), restoring PENIRQ. */
    uint16_t xs[3],ys[3],pressure;
    if(!sample(0xB0,&pressure)){++d.spiErrors;return publish(&d,false);}
    d.pressure=pressure;
    if(pressure<CLAW_TOUCH_PRESSURE_THRESHOLD){++d.lowPressureSamples;return publish(&d,false);}
    for(int i=0;i<3;++i){
        if(!sample(0xD0,&xs[i])||!sample(0x90,&ys[i])){++d.spiErrors;return publish(&d,false);}
    }
    *x=median(xs[0],xs[1],xs[2]);*y=median(ys[0],ys[1],ys[2]);
    d.rawX=*x;d.rawY=*y;
    bool valid=*x>100&&*x<4000&&*y>100&&*y<4000;
    if(valid)++d.validSamples;else ++d.badRawSamples;
    return publish(&d,valid);
}
