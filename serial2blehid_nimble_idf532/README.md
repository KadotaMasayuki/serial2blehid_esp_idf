# (nimble) serial to BLE HID for v5.3.2

## what is this

ref.
[OlegOS76/nimble_kbdhid_example](https://github.com/olegos76/nimble_kbdhid_example/tree/main/main)

を元に、ESP-IDF v5.3.2 でビルドできるよう修正。


## build

テストは [Seeed Studio XIAO ESP32C3](https://wiki.seeedstudio.com/XIAO_ESP32C3_Getting_Started/) でのみ実施済。

```
idf.py set-target esp32c3
idf.py menuconfig
idf.py build
```

```
[87/89] Generating binary image from built executable
esptool.py v4.8.1
Creating esp32c3 image...
Merged 2 ELF sections
Successfully created esp32c3 image.
Generated C:/Espressif/frameworks/esp-idf-v5.3.2/prj/serial2blehid_esp_idf/serial2blehid_nimble_idf532/build/bootloader/bootloader.bin
[88/89] C:\WINDOWS\system32\cmd.exe /C "cd /D C:\Espressif...rial2blehid_nimble_idf532/build/bootloader/bootloader.bin"
Bootloader binary size 0x53a0 bytes. 0x2c60 bytes (35%) free.
[1034/1038] Generating ld/sections.ld
warning: EXAMPLE_DISP_PASSWD (defined at C:/Espressif/frameworks/esp-idf-v5.3.2/prj/serial2blehid_esp_idf/serial2blehid_nimble_idf532/main/Kconfig.projbuild:42) defined with multiple prompts in single location
[1036/1038] Generating binary image from built executable
esptool.py v4.8.1
Creating esp32c3 image...
Merged 1 ELF section
Successfully created esp32c3 image.
Generated C:/Espressif/frameworks/esp-idf-v5.3.2/prj/serial2blehid_esp_idf/serial2blehid_nimble_idf532/build/ble_kbdhid.bin
[1037/1038] C:\WINDOWS\system32\cmd.exe /C "cd /D C:\Espre..._esp_idf/serial2blehid_nimble_idf532/build/ble_kbdhid.bin"
ble_kbdhid.bin binary size 0x8b950 bytes. Smallest app partition is 0x177000 bytes. 0xeb6b0 bytes (63%) free.
```

```
(menuconfig -> CONFIG_PARTITION_TABLE_SINGLE_APP_LARGE=y)
                            Memory Type Usage Summary
+--------------------------------------------------------------------------------+
| Memory Type/Section | Used [bytes] | Used [%] | Remain [bytes] | Total [bytes] |
|---------------------+--------------+----------+----------------+---------------|
| Flash Code          |       404006 |     4.82 |        7984570 |       8388576 |
|    .text            |       404006 |     4.82 |                |               |
| DRAM                |       101304 |    31.53 |         219992 |        321296 |
|    .text            |        78078 |     24.3 |                |               |
|    .bss             |        11552 |      3.6 |                |               |
|    .data            |        11416 |     3.55 |                |               |
| Flash Data          |        78116 |     0.93 |        8310460 |       8388576 |
|    .rodata          |        77860 |     0.93 |                |               |
|    .appdesc         |          256 |      0.0 |                |               |
| RTC SLOW            |          536 |     6.54 |           7656 |          8192 |
|    .rtc_reserved    |           24 |     0.29 |                |               |
+--------------------------------------------------------------------------------+
Total image size: 571616 bytes (.bin may be padded larger)
```


## main/ble_func.c の変更点

### MACSTRマクロが無くなったようで、ビルド失敗するため、13行目にMACSTRマクロを追加。

```#define MACSTR "%02x:%02x:%02x:%02x:%02x:%02x"```


### unsigned longをstring化しようとしているが、%dで指定している

269行目の%dを%luに変更する
```ESP_LOGI(tag, "Enter passkey %lu on the peer side", pkey.passkey);```


### 334行目 esp_nimble_hci_and_controller_init()が非推奨になっている

nimble_port_init()に含まれるようになったようなので、esp_nimble_hci_and_controller_init()は不要。よって、334行目を削除


## main/gpio_func.c の変更点


### 110行目 gpio_pad_select_gpio()がない

8行目 driver/gpio.h のインクルード文の前で rom/gpio.h をインクルードする。
```#include <rom/gpio.h>```


## main/main.c　の変更点

### 87行目 unsigned longを%dや%08Xでstring化しようとしている

%ldや%08lXにする


### 105行目 unsigned longを%dでstring化しようとしている

%luにする


