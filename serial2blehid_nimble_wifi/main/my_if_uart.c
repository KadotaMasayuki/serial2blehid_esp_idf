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

#define MY_IF_UART_NVS_NAME "A"
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
uint32_t my_if_uart_baud_rate = 4800;  // 1200, 2400, 4800, 9600 を選択可能
const uint32_t my_if_uart_baud_rate_min = 1200;
const uint32_t my_if_uart_baud_rate_max = 9600;
#define MY_IF_UART_TASK_STACK_SIZE (2048)
#define MY_IF_UART_BUF_SIZE (1024)
#define MY_IF_UART_TAG "IF_UART"

// リングバッファ
my_ring_buffer_t rb;

// 受信バッファサイズ
int my_if_uart_receive_buffer_len = 15;
const int my_if_uart_receive_buffer_len_min = 15;
const int my_if_uart_receive_buffer_len_max = 99;

// 受信開始のために送出するコマンド文字列
char *my_if_uart_request_command;

// 送出するコマンドの長さ
int my_if_uart_request_command_len = 0;
const int my_if_uart_request_command_len_min = 0;
const int my_if_uart_request_command_len_max = 40;

// 受信する末尾文字列
char *my_if_uart_terminator_sequence;

// 受信する末尾文字列の長さ
int my_if_uart_terminator_sequence_len = 0;
const int my_if_uart_terminator_sequence_len_min = 0;
const int my_if_uart_terminator_sequence_len_max = 40;

// 受信した末尾文字列を、送信するときにこの内容に置換する
char *my_if_uart_terminator_sequence_replace;

// 置換文字列の長さ
int my_if_uart_terminator_sequence_replace_len = 0;
const int my_if_uart_terminator_sequence_replace_len_min = 0;
const int my_if_uart_terminator_sequence_replace_len_max = 40;

// UARTで受信した値を格納するバッファを排他制御する
SemaphoreHandle_t my_if_uart_buffer_semaphore = NULL;

// main.cで参照するための、リングバッファのコピー。末端置換を行うので少し大きめに確保
uint8_t *my_if_uart_buffer = NULL;

// テスト等でUART接続先が無い場合、受信したことにするダミーコードへ分岐するフラグ。1:ダミーコード。0:本番
#define MY_IF_UART_NO_UART 0

/**
 * @brief
 * 16進表記の文字列を受け取り、8ビットずつの配列を返す
 * @param buf[]
 * 16進表記の文字列。文字数が奇数の場合、一番右の8ビット値は上位４ビットのみが保障される。
 * @param buf_len 16進表記文字列の長さ
 * @param out_arr[] 8ビットずつの配列
 * @return 成功したらゼロ
 */
int my_if_uart_str16_to_arr8(char buf[], int buf_len, char out_arr[]) {
    char *lastp;
    char buf2[3] = {0, 0, 0};
    int idx = 0;
    for (int i = 0; i < buf_len; i++) {
        if (i + 1 < buf_len) {
            buf2[0] = buf[2 * i];
            buf2[1] = buf[2 * i + 1];
        } else {
            buf2[0] = buf[2 * i];
            buf2[1] = 0;
        }
        out_arr[idx] = (char)(strtol(buf2, &lastp, 16) & 0xff);
        idx++;
    }
    return 0;
}

/**
 * @brief
 * １つの文字列を分解して各設定に格納する
 * @param buf[]
 * 設定がパックされている文字列。処理によりこの文字列は破壊されるため、困る場合はコピーを渡すこと。
 * @return 成功したらゼロ
 */
