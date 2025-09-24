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
 * my_httpd.h
 *   http server from Simple HTTP Server Example
 *     https://github.com/espressif/esp-idf/blob/master/examples/protocols/http_server/simple/main/main.c
 */


#ifndef MY_HTTPD_H
#define MY_HTTPD_H 1



#include <esp_err.h>
#include <esp_http_server.h>
#include <time.h>


// start httpd
extern esp_err_t my_httpd_start_webserver(void);

// stop httpd
extern esp_err_t my_httpd_stop_webserver(void);

// 最終通信時刻を取得
extern struct timeval my_httpd_last_com_tv;


#endif

