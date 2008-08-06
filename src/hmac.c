/* Copyright (c) 2007 The Kaphan Foundation
 *
 * Possession of a copy of this file grants no permission or license
 * to use, modify, or create derivate works.
 *
 * Please contact info@peerworks.org for further information.
 */

#include <string.h>
#include <stdlib.h>
#include "logging.h"
#include "hmac_internal.h"

typedef struct buffer {
  char *buf;
  int capacity, length;
};

static struct buffer * new_buffer(int size) {
  struct buffer *b = malloc(sizeof(struct buffer));
  b->buf = calloc(size, sizeof(char));
  b->capacity = size;
  b->length = 0;
  return b;
}

static void buffer_in(struct buffer *b, char * data, int in_size) {
  if (b->capacity - b->length <= in_size) {
    b->buf = realloc(b->buf, b->capacity + (2 * in_size));
    b->capacity += 2*in_size;
  }
  
  memcpy((b->buf) + (b->length), data, in_size);
  b->length += in_size;
}

char * canonical_string(const char * method, const char * path, const struct curl_slist *headers) {
  // TOOD handle this
  struct buffer *canon_s = new_buffer(256);
  buffer_in(canon_s, method, strlen(method));
  buffer_in(canon_s, "\n", 1);
  
  
  buffer_in(canon_s, path, strlen(path));
  
  buffer_in(canon_s, "\0", 1);
  char * return_string = canon_s->buf;
  free(canon_s);
  return return_string;
}