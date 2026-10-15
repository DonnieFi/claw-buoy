/* SPDX-License-Identifier: Apache-2.0 */

#include "esp_openclaw_node_example_ble_node_cmd.h"

#include <stdbool.h>
#include <stdio.h>
#include <string.h>

#include "cJSON.h"
#include "esp_bt.h"
#include "esp_bt_main.h"
#include "esp_check.h"
#include "esp_gap_bt_api.h"
#include "esp_gap_ble_api.h"
#include "esp_openclaw_node_example_json.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

#define BLE_SCAN_MAX_RESULTS 32
#define BLE_SCAN_DEFAULT_MS 5000
#define BLE_SCAN_MAX_MS 10000
#define BT_NAME_LOOKUP_TIMEOUT_MS 1200
#define BT_NAME_LOOKUP_MAX 3

typedef struct {
    esp_bd_addr_t address;
    esp_ble_addr_type_t address_type;
    int rssi;
    char name[ESP_BLE_ADV_NAME_LEN_MAX];
    bool has_name;
    uint8_t adv_data[ESP_BLE_ADV_DATA_LEN_MAX];
    uint8_t adv_len;
    uint8_t scan_rsp_data[ESP_BLE_SCAN_RSP_DATA_LEN_MAX];
    uint8_t scan_rsp_len;
} ble_result_t;

typedef struct {
    esp_bd_addr_t address;
    int rssi;
    uint32_t class_of_device;
    char name[ESP_BT_GAP_MAX_BDNAME_LEN + 1];
    bool has_name;
} bt_result_t;

static const char *TAG = "esp_openclaw_node_ble";
static SemaphoreHandle_t s_scan_done;
static SemaphoreHandle_t s_bt_scan_done;
static SemaphoreHandle_t s_bt_name_done;
static bool s_bt_ready;
static bool s_scan_active;
static ble_result_t s_results[BLE_SCAN_MAX_RESULTS];
static size_t s_result_count;
static bt_result_t s_bt_results[BLE_SCAN_MAX_RESULTS];
static size_t s_bt_result_count;

static const esp_ble_scan_params_t s_scan_params = {
    .scan_type = BLE_SCAN_TYPE_ACTIVE,
    .own_addr_type = BLE_ADDR_TYPE_PUBLIC,
    .scan_filter_policy = BLE_SCAN_FILTER_ALLOW_ALL,
    .scan_interval = 0x50,
    .scan_window = 0x30,
    .scan_duplicate = BLE_SCAN_DUPLICATE_DISABLE,
};

static int find_result(const esp_bd_addr_t address)
{
    for (size_t i = 0; i < s_result_count; ++i) {
        if (memcmp(s_results[i].address, address, ESP_BD_ADDR_LEN) == 0) {
            return (int)i;
        }
    }
    return -1;
}

static int find_bt_result(const esp_bd_addr_t address)
{
    for (size_t i = 0; i < s_bt_result_count; ++i) {
        if (memcmp(s_bt_results[i].address, address, ESP_BD_ADDR_LEN) == 0) {
            return (int)i;
        }
    }
    return -1;
}

