#ifndef my_hid_key_map_h
#define my_hid_key_map_h 1


#include <stdint.h>

/**
 * @brief charcode to hid_key & hide_modifier
 *        index is char-code.
 *        char-code 0 to 8, 11, 12, 14 to 31, 127 is no use.
 *        modifier=0 means no modifier, =2 means Shift-Key.
 *
 *        usage:
 *           char c = 'A';
 *           if (c < 128) {
 *             uint8_t k = keycode_hid_map[c][0];  // --> 4
 *             key_mask_t m = (key_mask_t)keycode_hid_map[c][1];  // --> 2
 *             if (k != 0) {
 *               esp_hidd_send_keyboard_value(hid_conn_id, m, &k, 1);
 *               esp_hidd_send_keyboard_value(hid_conn_id, m, &k, 0);
 *             }
 *           }
 *
 *           char str[] = "A12#";
 *           int len = 4;
 *           for (int i = 0; i < len, i ++) {
 *             uint8_t c = str[i];
 *             if (c < 128) {
 *               uint8_t k = keycode_hid_map[c][0];
 *               key_mask_t m = (key_mask_t)keycode_hid_map[c][1];
 *               if (k != 0) {
 *                 esp_hidd_send_keyboard_value(hid_conn_id, m, &k, 1);
 *                 esp_hidd_send_keyboard_value(hid_conn_id, m, &k, 0);
 *               }
 *             }
 *           }
 *
 */
