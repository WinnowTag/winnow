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

struct buffer {
  char *buf;
  int capacity;
  int length;
};

static struct buffer * new_buffer(int size) {
  struct buffer *b = malloc(sizeof(struct buffer));
  b->buf = calloc(size, sizeof(char));
  b->capacity = size;
  b->length = 0;
  return b;
}

static void buffer_in(struct buffer *b, const char * data, int in_size) {
  if (b->capacity - b->length <= in_size) {
    b->buf = realloc(b->buf, b->capacity + (2 * in_size));
    b->capacity += 2*in_size;
  }
  
  memcpy((b->buf) + (b->length), data, in_size);
  b->length += in_size;
}

static int prefix_of(const char * prefix, const char *string, int prefix_size) {
  int length = strlen(string);
  int i;
  int is_prefix = 0;
  
  if (prefix_size <= length) {
    is_prefix = 1;
    
    for (i = 0; i < length && i < prefix_size; i++) {
      if (string[i] != prefix[i]) {
        is_prefix = 0;
        break;
      }
    }
  }
  
  return is_prefix;
}

static char * get_header(const struct curl_slist *header, const char * prefix, int prefix_size) {  
  char * return_header = NULL;
  
  for (; header; header = header->next) {
    if (prefix_of(prefix, header->data, prefix_size)) {
      return_header = header->data;
      break;
    }
  }
  
  return return_header;
}

static void append_header(struct buffer *canon_s, const struct curl_slist *headers, const char * header) {
  int header_key_length = strlen(header);
  char *found_header = get_header(headers, header, header_key_length);
  
  if (found_header) {
    char *header_value = &found_header[header_key_length + 1];
    buffer_in(canon_s, header_value, strlen(header_value));
  }
  buffer_in(canon_s, "\n", 1);
}

char * canonical_string(const char * method, const char * path, const struct curl_slist *headers) {
  // TOOD handle this
  struct buffer *canon_s = new_buffer(256);
  buffer_in(canon_s, method, strlen(method));
  buffer_in(canon_s, "\n", 1);
  
  append_header(canon_s, headers, "Content-Type:");
  append_header(canon_s, headers, "Content-MD5:");
  append_header(canon_s, headers, "Date:");
  
  buffer_in(canon_s, path, strlen(path));
  
  buffer_in(canon_s, "\0", 1);
  char * return_string = canon_s->buf;
  free(canon_s);
  return return_string;
}