static void ble_gap_callback(esp_gap_ble_cb_event_t event, esp_ble_gap_cb_param_t *param)
{
    if (event != ESP_GAP_BLE_SCAN_RESULT_EVT || param == NULL) {
        return;
    }
    if (param->scan_rst.search_evt == ESP_GAP_SEARCH_INQ_CMPL_EVT) {
        s_scan_active = false;
        if (s_scan_done != NULL) {
            xSemaphoreGive(s_scan_done);
        }
        return;
    }
    if (param->scan_rst.search_evt != ESP_GAP_SEARCH_INQ_RES_EVT ||
        s_result_count >= BLE_SCAN_MAX_RESULTS) {
        return;
    }

    int index = find_result(param->scan_rst.bda);
    if (index < 0) {
        index = (int)s_result_count++;
        memcpy(s_results[index].address, param->scan_rst.bda, ESP_BD_ADDR_LEN);
        s_results[index].address_type = param->scan_rst.ble_addr_type;
        s_results[index].name[0] = '\0';
    }
    s_results[index].rssi = param->scan_rst.rssi;
    s_results[index].adv_len = param->scan_rst.adv_data_len;
    if (s_results[index].adv_len > sizeof(s_results[index].adv_data)) {
        s_results[index].adv_len = sizeof(s_results[index].adv_data);
    }
    memcpy(s_results[index].adv_data, param->scan_rst.ble_adv, s_results[index].adv_len);
    s_results[index].scan_rsp_len = param->scan_rst.scan_rsp_len;
    if (s_results[index].scan_rsp_len > sizeof(s_results[index].scan_rsp_data)) {
        s_results[index].scan_rsp_len = sizeof(s_results[index].scan_rsp_data);
    }
    memcpy(s_results[index].scan_rsp_data,
           param->scan_rst.ble_adv + param->scan_rst.adv_data_len,
           s_results[index].scan_rsp_len);

    uint8_t name_len = 0;
    uint8_t *name = esp_ble_resolve_adv_data(
        param->scan_rst.ble_adv,
        ESP_BLE_AD_TYPE_NAME_CMPL,
        &name_len);
    if (name == NULL) {
        name = esp_ble_resolve_adv_data(
            param->scan_rst.ble_adv,
            ESP_BLE_AD_TYPE_NAME_SHORT,
            &name_len);
    }
    if (name != NULL && name_len > 0) {
        size_t copy_len = name_len < sizeof(s_results[index].name) - 1
                              ? name_len
                              : sizeof(s_results[index].name) - 1;
        memcpy(s_results[index].name, name, copy_len);
        s_results[index].name[copy_len] = '\0';
        s_results[index].has_name = true;
    }
}

static void bt_gap_callback(esp_bt_gap_cb_event_t event, esp_bt_gap_cb_param_t *param)
{
    if (param == NULL) {
        return;
    }
    if (event == ESP_BT_GAP_DISC_STATE_CHANGED_EVT) {
        if (param->disc_st_chg.state == ESP_BT_GAP_DISCOVERY_STOPPED &&
            s_bt_scan_done != NULL) {
            xSemaphoreGive(s_bt_scan_done);
        }
        return;
    }
    if (event == ESP_BT_GAP_READ_REMOTE_NAME_EVT) {
        int index = find_bt_result(param->read_rmt_name.bda);
        if (index >= 0 && param->read_rmt_name.stat == ESP_BT_STATUS_SUCCESS) {
            size_t copy_len = strnlen((const char *)param->read_rmt_name.rmt_name,
                                      sizeof(s_bt_results[index].name) - 1);
            memcpy(s_bt_results[index].name, param->read_rmt_name.rmt_name, copy_len);
            s_bt_results[index].name[copy_len] = '\0';
            s_bt_results[index].has_name = copy_len > 0;
        }
        if (s_bt_name_done != NULL) {
            xSemaphoreGive(s_bt_name_done);
        }
        return;
    }
    if (event != ESP_BT_GAP_DISC_RES_EVT || s_bt_result_count >= BLE_SCAN_MAX_RESULTS) {
        return;
    }

    int index = find_bt_result(param->disc_res.bda);
    if (index < 0) {
        index = (int)s_bt_result_count++;
        memset(&s_bt_results[index], 0, sizeof(s_bt_results[index]));
        memcpy(s_bt_results[index].address, param->disc_res.bda, ESP_BD_ADDR_LEN);
        s_bt_results[index].rssi = -129;
    }
    for (int i = 0; i < param->disc_res.num_prop; ++i) {
        esp_bt_gap_dev_prop_t *property = &param->disc_res.prop[i];
        if (property->type == ESP_BT_GAP_DEV_PROP_RSSI && property->len > 0) {
            s_bt_results[index].rssi = *(int8_t *)property->val;
        } else if (property->type == ESP_BT_GAP_DEV_PROP_COD && property->len >= sizeof(uint32_t)) {
            s_bt_results[index].class_of_device = *(uint32_t *)property->val;
        } else if (property->type == ESP_BT_GAP_DEV_PROP_BDNAME && property->len > 0) {
            size_t copy_len = property->len < sizeof(s_bt_results[index].name) - 1
                                  ? property->len
                                  : sizeof(s_bt_results[index].name) - 1;
            memcpy(s_bt_results[index].name, property->val, copy_len);
            s_bt_results[index].name[copy_len] = '\0';
            s_bt_results[index].has_name = true;
        }
    }
}

