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
#include "freertos/task.h"


#include "my_ring_buffer.h"
#include "my_debug.h"


// デバッグ用
//#include "my_hid_key_map.h"
//#include "hid_dev.h"
//extern bool send_ble_hid_by_keycode(uint8_t k, key_mask_t m);

extern bool send_ble_hid_by_string(uint8_t *data, int len);

// take care of strapping pins for rx, tx and trigger.
#define MY_IF_UART_TRIGGER_PIN_GPIO (5)
#define MY_IF_UART_RXD_PIN_GPIO (3)
#define MY_IF_UART_TXD_PIN_GPIO (4)
#define MY_IF_UART_RTS_PIN_GPIO (UART_PIN_NO_CHANGE)
#define MY_IF_UART_CTS_PIN_GPIO (UART_PIN_NO_CHANGE)
#define MY_IF_UART_PORT_NUM (1)
#define MY_IF_UART_BAUD_RATE (4800)  // 1200, 2400, 4800, 9600 を選択可能
#define MY_IF_UART_TASK_STACK_SIZE (2048)
#define MY_IF_UART_BUF_SIZE (1024)
#define MY_IF_UART_TAG "IF_UART"

// 受信する末尾文字列
static const uint8_t terminator_sequence[] = {0x0d, 0x0a};
// 受信する末尾文字列の長さ
static const int terminator_sequence_len = 2;
// 受信した末尾文字列を、送信するときにこの内容に置換する
static const uint8_t terminator_sequence_replace[] = {0x09};
// 置換文字列の長さ
static const int terminator_sequence_replace_len = 1;
// 受信開始のために送出するコマンド
static const char request_command[] = {'Q', 'X', 0x0d, 0x0a};
// 送出するコマンドの長さ
static const int request_command_len = 4;
// 受信バッファサイズ
static const int receive_buffer_len = 15;


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
  gpio_reset_pin(MY_IF_UART_TRIGGER_PIN_GPIO);
  gpio_set_direction(MY_IF_UART_TRIGGER_PIN_GPIO, GPIO_MODE_INPUT);
  gpio_pullup_en(MY_IF_UART_TRIGGER_PIN_GPIO);
  
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

  ESP_ERROR_CHECK(uart_driver_install(MY_IF_UART_PORT_NUM,
                                      MY_IF_UART_BUF_SIZE,
                                      MY_IF_UART_BUF_SIZE,
                                      0, NULL,
                                      intr_alloc_flags));
  ESP_ERROR_CHECK(uart_param_config(MY_IF_UART_PORT_NUM, &uart_config));
  ESP_ERROR_CHECK(uart_set_pin(MY_IF_UART_PORT_NUM, MY_IF_UART_TXD_PIN_GPIO,
                               MY_IF_UART_RXD_PIN_GPIO, MY_IF_UART_RTS_PIN_GPIO,
                               MY_IF_UART_CTS_PIN_GPIO));
  DEBUGPRINT("UART inited");

  // 232C受信のためにリングバッファを用意する
  my_ring_buffer_t rb;
  my_ring_buffer_init(&rb, receive_buffer_len);
  DEBUGPRINT("ring buffer inited");


  // トリガーピンの、1回前の電圧
  int lvl0 = 1;
  // トリガーピンのH->Lを確認する
  while (1) {
    DEBUGPRINT("Trigger waiting");
    vTaskDelay(50 / portTICK_PERIOD_MS);
    //vTaskDelay(1000 / portTICK_PERIOD_MS);
    // トリガーピンの電圧
    int lvl1 = gpio_get_level(MY_IF_UART_TRIGGER_PIN_GPIO);
    DEBUGPRINT("lvl = %d -> %d", lvl0, lvl1);
    if (lvl0 == 1 && lvl1 == 0) {
      // トリガーが来た(一回前がH、現在がL）なら、データリクエストを送る
      ESP_LOGI(MY_IF_UART_TAG, "Triggered H->L");
      // リングバッファをクリアしておく
      my_ring_buffer_reset(&rb);
      // 送信前にreadバッファを空にしておく
      uart_flush(MY_IF_UART_PORT_NUM);
      // データリクエストコマンドを送信。 返り値は送信サイズと等しい・・はず
      int wrote_size = uart_write_bytes(MY_IF_UART_PORT_NUM, request_command, request_command_len);
      // 終端文字列が来るまで何回か受信する。終端文字列が来なければ受信失敗とみなして何もしない。
      int cnt = 3;
      bool receive_completed = false;
      while (cnt-- > 0) {
        // 終端文字列で終わるデータを受信する
        // データの総量はリングバッファにより制限される(receive_buffer_lenバイト)
        uint8_t tmp_buf[receive_buffer_len];
        int read_len =
            uart_read_bytes(MY_IF_UART_PORT_NUM, tmp_buf, receive_buffer_len,
                            50 / portTICK_PERIOD_MS);
        for (int i = 0; i < read_len; i ++) {
          // リングバッファに1文字格納
          my_ring_buffer_push(&rb, tmp_buf[i]);
          // ターミネーター末尾の文字と一致していたら、ターミネーター文字列全てが合致しているか、格納済みのリングバッファをチェック
          if (tmp_buf[i] == terminator_sequence[terminator_sequence_len - 1]) {
            bool terminated = true;
            uint8_t d;
            for (int j = 0; j < terminator_sequence_len; j ++) {
              my_ring_buffer_at(&rb, j - terminator_sequence_len, &d);
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
      }
      // 一定期間中に受信しきれなかった場合は受信できていないとみなし、なにもせずにトリガ待ちに移行する。次回受信時にリングバッファをクリアして再受信。
      // 受信しきれた場合は、処理を進める
      if (receive_completed) {
        // リングバッファから取り出す
        int ring_buffer_cnt = my_ring_buffer_content_length(&rb);
        uint8_t ring_buffer_str[ring_buffer_cnt + terminator_sequence_replace_len + 1];  // 末端置換のため、ちょっと大きめに確保
        for (int i = 0; i < ring_buffer_cnt; i ++) {
          if (!my_ring_buffer_pop(&rb, &(ring_buffer_str[i]))) {
            break;
          }
        }
#ifdef MYDEBUG
        // ログ出力のために\0終わりにし、出力する
        ring_buffer_str[ring_buffer_cnt] = 0;
        printf("Triggered. Ring-Buffer is %s\n", ring_buffer_str);
        printf(" -> ");
        for (int i = 0; i < ring_buffer_cnt; i ++) {
          printf(" %x", ring_buffer_str[i]);
        }
        printf("\n");
#endif
        // 末尾のターミネーター文字列を別の文字列に変換する
        int base_len = 0;
        if (ring_buffer_cnt > terminator_sequence_len) {
          base_len = ring_buffer_cnt - terminator_sequence_len;
        } else {
          base_len = 0;
        }
        for (int i = 0; i < terminator_sequence_replace_len; i ++) {
          ring_buffer_str[base_len + i] = terminator_sequence_replace[i];
        }
#ifdef MYDEBUG
        // ログ出力のために\0終わりにし、出力する
        ring_buffer_str[base_len + terminator_sequence_replace_len] = 0;
        printf("Triggered. Send-Buffer is %s\n", ring_buffer_str);
        printf(" -> ");
        for (int i = 0; i < base_len + terminator_sequence_replace_len; i ++) {
          printf(" %x", ring_buffer_str[i]);
        }
        printf("\n");
#endif
        // 送信する
        if (send_ble_hid_by_string(ring_buffer_str, base_len + terminator_sequence_replace_len)) {
          // 成功
          DEBUGPRINT("SUCCESS");
        } else {
          // 失敗。再送する？
          DEBUGPRINT("FAIL");
        }
      }
      // 送信後、ちょっと多めに待機する
      vTaskDelay(300 / portTICK_PERIOD_MS);
    }
    // トリガーピンの履歴を最新値にする
    lvl0 = lvl1;
  }
}

/**
 * @brief touch point for user defined interface
 */
void my_if_uart_begin(int priority) {
  xTaskCreate(my_if_uart_task, "i/f task uart", MY_IF_UART_TASK_STACK_SIZE,
              NULL, priority, NULL);
}

#endif

