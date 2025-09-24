/*
 * Copyright 2025 Kadota Masayuki
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

/*
 * my_httpd.c
 *   http server from Simple HTTP Server Example
 *     https://github.com/espressif/esp-idf/blob/master/examples/protocols/http_server/simple/main/main.c
 */

#include <esp_err.h>
#include <esp_log.h>
#include <nvs_flash.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <sys/param.h>
#include <sys/time.h>
#include <time.h>
#include <unistd.h>

#include "esp_check.h"
#include "esp_event.h"
#include "esp_http_server.h"
#include "esp_netif.h"
#include "esp_tls.h"
#include "esp_tls_crypto.h"
// #include "protocol_examples_common.h"
// #include "protocol_examples_utils.h"
#if !CONFIG_IDF_TARGET_LINUX
#include <esp_system.h>
#include <esp_wifi.h>

#include "esp_eth.h"
#include "nvs_flash.h"
#endif  // !CONFIG_IDF_TARGET_LINUX

#include "my_debug.h"
#include "my_httpd.h"
#include "my_ring_buffer.h"

// リングバッファ
extern my_ring_buffer_t rb;

// リングバッファを初期化する
extern bool my_ring_buffer_init(my_ring_buffer_t *rb, int size);

// 通信速度
extern uint32_t my_if_uart_baud_rate;
extern const uint32_t my_if_uart_baud_rate_min;
extern const uint32_t my_if_uart_baud_rate_max;

// 受信バッファサイズ
extern int my_if_uart_receive_buffer_len;
extern const int my_if_uart_receive_buffer_len_min;
extern const int my_if_uart_receive_buffer_len_max;

// 受信開始のために送出するコマンド文字列
extern char *my_if_uart_request_command;

// 送出するコマンド文字列の長さ
extern int my_if_uart_request_command_len;
extern const int my_if_uart_request_command_len_min;
extern const int my_if_uart_request_command_len_max;

// 受信する末尾文字列
extern char *my_if_uart_terminator_sequence;

// 受信する末尾文字列の長さ
extern int my_if_uart_terminator_sequence_len;
extern const int my_if_uart_terminator_sequence_len_min;
extern const int my_if_uart_terminator_sequence_len_max;

// 受信した末尾文字列を、送信するときにこの内容に置換する
extern char *my_if_uart_terminator_sequence_replace;

// 置換文字列の長さ
extern int my_if_uart_terminator_sequence_replace_len;
extern const int my_if_uart_terminator_sequence_replace_len_min;
extern const int my_if_uart_terminator_sequence_replace_len_max;

// 各設定のパック後の文字列長を得る
extern int my_if_uart_get_pack_config_max_len();

// 複数の設定値をNVSに保存
extern int my_if_uart_set_config();

// 16進表記の文字列を受け取り、８ビットずつの配列を返す
extern int my_if_uart_str16_to_arr8(char buf[], int buf_len, char out_arr[]);

// WiFi SoftAPを停止する（外部から利用）
extern esp_err_t my_softap_stop_ap(void);
// SoftApに接続している数（外部から使用）
extern int my_softap_connected_cnt;

#define EXAMPLE_HTTP_QUERY_KEY_MAX_LEN (64)

static httpd_handle_t httpd_server = NULL;
static const char *TAG_HTTPD = "httpd";

// 最終通信時刻（外部から使用する）
struct timeval my_httpd_last_com_tv;

/**
 * @brief URLデコード。書き換えて返す。
 *   3文字(%xx)を1文字に置換するため、変換後は入力文字数より少なくなる。
 *   なので、多めのバッファサイズを用意する必要は無い。
 * @param *buf URLデコードしたい文字列へのポインタ。子の中身が書き換えられる
 * @return int 成功：0、失敗：-1、デコード途中：デコード成功した最終文字位置
 */
static int my_httpd_url_decode_inner(char *buf) {
    // DEBUGPRINT("start %s\n", buf);
    char *match;
    while ((match = strstr(buf, "%")) != NULL) {
        // DEBUGPRINT("match %02x, index %lu\n", *match, match - buf);
        if (strlen(match) >= 3) {
            char x = 0;
            for (int i = 0; i < 2; i++) {
                char n = *(match + i + 1);
                // DEBUGPRINT("  n %c\n", n);
                if ('0' <= n && n <= '9')
                    x = x * 16 + n - '0';
                else if ('A' <= n && n <= 'F')
                    x = x * 16 + n - 'A' + 10;
                else if ('a' <= n && n <= 'f')
                    x = x * 16 + n - 'a' + 10;
                else
                    return -1;
            }
            // DEBUGPRINT("  x %c\n", x);
            if (0x20 <= x && x <= 0x7e) {
                // 表記できる文字だったら、その文字で置き換える
                *match = x;
                // 重なるメモリをstrcatするのは未定義動作なので、strcatではなくmemmoveで対応する
                memmove(match + 1, match + 3, strlen(match + 3) + 1);
                match++;
            } else {
                // 重なるメモリをstrcatするのは未定義動作なので、strcatではなくmemmoveで対応する
                memmove(match, match + 3, strlen(match + 3) + 1);
            }
            // DEBUGPRINT(">>>%s\n", buf);
        } else {
            // 位置を返す
            return (strlen(buf) - strlen(match));
        }
    }
    return 0;
}

/**
 * @brief URLデコード。出力用バッファに返す。
 *   出力用バッファサイズは入力と同じであること。
 * @param *dst 結果を格納する文字列へのポインタ。サイズは*src以上であること。
 * @param *src URLデコードしたい文字列へのポインタ
 * @return 成功：0、失敗：-1、デコード途中：デコード成功した最終文字位置
 */
static int my_httpd_url_decode(char *dst, const char *src) {
    strcpy(dst, src);
    return my_httpd_url_decode_inner(dst);
}

/**
 * @brief 設定値をHTML表示する。内容は<body>と</body>の間に入る部分。
 */
