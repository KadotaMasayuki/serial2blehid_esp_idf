#ifndef my_hid_key_map_h
#define my_hid_key_map_h 1

#include <stdint.h>

extern uint8_t keycode_hid_map[128][2];

/**
 * @brief 文字コードを与え、HIDコードを返す。
 *        インライン関数にするほどでもないのでマクロにしてある。
 *        例： uint8_t c = KEYCODE_TO_HIDCODE(0x21);  // 0x21 = '!'
 *        cは30となる。
 * @param キーコード。たとえば'!'なら0x21。
 * @return HIDコード。キーコード'!'を与えられた場合、HIDコードは0x21。
 *         変換表に無いキーコードが指定されたときはゼロを返す。
 */
#define KEYCODE_TO_HIDCODE(C) ((((C) < 0) || ((C) >= 128)) ? (uint8_t)0 : (uint8_t)keycode_hid_map[(C)][0])

/**
 * @brief 文字コードを与え、HIDマスクを返す。
 *        インライン関数にするほどでもないのでマクロにしてある。
 *        例： key_mask_t m = KEYCODE_TO_HIDMASK(0x21);  // 0x21 = '!'
 *        mは2となる。
 * @param キーコード。たとえば'!'なら0x21。
 * @return HIDマスク。キーコード'!'を与えられた場合、マスクは2。これはシフトキーを示す。
 *         変換表に無いキーコードが指定されたときはゼロを返す。
 */
#define KEYCODE_TO_HIDMASK(C) ((((C) < 0) || ((C) >= 128)) ? (key_mask_t)0 : (key_mask_t)keycode_hid_map[(C)][1])


#endif