const uint8_t keycode_hid_map[128][2] = {
    {0, 0},   // CharCode[0] <NO-EVENT>
    {0, 0},   // CharCode[1] <DUMMY>
    {0, 0},   // CharCode[2] <DUMMY>
    {0, 0},   // CharCode[3] <DUMMY>
    {0, 0},   // CharCode[4] <DUMMY>
    {0, 0},   // CharCode[5] <DUMMY>
    {0, 0},   // CharCode[6] <DUMMY>
    {0, 0},   // CharCode[7] <DUMMY>
    {0, 0},   // CharCode[8] <DUMMY>
    {43, 0},  // CharCode[9] <TAB>
    {40, 0},  // CharCode[10] <LF> to <RETURN>
    {0, 0},   // CharCode[11] <DUMMY>
    {0, 0},   // CharCode[12] <DUMMY>
    {40, 0},  // CharCode[13] <CR> to <RETURN>
    {0, 0},   // CharCode[14] <DUMMY>
    {0, 0},   // CharCode[15] <DUMMY>
    {0, 0},   // CharCode[16] <DUMMY>
    {0, 0},   // CharCode[17] <DUMMY>
    {0, 0},   // CharCode[18] <DUMMY>
    {0, 0},   // CharCode[19] <DUMMY>
    {0, 0},   // CharCode[20] <DUMMY>
    {0, 0},   // CharCode[21] <DUMMY>
    {0, 0},   // CharCode[22] <DUMMY>
    {0, 0},   // CharCode[23] <DUMMY>
    {0, 0},   // CharCode[24] <DUMMY>
    {0, 0},   // CharCode[25] <DUMMY>
    {0, 0},   // CharCode[26] <DUMMY>
    {0, 0},   // CharCode[27] <DUMMY>
    {0, 0},   // CharCode[28] <DUMMY>
    {0, 0},   // CharCode[29] <DUMMY>
    {0, 0},   // CharCode[30] <DUMMY>
    {0, 0},   // CharCode[31] <DUMMY>
    {44, 0},  // CharCode[32] <SPACE>
    {30, 2},  // CharCode[33] <!>
    {52, 2},  // CharCode[34] <">
    {32, 2},  // CharCode[35] <#>
    {33, 2},  // CharCode[36] <$>
    {34, 2},  // CharCode[37] <%>
    {36, 2},  // CharCode[38] <&>
    {52, 0},  // CharCode[39] <'>
    {38, 2},  // CharCode[40] <(>
    {39, 2},  // CharCode[41] <)>
    {37, 2},  // CharCode[42] <*>
    {46, 0},  // CharCode[43] <+>
    {54, 0},  // CharCode[44] <,>
    {45, 0},  // CharCode[45] <->
    {55, 0},  // CharCode[46] <.>
    {56, 0},  // CharCode[47] </>
    {39, 0},  // CharCode[48] <0>
    {30, 0},  // CharCode[49] <1>
    {31, 0},  // CharCode[50] <2>
    {32, 0},  // CharCode[51] <3>
    {33, 0},  // CharCode[52] <4>
    {34, 0},  // CharCode[53] <5>
    {35, 0},  // CharCode[54] <6>
    {36, 0},  // CharCode[55] <7>
    {37, 0},  // CharCode[56] <8>
    {38, 0},  // CharCode[57] <9>
    {51, 2},  // CharCode[58] <:>
    {51, 0},  // CharCode[59] <;>
    {54, 2},  // CharCode[60] <<>
    {46, 2},  // CharCode[61] <=>
    {55, 2},  // CharCode[62] <>>
    {56, 2},  // CharCode[63] <?>
    {31, 2},  // CharCode[64] <@>
    {4, 2},   // CharCode[65] <A>
    {5, 2},   // CharCode[66] <B>
    {6, 2},   // CharCode[67] <C>
    {7, 2},   // CharCode[68] <D>
    {8, 2},   // CharCode[69] <E>
    {9, 2},   // CharCode[70] <F>
    {10, 2},  // CharCode[71] <G>
    {11, 2},  // CharCode[72] <H>
    {12, 2},  // CharCode[73] <I>
    {13, 2},  // CharCode[74] <J>
    {14, 2},  // CharCode[75] <K>
    {15, 2},  // CharCode[76] <L>
    {16, 2},  // CharCode[77] <M>
    {17, 2},  // CharCode[78] <N>
    {18, 2},  // CharCode[79] <O>
    {19, 2},  // CharCode[80] <P>
    {20, 2},  // CharCode[81] <Q>
    {21, 2},  // CharCode[82] <R>
    {22, 2},  // CharCode[83] <S>
    {23, 2},  // CharCode[84] <T>
    {24, 2},  // CharCode[85] <U>
    {25, 2},  // CharCode[86] <V>
    {26, 2},  // CharCode[87] <W>
    {27, 2},  // CharCode[88] <X>
    {28, 2},  // CharCode[89] <Y>
    {29, 2},  // CharCode[90] <Z>
    {47, 0},  // CharCode[91] <[>
    {49, 0},  // CharCode[92] <\>
    {48, 0},  // CharCode[93] <]>
    {35, 2},  // CharCode[94] <^>
    {45, 2},  // CharCode[95] <_>
    {53, 0},  // CharCode[96] <`>
    {4, 0},   // CharCode[97] <a>
    {5, 0},   // CharCode[98] <b>
    {6, 0},   // CharCode[99] <c>
    {7, 0},   // CharCode[100] <d>
    {8, 0},   // CharCode[101] <e>
    {9, 0},   // CharCode[102] <f>
    {10, 0},  // CharCode[103] <g>
    {11, 0},  // CharCode[104] <h>
    {12, 0},  // CharCode[105] <i>
    {13, 0},  // CharCode[106] <j>
    {14, 0},  // CharCode[107] <k>
    {15, 0},  // CharCode[108] <l>
    {16, 0},  // CharCode[109] <m>
    {17, 0},  // CharCode[110] <n>
    {18, 0},  // CharCode[111] <o>
    {19, 0},  // CharCode[112] <p>
    {20, 0},  // CharCode[113] <q>
    {21, 0},  // CharCode[114] <r>
    {22, 0},  // CharCode[115] <s>
    {23, 0},  // CharCode[116] <t>
    {24, 0},  // CharCode[117] <u>
    {25, 0},  // CharCode[118] <v>
    {26, 0},  // CharCode[119] <w>
    {27, 0},  // CharCode[120] <x>
    {28, 0},  // CharCode[121] <y>
    {29, 0},  // CharCode[122] <z>
    {47, 2},  // CharCode[123] <{>
    {49, 2},  // CharCode[124] <|>
    {48, 2},  // CharCode[125] <}>
    {53, 2},  // CharCode[126] <~>
    {0, 0}    // CharCode[127] <DUMMY>
};

#endif

