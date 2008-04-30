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
#include <time.h>
#include "curl_response.h"
#include "logging.h"

#define URL_OK 0
#define URL_FAIL 1

/** Handles fetching URLs using curl.
 *
 *  Should support file and http at least.
 */
static int fetch_url(const char * url, time_t if_modified_since, char ** data, char ** error) {
  debug("fetching %s", url);
  int rc;
  char curlerr[512];
  struct RESPONSE response;
  response.size = 0;
  response.data = NULL;
  
  struct curl_slist *http_headers = NULL;
  http_headers = curl_slist_append(http_headers, "Accept: application/atom+xml");
  
  CURL *curl = curl_easy_init();
  curl_easy_setopt(curl, CURLOPT_URL, url);
  
  curl_easy_setopt(curl, CURLOPT_HTTP_VERSION, CURL_HTTP_VERSION_1_1);
  curl_easy_setopt(curl, CURLOPT_ERRORBUFFER, curlerr);
  curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);
  curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_response);
  curl_easy_setopt(curl, CURLOPT_HTTPHEADER, http_headers);
  
  if (if_modified_since > 0) {
    curl_easy_setopt(curl, CURLOPT_TIMEVALUE, if_modified_since);
    curl_easy_setopt(curl, CURLOPT_TIMECONDITION, CURL_TIMECOND_IFMODSINCE);
  }

  if (curl_easy_perform(curl)) {
    error("URL %s not accessible: %s", url, curlerr);
    response.data = NULL;
    rc = URL_FAIL;
  } else {
    *data = response.data;
    rc = URL_OK;
  }
  
  curl_slist_free_all(http_headers);
  curl_easy_cleanup(curl);

  debug("fetching complete");
  return rc;
}

#endif /* _FETCH_URL_H_ */
