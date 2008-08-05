/* Copyright (c) 2007 The Kaphan Foundation
 *
 * Possession of a copy of this file grants no permission or license
 * to use, modify, or create derivate works.
 *
 * Please contact info@peerworks.org for further information.
 */

#include <check.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <curl/curl.h>
#include "assertions.h"
#include "fixtures.h"
#include "../src/hmac_internal.h"

START_TEST (test_canonical_string_should_include_http_verb_when_GET) {  
  char * str = canonical_string(GET, "/path/to/thing", NULL);
  assert_match("^GET\n", str);
  free(str);
} END_TEST

START_TEST (test_canonical_string_should_include_http_verb_when_PUT) {
  char * str = canonical_string(PUT, "/path/to/thing", NULL);
  assert_match("^PUT\n", str);
  free(str);
} END_TEST

START_TEST (test_canonical_string_should_include_http_verb_when_POST) {
  char * str = canonical_string(POST, "/path/to/thing", NULL);
  assert_match("^POST\n", str);
  free(str);
} END_TEST

START_TEST (test_canonical_string_should_include_http_verb_when_DELETE) {
  char * str = canonical_string(DELETE, "/path/to/thing", NULL);
  assert_match("^DELETE\n", str);
  free(str);
} END_TEST

Suite *
hmac_shared_suite(void) {
  Suite *s = suite_create("HMAC Shared");  
  TCase *tc_case = tcase_create("CanonicalString");

// START_TESTS
  tcase_add_test(tc_case, test_canonical_string_should_include_http_verb_when_GET);
  tcase_add_test(tc_case, test_canonical_string_should_include_http_verb_when_PUT);
  tcase_add_test(tc_case, test_canonical_string_should_include_http_verb_when_POST);
  tcase_add_test(tc_case, test_canonical_string_should_include_http_verb_when_DELETE);
// END_TESTS

  suite_add_tcase(s, tc_case);
  return s;
}

int main(void) {
  initialize_logging("test.log");
  int number_failed;
  
  SRunner *sr = srunner_create(hmac_shared_suite());
  srunner_run_all(sr, CK_NORMAL);
  number_failed = srunner_ntests_failed(sr);
  srunner_free(sr);
  close_log();
  return (number_failed == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}