void my_httpd_show_config(httpd_req_t *req) {
    // 現在の設定値を表示
    int len = my_if_uart_get_pack_config_max_len();
    char buf[len + 40];  // 40 is for Header string
    // 受信バッファサイズ
    sprintf(buf, "Buffer-Size: %d <br>\n", my_if_uart_receive_buffer_len);
    httpd_resp_send_chunk(req, buf, HTTPD_RESP_USE_STRLEN);
    // 通信速度
    sprintf(buf, "Baud-Rate: %ld <br>\n", my_if_uart_baud_rate);
    httpd_resp_send_chunk(req, buf, HTTPD_RESP_USE_STRLEN);
    // 送信コマンド（１６進文字列で表示）
    httpd_resp_send_chunk(req, "TX-Command: ", HTTPD_RESP_USE_STRLEN);
    for (int i = 0; i < my_if_uart_request_command_len; i++) {
        sprintf(buf, "0x%02x('%c'), ", my_if_uart_request_command[i],
                my_if_uart_request_command[i]);
        httpd_resp_send_chunk(req, buf, HTTPD_RESP_USE_STRLEN);
    }
    httpd_resp_send_chunk(req, "<br>\n ", HTTPD_RESP_USE_STRLEN);
    // 受信ターミネータ（１６進文字列で表示）
    httpd_resp_send_chunk(req, "RX-Terminator String: ", HTTPD_RESP_USE_STRLEN);
    for (int i = 0; i < my_if_uart_terminator_sequence_len; i++) {
        sprintf(buf, "0x%02x('%c'), ", my_if_uart_terminator_sequence[i],
                my_if_uart_terminator_sequence[i]);
        httpd_resp_send_chunk(req, buf, HTTPD_RESP_USE_STRLEN);
    }
    httpd_resp_send_chunk(req, "<br>\n ", HTTPD_RESP_USE_STRLEN);
    // 受信ターミネータ置換（１６進文字列で表示）
    httpd_resp_send_chunk(
        req, "RX-Terminator Replace String: ", HTTPD_RESP_USE_STRLEN);
    for (int i = 0; i < my_if_uart_terminator_sequence_replace_len; i++) {
        sprintf(buf, "0x%02x('%c'), ",
                my_if_uart_terminator_sequence_replace[i],
                my_if_uart_terminator_sequence_replace[i]);
        httpd_resp_send_chunk(req, buf, HTTPD_RESP_USE_STRLEN);
    }
    httpd_resp_send_chunk(req, "<br>\n ", HTTPD_RESP_USE_STRLEN);
}

//
// URI アクセス時のハンドラ /////////////////////////////////////
//

// HTTP GET handler群

/**
 * @brief uriにより起動。ホームページを出力する
 *   各種情報の現在値、
 *   各種情報を変更
 *   保存等の制御リンク
 */
