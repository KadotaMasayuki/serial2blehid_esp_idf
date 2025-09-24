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
 * my_softap.c
 *   WiFi access point
 */


#include <stdint.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "esp_mac.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include <esp_err.h>
#include "esp_log.h"
#include "esp_netif_net_stack.h"
#include "esp_netif.h"
#include "nvs_flash.h"
#include "lwip/inet.h"
#include "lwip/netdb.h"
#include "lwip/sockets.h"
#if IP_NAPT
#include "lwip/lwip_napt.h"
#endif
#include "lwip/err.h"
#include "lwip/sys.h"

#include "my_debug.h"
#include "my_softap.h"




/* STA Configuration */
#define EXAMPLE_STA_ENABLE                  CONFIG_STA_ENABLE
#define EXAMPLE_ESP_WIFI_STA_SSID           CONFIG_ESP_WIFI_REMOTE_AP_SSID
#define EXAMPLE_ESP_WIFI_STA_PASSWD         CONFIG_ESP_WIFI_REMOTE_AP_PASSWORD
#define EXAMPLE_ESP_MAXIMUM_RETRY           CONFIG_ESP_MAXIMUM_STA_RETRY

#if CONFIG_ESP_WIFI_AUTH_OPEN
#define ESP_WIFI_SCAN_AUTH_MODE_THRESHOLD   WIFI_AUTH_OPEN
#elif CONFIG_ESP_WIFI_AUTH_WEP
#define ESP_WIFI_SCAN_AUTH_MODE_THRESHOLD   WIFI_AUTH_WEP
#elif CONFIG_ESP_WIFI_AUTH_WPA_PSK
#define ESP_WIFI_SCAN_AUTH_MODE_THRESHOLD   WIFI_AUTH_WPA_PSK
#elif CONFIG_ESP_WIFI_AUTH_WPA2_PSK
#define ESP_WIFI_SCAN_AUTH_MODE_THRESHOLD   WIFI_AUTH_WPA2_PSK
#elif CONFIG_ESP_WIFI_AUTH_WPA_WPA2_PSK
#define ESP_WIFI_SCAN_AUTH_MODE_THRESHOLD   WIFI_AUTH_WPA_WPA2_PSK
#elif CONFIG_ESP_WIFI_AUTH_WPA3_PSK
#define ESP_WIFI_SCAN_AUTH_MODE_THRESHOLD   WIFI_AUTH_WPA3_PSK
#elif CONFIG_ESP_WIFI_AUTH_WPA2_WPA3_PSK
#define ESP_WIFI_SCAN_AUTH_MODE_THRESHOLD   WIFI_AUTH_WPA2_WPA3_PSK
#elif CONFIG_ESP_WIFI_AUTH_WAPI_PSK
#define ESP_WIFI_SCAN_AUTH_MODE_THRESHOLD   WIFI_AUTH_WAPI_PSK
#endif


/* AP Configuration */
#define EXAMPLE_SOFTAP_ENABLE               CONFIG_SOFTAP_ENABLE
#define EXAMPLE_ESP_WIFI_AP_SSID            CONFIG_ESP_WIFI_AP_SSID
#define EXAMPLE_ESP_WIFI_AP_PASSWD          CONFIG_ESP_WIFI_AP_PASSWORD
#define EXAMPLE_ESP_WIFI_CHANNEL            CONFIG_ESP_WIFI_AP_CHANNEL
#define EXAMPLE_MAX_STA_CONN                CONFIG_ESP_MAX_STA_CONN_AP


/* Static IP Address Configuration */
#define EXAMPLE_STATIC_IP                   CONFIG_STATIC_IP
#define EXAMPLE_STATIC_MASK                 CONFIG_STATIC_MASK
#define EXAMPLE_STATIC_GW                   CONFIG_STATIC_GW


static const char *TAG_AP = "WiFi SoftAP";

// SoftApに接続している数
//   外部から参照して、起動して一定時間ゼロであればSoftAPを終了する処理を行う
int my_softap_connected_cnt = 0;


/**
 * @brief wifiのイベントハンドラ
 */