int my_if_uart_unpack_config(char buf[]) {
    char *p, *p2, *token;
    uint32_t tmp;
    // 分解開始
    p = buf;
    // 受信バッファサイズ
    token = strsep(&p, ",");
    if (*token == '\0') {
        // 長さゼロ
        ESP_LOGI(MY_IF_UART_TAG,
                 "Receive Buffer Size : < none >, Not available");
        return 1;
    } else {
        tmp = strtol(token, &p2, 10);
        if (tmp < my_if_uart_receive_buffer_len_min) {
            // 規定サイズ未満
            ESP_LOGI(MY_IF_UART_TAG,
                     "Receive Buffer Size : %lu is less than %d. Not available",
                     tmp, my_if_uart_receive_buffer_len_min);
            return 1;
        } else if (tmp > my_if_uart_receive_buffer_len_max) {
            // 規定サイズ越え
            ESP_LOGI(
                MY_IF_UART_TAG,
                "Receive Buffer Size : %lu is greater than %d. Not available",
                tmp, my_if_uart_receive_buffer_len_max);
            return 1;
        } else {
            my_if_uart_receive_buffer_len = tmp;
            ESP_LOGI(MY_IF_UART_TAG, "Receive Buffer Size : %s -> %d", token,
                     my_if_uart_receive_buffer_len);
        }
    }
    // 通信速度
    token = strsep(&p, ",");
    if (*token == '\0') {
        // 長さゼロ
        ESP_LOGI(MY_IF_UART_TAG, "Baud Rate : < none >, Not available");
        return 1;
    } else {
        tmp = strtol(token, &p2, 10);
        if (tmp < my_if_uart_baud_rate_min) {
            // 規定速度未満
            ESP_LOGI(MY_IF_UART_TAG,
                     "Baud Rate : %lu is less than %lu. Not available", tmp,
                     my_if_uart_baud_rate_min);
            return 1;
        } else if (tmp > my_if_uart_baud_rate_max) {
            // 規定速度超え
            ESP_LOGI(MY_IF_UART_TAG,
                     "Baud Rate : %lu is greater than %lu. Not available", tmp,
                     my_if_uart_baud_rate_max);
            return 1;
        } else {
            my_if_uart_baud_rate = tmp;
            ESP_LOGI(MY_IF_UART_TAG, "Baud Rate: %s -> %lu", token,
                     my_if_uart_baud_rate);
        }
    }
    // 送信コマンド。16進文字列になっているものを、8ビット配列にする
    char *buf8;
    token = strsep(&p, ",");
    if (*token == '\0') {
        // 長さゼロ
        ESP_LOGI(MY_IF_UART_TAG, "Request Command : < none >");
        my_if_uart_request_command_len = 0;
        free(my_if_uart_request_command);
        my_if_uart_request_command = malloc(1);
        my_if_uart_request_command[0] = 0;  // 空文字
    } else {
        tmp = (int)((strlen(token) + 1) / 2);
        if (tmp < my_if_uart_request_command_len_min) {
            // 規定サイズ未満
            ESP_LOGI(MY_IF_UART_TAG,
                     "Receive Buffer Size : %lu is less than %d. Not available",
                     tmp, my_if_uart_request_command_len_min);
            return 1;
        } else if (tmp > my_if_uart_request_command_len_max) {
            // 規定サイズ越え
            ESP_LOGI(
                MY_IF_UART_TAG,
                "Receive Buffer Size : %lu is greater than %d. Not available",
                tmp, my_if_uart_request_command_len_max);
            return 1;
        } else {
            if ((buf8 = malloc(tmp)) == NULL) return 1;
            if (my_if_uart_str16_to_arr8(token, strlen(token), buf8) != 0) {
                free(buf8);
                return 1;
            }
            free(my_if_uart_request_command);
            my_if_uart_request_command = buf8;
            my_if_uart_request_command_len = tmp;
            ESP_LOGI(MY_IF_UART_TAG, "Request Command : %s -> %s(%d)", token,
                     my_if_uart_request_command,
                     my_if_uart_request_command_len);
        }
    }
    // 受信ターミネータ。16進文字列になっているものを、8ビット配列にする
    token = strsep(&p, ",");
    if (*token == '\0') {
        // 長さゼロ
        ESP_LOGI(MY_IF_UART_TAG, "Receive Terminator : < none >");
        my_if_uart_terminator_sequence_len = 0;
        free(my_if_uart_terminator_sequence);
        my_if_uart_terminator_sequence = malloc(1);
        my_if_uart_terminator_sequence[0] = 0;  // 空文字
    } else {
        tmp = (int)((strlen(token) + 1) / 2);
        if (tmp < my_if_uart_terminator_sequence_len_min) {
            // 規定サイズ未満
            ESP_LOGI(MY_IF_UART_TAG,
                     "Terminator Size : %lu is less than %d. Not available",
                     tmp, my_if_uart_terminator_sequence_len_min);
            return 1;
        } else if (tmp > my_if_uart_terminator_sequence_len_max) {
            // 規定サイズ超え
            ESP_LOGI(MY_IF_UART_TAG,
                     "Terminator Size : %lu is greater than %d. Not available",
                     tmp, my_if_uart_terminator_sequence_len_max);
            return 1;
        } else {
            if ((buf8 = malloc(tmp)) == NULL) return 1;
            if (my_if_uart_str16_to_arr8(token, strlen(token), buf8) != 0) {
                free(buf8);
                return 1;
            }
            free(my_if_uart_terminator_sequence);
            my_if_uart_terminator_sequence = buf8;
            my_if_uart_terminator_sequence_len = tmp;
            ESP_LOGI(MY_IF_UART_TAG, "Receive Terminator : %s -> %s(%d)", token,
                     my_if_uart_terminator_sequence,
                     my_if_uart_terminator_sequence_len);
        }
    }
    // 受信ターミネータの変換先。16進文字列になっているものを、8ビット配列にする
    token = strsep(&p, ",");
    if (*token == '\0') {
        // 長さゼロ
        ESP_LOGI(MY_IF_UART_TAG, "Receive Terminator Replace : < none >");
        my_if_uart_terminator_sequence_replace_len = 0;
        free(my_if_uart_terminator_sequence_replace);
        my_if_uart_terminator_sequence_replace = malloc(1);
        my_if_uart_terminator_sequence_replace[0] = 0;  // 空文字
    } else {
        tmp = (int)((strlen(token) + 1) / 2);
        if (tmp < my_if_uart_terminator_sequence_replace_len_min) {
            // 規定サイズ未満
            ESP_LOGI(
                MY_IF_UART_TAG,
                "Terminator Replace Size : %lu is less than %d. Not available",
                tmp, my_if_uart_terminator_sequence_replace_len_min);
            return 1;
        } else if (tmp > my_if_uart_terminator_sequence_replace_len_max) {
            // 規定サイズ超え
            ESP_LOGI(MY_IF_UART_TAG,
                     "Terminator Replace Size : %lu is greater than %d. Not "
                     "available",
                     tmp, my_if_uart_terminator_sequence_replace_len_min);
            return 1;
        } else {
            if ((buf8 = malloc(tmp)) == NULL) return 1;
            if (my_if_uart_str16_to_arr8(token, strlen(token), buf8) != 0) {
                free(buf8);
                return 1;
            }
            free(my_if_uart_terminator_sequence_replace);
            my_if_uart_terminator_sequence_replace = buf8;
            my_if_uart_terminator_sequence_replace_len = tmp;
            ESP_LOGI(MY_IF_UART_TAG,
                     "Receive Terminator Replace : %s -> %s(%d)", token,
                     my_if_uart_terminator_sequence_replace,
                     my_if_uart_terminator_sequence_replace_len);
        }
    }
    return 0;
}