static esp_err_t my_httpd_home_get_handler(httpd_req_t *req) {
    // httpd通信の最終実行時刻を更新する
    gettimeofday(&my_httpd_last_com_tv, NULL);

    char buf[250];
    char buf16[100];
    char *p16;

    // ヘッダ
    httpd_resp_send_chunk(req, "<!doctype html><html><body>\n",
                          HTTPD_RESP_USE_STRLEN);

    // 各種情報
    httpd_resp_send_chunk(
        req, "<a href='/'>reload</a><br>\n<br>\n<h2>current status</h2>\n",
        HTTPD_RESP_USE_STRLEN);

    // 現在の接続数を取得
    int con_cnt = my_softap_connected_cnt;
    sprintf(buf, "current connection count is %d <br>\n", con_cnt);
    httpd_resp_send_chunk(req, buf, HTTPD_RESP_USE_STRLEN);

    // serparator
    httpd_resp_send_chunk(req, "<br><hr><br>\n", HTTPD_RESP_USE_STRLEN);

    // 現在の設定値を表示
    my_httpd_show_config(req);

    // serparator
    httpd_resp_send_chunk(req, "<br><hr><br>\n", HTTPD_RESP_USE_STRLEN);

    // 受信バッファサイズ
    sprintf(buf,
            "<form action='/buflen' method='post'>\n"
            "Receive Buffer Length : %d\n",
            my_if_uart_receive_buffer_len);
    httpd_resp_send_chunk(req, buf, HTTPD_RESP_USE_STRLEN);
    sprintf(buf,
            "<input type='text' name='buflen' value='%d' size=40>\n"
            "<input type='submit'>\n"
            "</form>\n",
            my_if_uart_receive_buffer_len);
    httpd_resp_send_chunk(req, buf, HTTPD_RESP_USE_STRLEN);
    httpd_resp_send_chunk(req, "<br>\n", HTTPD_RESP_USE_STRLEN);

    // 通信速度
    sprintf(buf,
            "<form action='/baudrate' method='post'>\n"
            "Baud Rate : %ld\n",
            my_if_uart_baud_rate);
    httpd_resp_send_chunk(req, buf, HTTPD_RESP_USE_STRLEN);
    sprintf(buf,
            "<input type='text' name='baudrate' value='%ld' size=40>\n"
            "<input type='submit'>\n"
            "</form>\n",
            my_if_uart_baud_rate);
    httpd_resp_send_chunk(req, buf, HTTPD_RESP_USE_STRLEN);
    httpd_resp_send_chunk(req, "<br>\n", HTTPD_RESP_USE_STRLEN);

    // リクエストコマンド
    sprintf(buf,
            "<form action='/reqcmd' method='post'>\n"
            "Requect Command : \n");
    httpd_resp_send_chunk(req, buf, HTTPD_RESP_USE_STRLEN);
    for (int i = 0; i < my_if_uart_request_command_len; i++) {
        sprintf(buf, "0x%02x('%c'), ", my_if_uart_request_command[i],
                my_if_uart_request_command[i]);
        httpd_resp_send_chunk(req, buf, HTTPD_RESP_USE_STRLEN);
    }
    buf16[0] = 0;
    p16 = buf16;
    for (int i = 0; i < my_if_uart_request_command_len; i++) {
        p16 += sprintf(p16, "%02x", my_if_uart_request_command[i]);
    }
    sprintf(buf,
            "<input type='text' name='reqcmd' value='%s' size=40>\n"
            "<input type='submit'>\n"
            "</form>\n",
            buf16);
    httpd_resp_send_chunk(req, buf, HTTPD_RESP_USE_STRLEN);
    httpd_resp_send_chunk(req, "<br>\n", HTTPD_RESP_USE_STRLEN);

    // 受信ターミネーター
    sprintf(buf,
            "<form action='/termseq' method='post'>\n"
            "Receive Terminator Sequence : %s\n",
            my_if_uart_terminator_sequence);
    httpd_resp_send_chunk(req, buf, HTTPD_RESP_USE_STRLEN);
    for (int i = 0; i < my_if_uart_terminator_sequence_len; i++) {
        sprintf(buf, "0x%02x('%c'), ", my_if_uart_terminator_sequence[i],
                my_if_uart_terminator_sequence[i]);
        httpd_resp_send_chunk(req, buf, HTTPD_RESP_USE_STRLEN);
    }
    buf16[0] = 0;
    p16 = buf16;
    for (int i = 0; i < my_if_uart_terminator_sequence_len; i++) {
        p16 += sprintf(p16, "%02x", my_if_uart_terminator_sequence[i]);
    }
    sprintf(buf,
            "<input type='text' name='termseq' value='%s' size=40>\n"
            "<input type='submit'>\n"
            "</form>\n",
            buf16);
    httpd_resp_send_chunk(req, buf, HTTPD_RESP_USE_STRLEN);
    httpd_resp_send_chunk(req, "<br>\n", HTTPD_RESP_USE_STRLEN);

    // 受信ターミネーターを変換
    sprintf(buf,
            "<form action='/termrep' method='post'>\n"
            "Receive Terminator Replace : %s\n",
            my_if_uart_terminator_sequence_replace);
    httpd_resp_send_chunk(req, buf, HTTPD_RESP_USE_STRLEN);
    for (int i = 0; i < my_if_uart_terminator_sequence_replace_len; i++) {
        sprintf(buf, "0x%02x('%c'), ",
                my_if_uart_terminator_sequence_replace[i],
                my_if_uart_terminator_sequence_replace[i]);
        httpd_resp_send_chunk(req, buf, HTTPD_RESP_USE_STRLEN);
    }
    buf16[0] = 0;
    p16 = buf16;
    for (int i = 0; i < my_if_uart_terminator_sequence_replace_len; i++) {
        p16 += sprintf(p16, "%02x", my_if_uart_terminator_sequence_replace[i]);
    }
    sprintf(buf,
            "<input type='text' name='termrep' value='%s' size=40>\n"
            "<input type='submit'>\n"
            "</form>\n",
            buf16);
    httpd_resp_send_chunk(req, buf, HTTPD_RESP_USE_STRLEN);
    httpd_resp_send_chunk(req, "<br>\n", HTTPD_RESP_USE_STRLEN);

    // その他情報を変更するためのフォームを出力

    // serparator
    httpd_resp_send_chunk(req, "<br><hr><br>\n", HTTPD_RESP_USE_STRLEN);

    // link to store nvs
    httpd_resp_send_chunk(req,
                          " | <form action='/store_nvs' method='post'><input "
                          "type='submit' value='Save Settings'></form>\n",
                          HTTPD_RESP_USE_STRLEN);

    // link to soft reset
    httpd_resp_send_chunk(req,
                          " | <form action='/soft_reset' method='post'><input "
                          "type='submit' value='Reset'></form>\n",
                          HTTPD_RESP_USE_STRLEN);

    // link to shutdown wifi-httpd server
    httpd_resp_send_chunk(
        req,
        " | <form action='/shutdown_server' method='post'><input type='submit' "
        "value='Shutdown Server'></form>\n",
        HTTPD_RESP_USE_STRLEN);

    // footer
    httpd_resp_send_chunk(req, "\n</body></html>\n", HTTPD_RESP_USE_STRLEN);

    // End response
    httpd_resp_send_chunk(req, NULL, 0);
    return ESP_OK;
}

// HTTP POST handler群

/**
 * @brief uriにより起動。Receive Buffer Length設定値を受け取り設定
 */
