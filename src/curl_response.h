/* Copyright (c) 2007 The Kaphan Foundation
 *
 * Possession of a copy of this file grants no permission or license
 * to use, modify, or create derivate works.
 *
 * Please contact info@peerworks.org for further information.
 */


#ifndef _CURL_RESPONSE_H_
#define _CURL_RESPONSE_H_

struct RESPONSE {
  int size;
  char *data;
};

static size_t write_response(void *ptr, size_t size, size_t nmemb, void *stream) {
  struct RESPONSE *response = (struct RESPONSE*) stream;
  
  if (response->size == 0) {
    response->size = size * nmemb;
    response->data = calloc(size + 1, nmemb);
    memcpy(response->data, ptr, size * nmemb);    
  } else {
    response->data = realloc(response->data, response->size + (size * nmemb) + 1);
    memcpy(&response->data[response->size], ptr, size * nmemb);
    response->size += (size * nmemb);
    response->data[response->size] = '\0';
  }
  
  return size * nmemb;
}

#endif /* _CURL_RESPONSE_H_ */
