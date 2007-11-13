/* Copyright (c) 2007 The Kaphan Foundation
 *
 * Possession of a copy of this file grants no permission or license
 * to use, modify, or create derivate works.
 *
 * Please contact info@peerworks.org for further information.
 */

#include <stdlib.h>
#include <check.h>
#include <config.h>
#include <stdio.h>
#include "../src/cls_config.h"
#include "../src/classification_engine.h"
#include "../src/httpd.h"
#include "assertions.h"

#define PORT 8008

Config *config;
ClassificationEngine *ce;
Httpd *httpd;
static FILE *test_data;

static void setup_httpd() {
  config = load_config("fixtures/real-db.conf");
  ce = create_classification_engine(config);  
  httpd = httpd_start(config, ce);
  test_data = fopen("http_test_data.log", "a");
}

static void teardown_httpd() {
  httpd_stop(httpd);
  ce_stop(ce);
  free_classification_engine(ce);
  free_config(config);
  fclose(test_data);
}

#ifdef HAVE_LIBCURL
#include <curl/curl.h>

START_TEST(test_http_initialization) {
  int code;
  char curlerr[CURL_ERROR_SIZE];
  CURL *curl = curl_easy_init();
  curl_easy_setopt(curl, CURLOPT_URL, "http://localhost:8008/");
  curl_easy_setopt(curl, CURLOPT_HTTP_VERSION, CURL_HTTP_VERSION_1_1);
  curl_easy_setopt(curl, CURLOPT_ERRORBUFFER, curlerr);
  curl_easy_setopt(curl, CURLOPT_WRITEDATA, test_data);
  CURLcode ccode = curl_easy_perform(curl);
  if (ccode) {
    fail("HTTP server not accessible: %s", curlerr);
  }
  
  if (CURLE_OK != curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &code)) {
    fail("Could not get response code");
  }
  assert_equal(404, code);
  curl_easy_cleanup(curl);
}
END_TEST
#endif

Suite * http_suite(void) {
  Suite *s = suite_create("http");
  TCase *tc_case = tcase_create("case");
  tcase_add_checked_fixture(tc_case, setup_httpd, teardown_httpd);
  
  // START_TESTS
#ifdef HAVE_LIBCURL
  tcase_add_test(tc_case, test_http_initialization);
  // END_TESTS
#endif
  suite_add_tcase(s, tc_case);
  return s;
}

int main(void) {
  initialize_logging("http_test.log");
  int number_failed;
  
  SRunner *sr = srunner_create(http_suite());
  
  srunner_run_all(sr, CK_NORMAL);
  number_failed = srunner_ntests_failed(sr);
  srunner_free(sr);
  close_log();
  return (number_failed == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}
