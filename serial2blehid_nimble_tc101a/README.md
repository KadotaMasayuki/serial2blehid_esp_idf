# (nimble) serial to BLE HID for v5.3.2 for TC-101A with Boot Button

## what is this

ref.
[serial2blehid_nimble_button](https://github.com/KadotaMasayuki/serial2blehid_esp_idf/tree/main/serial2blehid_nimble_button)
を元に、
[serial2blehid_bluedroid_tc101a](https://github.com/KadotaMasayuki/serial2blehid_esp_idf/tree/main/serial2blehid_bluedroid_tc101a)
を移植し、`Nimble`経由で`TC-101A`の測定値を送出できるようにした。


`Bluedroid`版
[serial2blehid_bluedroid_tc101a](https://github.com/KadotaMasayuki/serial2blehid_esp_idf/tree/main/serial2blehid_bluedroid_tc101a)
からの変更点は、

1. Nimbleに変更
1. GPIO/UARTタスク内でキーボード送信できないためタスク外で実行
1. セマフォを追加
1. トリガーは`Boot`ボタン


`Nimble`版
[serial2blehid_nimble_button](https://github.com/KadotaMasayuki/serial2blehid_esp_idf/tree/main/serial2blehid_nimble_button)
からの変更点は、

1. 計測器TC-101Aとの通信機能を追加


## build

テストは [Seeed Studio XIAO ESP32C3](https://wiki.seeedstudio.com/XIAO_ESP32C3_Getting_Started/) でのみ実施済。

```
idf.py set-target esp32c3
idf.py menuconfig
idf.py build
```

```
[6/9] Generating ld/sections.ld
warning: EXAMPLE_DISP_PASSWD (defined at C:/Espressif/frameworks/esp-idf-v5.3.2/prj/serial2blehid_esp_idf/serial2blehid_nimble_tc101a/main/Kconfig.projbuild:42) defined with multiple prompts in single location
[8/9] Generating binary image from built executable
esptool.py v4.8.1
Creating esp32c3 image...
Merged 1 ELF section
Successfully created esp32c3 image.
Generated C:/Espressif/frameworks/esp-idf-v5.3.2/prj/serial2blehid_esp_idf/serial2blehid_nimble_tc101a/build/ble_kbdhid.bin
[9/9] C:\WINDOWS\system32\cmd.exe /C "cd /D C:\Espressif\frameworks\esp...serial2blehid_esp_idf/serial2blehid_nimble_tc101a/build/ble_kbdhid.bin"
ble_kbdhid.bin binary size 0x904e0 bytes. Smallest app partition is 0x177000 bytes. 0xe6b20 bytes (62%) free.
```

```
(menuconfig -> CONFIG_PARTITION_TABLE_SINGLE_APP_LARGE=y)
                            Memory Type Usage Summary
+--------------------------------------------------------------------------------+
| Memory Type/Section | Used [bytes] | Used [%] | Remain [bytes] | Total [bytes] |
|---------------------+--------------+----------+----------------+---------------|
| Flash Code          |       414440 |     4.94 |        7974136 |       8388576 |
|    .text            |       414440 |     4.94 |                |               |
| DRAM                |       104896 |    32.65 |         216400 |        321296 |
|    .text            |        81894 |    25.49 |                |               |
|    .bss             |        11568 |      3.6 |                |               |
|    .data            |        11408 |     3.55 |                |               |
| Flash Data          |        83220 |     0.99 |        8305356 |       8388576 |
|    .rodata          |        82964 |     0.99 |                |               |
|    .appdesc         |          256 |      0.0 |                |               |
| RTC SLOW            |          536 |     6.54 |           7656 |          8192 |
|    .rtc_reserved    |           24 |     0.29 |                |               |
+--------------------------------------------------------------------------------+
Total image size: 590962 bytes (.bin may be padded larger)
```

[Bluedroid版](https://github.com/KadotaMasayuki/serial2blehid_esp_idf/tree/main/serial2blehid_bluedroid_tc101a#build)
と比べると20Kbytesほど容量が小さくなっている。


## TC-101A実機が無い状況でのテスト

`my_if_uart.c` において、 ```#define MY_IF_UART_NO_UART 0``` でUART通信する。```#define MY_IF_UART_NO_UART 1``` でUART通信せずダミーの受信データを用いる。




