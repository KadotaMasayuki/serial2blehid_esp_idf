#ifndef my_ring_buffer_h
#define my_ring_buffer_h 1

#include <stdint.h>
#include <stdbool.h>

/**
 * @brief リングバッファ型の定義
 */
typedef struct {
  uint8_t *buffer;
  int size;
  int head;
  int tail;
  bool is_full;
} my_ring_buffer_t;

bool my_ring_buffer_init(my_ring_buffer_t *rb, int size);
void my_ring_buffer_deinit(my_ring_buffer_t *rb);
int my_ring_buffer_content_length(my_ring_buffer_t *rb);
void my_ring_buffer_status(my_ring_buffer_t *rb);
void my_ring_buffer_push(my_ring_buffer_t *rb, uint8_t data);
bool my_ring_buffer_pop(my_ring_buffer_t *rb, uint8_t *data);
bool my_ring_buffer_at(my_ring_buffer_t *rb, int offset, uint8_t *data);
void my_ring_buffer_reset(my_ring_buffer_t *rb);

#endif

