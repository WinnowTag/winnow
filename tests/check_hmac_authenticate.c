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
#include "../src/hmac_internal.h"
#include "../src/hmac_sign.h"
#include "../src/hmac_auth.h"
#include "assertions.h"
#include "fixtures.h"

START_TEST (test_auth_should_return_false_when_there_is_no_authorization_header) {
  struct curl_slist *headers = NULL;
  int authenticated = hmac_auth("GET", "/path", headers, "access_id", "secret");
  assert_false(authenticated);
} END_TEST

START_TEST (test_auth_should_return_false_when_the_authorization_header_isnt_prefixed_with_AuthHMAC) {
  struct curl_slist *headers = NULL;
  headers = curl_slist_append(headers, "Authorization: blahblah");
  int authenticated = hmac_auth("GET", "/path", headers, "access_id", "secret");
  assert_false(authenticated);
} END_TEST

START_TEST (test_auth_should_return_false_when_the_signature_is_missing_a_colon) {
  struct curl_slist *headers = NULL;
  headers = curl_slist_append(headers, "Authorization: AuthHMAC blahblah");
  int authenticated = hmac_auth("GET", "/path", headers, "access_id", "secret");
  assert_false(authenticated);
} END_TEST

START_TEST (test_auth_should_return_false_when_there_is_no_hmac) {
  struct curl_slist *headers = NULL;
  headers = curl_slist_append(headers, "Authorization: AuthHMAC blahblah:");
  int authenticated = hmac_auth("GET", "/path", headers, "access_id", "secret");
  assert_false(authenticated);
} END_TEST

START_TEST (test_auth_should_return_false_when_access_id_doesnt_match_the_credentials) {
  struct curl_slist *headers = NULL;
  headers = hmac_sign("GET", "/path", headers, "access_id", "secret");
  
  int authenticated = hmac_auth("GET", "/path", headers, "other_access_id", "secret");
  assert_false(authenticated);
} END_TEST

START_TEST (test_auth_should_return_false_when_signature_doesnt_match_the_credentials) {
  struct curl_slist *headers = NULL;
  headers = curl_slist_append(headers, "Authorization: AuthHMAC access_id:blahblah");
  int authenticated = hmac_auth("GET", "/path", headers, "access_id", "secret");
  assert_false(authenticated);
} END_TEST

START_TEST (test_auth_should_return_true_when_everything_matches) {
  struct curl_slist *headers = NULL;
  headers = hmac_sign("GET", "/path", headers, "access_id", "secret");
  
  int authenticated = hmac_auth("GET", "/path", headers, "access_id", "secret");
  assert_true(authenticated);
} END_TEST

Suite *
hmac_sign_suite(void) {
  Suite *s = suite_create("HMAC Authentication");  
  TCase *tc_case = tcase_create("authentication");

// START_TESTS
  tcase_add_test(tc_case, test_auth_should_return_false_when_there_is_no_authorization_header);
  tcase_add_test(tc_case, test_auth_should_return_false_when_the_authorization_header_isnt_prefixed_with_AuthHMAC);
  tcase_add_test(tc_case, test_auth_should_return_false_when_the_signature_is_missing_a_colon);
  tcase_add_test(tc_case, test_auth_should_return_false_when_there_is_no_hmac);
  tcase_add_test(tc_case, test_auth_should_return_false_when_access_id_doesnt_match_the_credentials);
  tcase_add_test(tc_case, test_auth_should_return_false_when_signature_doesnt_match_the_credentials);
  tcase_add_test(tc_case, test_auth_should_return_true_when_everything_matches);
// END_TESTS

  suite_add_tcase(s, tc_case);
  return s;
}

int main(void) {
  initialize_logging("test.log");
  int number_failed;
  
  SRunner *sr = srunner_create(hmac_sign_suite());
  srunner_run_all(sr, CK_NORMAL);
  number_failed = srunner_ntests_failed(sr);
  srunner_free(sr);
  close_log();
  return (number_failed == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}