static esp_err_t ensure_ble(void)
{
    if (s_bt_ready) {
        return ESP_OK;
    }
    s_scan_done = xSemaphoreCreateBinary();
    ESP_RETURN_ON_FALSE(s_scan_done != NULL, ESP_ERR_NO_MEM, TAG, "BLE semaphore allocation failed");

    esp_bt_controller_config_t controller_config = BT_CONTROLLER_INIT_CONFIG_DEFAULT();
    ESP_RETURN_ON_ERROR(esp_bt_controller_init(&controller_config), TAG, "BLE controller init failed");
    ESP_RETURN_ON_ERROR(esp_bt_controller_enable(ESP_BT_MODE_BTDM), TAG, "Bluetooth controller enable failed");
    ESP_RETURN_ON_ERROR(esp_bluedroid_init(), TAG, "Bluedroid init failed");
    ESP_RETURN_ON_ERROR(esp_bluedroid_enable(), TAG, "Bluedroid enable failed");
    s_bt_scan_done = xSemaphoreCreateBinary();
    ESP_RETURN_ON_FALSE(s_bt_scan_done != NULL, ESP_ERR_NO_MEM, TAG, "Classic Bluetooth semaphore allocation failed");
    s_bt_name_done = xSemaphoreCreateBinary();
    ESP_RETURN_ON_FALSE(s_bt_name_done != NULL, ESP_ERR_NO_MEM, TAG, "Classic Bluetooth name semaphore allocation failed");
    ESP_RETURN_ON_ERROR(esp_bt_gap_register_callback(bt_gap_callback), TAG, "Classic Bluetooth callback registration failed");
    ESP_RETURN_ON_ERROR(esp_ble_gap_register_callback(ble_gap_callback), TAG, "BLE callback registration failed");
    ESP_RETURN_ON_ERROR(esp_ble_gap_set_scan_params((esp_ble_scan_params_t *)&s_scan_params), TAG, "BLE scan setup failed");
    s_bt_ready = true;
    return ESP_OK;
}