/**
 * @brief
 * 各設定のパック後の文字列長の最大値
 */
int my_if_uart_get_pack_config_max_len() {
    int l = 0;
    uint32_t a;
    a = my_if_uart_receive_buffer_len_max;
    do {
        l++;
    } while ((a /= 10) > 0);
    l++;  // カンマ
    a = my_if_uart_baud_rate_max;
    do {
        l++;
    } while ((a /= 10) > 0);
    l++;  // カンマ
    l += my_if_uart_request_command_len_max * 2;
    l++;  // カンマ
    l += my_if_uart_terminator_sequence_len_max * 2;
    l++;  // カンマ
    l += my_if_uart_terminator_sequence_replace_len_max * 2;
    l += 2;  // カンマと'\0'
    return l;
}

/**
 * @brief
 * 各設定のパック後の文字列長を得る
 */
int my_if_uart_get_pack_config_len() {
    int l = 0;
    uint32_t a;
    a = my_if_uart_receive_buffer_len;
    do {
        l++;
    } while ((a /= 10) > 0);
    l++;  // カンマ
    a = my_if_uart_baud_rate;
    do {
        l++;
    } while ((a /= 10) > 0);
    l++;  // カンマ
    l += my_if_uart_request_command_len * 2;
    l++;  // カンマ
    l += my_if_uart_terminator_sequence_len * 2;
    l++;  // カンマ
    l += my_if_uart_terminator_sequence_replace_len * 2;
    l += 2;  // カンマと'\0'
    return l;
}

