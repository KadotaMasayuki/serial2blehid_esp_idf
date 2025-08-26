# (nimble) serial to BLE HID for v5.3.2 with Boot Button

## what is this

ref.
[OlegOS76/nimble_kbdhid_example](https://github.com/olegos76/nimble_kbdhid_example/tree/main/main)

を元に、ESP-IDF v5.3.2 でビルドできるよう修正したもの [serial2blehid_nimble_532]() を用い、以下の機能を追加した。

1. 日本語キーボード配列を追加
1. 文字コードからスキャンコード(Usage ID)に変換するマクロを追加
1. `Boot` ボタンを押すと、文字コード `0x20～0x7e` を順にインクリメントし、1文字だけ、BLE-HIDを経由してPCなどに無線キーボード入力する


## build

テストは [Seeed Studio XIAO ESP32C3](https://wiki.seeedstudio.com/XIAO_ESP32C3_Getting_Started/) でのみ実施済。

```
idf.py set-target esp32c3
idf.py menuconfig
idf.py build
```

```
[1053/1060] Generating ld/sections.ld
warning: EXAMPLE_DISP_PASSWD (defined at C:/Espressif/frameworks/esp-idf-v5.3.2/prj/serial2blehid_esp_idf/serial2blehid_nimble_button/main/Kconfig.projbuild:42) defined with multiple prompts in single location
[98/99] Generating binary image from built executable
esptool.py v4.8.1
Creating esp32c3 image...
Merged 2 ELF sections
Successfully created esp32c3 image.
Generated C:/Espressif/frameworks/esp-idf-v5.3.2/prj/serial2blehid_esp_idf/serial2blehid_nimble_button/build/bootloader/bootloader.bin
[99/99] C:\WINDOWS\system32\cmd.exe /C "cd /D C:\Espressif...rial2blehid_nimble_button/build/bootloader/bootloader.bin"
Bootloader binary size 0x57d0 bytes. 0x2830 bytes (31%) free.
[1059/1060] Generating binary image from built executable
esptool.py v4.8.1
Creating esp32c3 image...
Merged 1 ELF section
Successfully created esp32c3 image.
Generated C:/Espressif/frameworks/esp-idf-v5.3.2/prj/serial2blehid_esp_idf/serial2blehid_nimble_button/build/ble_kbdhid.bin
[1060/1060] C:\WINDOWS\system32\cmd.exe /C "cd /D C:\Espre..._esp_idf/serial2blehid_nimble_button/build/ble_kbdhid.bin"
ble_kbdhid.bin binary size 0x8bbd0 bytes. Smallest app partition is 0x177000 bytes. 0xeb430 bytes (63%) free.
```

```
(menuconfig -> CONFIG_PARTITION_TABLE_SINGLE_APP_LARGE=y)
                            Memory Type Usage Summary
+--------------------------------------------------------------------------------+
| Memory Type/Section | Used [bytes] | Used [%] | Remain [bytes] | Total [bytes] |
|---------------------+--------------+----------+----------------+---------------|
| Flash Code          |       404276 |     4.82 |        7984300 |       8388576 |
|    .text            |       404276 |     4.82 |                |               |
| DRAM                |       101288 |    31.52 |         220008 |        321296 |
|    .text            |        78076 |     24.3 |                |               |
|    .bss             |        11552 |      3.6 |                |               |
|    .data            |        11400 |     3.55 |                |               |
| Flash Data          |        78508 |     0.94 |        8310068 |       8388576 |
|    .rodata          |        78252 |     0.93 |                |               |
|    .appdesc         |          256 |      0.0 |                |               |
| RTC SLOW            |          536 |     6.54 |           7656 |          8192 |
|    .rtc_reserved    |           24 |     0.29 |                |               |
+--------------------------------------------------------------------------------+
Total image size: 572260 bytes (.bin may be padded larger)
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




