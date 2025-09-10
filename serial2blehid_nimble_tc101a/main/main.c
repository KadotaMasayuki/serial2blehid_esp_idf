/*
 * Licensed to the Apache Software Foundation (ASF) under one
 * or more contributor license agreements.  See the NOTICE file
 * distributed with this work for additional information
 * regarding copyright ownership.  The ASF licenses this file
 * to you under the Apache License, Version 2.0 (the
 * "License"); you may not use this file except in compliance
 * with the License.  You may obtain a copy of the License at
 *
 *  http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing,
 * software distributed under the License is distributed on an
 * "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
 * KIND, either express or implied.  See the License for the
 * specific language governing permissions and limitations
 * under the License.
 */

#include <string.h>

#include "driver/gpio.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/semphr.h"
#include "freertos/task.h"
#include "hid_codes.h"
#include "hid_func.h"
// #include "gpio_func.h"

#include "my_hid_key_map.h"
#include "my_if_uart.h"

/* for nvs_storage*/
#define LOCAL_NAMESPACE "storage"

#if ESP_IDF_VERSION < ESP_IDF_VERSION_VAL(4, 0, 0)
typedef nvs_handle nvs_handle_t;
#endif

static const char *tag = "NimBLEKBD_main";

nvs_handle_t Nvs_storage_handle = 0;

/* from ble_func.c */
extern void ble_init();

// UARTで受信した値を格納するバッファを排他制御
extern SemaphoreHandle_t my_if_uart_buffer_semaphore;
// GPIO/UARTの共用バッファ
extern uint8_t *my_if_uart_buffer;

void app_main(void) {
    /* Initialize NVS — it is used to store PHY calibration data and Nimble
     * bonding data */
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES ||
        ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);
    ESP_ERROR_CHECK(
        nvs_open(LOCAL_NAMESPACE, NVS_READWRITE, &Nvs_storage_handle));

    // BLE initialize
    ble_init();
    ESP_LOGI(tag, "BLE init ok");

    // 共用バッファ、セマフォ、GPIO/UART を準備し監視タスクを開始する
    my_if_uart_begin(5);
    ESP_LOGI(tag, "GPIO and UART init ok, waiting trigger ...");

    // GPIO/UARTタスクの共用バッファを監視し、タスクの外からBLE-HID送信する
    while (1) {
        vTaskDelay(50 / portTICK_PERIOD_MS);
        if (xSemaphoreTake(my_if_uart_buffer_semaphore, (TickType_t)10) ==
            pdTRUE) {
            int i = 0;
            while (my_if_uart_buffer[i] != 0) {
                uint8_t k, m;
                uint8_t c = my_if_uart_buffer[i];
                k = KEYCODE_TO_HIDCODE(c);
                m = KEYCODE_TO_HIDMASK(c);
                if (k != 0) {
                    // press -- まずはShiftキーを押してから文字キーを押す
                    if (m > 0) {
                        hid_keyboard_change_key(HID_KEY_LEFT_SHIFT, true);
                        vTaskDelay(50 / portTICK_PERIOD_MS);
                    }
                    hid_keyboard_change_key(k, true);
                    vTaskDelay(50 / portTICK_PERIOD_MS);
                    // release -- まずは文字キーを離してからShiftキーを離す
                    hid_keyboard_change_key(k, false);
                    if (m > 0) {
                        vTaskDelay(50 / portTICK_PERIOD_MS);
                        hid_keyboard_change_key(HID_KEY_LEFT_SHIFT, false);
                    }
                    vTaskDelay(50 / portTICK_PERIOD_MS);
                }
                i++;
            }
            // 処理済みとしてバッファをゼロにしておく
            my_if_uart_buffer[0] = 0;
            // セマフォを返却
            xSemaphoreGive(my_if_uart_buffer_semaphore);
        }
    }
}