/**
 * @brief
 * 各設定を１つの文字列にパックする
 * @param buf[]
 * パックした文字列が格納される。サイズはmy_if_uart_get_pack_config_len()による。
 * @return 成功したらゼロ
 */
int my_if_uart_pack_config(char buf[]) {
    // 設定値それぞれを文字列化してカンマで連結したときに必要なサイズを算出
    char *p = buf;
    int len = my_if_uart_get_pack_config_len();
    char buf16[len];
    char *p16;
    // 受信バッファサイズを文字列化しカンマを追加
    p += sprintf(p, "%d,", my_if_uart_receive_buffer_len);
    // 通信速度を文字列化しカンマを追加
    p += sprintf(p, "%ld,", my_if_uart_baud_rate);
    // 送信コマンドを１６進文字列化しカンマを追加
    buf16[0] = 0;
    p16 = buf16;
    for (int i = 0; i < my_if_uart_request_command_len; i++) {
        p16 += sprintf(p16, "%02x", my_if_uart_request_command[i]);
    }
    p += sprintf(p, "%s,", buf16);
    // 受信ターミネータを１６進文字列化しカンマを追加
    buf16[0] = 0;
    p16 = buf16;
    for (int i = 0; i < my_if_uart_terminator_sequence_len; i++) {
        p16 += sprintf(p16, "%02x", my_if_uart_terminator_sequence[i]);
    }
    p += sprintf(p, "%s,", buf16);
    // 受信ターミネータ置換を１６進文字列化しカンマを追加
    // 最後の要素の末尾にも区切り文字を入れることで、最後の要素が空白の時でもstrepが正しく動く
    buf16[0] = 0;
    p16 = buf16;
    for (int i = 0; i < my_if_uart_terminator_sequence_replace_len; i++) {
        p16 += sprintf(p16, "%02x", my_if_uart_terminator_sequence_replace[i]);
    }
    p += sprintf(p, "%s,", buf16);
    return 0;
}

/**
 * @brief
 * 設定値をNVSから読み出して各変数に格納する
 * @return 成功したらゼロ
 */
int my_if_uart_get_config() {
    size_t len;
    esp_err_t err;
    nvs_handle_t handle;
    // open
    err = nvs_open(MY_IF_UART_NVS_NAME, NVS_READWRITE, &handle);
    if (err != ESP_OK) return err;
    // get size, include the '\0' terminator.
    err = nvs_get_str(handle, MY_IF_UART_NVS_NAME, 0, &len);
    if (err != ESP_OK) {
        ESP_LOGI(MY_IF_UART_TAG, "read nvs: fail to open");
        nvs_close(handle);
        return err;
    }
    char buf[len];
    // load
    DEBUGPRINT("NVS LOAD CONFIG: %s, %d byte\n", MY_IF_UART_NVS_NAME, len);
    err = nvs_get_str(handle, MY_IF_UART_NVS_NAME, buf, &len);
    if (err != ESP_OK) {
        ESP_LOGI(MY_IF_UART_TAG, "read nvs: fail to read string");
        nvs_close(handle);
        return err;
    }
    // close
    nvs_close(handle);
    ESP_LOGI(MY_IF_UART_TAG, "data has read : %s", buf);
    // アンパックする
    if (my_if_uart_unpack_config(buf) != 0) {
        ESP_LOGI(MY_IF_UART_TAG, "read nvs: unpack fail");
        return 1;
    }
    return 0;
}

/**
 * @brief
 * 設定値を１行の文字列にまとめて保存する
 * @return 成功したらゼロ
 */
int my_if_uart_set_config() {
    int len = my_if_uart_get_pack_config_len();
    char buf[len];
    // パックした文字列を取得
    if (my_if_uart_pack_config(buf) != 0) {
        return 1;
    }
    ESP_LOGI(MY_IF_UART_TAG, "data to write : %s", buf);
    // パックした文字列を保存
    esp_err_t err;
    nvs_handle_t handle;
    // open
    err = nvs_open(MY_IF_UART_NVS_NAME, NVS_READWRITE, &handle);
    if (err != ESP_OK) return 1;
    // store
    DEBUGPRINT("NVS STORE CONFIG: %s, %d byte\n", MY_IF_UART_NVS_NAME,
               strlen(buf));
    err = nvs_set_str(handle, MY_IF_UART_NVS_NAME, buf);
    if (err != ESP_OK) {
        nvs_close(handle);
        return 1;
    }
    // commit
    nvs_commit(handle);
    if (err != ESP_OK) {
        nvs_close(handle);
        return 1;
    }
    // close
    nvs_close(handle);
    return 0;
}