static esp_err_t my_httpd_receive_buffer_length_post_handler(httpd_req_t *req) {
    // httpd通信の最終実行時刻を更新する
    gettimeofday(&my_httpd_last_com_tv, NULL);

    ESP_LOGI(TAG_HTTPD, "-> command: config receive buffer length");

    esp_err_t err = ESP_OK;
    int ret;
    char buf1[250];
    char buf2[100];

    // http出力しながら実行するので、まずはヘッダを出力しておく
    httpd_resp_send_chunk(req, "<!doctype html><html><body>\n",
                          HTTPD_RESP_USE_STRLEN);

    // buf1に読み込む
    int remaining = req->content_len;
    while (remaining > 0) {
        if ((ret = httpd_req_recv(req, buf1,
                                  MIN(remaining, sizeof(buf1) - 1))) <= 0) {
            if (ret == HTTPD_SOCK_ERR_TIMEOUT) {
                /* Retry receiving if timeout occurred */
                continue;
            } else {
                return ESP_FAIL;
            }
        }
        // httpd_req_recvは末尾に'\0'を付けてくれないっぽいので、付ける
        buf1[ret] = '\0';
        break;
    }
    DEBUGPRINT("read from query: %s\n", buf1);

    // input name='buflen' を取得
    if (httpd_query_key_value(buf1, "buflen", buf2, sizeof(buf2)) != ESP_OK) {
        err = ESP_FAIL;
    } else {
        DEBUGPRINT("Receive Buffer Length : %s\n", buf2);
        // URLデコードする
        my_httpd_url_decode_inner(buf2);
        DEBUGPRINT("config decode query: %s\n", buf2);
        // 文字長がゼロならエラーとする
        if (strlen(buf2) == 0) {
            httpd_resp_send_chunk(req, "there is no input, please retry.\n",
                                  HTTPD_RESP_USE_STRLEN);
            err = ESP_FAIL;
        } else {
            // 文字列をlong値に変換
            int tmp;
            char *p2;
            char buf3[100];
            // 文字列をlong値に変換
            tmp = strtol(buf2, &p2, 10);
            if (tmp < my_if_uart_receive_buffer_len_min) {
                p2 = buf3;
                sprintf(p2,
                        "Receive Buffer Length needs >= %d. input is %d .\n",
                        my_if_uart_receive_buffer_len_min, tmp);
                httpd_resp_send_chunk(req, buf3, HTTPD_RESP_USE_STRLEN);
                err = ESP_FAIL;
            } else if (tmp > my_if_uart_receive_buffer_len_max) {
                sprintf(buf3,
                        "Receive Buffer Length needs <= %d. input is %d .\n",
                        my_if_uart_receive_buffer_len_max, tmp);
                httpd_resp_send_chunk(req, buf3, HTTPD_RESP_USE_STRLEN);
                err = ESP_FAIL;
            } else {
                my_if_uart_receive_buffer_len = tmp;
            }
        }
    }

    // リングバッファのサイズを変更
    my_ring_buffer_init(&rb, my_if_uart_receive_buffer_len);

    // 終了
    if (err == ESP_OK) {
        httpd_resp_send_chunk(req, "OK\n", HTTPD_RESP_USE_STRLEN);
    } else {
        httpd_resp_send_chunk(req, "NG\n", HTTPD_RESP_USE_STRLEN);
    }
    httpd_resp_send_chunk(req, "<hr>\n", HTTPD_RESP_USE_STRLEN);

    // 現在の設定値を表示
    my_httpd_show_config(req);

    // フッタを出力
    httpd_resp_send_chunk(req, "<hr>\n", HTTPD_RESP_USE_STRLEN);
    httpd_resp_send_chunk(req, "<a href='/'>go to home</a>\n",
                          HTTPD_RESP_USE_STRLEN);
    httpd_resp_send_chunk(req, "</body></html>\n", HTTPD_RESP_USE_STRLEN);
    httpd_resp_send_chunk(req, NULL, 0);

    return err;
}

/**
 * @brief uriにより起動。Baud Rate設定値を受け取り設定
 */
static esp_err_t my_httpd_baud_rate_post_handler(httpd_req_t *req) {
    // httpd通信の最終実行時刻を更新する
    gettimeofday(&my_httpd_last_com_tv, NULL);

    ESP_LOGI(TAG_HTTPD, "-> command: config baud rate");

    esp_err_t err = ESP_OK;
    int ret;
    char buf1[250];
    char buf2[100];

    // http出力しながら実行するので、まずはヘッダを出力しておく
    httpd_resp_send_chunk(req, "<!doctype html><html><body>\n",
                          HTTPD_RESP_USE_STRLEN);

    // buf1に読み込む
    int remaining = req->content_len;
    while (remaining > 0) {
        if ((ret = httpd_req_recv(req, buf1,
                                  MIN(remaining, sizeof(buf1) - 1))) <= 0) {
            if (ret == HTTPD_SOCK_ERR_TIMEOUT) {
                /* Retry receiving if timeout occurred */
                continue;
            } else {
                return ESP_FAIL;
            }
        }
        // httpd_req_recvは末尾に'\0'を付けてくれないっぽいので、付ける
        buf1[ret] = '\0';
        break;
    }
    DEBUGPRINT("read from query: %s\n", buf1);

    // input name='baudrete' を取得
    if (httpd_query_key_value(buf1, "baudrate", buf2, sizeof(buf2)) != ESP_OK) {
        err = ESP_FAIL;
    } else {
        DEBUGPRINT("Baud Rate : %s\n", buf2);
        // URLデコードする
        my_httpd_url_decode_inner(buf2);
        DEBUGPRINT("config decode query: %s\n", buf2);
        // 文字長がゼロならエラーとする
        if (strlen(buf2) == 0) {
            httpd_resp_send_chunk(req, "there is no input, please retry.\n",
                                  HTTPD_RESP_USE_STRLEN);
            err = ESP_FAIL;
        } else {
            // 文字列をlong値に変換
            char *p2;
            char buf3[100];
            uint32_t tmp;
            tmp = strtol(buf2, &p2, 10);
            if (tmp < my_if_uart_baud_rate_min) {
                sprintf(buf3, "Baud Rate needs >= %lu. input is %lu .\n",
                        my_if_uart_baud_rate_min, tmp);
                httpd_resp_send_chunk(req, buf3, HTTPD_RESP_USE_STRLEN);
                err = ESP_FAIL;
            } else if (tmp > my_if_uart_baud_rate_max) {
                sprintf(buf3, "Baud Rate needs <= %lu. input is %lu .\n",
                        my_if_uart_baud_rate_max, tmp);
                httpd_resp_send_chunk(req, buf3, HTTPD_RESP_USE_STRLEN);
                err = ESP_FAIL;
            } else {
                my_if_uart_baud_rate = tmp;
            }
        }
    }

    // 終了
    if (err == ESP_OK) {
        httpd_resp_send_chunk(
            req, "OK. Please 'save' and restart to use new Baud-Rate.\n",
            HTTPD_RESP_USE_STRLEN);
    } else {
        httpd_resp_send_chunk(req, "NG\n", HTTPD_RESP_USE_STRLEN);
    }
    httpd_resp_send_chunk(req, "<hr>\n", HTTPD_RESP_USE_STRLEN);

    // 現在の設定値を表示
    my_httpd_show_config(req);

    // フッタを出力
    httpd_resp_send_chunk(req, "<hr>\n", HTTPD_RESP_USE_STRLEN);
    httpd_resp_send_chunk(req, "<a href='/'>go to home</a>\n",
                          HTTPD_RESP_USE_STRLEN);
    httpd_resp_send_chunk(req, "</body></html>\n", HTTPD_RESP_USE_STRLEN);
    httpd_resp_send_chunk(req, NULL, 0);

    return err;
}

