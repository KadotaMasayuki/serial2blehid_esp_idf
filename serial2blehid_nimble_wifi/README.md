# (nimble) serial to BLE HID for v5.3.2 for TC-101A with WiFi config

## what is this

ref.
[serial2blehid_nimble_tc101a](https://github.com/KadotaMasayuki/serial2blehid_esp_idf/tree/main/serial2blehid_nimble_tc101a)
を元に、WiFiで設定できるようにした。


[serial2blehid_nimble_tc101a](https://github.com/KadotaMasayuki/serial2blehid_esp_idf/tree/main/serial2blehid_nimble_tc101a)
からの変更点は、

1. 電源ONまたはリセットから60秒間、WiFiアクセスポイントを起動
1. WiFi接続により、受信バッファサイズ、通信速度、コマンド文字列、受信末尾文字列、受信末尾文字列の変換文字列、を設定できる
1. トリガーは`GPIO(5)`に変更

`idf.py menuconfig`での変更点は、

1. Component config / Bluetooth / Nimble Options / BLE GAP default device name を9文字以下で指定（これ以上だと実行時にエラーになりアドバタイズしてくれない）
1. Component config / HTTP Server / Max HTTP Request Header Length を2048に
1. Component config / HTTP Server / Max HTTP URI Length を2048に
1. EXAMPLE SoftAP Configration / WiFi AP SSID を設定（サンプルでは、識別しやすくするためにBLEデバイス名と同じにしてあるが、別の文字列にしても良い）
1. EXAMPLE SoftAP Configration / WiFi AP Password を設定
1. EXAMPLE Static IP Address Configuration / Static IP Address Configuration で、HTTPサーバーのIPアドレスを設定する。（サンプルではIP=192.168.4.1、MASK=255.255.255.0、GW=192.168.4.1 にしてある）


## build

テストは [Seeed Studio XIAO ESP32C3](https://wiki.seeedstudio.com/XIAO_ESP32C3_Getting_Started/) でのみ実施済。

```
idf.py set-target esp32c3
idf.py menuconfig
idf.py build
```

```
idf.py size
Executing action: size
Running ninja in directory C:\Espressif\frameworks\esp-idf-v5.3.2\prj\serial2blehid_esp_idf\serial2blehid_nimble_wifi\build
Executing "ninja all"...
[1/4] C:\WINDOWS\system32\cmd.exe /C "cd /D C:\Espressif\frameworks\esp-idf-v5.3.2\pr...-idf-v5.3.2/prj/serial2blehid_esp_idf/serial2blehid_nimble_wifi/build/ble_kbdhid.bin
ble_kbdhid.bin binary size 0x10f550 bytes. Smallest app partition is 0x177000 bytes. 0x67ab0 bytes (28%) free.
[1/1] C:\WINDOWS\system32\cmd.exe /C "cd /D C:\Espressif\frameworks\esp-idf-v5.3.2\pr.../prj/serial2blehid_esp_idf/serial2blehid_nimble_wifi/build/bootloader/bootloader.bin
Bootloader binary size 0x57d0 bytes. 0x2830 bytes (31%) free.
[4/4] Completed 'bootloader'Running ninja in directory C:\Espressif\frameworks\esp-idf-v5.3.2\prj\serial2blehid_esp_idf\serial2blehid_nimble_wifi\build
Executing "ninja size"...
[0/1] C:\WINDOWS\system32\cmd.exe /C "cd /D C:\Espressif\frameworks\esp-idf-v5.3.2\prj\serial2blehid_esp_idf\serial2blehid_nimble_wifi\build && C:\Espressif\tools\cmake\3.30.2\bin\cmake.exe -D IDF_SIZE_TOOL=C:/Espressif/python_env/idf5.3_py3.11_env/Scripts/python.exe;-m;esp_idf_size -D MAP_FILE=C:/Espressif/frameworks/esp-idf-v5.3.2/prj/serial2blehid_esp_idf/serial2blehid_nimble_wifi/build/ble_kbdhid.map -D OUTPUT_JSON= -P C:/Espressif/frameworks/esp-idf-v5.3.2/tools/cmake/run_size_tool.cmake"
```

```
(menuconfig -> CONFIG_PARTITION_TABLE_SINGLE_APP_LARGE=y)
                            Memory Type Usage Summary
+--------------------------------------------------------------------------------+
| Memory Type/Section | Used [bytes] | Used [%] | Remain [bytes] | Total [bytes] |
|---------------------+--------------+----------+----------------+---------------|
| Flash Code          |       844028 |    10.06 |        7544548 |       8388576 |
|    .text            |       844028 |    10.06 |                |               |
| Flash Data          |       147712 |     1.76 |        8240864 |       8388576 |
|    .rodata          |       147456 |     1.76 |                |               |
|    .appdesc         |          256 |      0.0 |                |               |
| DRAM                |       144152 |    44.87 |         177144 |        321296 |
|    .text            |       103766 |     32.3 |                |               |
|    .bss             |        24440 |     7.61 |                |               |
|    .data            |        15768 |     4.91 |                |               |
| RTC SLOW            |          536 |     6.54 |           7656 |          8192 |
|    .rtc_reserved    |           24 |     0.29 |                |               |
+--------------------------------------------------------------------------------+
Total image size: 1111274 bytes (.bin may be padded larger)
```


## TC-101A実機が無い状況でのテスト

`my_if_uart.c` において、 ```#define MY_IF_UART_NO_UART 0``` でUART通信する。```#define MY_IF_UART_NO_UART 1``` でUART通信せずダミーの受信データを用いる。




