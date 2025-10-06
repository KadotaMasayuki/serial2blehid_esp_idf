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
#include <sys/time.h>

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
#include "my_httpd.h"
#include "my_if_uart.h"
#include "my_softap.h"

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

// 設定値をNVSから読み出して各変数に格納する
extern int my_if_uart_get_config();

void app_main(void) {
    // どうさかくにん
    struct timeval tv_prev1;
    gettimeofday(&tv_prev1, NULL);

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

    // NVS status
    // https://elchika.com/article/6a0634d7-78d9-452d-b233-1d17351e3733/
    nvs_stats_t nvs_stats;
    ret = nvs_get_stats(NULL, &nvs_stats);
    ESP_ERROR_CHECK(ret);
    ESP_LOGI("NVS",
             "Count: UsedEntries = (%lu), FreeEntries = (%lu),"
             " AvailableEntries = (%lu), AllEntries = (%lu)\n",
             (uint32_t)nvs_stats.used_entries, (uint32_t)nvs_stats.free_entries,
             (uint32_t)nvs_stats.available_entries,
             (uint32_t)nvs_stats.total_entries);

    // Soft AP の初期化と使用開始
    my_softap_wifi_init();

    // HTTPd の初期化と使用開始
    my_httpd_start_webserver();

    // BLE initialize
    ble_init();
    ESP_LOGI(tag, "BLE init ok");

    // GPIO/UARTの設定値をNVSから読み出す
    my_if_uart_get_config();

    // 共用バッファ、セマフォ、GPIO/UART を準備し監視タスクを開始する
    my_if_uart_begin(5);
    ESP_LOGI(tag, "GPIO and UART init ok, waiting trigger ...");

    // SoftAPとhttpdが起動していることを示すフラグ
    bool is_softap_live = true;

    // 初期化が一通り終わった時点の時刻を記録しておく
    struct timeval tv_prev;
    gettimeofday(&tv_prev, NULL);

    while (1) {
        vTaskDelay(50 / portTICK_PERIOD_MS);

        // GPIO/UARTタスクの共用バッファを監視し、タスクの外からBLE-HID送信する
        if (xSemaphoreTake(my_if_uart_buffer_semaphore, (TickType_t)10) ==
            pdTRUE) {
            int i = 0;
            while (my_if_uart_buffer[i] != 0) {
                uint8_t k, m;
                uint8_t c = my_if_uart_buffer[i];
                k = KEYCODE_TO_HIDCODE(c);
                m = KEYCODE_TO_HIDMASK(c);
                if (k != 0) {
                    // press
                    if (m > 0) {
                        hid_keyboard_change_keycombination_single(HID_KEY_LEFT_SHIFT, k, true);
                    } else {
                        hid_keyboard_change_keycombination_single(0, k, true);
                    }
                    vTaskDelay(50 / portTICK_PERIOD_MS);
                    // release
                    hid_keyboard_change_keycombination_single(0, 0, false);
                    vTaskDelay(50 / portTICK_PERIOD_MS);
                }
                i++;
            }
            // 処理済みとしてバッファをゼロにしておく
            my_if_uart_buffer[0] = 0;
            // セマフォを返却
            xSemaphoreGive(my_if_uart_buffer_semaphore);
        }

        // 規定時間のhttpd通信無し状態などが続いたら、SoftAPとhttpdを終了する。
        // なお、解除するにはリセットが必要
        //   条件1  SoftAPと接続していない状態で規定時間が経過
        //   条件2  httpdへのアクセスが無い状態で規定時間が経過
        //   条件3  httpdでshutdown_serverリンクを踏む
        if (is_softap_live) {
            // 現在時刻を取得
            struct timeval tv_now;
            gettimeofday(&tv_now, NULL);
            // 判断
            bool should_end_server = false;
            // SoftAP接続が無い状態で規定時間経過したら・・
            if (my_softap_connected_cnt < 1) {
                if (tv_now.tv_sec - tv_prev.tv_sec > 60) {
                    should_end_server = true;
                }
            }
            // httpd通信が無い状態で規定時間経過したら・・
            if (tv_now.tv_sec - my_httpd_last_com_tv.tv_sec > 1200) {
                should_end_server = true;
            }
            // SoftAPとhttpdを終了する
            if (should_end_server) {
                ESP_LOGI(
                    "TIMEUP",
                    "time up -- SoftAP and Http servers are shuttig down..");
                ESP_LOGI("WiFi SoftAP", "Shutdown because of timeup");
                my_softap_stop_ap();
                ESP_LOGI("httpd", "Shutdown because of timeup");
                my_httpd_stop_webserver();
                is_softap_live = false;
            }
        }
    }
}