static void my_softap_wifi_event_handler(void *arg, esp_event_base_t event_base,
    int32_t event_id, void *event_data)
{
  if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_AP_STACONNECTED) {
    // APのイベント: STAが接続して来た
    wifi_event_ap_staconnected_t *event = (wifi_event_ap_staconnected_t *) event_data;
    my_softap_connected_cnt ++;
    ESP_LOGI(TAG_AP, "CONNECTED: Station "MACSTR" joined, AID=%d",
        MAC2STR(event->mac), event->aid);
  } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_AP_STADISCONNECTED) {
    // APのイベント: STAが切断した
    wifi_event_ap_stadisconnected_t *event = (wifi_event_ap_stadisconnected_t *) event_data;
    my_softap_connected_cnt --;
    ESP_LOGI(TAG_AP, "DISCONNECTED: Station "MACSTR" left, AID=%d, reason:%d",
        MAC2STR(event->mac), event->aid, event->reason);
  }
}


/**
 * @brief WiFi-APを停止する 2025/3/10
 *  https://esp32.com/viewtopic.php?t=34332
 *    esp_wifi_stop()などを使ってもSSIDが見えるし接続もできてしまう？
 *    esp_wifi_set_mode(WIFI_MODE_NULL)とすることでSoftAPを解除できるようだ。
 *  https://esp32.com/viewtopic.php?t=25508
 *    SSIDは300秒くらいキャッシュされるらしい。なので、5分くらいすればWiFiAPのリストから消えるみたい。
 * @return esp_err_t
 */
esp_err_t my_softap_stop_ap(void) {
  //return esp_wifi_stop();
  //return esp_wifi_deinit();
  return esp_wifi_set_mode(WIFI_MODE_NULL);
}


/**
 * @brief Initialize and start Soft AP
 *  WiFi
 *  DHCPサーバー
 *  自分のIPアドレス
 *  を開始する
 */
void my_softap_wifi_init(void)
{
  ESP_LOGI(TAG_AP, "init ESP_WIFI_MODE_AP");

  ESP_ERROR_CHECK(esp_netif_init());
  ESP_ERROR_CHECK(esp_event_loop_create_default());

  esp_netif_t *esp_netif_ap = esp_netif_create_default_wifi_ap();

  my_softap_connected_cnt = 0;

  /*Initialize WiFi */
  wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
  ESP_ERROR_CHECK(esp_wifi_init(&cfg));

  /* Register Event handler */
  ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT,
        ESP_EVENT_ANY_ID,
        &my_softap_wifi_event_handler,
        NULL,
        NULL));

  // DHCPサーバーを停止
  ESP_ERROR_CHECK_WITHOUT_ABORT(esp_netif_dhcps_stop(esp_netif_ap));
  
  // 自分のIPアドレスを設定
  esp_netif_ip_info_t ip_info;
  memset(&ip_info, 0, sizeof(esp_netif_ip_info_t));
  ip_info.ip.addr = ipaddr_addr(EXAMPLE_STATIC_IP);
  ip_info.netmask.addr = ipaddr_addr(EXAMPLE_STATIC_MASK);
  ip_info.gw.addr = ipaddr_addr(EXAMPLE_STATIC_GW);
  ESP_ERROR_CHECK(esp_netif_set_ip_info(esp_netif_ap, &ip_info));

  // AP設定
  wifi_config_t wifi_ap_config = {
    .ap = {
      .ssid = EXAMPLE_ESP_WIFI_AP_SSID,
      .ssid_len = strlen(EXAMPLE_ESP_WIFI_AP_SSID),
      .channel = EXAMPLE_ESP_WIFI_CHANNEL,
      .password = EXAMPLE_ESP_WIFI_AP_PASSWD,
      .max_connection = EXAMPLE_MAX_STA_CONN,
      .authmode = WIFI_AUTH_WPA2_PSK,
      .pmf_cfg = {
        .required = false,
      },
    },
  };
  // パスワード設定が無ければオープン接続
  if (strlen(EXAMPLE_ESP_WIFI_AP_PASSWD) == 0) {
    wifi_ap_config.ap.authmode = WIFI_AUTH_OPEN;
  }

  ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_AP));
  ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_AP, &wifi_ap_config));
  ESP_LOGI(TAG_AP, "softap_wifi_init finished. SSID:%s password:%s channel:%d",
      EXAMPLE_ESP_WIFI_AP_SSID, EXAMPLE_ESP_WIFI_AP_PASSWD, EXAMPLE_ESP_WIFI_CHANNEL);

  // DHCPサーバーを開始
  ESP_ERROR_CHECK_WITHOUT_ABORT(esp_netif_dhcps_start(esp_netif_ap));

  /* Start WiFi */
  ESP_ERROR_CHECK(esp_wifi_start());
}



