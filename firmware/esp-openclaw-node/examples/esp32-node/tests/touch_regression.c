#include <assert.h>
#include <stdio.h>
#include "claw_box_touch.h"
#include "driver/gpio.h"
#include "driver/spi_master.h"
static int irq=1,spi_fail,pressure;static uint16_t rx=2200,ry=3000;
esp_err_t gpio_config(const gpio_config_t *cfg){(void)cfg;return ESP_OK;}
int gpio_get_level(int pin){(void)pin;return irq;}
esp_err_t spi_bus_initialize(int h,const spi_bus_config_t *c,int d){(void)h;(void)c;(void)d;return ESP_OK;}
esp_err_t spi_bus_add_device(int h,const spi_device_interface_config_t *c,spi_device_handle_t *out){(void)h;(void)c;*out=(void*)1;return ESP_OK;}
esp_err_t spi_bus_free(int h){(void)h;return ESP_OK;}
esp_err_t spi_device_polling_transmit(spi_device_handle_t dev,spi_transaction_t *t){
    assert(dev);if(spi_fail)return -1;
    unsigned value=t->tx_data[0]==0xB0?pressure:t->tx_data[0]==0xD0?rx:ry;
    t->rx_data[1]=(value<<3)>>8;t->rx_data[2]=(value<<3)&255;return ESP_OK;
}
int main(void){
    uint16_t x,y;claw_box_touch_diagnostics_t d;
    assert(!claw_box_touch_read(&x,&y));claw_box_touch_get_diagnostics(&d);assert(d.polls==1&&!d.initialized);
    assert(claw_box_touch_start()==ESP_OK);pressure=0;
    assert(!claw_box_touch_read(&x,&y));claw_box_touch_get_diagnostics(&d);assert(d.initialized&&d.irqLevel==1&&d.lowPressureSamples==1);
    pressure=500;assert(claw_box_touch_read(&x,&y));assert(x==rx&&y==ry);
    claw_box_touch_get_diagnostics(&d);assert(d.irqHighSamples==3&&d.validSamples==1&&d.penSamples==0);
    irq=0;assert(claw_box_touch_read(&x,&y));claw_box_touch_get_diagnostics(&d);assert(d.penSamples==1&&d.validSamples==2);
    rx=4095;assert(!claw_box_touch_read(&x,&y));claw_box_touch_get_diagnostics(&d);assert(d.badRawSamples==1);
    spi_fail=1;assert(!claw_box_touch_read(&x,&y));claw_box_touch_get_diagnostics(&d);assert(d.spiErrors==1);
    puts("PASS: production touch reader uninitialized, idle, IRQ-high contact, IRQ-low contact, invalid raw, SPI failure");
}
