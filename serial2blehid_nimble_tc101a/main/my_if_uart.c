/**
 * @file my_if_uart.c
 */

#define MYDEBUG 1

#ifndef my_if_uart_c
#define my_if_uart_c 1

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "driver/gpio.h"
#include "driver/uart.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "freertos/semphr.h"
#include "freertos/task.h"
#include "hid_codes.h"
#include "my_debug.h"
#include "my_hid_key_map.h"
#include "my_ring_buffer.h"

// take care of strapping pins for rx, tx and trigger.
#define MY_IF_UART_LED_NUMLOCK_PIN_GPIO (-1)
#define MY_IF_UART_LED_CAPSLOCK_PIN_GPIO (-1)
#define MY_IF_UART_LED_SCROLLLOCK_PIN_GPIO (-1)
#define MY_IF_UART_LED_COMPOSE_PIN_GPIO (-1)
#define MY_IF_UART_LED_KANA_PIN_GPIO (-1)
// #define MY_IF_UART_TRIGGER_PIN_GPIO (5)
#define MY_IF_UART_TRIGGER_PIN_GPIO (9)  // 9 is Boot Swith at Xiao-ESP32C3
#define MY_IF_UART_RXD_PIN_GPIO (3)
#define MY_IF_UART_TXD_PIN_GPIO (4)
#define MY_IF_UART_RTS_PIN_GPIO (UART_PIN_NO_CHANGE)
#define MY_IF_UART_CTS_PIN_GPIO (UART_PIN_NO_CHANGE)
#define MY_IF_UART_PORT_NUM (1)
#define MY_IF_UART_BAUD_RATE (4800)  // 1200, 2400, 4800, 9600 を選択可能
#define MY_IF_UART_TASK_STACK_SIZE (2048)
#define MY_IF_UART_BUF_SIZE (1024)
#define MY_IF_UART_TAG "IF_UART"

// 受信開始のために送出するコマンド
char request_command[] = {'Q', 'X', 0x0d, 0x0a};
// 送出するコマンドの長さ
int request_command_len = 4;

// 受信バッファサイズ
int receive_buffer_len = 15;

// 受信する末尾文字列
uint8_t terminator_sequence[] = {0x0d, 0x0a};
// 受信する末尾文字列の長さ
int terminator_sequence_len = 2;

// 受信した末尾文字列を、送信するときにこの内容に置換する
uint8_t terminator_sequence_replace[] = {0x09};
// 置換文字列の長さ
int terminator_sequence_replace_len = 1;

// UARTで受信した値を格納するバッファを排他制御する
SemaphoreHandle_t my_if_uart_buffer_semaphore = NULL;
// main.cで参照するための、リングバッファのコピー。末端置換を行うので少し大きめに確保
uint8_t *my_if_uart_buffer = NULL;


// テスト等でUART接続先が無い場合、受信したことにするダミーコードへ分岐するフラグ。1:ダミーコード。0:本番
#define MY_IF_UART_NO_UART 0



/**
 * @brief
 * 共用バッファを確保しなおす。
 * @return 成功すればゼロ、失敗すれば１
 */
int my_if_uart_reset_buffer() {
    if (xSemaphoreTake(my_if_uart_buffer_semaphore, (TickType_t)10) == pdTRUE) {
        free(my_if_uart_buffer);
        my_if_uart_buffer = (uint8_t *)calloc(
            receive_buffer_len + terminator_sequence_replace_len + 1,
            sizeof(uint8_t));
        xSemaphoreGive(my_if_uart_buffer_semaphore);
        if (my_if_uart_buffer == NULL) {
            return 1;
        } else {
            return 0;
        }
    } else {
        return 1;
    }
}

/**
 * @brief
 * キーボードのLEDを点灯させるよう通信があった場合、GPIOピンを操作することで疑似的に対応する
 * @param hid_leds  bit0から4をNUMLOCK, CAPSLOCK, SCROLLLOCK, COMPOSE,
 * KANAに割り当て
 */
