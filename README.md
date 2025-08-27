# serial2blehid_esp_idf

## 何ですか

ESP32-XXがUART経由で受信した文字を、キーボードデバイスとしてBLE-HID経由で送信するもの。
開発環境は `ESP-IDF v5.3.2` 。
主に使用している基板は `Seeed Studio XIAO ESP32C3` 。


## serial2blehid_bluedroid_tc101a

Bluedroid版。
`examples/bluetooth/bluedroid/ble/ble_hid_device_demo` を元に、日本語キーボード配列を追加し、`TC-101A` という装置とUART通信し、得られた値をBLE-HID送信するもの。繋げば使える。


## serial2blehid_nimble_idf532

NimBLE版。
`OlegOS76/nimble_kbdhid_example` が esp-idf v4.1用のため、これを esp-idf v5.3.2でビルドできるようにしたもの。
挙動はオリジナルと同じ。


## serial2blehid_nimble_button

NimBLE版。
さらに、日本語キーボード配列を追加し、基板の `Boot` ボタンを押すことで、文字コード 0x20 から 0x7e までを1文字ずつ順にBLE-HID送信するもの。

