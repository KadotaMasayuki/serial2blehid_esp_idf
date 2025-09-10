#ifndef my_hid_key_map_h
#define my_hid_key_map_h 1

//
// for JAPANESE KEYBOARD LAYOUT
//


#include <stdint.h>
#include "hid_codes.h"

/**
 * @brief charcode to hid_usage_id & hid_modifier
 *        index is char-code.
 *        char-code 0 to 8, 11, 12, 14 to 31, 127 is no use.
 *        modifier=0 means no modifier, =2 means Shift-Key.
 *
 *        usage:
 *           uint8_t keyboard_buffer[6];
 *           char c = 'A';
 *           if (0 <= c && c < 128) {
 *             uint8_t k = keycode_hid_map[c][0];  // --> 4
 *             uint8_t m = keycode_hid_map[c][1];  // --> 2
 *             keyboard_buffer[0] = m;
 *             keyboard_buffer[2] = k;
 *             if (k != 0) {
 *               SOME_REPORT_SENDER(keyboard_buffer);  // press
 *               SOME_WAIT_50ms();
 *               keyboard_buffer[0] = 0;
 *               keyboard_buffer[2] = 0;
 *               SOME_REPORT_SENDER(keyboard_buffer);  // release
 *             }
 *           }
 *
 *           char str[] = "A12#";
 *           int len = 4;
 *           for (int i = 0; i < len, i ++) {
 *             uint8_t c = str[i];
 *             if (0 <= c && c < 128) {
 *               uint8_t k = keycode_hid_map[c][0];
 *               uint8_t m = keycode_hid_map[c][1];
 *               keyboard_buffer[0] = m;
 *               keyboard_buffer[2] = k;
 *               if (k != 0) {
 *                 SOME_REPORT_SENDER(keyboard_buffer);  // press
 *                 SOME_WAIT_50ms();
 *                 keyboard_buffer[0] = 0;
 *                 keyboard_buffer[2] = 0;
 *                 SOME_REPORT_SENDER(keyboard_buffer);  // release
 *               }
 *             }
 *           }
 *
 */
