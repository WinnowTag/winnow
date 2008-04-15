/* Copyright (c) 2007 The Kaphan Foundation
 *
 * Possession of a copy of this file grants no permission or license
 * to use, modify, or create derivate works.
 *
 * Please contact info@peerworks.org for further information.
 */

#ifndef _FETCH_URL_H_
#define _FETCH_URL_H_

#include <curl/curl.h>
#include "curl_response.h"
#include "logging.h"

#define URL_OK 0
#define URL_FAIL 1

/** Handles fetching URLs using curl.
 *
 *  Should support file and http at least.
 */
static int fetch_url(const char * url, char ** data) {
  int rc;
  char curlerr[512];
  struct RESPONSE response;
  response.size = 0;
  response.data = NULL;
  
  CURL *curl = curl_easy_init();
  curl_easy_setopt(curl, CURLOPT_URL, url);
  
  curl_easy_setopt(curl, CURLOPT_HTTP_VERSION, CURL_HTTP_VERSION_1_1);
  curl_easy_setopt(curl, CURLOPT_ERRORBUFFER, curlerr);
  curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);
  curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_response);

  if (curl_easy_perform(curl)) {
    error("URL %s not accessible: %s", url, curlerr);
    response.data = NULL;
    rc = URL_FAIL;
  } else {
    *data = response.data;
    rc = URL_OK;
  }
  
  curl_easy_cleanup(curl);

  return rc;
}

#endif /* _FETCH_URL_H_ */