/**
 * @brief uriにより起動。Request Command設定値を受け取り設定
 */
static esp_err_t my_httpd_request_command_post_handler(httpd_req_t *req) {
    // httpd通信の最終実行時刻を更新する
    gettimeofday(&my_httpd_last_com_tv, NULL);

    ESP_LOGI(TAG_HTTPD, "-> command: config request command");

    esp_err_t err = ESP_OK;
    int ret;
    char buf1[250];
    char buf2[100];

    // http出力しながら実行するので、まずはヘッダを出力しておく
    httpd_resp_send_chunk(req, "<!doctype html><html><body>\n",
                          HTTPD_RESP_USE_STRLEN);

    // buf1に読み込む
    int remaining = req->content_len;
    while (remaining > 0) {
        if ((ret = httpd_req_recv(req, buf1,
                                  MIN(remaining, sizeof(buf1) - 1))) <= 0) {
            if (ret == HTTPD_SOCK_ERR_TIMEOUT) {
                /* Retry receiving if timeout occurred */
                continue;
            } else {
                return ESP_FAIL;
            }
        }
        // httpd_req_recvは末尾に'\0'を付けてくれないっぽいので、付ける
        buf1[ret] = '\0';
        break;
    }
    DEBUGPRINT("read from query: %s\n", buf1);

    // input name='reqcmd' を取得
    if (httpd_query_key_value(buf1, "reqcmd", buf2, sizeof(buf2)) != ESP_OK) {
        err = ESP_FAIL;
    } else {
        DEBUGPRINT("Request Command : %s\n", buf2);
        // URLデコードする
        my_httpd_url_decode_inner(buf2);
        DEBUGPRINT("config decode query: %s\n", buf2);
        // 文字長がゼロなら空文字を作る
        if (strlen(buf2) == 0) {
            my_if_uart_request_command_len = 0;
            if ((my_if_uart_request_command = malloc(1)) == NULL) {
                err = ESP_FAIL;
            } else {
                my_if_uart_request_command[0] = 0;
            }
        } else {
            int tmp = strlen(buf2);
            char buf3[100];
            if (tmp < my_if_uart_request_command_len_min) {
                sprintf(buf3,
                        "request command length needs >= %d. input is %d .\n",
                        my_if_uart_request_command_len_min, tmp);
                httpd_resp_send_chunk(req, buf3, HTTPD_RESP_USE_STRLEN);
                err = ESP_FAIL;
            } else if (tmp > my_if_uart_request_command_len_max) {
                sprintf(buf3,
                        "request command length needs <= %d. input is %d .\n",
                        my_if_uart_request_command_len_max, tmp);
                httpd_resp_send_chunk(req, buf3, HTTPD_RESP_USE_STRLEN);
                err = ESP_FAIL;
            } else {
                // 16進文字列をchar配列に変換
                my_if_uart_request_command_len = (int)((strlen(buf2) + 1) / 2);
                if ((my_if_uart_request_command =
                         malloc(my_if_uart_request_command_len)) == NULL) {
                    err = ESP_FAIL;
                } else {
                    if (my_if_uart_str16_to_arr8(buf2, strlen(buf2),
                                                 my_if_uart_request_command) !=
                        0) {
                        err = ESP_FAIL;
                    }
                }
            }
        }
    }

    // 終了
    if (err == ESP_OK) {
        httpd_resp_send_chunk(req, "OK\n", HTTPD_RESP_USE_STRLEN);
    } else {
        httpd_resp_send_chunk(req, "NG\n", HTTPD_RESP_USE_STRLEN);
    }
    httpd_resp_send_chunk(req, "<hr>\n", HTTPD_RESP_USE_STRLEN);

    // 現在の設定値を表示
    my_httpd_show_config(req);

    // フッタを出力
    httpd_resp_send_chunk(req, "<hr>\n", HTTPD_RESP_USE_STRLEN);
    httpd_resp_send_chunk(req, "<a href='/'>go to home</a>\n",
                          HTTPD_RESP_USE_STRLEN);
    httpd_resp_send_chunk(req, "</body></html>\n", HTTPD_RESP_USE_STRLEN);
    httpd_resp_send_chunk(req, NULL, 0);

    return err;
}

/**
 * @brief uriにより起動。Terminator Sequence設定値を受け取り設定
 */