const uint8_t keycode_hid_map[128][2] = {
    {HID_KEY_NONE, 0},   // CharCode[0] <NO-EVENT>
    {HID_KEY_NONE, 0},   // CharCode[1] <DUMMY>
    {HID_KEY_NONE, 0},   // CharCode[2] <DUMMY>
    {HID_KEY_NONE, 0},   // CharCode[3] <DUMMY>
    {HID_KEY_NONE, 0},   // CharCode[4] <DUMMY>
    {HID_KEY_NONE, 0},   // CharCode[5] <DUMMY>
    {HID_KEY_NONE, 0},   // CharCode[6] <DUMMY>
    {HID_KEY_NONE, 0},   // CharCode[7] <DUMMY>
    {HID_KEY_NONE, 0},   // CharCode[8] <DUMMY>
    {HID_KEY_TAB, 0},  // CharCode[9] <TAB>
    {HID_KEY_RETURN, 0},  // CharCode[10] <LF> to <RETURN>
    {HID_KEY_NONE, 0},   // CharCode[11] <DUMMY>
    {HID_KEY_NONE, 0},   // CharCode[12] <DUMMY>
    {HID_KEY_RETURN, 0},  // CharCode[13] <CR> to <RETURN>
    {HID_KEY_NONE, 0},   // CharCode[14] <DUMMY>
    {HID_KEY_NONE, 0},   // CharCode[15] <DUMMY>
    {HID_KEY_NONE, 0},   // CharCode[16] <DUMMY>
    {HID_KEY_NONE, 0},   // CharCode[17] <DUMMY>
    {HID_KEY_NONE, 0},   // CharCode[18] <DUMMY>
    {HID_KEY_NONE, 0},   // CharCode[19] <DUMMY>
    {HID_KEY_NONE, 0},   // CharCode[20] <DUMMY>
    {HID_KEY_NONE, 0},   // CharCode[21] <DUMMY>
    {HID_KEY_NONE, 0},   // CharCode[22] <DUMMY>
    {HID_KEY_NONE, 0},   // CharCode[23] <DUMMY>
    {HID_KEY_NONE, 0},   // CharCode[24] <DUMMY>
    {HID_KEY_NONE, 0},   // CharCode[25] <DUMMY>
    {HID_KEY_NONE, 0},   // CharCode[26] <DUMMY>
    {HID_KEY_NONE, 0},   // CharCode[27] <DUMMY>
    {HID_KEY_NONE, 0},   // CharCode[28] <DUMMY>
    {HID_KEY_NONE, 0},   // CharCode[29] <DUMMY>
    {HID_KEY_NONE, 0},   // CharCode[30] <DUMMY>
    {HID_KEY_NONE, 0},   // CharCode[31] <DUMMY>
    {HID_KEY_SPACE, 0},  // CharCode[32] <SPACE>
    {HID_KEY_1, 2},  // CharCode[33] <!>
    {HID_KEY_2, 2},  // CharCode[34] <">
    {HID_KEY_3, 2},  // CharCode[35] <#>
    {HID_KEY_4, 2},  // CharCode[36] <$>
    {HID_KEY_5, 2},  // CharCode[37] <%>
    {HID_KEY_6, 2},  // CharCode[38] <&>
    {HID_KEY_7, 2},  // CharCode[39] <'>
    {HID_KEY_8, 2},  // CharCode[40] <(>
    {HID_KEY_9, 2},  // CharCode[41] <)>
    {HID_KEY_SGL_QUOTE, 2},  // CharCode[42] <*>
    {HID_KEY_SEMI_COLON, 2},  // CharCode[43] <+>
    {HID_KEY_COMMA, 0},  // CharCode[44] <,>
    {HID_KEY_MINUS, 0},  // CharCode[45] <->
    {HID_KEY_DOT, 0},  // CharCode[46] <.>
    {HID_KEY_FWD_SLASH, 0},  // CharCode[47] </>
    {HID_KEY_0, 0},  // CharCode[48] <0>
    {HID_KEY_1, 0},  // CharCode[49] <1>
    {HID_KEY_2, 0},  // CharCode[50] <2>
    {HID_KEY_3, 0},  // CharCode[51] <3>
    {HID_KEY_4, 0},  // CharCode[52] <4>
    {HID_KEY_5, 0},  // CharCode[53] <5>
    {HID_KEY_6, 0},  // CharCode[54] <6>
    {HID_KEY_7, 0},  // CharCode[55] <7>
    {HID_KEY_8, 0},  // CharCode[56] <8>
    {HID_KEY_9, 0},  // CharCode[57] <9>
    {HID_KEY_SGL_QUOTE, 0},  // CharCode[58] <:>
    {HID_KEY_SEMI_COLON, 0},  // CharCode[59] <;>
    {HID_KEY_COMMA, 2},  // CharCode[60] <<>
    {HID_KEY_MINUS, 2},  // CharCode[61] <=>
    {HID_KEY_DOT, 2},  // CharCode[62] <>>
    {HID_KEY_FWD_SLASH, 2},  // CharCode[63] <?>
    {HID_KEY_LEFT_BRKT, 0},  // CharCode[64] <@>
    {HID_KEY_A, 2},   // CharCode[65] <A>
    {HID_KEY_B, 2},   // CharCode[66] <B>
    {HID_KEY_C, 2},   // CharCode[67] <C>
    {HID_KEY_D, 2},   // CharCode[68] <D>
    {HID_KEY_E, 2},   // CharCode[69] <E>
    {HID_KEY_F, 2},   // CharCode[70] <F>
    {HID_KEY_G, 2},  // CharCode[71] <G>
    {HID_KEY_H, 2},  // CharCode[72] <H>
    {HID_KEY_I, 2},  // CharCode[73] <I>
    {HID_KEY_J, 2},  // CharCode[74] <J>
    {HID_KEY_K, 2},  // CharCode[75] <K>
    {HID_KEY_L, 2},  // CharCode[76] <L>
    {HID_KEY_M, 2},  // CharCode[77] <M>
    {HID_KEY_N, 2},  // CharCode[78] <N>
    {HID_KEY_O, 2},  // CharCode[79] <O>
    {HID_KEY_P, 2},  // CharCode[80] <P>
    {HID_KEY_Q, 2},  // CharCode[81] <Q>
    {HID_KEY_R, 2},  // CharCode[82] <R>
    {HID_KEY_S, 2},  // CharCode[83] <S>
    {HID_KEY_T, 2},  // CharCode[84] <T>
    {HID_KEY_U, 2},  // CharCode[85] <U>
    {HID_KEY_V, 2},  // CharCode[86] <V>
    {HID_KEY_W, 2},  // CharCode[87] <W>
    {HID_KEY_X, 2},  // CharCode[88] <X>
    {HID_KEY_Y, 2},  // CharCode[89] <Y>
    {HID_KEY_Z, 2},  // CharCode[90] <Z>
    {HID_KEY_RIGHT_BRKT, 0},  // CharCode[91] <[>
    {HID_KEY_INTERNATIONAL3, 0},  // CharCode[92] <\>
    {HID_KEY_BACK_SLASH, 0},  // CharCode[93] <]>
    {HID_KEY_EQUAL, 0},  // CharCode[94] <^>
    {HID_KEY_INTERNATIONAL1, 2},  // CharCode[95] <_>
    {HID_KEY_LEFT_BRKT, 2},  // CharCode[96] <`>
    {HID_KEY_A, 0},   // CharCode[97] <a>
    {HID_KEY_B, 0},   // CharCode[98] <b>
    {HID_KEY_C, 0},   // CharCode[99] <c>
    {HID_KEY_D, 0},   // CharCode[100] <d>
    {HID_KEY_E, 0},   // CharCode[101] <e>
    {HID_KEY_F, 0},   // CharCode[102] <f>
    {HID_KEY_G, 0},  // CharCode[103] <g>
    {HID_KEY_H, 0},  // CharCode[104] <h>
    {HID_KEY_I, 0},  // CharCode[105] <i>
    {HID_KEY_J, 0},  // CharCode[106] <j>
    {HID_KEY_K, 0},  // CharCode[107] <k>
    {HID_KEY_L, 0},  // CharCode[108] <l>
    {HID_KEY_M, 0},  // CharCode[109] <m>
    {HID_KEY_N, 0},  // CharCode[110] <n>
    {HID_KEY_O, 0},  // CharCode[111] <o>
    {HID_KEY_P, 0},  // CharCode[112] <p>
    {HID_KEY_Q, 0},  // CharCode[113] <q>
    {HID_KEY_R, 0},  // CharCode[114] <r>
    {HID_KEY_S, 0},  // CharCode[115] <s>
    {HID_KEY_T, 0},  // CharCode[116] <t>
    {HID_KEY_U, 0},  // CharCode[117] <u>
    {HID_KEY_V, 0},  // CharCode[118] <v>
    {HID_KEY_W, 0},  // CharCode[119] <w>
    {HID_KEY_X, 0},  // CharCode[120] <x>
    {HID_KEY_Y, 0},  // CharCode[121] <y>
    {HID_KEY_Z, 0},  // CharCode[122] <z>
    {HID_KEY_RIGHT_BRKT, 2},  // CharCode[123] <{>
    {HID_KEY_INTERNATIONAL3, 2},  // CharCode[124] <|>
    {HID_KEY_BACK_SLASH, 2},  // CharCode[125] <}>
    {HID_KEY_EQUAL, 2},  // CharCode[126] <~>
    {HID_KEY_NONE, 0}    // CharCode[127] <DUMMY>
};

#endif

