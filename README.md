# serial2blehid_esp_idf
ESP32-XXがUART経由で受信した文字を、キーボードデバイスとしてBLE-HID経由で送信する
開発環境は ESP-IDF v5.3.2。

## serial2blehid_bluedroid_tc101a

`examples/bluetooth/bluedroid/ble/ble_hid_device_demo` を元に、日本語キーボード配列を追加し、`TC-101A` という装置とUART通信し、得られた値を送信するもの


## serial2blehid_nimble_idf532

`OlegOS76/nimble_kbdhid_example` を esp-idf v5.3.2でビルドできるようにしたもの


## serial2blehid_nimble_button

さらに、日本語キーボード配列を追加し、`Boot` ボタンを押すことで、文字コード 0x20 から 0x7e までを1文字ずつ順に送信するもの