/**
 * @brief
 * 共用バッファを確保しなおす。
 * @return 成功すればゼロ、失敗すれば１
 */
int my_if_uart_reset_buffer() {
    if (xSemaphoreTake(my_if_uart_buffer_semaphore, (TickType_t)10) == pdTRUE) {
        free(my_if_uart_buffer);
        my_if_uart_buffer = (uint8_t *)calloc(
            my_if_uart_receive_buffer_len +
                my_if_uart_terminator_sequence_replace_len + 1,
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
    //   キーボードのインジケーター
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
        .baud_rate = my_if_uart_baud_rate,
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
    my_ring_buffer_init(&rb, my_if_uart_receive_buffer_len);
    DEBUGPRINT("ring buffer inited");

    DEBUGPRINT("Trigger waiting...");

    // UART通信状態。trueで通信。
    bool on_communication = false;
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
        // トリガを解釈して通信を行う
        // BLE-HID送信中にトリガを解釈しないよう、セマフォを取得してから解釈する
        if (xSemaphoreTake(my_if_uart_buffer_semaphore, (TickType_t)10) ==
            pdTRUE) {
            if (lvl0 == 0 && lvl1 == 1) {
                if (my_if_uart_request_command_len > 0) {
                    // リクエストコマンドがある場合はここでonし、通信終了時にoffする。
                    on_communication = true;
                } else {
                    // リクエストコマンドがない場合は、通信状態を切り替える
                    on_communication = !on_communication;
                }
            }
            if (on_communication) {
                ESP_LOGI(MY_IF_UART_TAG, "Triggered L->H");
                // リングバッファをクリアしておく
                my_ring_buffer_reset(&rb);
                // 送信前にreadバッファを空にしておく
                uart_flush(MY_IF_UART_PORT_NUM);
#if MY_IF_UART_NO_UART == 0  // UART接続部分
                ESP_LOGI(MY_IF_UART_TAG, "Process UART");
                // データリクエストコマンドがあればここで送信。
                if (my_if_uart_request_command_len > 0) {
                    // 返り値は送信サイズと等しい・・はず
                    int wrote_size = uart_write_bytes(
                        MY_IF_UART_PORT_NUM, my_if_uart_request_command,
                        my_if_uart_request_command_len);
                }
                // 終端文字列が来るまで何回か受信する。終端文字列が来なければ受信失敗とみなして何もしない。
                bool receive_completed = false;
                int cnt = 3;
                while (cnt-- > 0) {
                    // 終端文字列で終わるデータを受信する
                    // データの総量はリングバッファにより制限される(my_if_uart_receive_buffer_lenバイト)
                    uint8_t tmp_buf[my_if_uart_receive_buffer_len];
                    int read_len = uart_read_bytes(
                        MY_IF_UART_PORT_NUM, tmp_buf,
                        my_if_uart_receive_buffer_len, 50 / portTICK_PERIOD_MS);
                    for (int i = 0; i < read_len; i++) {
                        ESP_LOGI(MY_IF_UART_TAG, "Receive 1 byte: %c(0x%02x)",
                                 tmp_buf[i], tmp_buf[i]);
                        // リングバッファに1文字格納
                        my_ring_buffer_push(&rb, tmp_buf[i]);
                        if (my_if_uart_terminator_sequence_len <= 0) {
                            // ターミネーター文字列が指定されていない場合は、
                            // １文字でも読み込めたら受信完了フラグを立てておく。
                            receive_completed = true;
                        } else {
                            // ターミネーター末尾の文字と一致していたら、
                            // ターミネーター文字列全てが合致しているかリングバッファをチェック
                            if (tmp_buf[i] ==
                                my_if_uart_terminator_sequence
                                    [my_if_uart_terminator_sequence_len - 1]) {
                                bool terminated = true;
                                uint8_t d;
                                for (int j = 0;
                                     j < my_if_uart_terminator_sequence_len;
                                     j++) {
                                    my_ring_buffer_at(
                                        &rb,
                                        j - my_if_uart_terminator_sequence_len,
                                        &d);
                                    if (my_if_uart_terminator_sequence[j] !=
                                        d) {
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
                    char buf_str[(my_if_uart_receive_buffer_len +
                                  my_if_uart_terminator_sequence_replace_len +
                                  1) *
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
                    if (ring_buffer_cnt > my_if_uart_terminator_sequence_len) {
                        base_len = ring_buffer_cnt -
                                   my_if_uart_terminator_sequence_len;
                    } else {
                        base_len = 0;
                    }
                    for (int i = 0;
                         i < my_if_uart_terminator_sequence_replace_len; i++) {
                        my_if_uart_buffer[base_len + i] =
                            my_if_uart_terminator_sequence_replace[i];
                    }
                    // 末尾に\0を付与しておく
                    my_if_uart_buffer
                        [base_len +
                         my_if_uart_terminator_sequence_replace_len] = 0;
#ifdef MYDEBUG
                    // 変換後のバッファの内容を表示
                    buf_ptr = buf_str;
                    for (int i = 0;
                         i <
                         base_len + my_if_uart_terminator_sequence_replace_len;
                         i++) {
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
                // リクエストコマンド送信後は、ちょっと多めに待機し、通信状態をOFFにする
                // リクエストコマンドが無い場合はONのまま
                if (my_if_uart_request_command_len > 0) {
                    vTaskDelay(300 / portTICK_PERIOD_MS);
                    on_communication = false;
                }
            } else {
                // セマフォを返却する
                xSemaphoreGive(my_if_uart_buffer_semaphore);
            }  // on communication
        } else {
            ESP_LOGI(MY_IF_UART_TAG, "can not take semaphore");
        }  // xSemaphoreTake
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
    // 設定値を読み出す
    if (my_if_uart_get_config() == ESP_OK) {
        ESP_LOGI(MY_IF_UART_TAG, "success load config");
    } else {
        // 読み出し失敗した場合、空の設定値で起動し、あとで外部から設定する。
        ESP_LOGI(MY_IF_UART_TAG, "fail load config");
        my_if_uart_baud_rate = 4800;
        my_if_uart_receive_buffer_len = 15;
        my_if_uart_request_command_len = 0;
        my_if_uart_terminator_sequence_len = 0;
        my_if_uart_terminator_sequence_replace_len = 0;
        my_if_uart_request_command = (char *)calloc(1, sizeof(char));
        my_if_uart_terminator_sequence = (char *)calloc(1, sizeof(char));
        my_if_uart_terminator_sequence_replace =
            (char *)calloc(1, sizeof(char));

        if (0) {
            // 初期値を使う場合
            ESP_LOGI(MY_IF_UART_TAG, " ... use default");
            my_if_uart_baud_rate = 4800;

            my_if_uart_receive_buffer_len = 15;

            free(my_if_uart_request_command);
            my_if_uart_request_command = malloc(4);
            my_if_uart_request_command[0] = 'Q';
            my_if_uart_request_command[1] = 'X';
            my_if_uart_request_command[2] = '\x0d';
            my_if_uart_request_command[3] = '\x0a';
            my_if_uart_request_command_len = 4;

            free(my_if_uart_terminator_sequence);
            my_if_uart_terminator_sequence = malloc(2);
            my_if_uart_terminator_sequence[0] = '\x0d';
            my_if_uart_terminator_sequence[1] = '\x0a';
            my_if_uart_terminator_sequence_len = 2;

            free(my_if_uart_terminator_sequence_replace);
            my_if_uart_terminator_sequence_replace = malloc(1);
            my_if_uart_terminator_sequence_replace[0] = '\x09';
            my_if_uart_terminator_sequence_replace_len = 1;
        }
    }
    {
        // 設定一つずつを目に見える形で表示するのがめんどくさいので１行で。
        int len = my_if_uart_get_pack_config_len();
        char buf[len];
        if (my_if_uart_pack_config(buf) == 0) {
            ESP_LOGI(MY_IF_UART_TAG, "config is %s", buf);
        }
    }

    // GPIO/UART監視タスク
    xTaskCreate(my_if_uart_task, "i/f task uart", MY_IF_UART_TASK_STACK_SIZE,
                NULL, priority, NULL);
}

#endif