int my_if_uart_set_leds(uint8_t hid_leds) {
    // LEDS: bit 0 NUM LOCK, 1 CAPS LOCK, 2 SCROLL LOCK, 3 COMPOSE, 4 KANA, 5 to
    // 7 CONSTANT
    if (MY_IF_UART_LED_NUMLOCK_PIN_GPIO >= 0)
        gpio_set_level(MY_IF_UART_LED_NUMLOCK_PIN_GPIO,
                       (hid_leds & 0x01) ? 1 : 0);
    if (MY_IF_UART_LED_CAPSLOCK_PIN_GPIO >= 0)
        gpio_set_level(MY_IF_UART_LED_CAPSLOCK_PIN_GPIO,
                       (hid_leds & 0x02) ? 1 : 0);
    if (MY_IF_UART_LED_SCROLLLOCK_PIN_GPIO >= 0)
        gpio_set_level(MY_IF_UART_LED_SCROLLLOCK_PIN_GPIO,
                       (hid_leds & 0x04) ? 1 : 0);
    if (MY_IF_UART_LED_COMPOSE_PIN_GPIO >= 0)
        gpio_set_level(MY_IF_UART_LED_COMPOSE_PIN_GPIO,
                       (hid_leds & 0x08) ? 1 : 0);
    if (MY_IF_UART_LED_KANA_PIN_GPIO >= 0)
        gpio_set_level(MY_IF_UART_LED_KANA_PIN_GPIO, (hid_leds & 0x10) ? 1 : 0);
    return 0;
}

/**
 * @brief process uart, send command and wait response. called by xCreateTask()
 *        Nicon TC-101A RS-232C interface
 *          データ伝送方式：非同期全二重通信
 *          データビット長：８ビット
 *          ストップビット：２ビット
 *          パリティ：なし
 *          デリミタ：CR+LF
 *          ボーレート：1200, 2400, 4800, 9600 のいずれか（初期値4800）
 *          TC-101Aが受け取るデータ：
 *              リセットコマンド    "RX" + CR + LF
 *              リクエストコマンド  "QX" + CR + LF
 *          TC-101Aが送出するデータ長：13文字 + CR + LF
 */
