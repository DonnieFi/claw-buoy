/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "esp_openclaw_node_example_aht20_node_cmd.h"

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include "esp_console.h"

#include "claw_box_display.h"
#include "cJSON.h"
#include "driver/i2c_master.h"
#include "esp_check.h"
#include "esp_openclaw_node_example_json.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"

#define AHT20_I2C_ADDRESS 0x38
#define AHT20_SDA_GPIO 21
#define AHT20_SCL_GPIO 22

static const char *TAG = "esp_openclaw_node_aht20";
static SemaphoreHandle_t s_sensor_lock;
static i2c_master_bus_handle_t s_bus;
static i2c_master_dev_handle_t s_device;

static esp_err_t ensure_aht20(void)
{
    if (s_device != NULL) {
        return ESP_OK;
    }

    i2c_master_bus_config_t bus_config = {
        .i2c_port = I2C_NUM_0,
        .sda_io_num = AHT20_SDA_GPIO,
        .scl_io_num = AHT20_SCL_GPIO,
        .clk_source = I2C_CLK_SRC_DEFAULT,
        .glitch_ignore_cnt = 7,
        .flags.enable_internal_pullup = true,
    };
    ESP_RETURN_ON_ERROR(i2c_new_master_bus(&bus_config, &s_bus), TAG, "I2C bus init failed");

    i2c_device_config_t device_config = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address = AHT20_I2C_ADDRESS,
        .scl_speed_hz = 100000,
    };
    esp_err_t err = i2c_master_bus_add_device(s_bus, &device_config, &s_device);
    if (err != ESP_OK) {
        i2c_del_master_bus(s_bus);
        s_bus = NULL;
    }
    return err;
}

static esp_err_t read_aht20_locked(
    esp_openclaw_node_handle_t node,
    void *context,
    const char *params_json,
    size_t params_len,
    char **out_payload_json,
    esp_openclaw_node_error_t *out_error)
{
    (void)node;
    (void)context;
    (void)params_json;
    (void)params_len;

    esp_err_t err = ensure_aht20();
    if (err != ESP_OK) {
        out_error->message = "AHT20 not detected on I2C address 0x38";
        return err;
    }

    const uint8_t init_command[] = {0xBE, 0x08, 0x00};
    err = i2c_master_transmit(s_device, init_command, sizeof(init_command), 100);
    if (err != ESP_OK) {
        out_error->message = "AHT20 initialization failed";
        return err;
    }

    vTaskDelay(pdMS_TO_TICKS(10));
    const uint8_t measure_command[] = {0xAC, 0x33, 0x00};
    err = i2c_master_transmit(s_device, measure_command, sizeof(measure_command), 100);
    if (err != ESP_OK) {
        out_error->message = "AHT20 measurement command failed";
        return err;
    }

    vTaskDelay(pdMS_TO_TICKS(80));
    uint8_t data[6] = {0};
    err = i2c_master_receive(s_device, data, sizeof(data), 100);
    if (err != ESP_OK) {
        out_error->message = "AHT20 measurement read failed";
        return err;
    }
    if ((data[0] & 0x80) != 0) {
        out_error->message = "AHT20 measurement is still busy";
        return ESP_ERR_INVALID_STATE;
    }

    uint32_t humidity_raw = ((uint32_t)data[1] << 12) |
                            ((uint32_t)data[2] << 4) |
                            (data[3] >> 4);
    uint32_t temperature_raw = (((uint32_t)data[3] & 0x0F) << 16) |
                               ((uint32_t)data[4] << 8) |
                               data[5];
    double humidity = (humidity_raw * 100.0) / 1048576.0;
    double temperature = (temperature_raw * 200.0) / 1048576.0 - 50.0;

    claw_box_display_sensor(temperature, humidity);

    cJSON *payload = cJSON_CreateObject();
    if (payload == NULL) {
        return ESP_ERR_NO_MEM;
    }
    cJSON_AddNumberToObject(payload, "address", AHT20_I2C_ADDRESS);
    cJSON_AddNumberToObject(payload, "sda", AHT20_SDA_GPIO);
    cJSON_AddNumberToObject(payload, "scl", AHT20_SCL_GPIO);
    cJSON_AddNumberToObject(payload, "temperatureC", temperature);
    cJSON_AddNumberToObject(payload, "humidityPercent", humidity);
    return esp_openclaw_node_example_take_json_payload(payload, out_payload_json);
}

/* Serialize local polling with remote and console reads. */
static esp_err_t handle_aht20_read(
    esp_openclaw_node_handle_t node, void *context, const char *params_json,
    size_t params_len, char **out_payload_json, esp_openclaw_node_error_t *out_error)
{
    if (s_sensor_lock == NULL ||
        xSemaphoreTake(s_sensor_lock, pdMS_TO_TICKS(1000)) != pdTRUE) {
        out_error->message = "AHT20 read busy";
        return ESP_ERR_TIMEOUT;
    }
    esp_err_t err = read_aht20_locked(node, context, params_json, params_len,
                                     out_payload_json, out_error);
    xSemaphoreGive(s_sensor_lock);
    return err;
}

#ifdef CONFIG_CLAW_BOX_DISPLAY
static void sensor_display_task(void *context)
{
    (void)context;
    /* Display is initialized immediately after command registration. */
    vTaskDelay(pdMS_TO_TICKS(1000));
    for (;;) {
        char *payload = NULL;
        esp_openclaw_node_error_t error = {0};
        esp_err_t err = handle_aht20_read(NULL, NULL, "{}", 2, &payload, &error);
        free(payload);
        if (err != ESP_OK) ESP_LOGW(TAG, "Local sensor sample failed: %s", esp_err_to_name(err));
        vTaskDelay(pdMS_TO_TICKS(30000));
    }
}
#endif

static int serial_aht20_read(int argc, char **argv)
{
    (void)argc; (void)argv;
    char *payload = NULL;
    esp_openclaw_node_error_t error = {0};
    esp_err_t err = handle_aht20_read(NULL, NULL, "{}", 2, &payload, &error);
    if (err == ESP_OK) printf("AHT20 reading: %s\n", payload);
    else printf("AHT20 read failed: %s (%s)\n", error.message ? error.message : "unknown", esp_err_to_name(err));
    free(payload);
    return err == ESP_OK ? 0 : 1;
}

esp_err_t esp_openclaw_node_example_register_aht20_node_commands(
    esp_openclaw_node_handle_t node)
{
    s_sensor_lock = xSemaphoreCreateMutex();
    if (s_sensor_lock == NULL) return ESP_ERR_NO_MEM;
    const esp_console_cmd_t serial = {.command="aht20-read", .help="Read temperature and humidity locally", .func=serial_aht20_read};
    ESP_RETURN_ON_ERROR(esp_console_cmd_register(&serial), TAG, "registering local sensor read failed");
    static const esp_openclaw_node_command_t command = {
        .name = "aht20.read",
        .handler = handle_aht20_read,
        .context = NULL,
    };
    ESP_RETURN_ON_ERROR(
        esp_openclaw_node_register_capability(node, "aht20"),
        TAG,
        "registering AHT20 capability failed");
    ESP_RETURN_ON_ERROR(esp_openclaw_node_register_command(node, &command),
                        TAG, "registering AHT20 command failed");
#ifdef CONFIG_CLAW_BOX_DISPLAY
    if (xTaskCreate(sensor_display_task, "buoy_sensor", 3072, NULL, 3, NULL) != pdPASS)
        return ESP_ERR_NO_MEM;
#endif
    return ESP_OK;
}
