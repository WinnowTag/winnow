/* Copyright (c) 2007 The Kaphan Foundation
 *
 * Possession of a copy of this file grants no permission or license
 * to use, modify, or create derivate works.
 *
 * Please contact info@peerworks.org for further information.
 */

#include <stdlib.h>
#include "buffer.h"

Buffer * new_buffer(int size) {
  Buffer *b = malloc(sizeof(struct BUFFER));
  b->buf = calloc(size, sizeof(char));
  b->capacity = size;
  b->length = 0;
  return b;
}

void buffer_in(Buffer *b, const char * data, int in_size) {
  if (b->capacity - b->length <= in_size) {
    // TODO handle realloc failure?
    b->buf = realloc(b->buf, b->capacity + (2 * in_size));
    b->capacity += 2*in_size;
  }
  
  memcpy((b->buf) + (b->length), data, in_size);
  b->length += in_size;
}

void free_buffer(Buffer *b) {
  if (b) {
    free(b->buf);
    free(b);
  }
}