void my_if_uart_task(void *arg) {
    ESP_LOGI(MY_IF_UART_TAG, "starting");

    // GPIO
    DEBUGPRINT("GPIO init");
    // GPIO input
    //   Trigger input
    gpio_reset_pin(MY_IF_UART_TRIGGER_PIN_GPIO);
    gpio_set_direction(MY_IF_UART_TRIGGER_PIN_GPIO, GPIO_MODE_INPUT);
    gpio_pullup_en(MY_IF_UART_TRIGGER_PIN_GPIO);
    // GPIO output
    int out_pins[5] = {
        MY_IF_UART_LED_NUMLOCK_PIN_GPIO, MY_IF_UART_LED_CAPSLOCK_PIN_GPIO,
        MY_IF_UART_LED_SCROLLLOCK_PIN_GPIO, MY_IF_UART_LED_COMPOSE_PIN_GPIO,
        MY_IF_UART_LED_KANA_PIN_GPIO};
    for (int i = 0; i < 5; i++) {
        if (out_pins[i] >= 0) {
            gpio_reset_pin(out_pins[i]);
            gpio_set_direction(out_pins[i], GPIO_MODE_INPUT);
            gpio_set_level(out_pins[i], 0);
        }
    }

    // UART
    DEBUGPRINT("UART init");
    uart_config_t uart_config = {
        .baud_rate = MY_IF_UART_BAUD_RATE,
        .data_bits = UART_DATA_8_BITS,
        .parity = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_2,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
        .source_clk = UART_SCLK_DEFAULT,
    };
    int intr_alloc_flags = 0;

#if CONFIG_UART_ISR_IN_IRAM
    intr_alloc_flags = ESP_INTR_FLAG_IRAM;
#endif

    ESP_ERROR_CHECK(
        uart_driver_install(MY_IF_UART_PORT_NUM, MY_IF_UART_BUF_SIZE,
                            MY_IF_UART_BUF_SIZE, 0, NULL, intr_alloc_flags));
    ESP_ERROR_CHECK(uart_param_config(MY_IF_UART_PORT_NUM, &uart_config));
    ESP_ERROR_CHECK(uart_set_pin(
        MY_IF_UART_PORT_NUM, MY_IF_UART_TXD_PIN_GPIO, MY_IF_UART_RXD_PIN_GPIO,
        MY_IF_UART_RTS_PIN_GPIO, MY_IF_UART_CTS_PIN_GPIO));
    DEBUGPRINT("UART inited");

    // UART受信のためにリングバッファを用意する
    my_ring_buffer_t rb;
    my_ring_buffer_init(&rb, receive_buffer_len);
    DEBUGPRINT("ring buffer inited");

    DEBUGPRINT("Trigger waiting...");

    // トリガーピンの、1回前の電圧
    int lvl0 = 1;
    // トリガーピンのL->Hを確認する
    while (1) {
        vTaskDelay(50 / portTICK_PERIOD_MS);
        // vTaskDelay(1000 / portTICK_PERIOD_MS);
        //  トリガーピンの電圧
        int lvl1 = gpio_get_level(MY_IF_UART_TRIGGER_PIN_GPIO);
        if (lvl0 != lvl1) {
            DEBUGPRINT("Trigger Level Changed = %d -> %d", lvl0, lvl1);
        }
        if (lvl0 == 0 && lvl1 == 1) {
            if (xSemaphoreTake(my_if_uart_buffer_semaphore, (TickType_t)10) ==
                pdTRUE) {
                // トリガーが来た(一回前がL、現在がH）なら、データリクエストを送る
                ESP_LOGI(MY_IF_UART_TAG, "Triggered L->H");
                // リングバッファをクリアしておく
                my_ring_buffer_reset(&rb);
                // 送信前にreadバッファを空にしておく
                uart_flush(MY_IF_UART_PORT_NUM);
#if MY_IF_UART_NO_UART==0  // UART接続部分
                ESP_LOGI(MY_IF_UART_TAG, "Process UART");
                // データリクエストコマンドを送信。
                // 返り値は送信サイズと等しい・・はず
                int wrote_size = uart_write_bytes(
                    MY_IF_UART_PORT_NUM, request_command, request_command_len);
                // 終端文字列が来るまで何回か受信する。終端文字列が来なければ受信失敗とみなして何もしない。
                bool receive_completed = false;
                int cnt = 3;
                while (cnt-- > 0) {
                    // 終端文字列で終わるデータを受信する
                    // データの総量はリングバッファにより制限される(receive_buffer_lenバイト)
                    uint8_t tmp_buf[receive_buffer_len];
                    int read_len = uart_read_bytes(MY_IF_UART_PORT_NUM, tmp_buf,
                                                   receive_buffer_len,
                                                   50 / portTICK_PERIOD_MS);
                    for (int i = 0; i < read_len; i++) {
                        // リングバッファに1文字格納
                        my_ring_buffer_push(&rb, tmp_buf[i]);
                        // ターミネーター末尾の文字と一致していたら、ターミネーター文字列全てが合致しているか、
                        // 格納済みのリングバッファをチェック
                        if (tmp_buf[i] ==
                            terminator_sequence[terminator_sequence_len - 1]) {
                            bool terminated = true;
                            uint8_t d;
                            for (int j = 0; j < terminator_sequence_len; j++) {
                                my_ring_buffer_at(
                                    &rb, j - terminator_sequence_len, &d);
                                if (terminator_sequence[j] != d) {
                                    terminated = false;
                                    break;
                                }
                            }
                            if (terminated) {
                                receive_completed = true;
                                break;
                            }
                        }
                    }
                    if (receive_completed) {
                        break;
                    }
                    vTaskDelay(100 / portTICK_PERIOD_MS);
                }  // while(cnt...)
                if (!receive_completed) {
                    ESP_LOGI(MY_IF_UART_TAG, "Failed receive UART");
                }
                // 一定期間中に受信しきれなかった場合は受信できていないとみなし、なにもせずにトリガ待ちに移行する。
                // 次回受信時にリングバッファをクリアして再受信。
                // 受信しきれた場合は、処理を進める
#else  // MY_IF_UART_NO_UART //
       // UART接続先が居ないテスト環境の時などに、受信したふりをする
                ESP_LOGI(MY_IF_UART_TAG, "Dummy UART");
                my_ring_buffer_push(&rb, '0');
                my_ring_buffer_push(&rb, '1');
                my_ring_buffer_push(&rb, '2');
                my_ring_buffer_push(&rb, '!');
                my_ring_buffer_push(&rb, '#');
                my_ring_buffer_push(&rb, '$');
                my_ring_buffer_push(&rb, '%');
                my_ring_buffer_push(&rb, '@');
                my_ring_buffer_push(&rb, ';');
                my_ring_buffer_push(&rb, '9');
                my_ring_buffer_push(&rb, '\x0d');
                my_ring_buffer_push(&rb, '\x0a');
                bool receive_completed = true;
#endif  // MY_IF_UART_NO_UART
                if (receive_completed) {
                    // 16進表示用のバッファを準備
                    char buf_str[(receive_buffer_len +
                                  terminator_sequence_replace_len + 1) *
                                 4];
                    char *buf_ptr;
                    // リングバッファから共用バッファに取り出す
                    int ring_buffer_cnt = my_ring_buffer_content_length(&rb);
                    for (int i = 0; i < ring_buffer_cnt; i++) {
                        if (!my_ring_buffer_pop(&rb, &(my_if_uart_buffer[i]))) {
                            break;
                        }
                    }
                    // 末尾に\0を付与しておく
                    my_if_uart_buffer[ring_buffer_cnt] = 0;
#ifdef MYDEBUG
                    // リングバッファの内容を表示
                    buf_ptr = buf_str;
                    for (int i = 0; i < ring_buffer_cnt; i++) {
                        buf_ptr += sprintf(buf_ptr, " %02X",
                                           (unsigned char)my_if_uart_buffer[i]);
                    }
                    ESP_LOGI(MY_IF_UART_TAG, "Received. Ring-Buffer is %s",
                             my_if_uart_buffer);
                    ESP_LOGI(MY_IF_UART_TAG, "  -> %s", buf_str);
#endif
                    // 末尾のターミネーター文字列を別の文字列に変換する
                    int base_len = 0;
                    if (ring_buffer_cnt > terminator_sequence_len) {
                        base_len = ring_buffer_cnt - terminator_sequence_len;
                    } else {
                        base_len = 0;
                    }
                    for (int i = 0; i < terminator_sequence_replace_len; i++) {
                        my_if_uart_buffer[base_len + i] =
                            terminator_sequence_replace[i];
                    }
                    // 末尾に\0を付与しておく
                    my_if_uart_buffer[base_len +
                                      terminator_sequence_replace_len] = 0;
#ifdef MYDEBUG
                    // 変換後のバッファの内容を表示
                    buf_ptr = buf_str;
                    for (int i = 0;
                         i < base_len + terminator_sequence_replace_len; i++) {
                        buf_ptr += sprintf(buf_ptr, " %02X",
                                           (unsigned char)my_if_uart_buffer[i]);
                    }
                    ESP_LOGI(MY_IF_UART_TAG, "Translated. Send-Buffer is %s",
                             my_if_uart_buffer);
                    ESP_LOGI(MY_IF_UART_TAG, "  -> %s", buf_str);
#endif
                }  // receive completed
                // セマフォを返却する
                xSemaphoreGive(my_if_uart_buffer_semaphore);
                // 送信後、ちょっと多めに待機する
                vTaskDelay(300 / portTICK_PERIOD_MS);
            } else {
                ESP_LOGI(MY_IF_UART_TAG,
                         "Triggered. but can not take semaphore.");
            }  // xSemaphoreTake
        }      // trigger (H -> L)
        // トリガーピンの履歴を更新する
        lvl0 = lvl1;
    }  // while(1)
}

/**
 * @brief touch point for user defined interface
 * 共用バッファ用のセマフォと共用バッファを準備する。
 * GPIO/UART監視タスクを起動する。
 */
void my_if_uart_begin(int priority) {
    // 共用バッファ用のセマフォ
    my_if_uart_buffer_semaphore = xSemaphoreCreateBinary();
    if (my_if_uart_buffer_semaphore == NULL) {
        ESP_LOGE(MY_IF_UART_TAG, "Can not create semaphore! %p",
                 my_if_uart_buffer_semaphore);
        vTaskDelay(30000 / portTICK_PERIOD_MS);
        esp_restart();
    }
    // xCreateするとTake済み状態になるのでGiveしておく
    xSemaphoreGive(my_if_uart_buffer_semaphore);
    // 共用バッファを確保
    if (my_if_uart_reset_buffer() != 0) {
        ESP_LOGE(MY_IF_UART_TAG, "Can not allocate my_if_uart_buffer");
        vTaskDelay(30000 / portTICK_PERIOD_MS);
        esp_restart();
    }
    // GPIO/UART監視タスク
    xTaskCreate(my_if_uart_task, "i/f task uart", MY_IF_UART_TASK_STACK_SIZE,
                NULL, priority, NULL);
}

#endif