static esp_err_t handle_ble_scan(
    esp_openclaw_node_handle_t node,
    void *context,
    const char *params_json,
    size_t params_len,
    char **out_payload_json,
    esp_openclaw_node_error_t *out_error)
{
    (void)node;
    (void)context;
    (void)params_len;

    cJSON *params = NULL;
    esp_err_t err = esp_openclaw_node_example_parse_json_params(params_json, &params, out_error);
    if (err != ESP_OK) {
        return err;
    }
    int scan_ms = BLE_SCAN_DEFAULT_MS;
    cJSON *duration = cJSON_GetObjectItemCaseSensitive(params, "durationMs");
    if (cJSON_IsNumber(duration)) {
        scan_ms = duration->valueint;
    }
    cJSON_Delete(params);
    if (scan_ms < 100 || scan_ms > BLE_SCAN_MAX_MS) {
        out_error->code = "INVALID_PARAMS";
        out_error->message = "durationMs must be between 100 and 10000";
        return ESP_ERR_INVALID_ARG;
    }

    err = ensure_ble();
    if (err != ESP_OK) {
        out_error->message = "BLE initialization failed";
        return err;
    }
    memset(s_results, 0, sizeof(s_results));
    s_result_count = 0;
    s_scan_active = true;
    err = esp_ble_gap_start_scanning((uint32_t)((scan_ms + 999) / 1000));
    if (err != ESP_OK) {
        s_scan_active = false;
        out_error->message = "BLE scan could not start";
        return err;
    }
    if (xSemaphoreTake(s_scan_done, pdMS_TO_TICKS(scan_ms + 3000)) != pdTRUE) {
        if (s_scan_active) {
            esp_ble_gap_stop_scanning();
            s_scan_active = false;
        }
        out_error->message = "BLE scan timed out";
        return ESP_ERR_TIMEOUT;
    }

    cJSON *payload = cJSON_CreateObject();
    cJSON *devices = cJSON_AddArrayToObject(payload, "devices");
    cJSON_AddNumberToObject(payload, "durationMs", scan_ms);
    for (size_t i = 0; i < s_result_count; ++i) {
        char address[18];
        snprintf(address, sizeof(address), "%02X:%02X:%02X:%02X:%02X:%02X",
                 s_results[i].address[0], s_results[i].address[1], s_results[i].address[2],
                 s_results[i].address[3], s_results[i].address[4], s_results[i].address[5]);
        cJSON *device = cJSON_CreateObject();
        cJSON_AddStringToObject(device, "address", address);
        cJSON_AddNumberToObject(device, "addressType", s_results[i].address_type);
        cJSON_AddNumberToObject(device, "rssi", s_results[i].rssi);
        char adv_hex[ESP_BLE_ADV_DATA_LEN_MAX * 2 + 1];
        char scan_rsp_hex[ESP_BLE_SCAN_RSP_DATA_LEN_MAX * 2 + 1];
        for (size_t j = 0; j < s_results[i].adv_len; ++j) {
            snprintf(&adv_hex[j * 2], 3, "%02X", s_results[i].adv_data[j]);
        }
        adv_hex[s_results[i].adv_len * 2] = '\0';
        for (size_t j = 0; j < s_results[i].scan_rsp_len; ++j) {
            snprintf(&scan_rsp_hex[j * 2], 3, "%02X", s_results[i].scan_rsp_data[j]);
        }
        scan_rsp_hex[s_results[i].scan_rsp_len * 2] = '\0';
        if (s_results[i].adv_len > 0) {
            cJSON_AddStringToObject(device, "advertisement", adv_hex);
        }
        if (s_results[i].scan_rsp_len > 0) {
            cJSON_AddStringToObject(device, "scanResponse", scan_rsp_hex);
        }
        if (s_results[i].has_name) {
            cJSON_AddStringToObject(device, "name", s_results[i].name);
        }
        cJSON_AddItemToArray(devices, device);
    }
    return esp_openclaw_node_example_take_json_payload(payload, out_payload_json);
}