static esp_err_t my_httpd_terminator_sequence_post_handler(httpd_req_t *req) {
    // httpd通信の最終実行時刻を更新する
    gettimeofday(&my_httpd_last_com_tv, NULL);

    ESP_LOGI(TAG_HTTPD, "-> command: config terminator sequence");

    esp_err_t err = ESP_OK;
    int ret;
    char buf1[250];
    char buf2[100];

    // http出力しながら実行するので、まずはヘッダを出力しておく
    httpd_resp_send_chunk(req, "<!doctype html><html><body>\n",
                          HTTPD_RESP_USE_STRLEN);

    // buf1に読み込む
    int remaining = req->content_len;
    while (remaining > 0) {
        if ((ret = httpd_req_recv(req, buf1,
                                  MIN(remaining, sizeof(buf1) - 1))) <= 0) {
            if (ret == HTTPD_SOCK_ERR_TIMEOUT) {
                /* Retry receiving if timeout occurred */
                continue;
            } else {
                return ESP_FAIL;
            }
        }
        // httpd_req_recvは末尾に'\0'を付けてくれないっぽいので、付ける
        buf1[ret] = '\0';
        break;
    }
    DEBUGPRINT("read from query: %s\n", buf1);

    // input name='termseq' を取得
    if (httpd_query_key_value(buf1, "termseq", buf2, sizeof(buf2)) != ESP_OK) {
        err = ESP_FAIL;
    } else {
        DEBUGPRINT("Terminator Sequence : %s\n", buf2);
        // URLデコードする
        my_httpd_url_decode_inner(buf2);
        DEBUGPRINT("config decode query: %s\n", buf2);
        // 文字長がゼロなら空文字を作る
        if (strlen(buf2) == 0) {
            my_if_uart_terminator_sequence_len = 0;
            if ((my_if_uart_terminator_sequence = malloc(1)) == NULL) {
                err = ESP_FAIL;
            } else {
                my_if_uart_terminator_sequence[0] = 0;
            }
        } else {
            int tmp = strlen(buf2);
            char buf3[100];
            if (tmp < my_if_uart_terminator_sequence_len_min) {
                sprintf(
                    buf3,
                    "terminator sequence length needs >= %d. input is %d .\n",
                    my_if_uart_terminator_sequence_len_min, tmp);
                httpd_resp_send_chunk(req, buf3, HTTPD_RESP_USE_STRLEN);
                err = ESP_FAIL;
            } else if (tmp > my_if_uart_terminator_sequence_len_max) {
                sprintf(
                    buf3,
                    "terminator sequence length needs <= %d. input is %d .\n",
                    my_if_uart_terminator_sequence_len_max, tmp);
                httpd_resp_send_chunk(req, buf3, HTTPD_RESP_USE_STRLEN);
                err = ESP_FAIL;
            } else {
                // 16進文字列をchar配列に変換
                my_if_uart_terminator_sequence_len =
                    (int)((strlen(buf2) + 1) / 2);
                if ((my_if_uart_terminator_sequence =
                         malloc(my_if_uart_terminator_sequence_len)) == NULL) {
                    err = ESP_FAIL;
                } else {
                    if (my_if_uart_str16_to_arr8(
                            buf2, strlen(buf2),
                            my_if_uart_terminator_sequence) != 0) {
                        err = ESP_FAIL;
                    }
                }
            }
        }
    }

    // 終了
    if (err == ESP_OK) {
        httpd_resp_send_chunk(req, "OK\n", HTTPD_RESP_USE_STRLEN);
    } else {
        httpd_resp_send_chunk(req, "NG\n", HTTPD_RESP_USE_STRLEN);
    }
    httpd_resp_send_chunk(req, "<hr>\n", HTTPD_RESP_USE_STRLEN);

    // 現在の設定値を表示
    my_httpd_show_config(req);

    // フッタを出力
    httpd_resp_send_chunk(req, "<hr>\n", HTTPD_RESP_USE_STRLEN);
    httpd_resp_send_chunk(req, "<a href='/'>go to home</a>\n",
                          HTTPD_RESP_USE_STRLEN);
    httpd_resp_send_chunk(req, "</body></html>\n", HTTPD_RESP_USE_STRLEN);
    httpd_resp_send_chunk(req, NULL, 0);

    return err;
}

/**
 * @brief uriにより起動。Terminator Sequence Replace設定値を受け取り設定
 */
static esp_err_t my_httpd_terminator_sequence_replace_post_handler(
    httpd_req_t *req) {
    // httpd通信の最終実行時刻を更新する
    gettimeofday(&my_httpd_last_com_tv, NULL);

    ESP_LOGI(TAG_HTTPD, "-> command: config terminator sequence replace");

    esp_err_t err = ESP_OK;
    int ret;
    char buf1[250];
    char buf2[100];

    // http出力しながら実行するので、まずはヘッダを出力しておく
    httpd_resp_send_chunk(req, "<!doctype html><html><body>\n",
                          HTTPD_RESP_USE_STRLEN);

    // buf1に読み込む
    int remaining = req->content_len;
    while (remaining > 0) {
        if ((ret = httpd_req_recv(req, buf1,
                                  MIN(remaining, sizeof(buf1) - 1))) <= 0) {
            if (ret == HTTPD_SOCK_ERR_TIMEOUT) {
                /* Retry receiving if timeout occurred */
                continue;
            } else {
                return ESP_FAIL;
            }
        }
        // httpd_req_recvは末尾に'\0'を付けてくれないっぽいので、付ける
        buf1[ret] = '\0';
        break;
    }
    DEBUGPRINT("read from query: %s\n", buf1);

    // input name='termrep' を取得
    if (httpd_query_key_value(buf1, "termrep", buf2, sizeof(buf2)) != ESP_OK) {
        err = ESP_FAIL;
    } else {
        DEBUGPRINT("Terminator Sequence Replace : %s\n", buf2);
        // URLデコードする
        my_httpd_url_decode_inner(buf2);
        DEBUGPRINT("config decode query: %s\n", buf2);
        // 文字長がゼロなら空文字を作る
        if (strlen(buf2) == 0) {
            my_if_uart_terminator_sequence_replace_len = 0;
            if ((my_if_uart_terminator_sequence_replace = malloc(1)) == NULL) {
                err = ESP_FAIL;
            } else {
                my_if_uart_terminator_sequence_replace[0] = 0;
            }
        } else {
            int tmp = strlen(buf2);
            char buf3[100];
            if (tmp < my_if_uart_terminator_sequence_replace_len_min) {
                sprintf(buf3,
                        "terminator sequence replace length needs >= %d. input "
                        "is %d .\n",
                        my_if_uart_terminator_sequence_replace_len_min, tmp);
                httpd_resp_send_chunk(req, buf3, HTTPD_RESP_USE_STRLEN);
                err = ESP_FAIL;
            } else if (tmp > my_if_uart_terminator_sequence_replace_len_max) {
                sprintf(buf3,
                        "terminator sequence replace length needs >= %d. input "
                        "is %d .\n",
                        my_if_uart_terminator_sequence_replace_len_max, tmp);
                httpd_resp_send_chunk(req, buf3, HTTPD_RESP_USE_STRLEN);
                err = ESP_FAIL;
            } else {
                // 16進文字列をchar配列に変換
                my_if_uart_terminator_sequence_replace_len =
                    (int)((strlen(buf2) + 1) / 2);
                if ((my_if_uart_terminator_sequence_replace = malloc(
                         my_if_uart_terminator_sequence_replace_len)) == NULL) {
                    err = ESP_FAIL;
                } else {
                    if (my_if_uart_str16_to_arr8(
                            buf2, strlen(buf2),
                            my_if_uart_terminator_sequence_replace) != 0) {
                        err = ESP_FAIL;
                    }
                }
            }
        }
    }

    // 終了
    if (err == ESP_OK) {
        httpd_resp_send_chunk(req, "OK\n", HTTPD_RESP_USE_STRLEN);
    } else {
        httpd_resp_send_chunk(req, "NG\n", HTTPD_RESP_USE_STRLEN);
    }
    httpd_resp_send_chunk(req, "<hr>\n", HTTPD_RESP_USE_STRLEN);

    // 現在の設定値を表示
    my_httpd_show_config(req);

    // フッタを出力
    httpd_resp_send_chunk(req, "<hr>\n", HTTPD_RESP_USE_STRLEN);
    httpd_resp_send_chunk(req, "<a href='/'>go to home</a>\n",
                          HTTPD_RESP_USE_STRLEN);
    httpd_resp_send_chunk(req, "</body></html>\n", HTTPD_RESP_USE_STRLEN);
    httpd_resp_send_chunk(req, NULL, 0);

    return err;
}

