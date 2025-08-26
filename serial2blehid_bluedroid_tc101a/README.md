# (bluedroid) serial to BLE HID for TC-101A

## what is this

ref. [ble hid device demo](https://github.com/espressif/esp-idf/tree/master/examples/bluetooth/bluedroid/ble/ble_hid_device_demo)

を元にして、以下の機能を付けた。

1. 日本語キーボードレイアウトを追加
1. 文字コードからスキャンコード(Usage-ID)に変換するマクロを追加
1. GPIO-5をLにすると、UARTから"QX\r\n"を送出し、UARTから"\r\n"で終わる文字列を受信する
1. 受信した文字列の終端文字列をTABキーで置き換える
1. 受信した文字列をBLE-HIDを経由してPCなどに無線キーボード入力する


## build

テストは [Seeed Studio XIAO ESP32C3](https://wiki.seeedstudio.com/XIAO_ESP32C3_Getting_Started/) でのみ実施済。

```
idf.py set-target esp32c3
idf.py menuconfig
idf.py build
```

```
[98/99] Generating binary image from built executable
esptool.py v4.8.1
Creating esp32c3 image...
Merged 2 ELF sections
Successfully created esp32c3 image.
Generated C:/Espressif/frameworks/esp-idf-v5.3.2/prj/serial2blehid_bluedroid_tc101a/build/bootloader/bootloader.bin
[99/99] C:\WINDOWS\system32\cmd.exe /C "cd /D C:\Espressif...l2blehid_bluedroid_tc101a/build/bootloader/bootloader.bin"
Bootloader binary size 0x57d0 bytes. 0x2830 bytes (31%) free.
[1253/1254] Generating binary image from built executable
esptool.py v4.8.1
Creating esp32c3 image...
Merged 1 ELF section
Successfully created esp32c3 image.
Generated C:/Espressif/frameworks/esp-idf-v5.3.2/prj/serial2blehid_bluedroid_tc101a/build/hidd_demos.bin
[1254/1254] C:\WINDOWS\system32\cmd.exe /C "cd /D C:\Espre...2/prj/serial2blehid_bluedroid_tc101a/build/hidd_demos.bin"
hidd_demos.bin binary size 0xd05b0 bytes. Smallest app partition is 0x177000 bytes. 0xa6a50 bytes (44%) free.
```

```
(menuconfig -> CONFIG_PARTITION_TABLE_SINGLE_APP_LARGE=y)
                            Memory Type Usage Summary
+--------------------------------------------------------------------------------+
| Memory Type/Section | Used [bytes] | Used [%] | Remain [bytes] | Total [bytes] |
|---------------------+--------------+----------+----------------+---------------|
| Flash Code          |       619782 |     7.39 |        7768794 |       8388576 |
|    .text            |       619782 |     7.39 |                |               |
| Flash Data          |       142348 |      1.7 |        8246228 |       8388576 |
|    .rodata          |       142092 |     1.69 |                |               |
|    .appdesc         |          256 |      0.0 |                |               |
| DRAM                |       109920 |    34.21 |         211376 |        321296 |
|    .text            |        80534 |    25.07 |                |               |
|    .bss             |        18368 |     5.72 |                |               |
|    .data            |        10656 |     3.32 |                |               |
| RTC SLOW            |          536 |     6.54 |           7656 |          8192 |
|    .rtc_reserved    |           24 |     0.29 |                |               |
+--------------------------------------------------------------------------------+
Total image size: 853320 bytes (.bin may be padded larger)
```


## 日本語キーボードレイアウト

日本語キーボード配列用の `my_hid_key_map_jp.c` ファイルを用意したが、そのままでは `\` や `_` や `|` が入力されない。

日本語キーボード配列で送信されるスキャンコード(Usage ID）には、US配列で用いられるスキャンコード範囲の他に、 135 と 137 が追加されているが、オリジナルの `gatt_vars.c` では 101 までしか受け付けないようにされている。 そのため、135と137を用いる `\` や `_` や `|` が無視される。そこで、 `gatt_vars.c` の KEYBOARD REPORT 部を下記の右矢印のように変えることで、スキャンコード(Usage ID)が0xffまで許容されるようになり、無事 `\` などが入力されるようになる。

```
    //   Key arrays (6 bytes)
    0x95, 0x06,  //   Report Count (6)
    0x75, 0x08,  //   Report Size (8)
    0x15, 0x00,  //   Log Min (0)
    0x25, 0x65,  //   Log Max (101)  →  0x25, 0xff に変更
    0x05, 0x07,  //   Usage Pg (Key Codes)
    0x19, 0x00,  //   Usage Min (0)
    0x29, 0x65,  //   Usage Max (101)  →  0x29, 0xff に変更
    0x81, 0x00,  //   Input: (Data, Array)
```

## キーボードレイアウト変更するには

日本語レイアウトなら、 ```CmakeLists.txt``` の ```idf_component_register(SRCS ...)``` に、 ```my_hid_key_map_jp.c``` を書けば良い。

USレイアウトなら、 ```CmakeLists.txt``` の ```idf_component_register(SRCS ...)``` に、 ```my_hid_key_map_us.c``` を書けば良い。