static esp_err_t handle_bt_scan(
    esp_openclaw_node_handle_t node,
    void *context,
    const char *params_json,
    size_t params_len,
    char **out_payload_json,
    esp_openclaw_node_error_t *out_error)
{
    (void)node;
    (void)context;
    (void)params_len;

    cJSON *params = NULL;
    esp_err_t err = esp_openclaw_node_example_parse_json_params(params_json, &params, out_error);
    if (err != ESP_OK) {
        return err;
    }
    int scan_ms = BLE_SCAN_DEFAULT_MS;
    cJSON *duration = cJSON_GetObjectItemCaseSensitive(params, "durationMs");
    if (cJSON_IsNumber(duration)) {
        scan_ms = duration->valueint;
    }
    cJSON_Delete(params);
    if (scan_ms < 100 || scan_ms > BLE_SCAN_MAX_MS) {
        out_error->code = "INVALID_PARAMS";
        out_error->message = "durationMs must be between 100 and 10000";
        return ESP_ERR_INVALID_ARG;
    }

    err = ensure_ble();
    if (err != ESP_OK) {
        out_error->message = "Bluetooth initialization failed";
        return err;
    }
    memset(s_bt_results, 0, sizeof(s_bt_results));
    s_bt_result_count = 0;
    uint8_t inquiry_length = (uint8_t)((scan_ms + 1279) / 1280);
    if (inquiry_length < ESP_BT_GAP_MIN_INQ_LEN) {
        inquiry_length = ESP_BT_GAP_MIN_INQ_LEN;
    }
    if (inquiry_length > ESP_BT_GAP_MAX_INQ_LEN) {
        inquiry_length = ESP_BT_GAP_MAX_INQ_LEN;
    }
    err = esp_bt_gap_start_discovery(ESP_BT_INQ_MODE_GENERAL_INQUIRY, inquiry_length, 0);
    if (err != ESP_OK) {
        out_error->message = "Classic Bluetooth inquiry could not start";
        return err;
    }
    if (xSemaphoreTake(s_bt_scan_done, pdMS_TO_TICKS(scan_ms + 4000)) != pdTRUE) {
        esp_bt_gap_cancel_discovery();
        out_error->message = "Classic Bluetooth inquiry timed out";
        return ESP_ERR_TIMEOUT;
    }

    size_t names_requested = 0;
    for (size_t i = 0; i < s_bt_result_count && names_requested < BT_NAME_LOOKUP_MAX; ++i) {
        if (s_bt_results[i].has_name) {
            continue;
        }
        while (xSemaphoreTake(s_bt_name_done, 0) == pdTRUE) {
        }
        if (esp_bt_gap_read_remote_name(s_bt_results[i].address) == ESP_OK) {
            (void)xSemaphoreTake(s_bt_name_done, pdMS_TO_TICKS(BT_NAME_LOOKUP_TIMEOUT_MS));
        }
        names_requested++;
    }

    cJSON *payload = cJSON_CreateObject();
    cJSON *devices = cJSON_AddArrayToObject(payload, "devices");
    cJSON_AddNumberToObject(payload, "durationMs", scan_ms);
    for (size_t i = 0; i < s_bt_result_count; ++i) {
        char address[18];
        snprintf(address, sizeof(address), "%02X:%02X:%02X:%02X:%02X:%02X",
                 s_bt_results[i].address[0], s_bt_results[i].address[1], s_bt_results[i].address[2],
                 s_bt_results[i].address[3], s_bt_results[i].address[4], s_bt_results[i].address[5]);
        cJSON *device = cJSON_CreateObject();
        cJSON_AddStringToObject(device, "address", address);
        cJSON_AddNumberToObject(device, "rssi", s_bt_results[i].rssi);
        cJSON_AddNumberToObject(device, "classOfDevice", s_bt_results[i].class_of_device);
        if (s_bt_results[i].has_name) {
            cJSON_AddStringToObject(device, "name", s_bt_results[i].name);
        }
        cJSON_AddItemToArray(devices, device);
    }
    return esp_openclaw_node_example_take_json_payload(payload, out_payload_json);
}

esp_err_t esp_openclaw_node_example_register_ble_node_commands(
    esp_openclaw_node_handle_t node)
{
    static const esp_openclaw_node_command_t command = {
        .name = "ble.scan",
        .handler = handle_ble_scan,
        .context = NULL,
    };
    static const esp_openclaw_node_command_t classic_command = {
        .name = "bt.scan",
        .handler = handle_bt_scan,
        .context = NULL,
    };
    ESP_RETURN_ON_ERROR(
        esp_openclaw_node_register_capability(node, "bluetooth"),
        TAG,
        "registering Bluetooth capability failed");
    ESP_RETURN_ON_ERROR(
        esp_openclaw_node_register_command(node, &command),
        TAG,
        "registering BLE scan command failed");
    return esp_openclaw_node_register_command(node, &classic_command);
}