/**
 * @brief uriにより起動。メモリ上にある設定をNVSに保存する
 */
static esp_err_t my_httpd_store_nvs_post_handler(httpd_req_t *req) {
    // httpd通信の最終実行時刻を更新する
    gettimeofday(&my_httpd_last_com_tv, NULL);

    ESP_LOGI(TAG_HTTPD, "-> command: Save");
    httpd_resp_send_chunk(req, "<!doctype html><html><body>\n",
                          HTTPD_RESP_USE_STRLEN);
    if (my_if_uart_set_config() == 0) {
        ESP_LOGI(TAG_HTTPD, "  saved");
        httpd_resp_send_chunk(req, "saved\n<br>", HTTPD_RESP_USE_STRLEN);
    } else {
        ESP_LOGI(TAG_HTTPD, "  save failed");
        httpd_resp_send_chunk(req, "failed\n<br>", HTTPD_RESP_USE_STRLEN);
    }
    httpd_resp_send_chunk(req, "<a href='/'>return to home</a>\n",
                          HTTPD_RESP_USE_STRLEN);
    httpd_resp_send_chunk(req, "</body></html>\n", HTTPD_RESP_USE_STRLEN);
    httpd_resp_send_chunk(req, NULL, 0);
    return ESP_OK;
}

/**
 * @brief uriにより起動。ソフトリセットする
 */
static esp_err_t my_httpd_soft_reset_post_handler(httpd_req_t *req) {
    // httpd通信の最終実行時刻を更新する
    gettimeofday(&my_httpd_last_com_tv, NULL);

    ESP_LOGI(TAG_HTTPD, "-> command: Soft Reset");
    httpd_resp_send_chunk(req, "<!doctype html><html><body>\n",
                          HTTPD_RESP_USE_STRLEN);
    httpd_resp_send_chunk(req, "going to reset in few seconds\n<br>",
                          HTTPD_RESP_USE_STRLEN);
    httpd_resp_send_chunk(
        req, "<a href='/'>wait 5 seconds and click here to go to home</a>\n",
        HTTPD_RESP_USE_STRLEN);
    httpd_resp_send_chunk(req, "</body></html>\n", HTTPD_RESP_USE_STRLEN);
    httpd_resp_send_chunk(req, NULL, 0);
    // すぐリセットするとページ表示が終わらないみたいなので、3000ms待ってからリセットする。
    // これでだめなら、もしかしてreturnしないと表示完了しないのかも。そうだとすればタイマー割込みさせるか・・
    // https://docs.espressif.com/projects/esp-idf/en/v4.3/esp32/api-reference/system/freertos.html#id9
    // →3000ms待つだけで良いみたい。
    vTaskDelay(3000 / portTICK_PERIOD_MS);
    esp_restart();
    return ESP_OK;
}

/**
 * @brief uriにより起動。httpサーバーを終了し、softapを終了する。
 * httpサーバーとsoftapは、設定変更時やモニター時にのみ使用するため、通常は起動している必要が無い。
 */
static esp_err_t my_httpd_shutdown_server_post_handler(httpd_req_t *req) {
    // httpd通信の最終実行時刻を更新する
    gettimeofday(&my_httpd_last_com_tv, NULL);

    ESP_LOGI(TAG_HTTPD, "-> command: shutdown server");
    esp_err_t err = ESP_OK;

    // http出力後に停止する
    httpd_resp_send_chunk(req, "<!doctype html><html><body>\n",
                          HTTPD_RESP_USE_STRLEN);
    httpd_resp_send_chunk(req, "<a href='/'>return to home</a>\n",
                          HTTPD_RESP_USE_STRLEN);
    httpd_resp_send_chunk(req, "</body></html>\n", HTTPD_RESP_USE_STRLEN);
    httpd_resp_send_chunk(req, NULL, 0);
    vTaskDelay(3000 / portTICK_PERIOD_MS);
    // httpdサーバーを終了する
    err = my_httpd_stop_webserver();
    if (err != ESP_OK) {
        ESP_LOGI(TAG_HTTPD, "ERROR : cannot stop httpd srever");
        return err;
    } else {
        ESP_LOGI(TAG_HTTPD, " -> succeed to shutdown httpd srever");
    }
    // softapを終了する
    err = my_softap_stop_ap();
    if (err != ESP_OK) {
        ESP_LOGI(TAG_HTTPD, "ERROR : cannot stop SoftAP");
        return err;
    } else {
        ESP_LOGI(TAG_HTTPD, " -> succeed to shutdown SoftAP");
    }
    return ESP_OK;
}

