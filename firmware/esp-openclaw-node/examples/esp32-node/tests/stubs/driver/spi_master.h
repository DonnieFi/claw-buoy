#pragma once
#include <stdint.h>
#include "esp_err.h"
typedef void *spi_device_handle_t;
typedef struct {int mosi_io_num,miso_io_num,sclk_io_num,quadwp_io_num,quadhd_io_num,max_transfer_sz;} spi_bus_config_t;
typedef struct {int clock_speed_hz,mode,spics_io_num,queue_size;} spi_device_interface_config_t;
typedef struct {int length,flags;uint8_t tx_data[4],rx_data[4];} spi_transaction_t;
#define SPI3_HOST 3
#define SPI_DMA_DISABLED 0
#define SPI_TRANS_USE_TXDATA 1
#define SPI_TRANS_USE_RXDATA 2
esp_err_t spi_bus_initialize(int host,const spi_bus_config_t *cfg,int dma);
esp_err_t spi_bus_add_device(int host,const spi_device_interface_config_t *cfg,spi_device_handle_t *out);
esp_err_t spi_bus_free(int host);
esp_err_t spi_device_polling_transmit(spi_device_handle_t dev,spi_transaction_t *transaction);
