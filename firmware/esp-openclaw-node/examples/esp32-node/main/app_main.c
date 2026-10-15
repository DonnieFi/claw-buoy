/* Modified by Donnie Fiander for Claw Buoy, 2026-10-15.
 * Local application integration and/or memory-conscious handshake diagnostics;
 * see docs/openclaw-integration.md at the repository root for scope.
 */
/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "esp_openclaw_node_example_cmd.h"
#include "esp_openclaw_node_example_repl.h"
#include "esp_openclaw_node_example_repl_cmd.h"
#include "esp_openclaw_node_example_saved_session_reconnect.h"
#include "esp_openclaw_node.h"
#include "esp_openclaw_node_wifi.h"

#include "claw_box_display.h"
#include "esp_event.h"
#include "esp_heap_caps.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "nvs_flash.h"
#include "soc/soc_caps.h"

static const char *TAG = "app_main";
static esp_openclaw_node_handle_t s_node;
static esp_openclaw_node_example_saved_session_reconnect_t s_saved_session_reconnect;

static void handle_node_event(
    esp_openclaw_node_handle_t node,
    esp_openclaw_node_event_t event,
    const void *event_data,
    void *user_ctx)
{
    (void)node;

    esp_openclaw_node_example_saved_session_reconnect_handle_event(
        (esp_openclaw_node_example_saved_session_reconnect_t *)user_ctx,
        event,
        event_data);

    if (event == ESP_OPENCLAW_NODE_EVENT_CONNECTED) {
        claw_box_display_connected();
        ESP_LOGI(TAG, "OpenClaw session connected");
    } else if (event == ESP_OPENCLAW_NODE_EVENT_CONNECT_FAILED) {
        claw_box_display_status("OpenClaw connection failed");
        const esp_openclaw_node_connect_failed_event_t *failed = event_data;
        ESP_LOGW(
            TAG,
            "OpenClaw connect failed: reason=%d local_err=%s gateway_detail=%s",
            failed != NULL ? failed->reason : -1,
            failed != NULL ? esp_err_to_name(failed->local_err) : "n/a",
            failed != NULL && failed->gateway_detail_code != NULL ? failed->gateway_detail_code : "");
    } else if (event == ESP_OPENCLAW_NODE_EVENT_DISCONNECTED) {
        claw_box_display_status("OpenClaw disconnected");
        const esp_openclaw_node_disconnected_event_t *disconnected = event_data;
        ESP_LOGW(
            TAG,
            "OpenClaw disconnected: reason=%d local_err=%s",
            disconnected != NULL ? disconnected->reason : -1,
            disconnected != NULL ? esp_err_to_name(disconnected->local_err) : "n/a");
    }
}

#if !SOC_WIFI_SUPPORTED
#error "examples/esp32-node is a Wi-Fi-specific example and requires a Wi-Fi-capable ESP32 target"
#endif

static void failed_alloc(size_t size, uint32_t caps, const char *function)
{
    ESP_EARLY_LOGW(TAG, "[DEBUG-buoy] alloc_failed size=%u caps=%u function=%s", (unsigned)size, (unsigned)caps, function);
}

void app_main(void)
{
    ESP_ERROR_CHECK(heap_caps_register_failed_alloc_callback(failed_alloc));
    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        err = nvs_flash_init();
    }
    ESP_ERROR_CHECK(err);
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());

    ESP_ERROR_CHECK(esp_openclaw_node_wifi_start());

    esp_openclaw_node_config_t node_config = {0};
    esp_openclaw_node_config_init_default(&node_config);
    node_config.event_cb = handle_node_event;
    node_config.event_user_ctx = &s_saved_session_reconnect;
    ESP_ERROR_CHECK(esp_openclaw_node_create(&node_config, &s_node));
    ESP_ERROR_CHECK(esp_openclaw_node_example_register_node_commands(s_node));
    ESP_ERROR_CHECK(esp_openclaw_node_example_repl_start(s_node));
    err = claw_box_display_start(s_node);
    if (err != ESP_OK) ESP_LOGE(TAG, "Display unavailable: %s; node continues", esp_err_to_name(err));
    ESP_ERROR_CHECK(
        esp_openclaw_node_example_saved_session_reconnect_start(
            &s_saved_session_reconnect,
            s_node,
            "esp_openclaw_node_reconnect"));

    if (!esp_openclaw_node_wifi_is_connected()) {
        ESP_LOGI(TAG, "wifi not connected yet; provision it from the REPL or wait for Wi-Fi to reconnect");
        ESP_LOGI(TAG, "after Wi-Fi is up, use `gateway setup-code`, `gateway token`, `gateway password`, or `gateway no-auth` for a first connect");
        ESP_LOGI(TAG, "saved-session reconnect runs automatically after Wi-Fi returns when a saved session is present");
    }
}
