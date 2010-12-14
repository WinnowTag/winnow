// Copyright (c) 2007-2010 The Kaphan Foundation
// 
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
// 
// The above copyright notice and this permission notice shall be included in
// all copies or substantial portions of the Software.
// 
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
// THE SOFTWARE.

// contact@winnowtag.org


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
