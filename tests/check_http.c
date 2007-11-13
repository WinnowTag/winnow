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


#define PORT 8008

static void setup_httpd() {
  
}

static void teardown_httpd() {
  
}

#ifdef HAVE_LIBCURL
#include <curl/curl.h>

START_TEST(test_http_initialization) {
  char curlerr[CURL_ERROR_SIZE];
  CURL *curl = curl_easy_init();
  curl_easy_setopt(curl, CURLOPT_URL, "http://localhost:8008/classifier/jobs");
  curl_easy_setopt(curl, CURLOPT_HTTP_VERSION, CURL_HTTP_VERSION_1_1);
  curl_easy_setopt(curl, CURLOPT_ERRORBUFFER, curlerr);
  CURLcode code = curl_easy_perform(curl);
  if (code) {
    fail("HTTP server not accessible: %s", curlerr);
  }
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