// uriごとの挙動
static const httpd_uri_t my_httpd_uri_home_get = {
    .uri = "/",
    .method = HTTP_GET,
    .handler = my_httpd_home_get_handler,
    .user_ctx = NULL};
static const httpd_uri_t my_httpd_uri_receive_buffer_length_post = {
    .uri = "/buflen",
    .method = HTTP_POST,
    .handler = my_httpd_receive_buffer_length_post_handler,
    .user_ctx = NULL};
static const httpd_uri_t my_httpd_uri_baud_rate_post = {
    .uri = "/baudrate",
    .method = HTTP_POST,
    .handler = my_httpd_baud_rate_post_handler,
    .user_ctx = NULL};
static const httpd_uri_t my_httpd_uri_request_command_post = {
    .uri = "/reqcmd",
    .method = HTTP_POST,
    .handler = my_httpd_request_command_post_handler,
    .user_ctx = NULL};
static const httpd_uri_t my_httpd_uri_terminator_sequence_post = {
    .uri = "/termseq",
    .method = HTTP_POST,
    .handler = my_httpd_terminator_sequence_post_handler,
    .user_ctx = NULL};
static const httpd_uri_t my_httpd_uri_terminator_sequence_replace_post = {
    .uri = "/termrep",
    .method = HTTP_POST,
    .handler = my_httpd_terminator_sequence_replace_post_handler,
    .user_ctx = NULL};
static const httpd_uri_t my_httpd_uri_store_nvs_post = {
    .uri = "/store_nvs",
    .method = HTTP_POST,
    .handler = my_httpd_store_nvs_post_handler,
    .user_ctx = NULL};
static const httpd_uri_t my_httpd_uri_soft_reset_post = {
    .uri = "/soft_reset",
    .method = HTTP_POST,
    .handler = my_httpd_soft_reset_post_handler,
    .user_ctx = NULL};
static const httpd_uri_t my_httpd_uri_shutdown_server_post = {
    .uri = "/shutdown_server",
    .method = HTTP_POST,
    .handler = my_httpd_shutdown_server_post_handler,
    .user_ctx = NULL};

/**
 * @brief httpサーバーを開始する
 */
esp_err_t my_httpd_start_webserver(void) {
    // httpd通信の最終実行時刻を更新する
    gettimeofday(&my_httpd_last_com_tv, NULL);

    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.max_uri_handlers =
        12;  // http_register_uri_handlerに登録できるURIの上限
#if CONFIG_IDF_TARGET_LINUX
    config.server_port = 8001;
#else
    config.server_port = 80;
#endif
    config.lru_purge_enable = true;

    // Start the httpd server
    ESP_LOGI(TAG_HTTPD, "Starting server on port: '%d'", config.server_port);
    if (httpd_start(&httpd_server, &config) == ESP_OK) {
        // Set URI handlers
        ESP_LOGI(TAG_HTTPD, "Registering URI handlers");
        httpd_register_uri_handler(httpd_server, &my_httpd_uri_home_get);
        httpd_register_uri_handler(httpd_server,
                                   &my_httpd_uri_receive_buffer_length_post);
        httpd_register_uri_handler(httpd_server, &my_httpd_uri_baud_rate_post);
        httpd_register_uri_handler(httpd_server,
                                   &my_httpd_uri_request_command_post);
        httpd_register_uri_handler(httpd_server,
                                   &my_httpd_uri_terminator_sequence_post);
        httpd_register_uri_handler(
            httpd_server, &my_httpd_uri_terminator_sequence_replace_post);
        httpd_register_uri_handler(httpd_server, &my_httpd_uri_store_nvs_post);
        httpd_register_uri_handler(httpd_server, &my_httpd_uri_soft_reset_post);
        httpd_register_uri_handler(httpd_server,
                                   &my_httpd_uri_shutdown_server_post);
        return ESP_OK;
    }

    ESP_LOGI(TAG_HTTPD, "Error starting server!");
    return ESP_FAIL;
}

#if !CONFIG_IDF_TARGET_LINUX
/**
 * @brief httpサーバーを停止する
 * @param server 停止したいhttpサーバー
 */
static esp_err_t my_httpd_stop_webserver_internal(httpd_handle_t server) {
    // Stop the httpd server
    return httpd_stop(server);
}

/**
 * @brief 現在のhttpサーバーを停止する
 */
esp_err_t my_httpd_stop_webserver() {
    return my_httpd_stop_webserver_internal(httpd_server);
}

/**
 * @brief disconnectハンドラ。使ってないかも？？
 */
static void my_httpd_disconnect_handler(void *arg, esp_event_base_t event_base,
                                        int32_t event_id, void *event_data) {
    httpd_handle_t *server = (httpd_handle_t *)arg;
    if (*server) {
        ESP_LOGI(TAG_HTTPD, "Stopping webserver");
        if (my_httpd_stop_webserver_internal(*server) == ESP_OK) {
            *server = NULL;
        } else {
            ESP_LOGE(TAG_HTTPD, "Failed to stop http server");
        }
    }
}

/**
 * @brief connectハンドラ。使ってないかも？？
 */
static void my_httpd_connect_handler(void *arg, esp_event_base_t event_base,
                                     int32_t event_id, void *event_data) {
    httpd_handle_t *server = (httpd_handle_t *)arg;
    if (*server == NULL) {
        ESP_LOGI(TAG_HTTPD, "Starting webserver with connection");
        my_httpd_start_webserver();
        *server = httpd_server;
    }
}
#endif  // !CONFIG_IDF_TARGET_LINUX

