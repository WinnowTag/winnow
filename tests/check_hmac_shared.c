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

START_TEST (test_canonical_string_should_include_path_at_the_end) {
  char *str = canonical_string(GET, "/path/to/thing", NULL);
  assert_match("/path/to/thing$", str);
  free(str);
} END_TEST

START_TEST (test_canonical_string_should_include_the_content_type) {
  struct curl_slist *headers = NULL;
  headers = curl_slist_append(headers, "Content-Type: application/xml");
  char * str = canonical_string(POST, "/path/to/thing", headers);
  assert_match("\napplication/xml\n", str);
  free(str);
} END_TEST

START_TEST (test_canonical_string_should_include_the_content_md5) {
  struct curl_slist *headers = NULL;
  headers = curl_slist_append(headers, "Content-MD5: asdfasdf");
  char * str = canonical_string(POST, "/path/to/thing", headers);
  assert_match("\nasdfasdf\n", str);
  free(str);
} END_TEST

START_TEST (test_canonical_string_should_include_the_date) {
  struct curl_slist *headers = NULL;
  headers = curl_slist_append(headers, "Date: Thu, 10 Jul 2008 03:29:56 GMT");
  char * str = canonical_string(POST, "/path/to/thing", headers);
  assert_match("\nThu, 10 Jul 2008 03:29:56 GMT\n", str);
  free(str);
} END_TEST

START_TEST (test_canonical_string_should_build_the_correct_string) {
  struct curl_slist *headers = NULL;
  headers = curl_slist_append(headers, "Date: Thu, 10 Jul 2008 03:29:56 GMT");
  headers = curl_slist_append(headers, "Content-Type: application/xml");
  headers = curl_slist_append(headers, "Content-MD5: asdfasdf");
  char * str = canonical_string(POST, "/path/to/thing", headers);
  assert_equal_s("POST\napplication/xml\nasdfasdf\nThu, 10 Jul 2008 03:29:56 GMT\n/path/to/thing", str);
  free(str);
} END_TEST

START_TEST (test_canonical_string_should_build_the_correct_string_when_some_elements_are_missing) {
  struct curl_slist *headers = NULL;
  headers = curl_slist_append(headers, "Date: Thu, 10 Jul 2008 03:29:56 GMT");
  char * str = canonical_string(POST, "/path/to/thing", headers);
  assert_equal_s("POST\n\n\nThu, 10 Jul 2008 03:29:56 GMT\n/path/to/thing", str);
  free(str);
} END_TEST

Suite *
hmac_shared_suite(void) {
  Suite *s = suite_create("HMAC Shared");  
  TCase *canonical = tcase_create("CanonicalString");

// START_TESTS
  tcase_add_test(canonical, test_canonical_string_should_include_http_verb_when_GET);
  tcase_add_test(canonical, test_canonical_string_should_include_http_verb_when_PUT);
  tcase_add_test(canonical, test_canonical_string_should_include_http_verb_when_POST);
  tcase_add_test(canonical, test_canonical_string_should_include_http_verb_when_DELETE);
  tcase_add_test(canonical, test_canonical_string_should_include_path_at_the_end);
  tcase_add_test(canonical, test_canonical_string_should_include_the_content_type);
  tcase_add_test(canonical, test_canonical_string_should_include_the_content_md5);
  tcase_add_test(canonical, test_canonical_string_should_include_the_date);
  tcase_add_test(canonical, test_canonical_string_should_build_the_correct_string);
  tcase_add_test(canonical, test_canonical_string_should_build_the_correct_string_when_some_elements_are_missing);
// END_TESTS

  suite_add_tcase(s, canonical);
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
