// General info: http://doc.winnowtag.org/open-source
// Source code repository: http://github.com/winnowtag
// Questions and feedback: contact@winnowtag.org
//
// Copyright (c) 2007-2011 The Kaphan Foundation
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

#ifndef _FETCH_URL_H_
#define _FETCH_URL_H_

#include <curl/curl.h>
#include <time.h>
#include "curl_response.h"
#include "logging.h"
#include "hmac_sign.h"
#include <libxml/uri.h>

#define URL_OK 0
#define URL_FAIL 1

/** Handles fetching URLs using curl.
 *
 *  Should support file and http at least.
 */
static int fetch_url(const char * url, time_t if_modified_since, const Credentials * credentials, char ** data, char ** errmsg) {
  info("fetching %s", url);
  int rc;
  char curlerr[CURL_ERROR_SIZE];
  struct RESPONSE response;
  response.size = 0;
  response.data = NULL;
  
  char * path = "";
  xmlURIPtr uri = xmlParseURIRaw(url, 1);
  if (uri) {
    path = uri->path;
  }
  
  struct curl_slist *http_headers = NULL;
  http_headers = curl_slist_append(http_headers, "Accept: application/atom+xml");
  
  if (valid_credentials(credentials)) {
    debug("Signing request with %s:XXXXX", credentials->access_id);
    http_headers = hmac_sign("GET", path, http_headers, credentials);
  }
  
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
    if (errmsg) {
      *errmsg = strdup(curlerr);
    }
  } else {
    *data = response.data;
    rc = URL_OK;
  }
  
  curl_slist_free_all(http_headers);
  curl_easy_cleanup(curl);
  if (uri) {
    xmlFreeURI(uri);
  }

  debug("fetching complete");
  return rc;
}

#endif /* _FETCH_URL_H_ */
