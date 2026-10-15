/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include "esp_err.h"
#include "esp_openclaw_node.h"

esp_err_t esp_openclaw_node_example_register_aht20_node_commands(
    esp_openclaw_node_handle_t node);
