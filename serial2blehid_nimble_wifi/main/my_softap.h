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
 * my_softap.h
 *   WiFi access point
 */


#ifndef MY_SOFTAP_H
#define MY_SOFTAP_H 1


#include <esp_err.h>

// Initialize and start Soft AP
extern void my_softap_wifi_init(void);
// SoftAPを停止する
extern esp_err_t my_softap_stop_ap(void);
// SoftApに接続している数（外部から使用）
extern int my_softap_connected_cnt;



#endif

