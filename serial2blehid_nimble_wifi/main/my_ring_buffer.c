#include <my_ring_buffer.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/**
 * @brief リングバッファを指定サイズで初期化する
 */
bool my_ring_buffer_init(my_ring_buffer_t *rb, int size) {
    free(rb->buffer);
    rb->buffer = (uint8_t *)malloc(sizeof(uint8_t) * size);
    if (rb->buffer == NULL) return false;
    rb->size = size;
    rb->head = 0;
    rb->tail = 0;
    rb->is_full = false;
    return true;
}

void my_ring_buffer_deinit(my_ring_buffer_t *rb) { free(rb->buffer); }

/**
 * @brief リングバッファ内のデータ数を返す
 */
int my_ring_buffer_content_length(my_ring_buffer_t *rb) {
    if (rb->is_full) {
        return rb->size;
    } else {
        if (rb->head < rb->tail) {
            return rb->size + rb->head - rb->tail;
        } else {
            return rb->head - rb->tail;
        }
    }
}

/**
 * @brief リングバッファの状態をprintfで表す
 */
void my_ring_buffer_status(my_ring_buffer_t *rb) {
    printf("full : %d\n", rb->is_full);
    printf("head : %d\n", rb->head);
    printf("tail : %d\n", rb->tail);
    printf("buffer :");
    for (int i = 0; i < rb->size; i++) {
        printf(" %c", rb->buffer[i]);
    }
    printf("\n");
    printf("len : %d\n", my_ring_buffer_content_length(rb));
}

/**
 * @brief リングバッファにデータを1つ格納。リングバッファは更新される。
 * @param data 格納したいデータ
 */
void my_ring_buffer_push(my_ring_buffer_t *rb, uint8_t data) {
    rb->buffer[rb->head] = data;
    rb->head = (rb->head + 1) % rb->size;
    if (rb->is_full) rb->tail = rb->head;
    rb->is_full = (rb->head == rb->tail);
}

/**
 * @brief
 * リングバッファからFIFOでデータを1つ取り出し。リングバッファは更新される。
 * @param *data 取得したデータが格納される
 * @return 失敗（空っぽ）のときはfalse
 */
bool my_ring_buffer_pop(my_ring_buffer_t *rb, uint8_t *data) {
    if (!rb->is_full && rb->head == rb->tail) {
        return false;  // empty
    }
    *data = rb->buffer[rb->tail];
    rb->tail = (rb->tail + 1) % rb->size;
    rb->is_full = false;
    return true;
}

/**
 * @brief
 * リングバッファの指定位置のデータのコピーを取得。リングバッファは変更されない。
 * @param offset 0:先頭 1:先頭の1つ次の位置 -1:末尾 -2:末尾の1個前の位置
 * @param *data 取得したデータが格納される
 * @return 失敗（空っぽ）のときはfalse
 */
bool my_ring_buffer_at(my_ring_buffer_t *rb, int offset, uint8_t *data) {
    int len = my_ring_buffer_content_length(rb);
    if (len == 0) {
        return false;  // empty
    }
    int index;
    if (offset >= 0) {
        if (offset >= len) offset = len - 1;
        index = (rb->tail + offset) % rb->size;
    } else {
        if (offset < (-1 * len)) offset = -1 * len;
        index = (rb->head + offset + rb->size) % rb->size;
    }
    *data = rb->buffer[index];
    return true;
}

/**
 * @brief リングバッファの中身を空にする。
 *        実際は、始点と終点をゼロ位置にし、満杯フラグをfalseにする。
 */
void my_ring_buffer_reset(my_ring_buffer_t *rb) {
    rb->head = 0;
    rb->tail = 0;
    rb->is_full = false;
}

