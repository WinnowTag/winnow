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

static Credentials credentials = {"access_id", "secret"};

START_TEST (test_auth_should_return_false_when_there_is_no_authorization_header) {
  struct curl_slist *headers = NULL;
  int authenticated = hmac_auth("GET", "/path", headers, &credentials);
  assert_false(authenticated);
} END_TEST

START_TEST (test_auth_should_return_false_when_the_authorization_header_isnt_prefixed_with_AuthHMAC) {
  struct curl_slist *headers = NULL;
  headers = curl_slist_append(headers, "Authorization: blahblah");
  int authenticated = hmac_auth("GET", "/path", headers, &credentials);
  assert_false(authenticated);
} END_TEST

START_TEST (test_auth_should_return_false_when_the_signature_is_missing_a_colon) {
  struct curl_slist *headers = NULL;
  headers = curl_slist_append(headers, "Authorization: AuthHMAC blahblah");
  int authenticated = hmac_auth("GET", "/path", headers, &credentials);
  assert_false(authenticated);
} END_TEST

START_TEST (test_auth_should_return_false_when_there_is_no_hmac) {
  struct curl_slist *headers = NULL;
  headers = curl_slist_append(headers, "Authorization: AuthHMAC blahblah:");
  int authenticated = hmac_auth("GET", "/path", headers, &credentials);
  assert_false(authenticated);
} END_TEST

START_TEST (test_auth_should_return_false_when_access_id_doesnt_match_the_credentials) {
  struct curl_slist *headers = NULL;
  headers = hmac_sign("GET", "/path", headers, &credentials);
  
  Credentials other = {"other_access_id", "secret"};
  
  int authenticated = hmac_auth("GET", "/path", headers, &other);
  assert_false(authenticated);
} END_TEST

START_TEST (test_auth_should_return_false_when_signature_doesnt_match_the_credentials) {
  struct curl_slist *headers = NULL;
  headers = curl_slist_append(headers, "Authorization: AuthHMAC access_id:blahblah");
  int authenticated = hmac_auth("GET", "/path", headers, &credentials);
  assert_false(authenticated);
} END_TEST

START_TEST (test_auth_should_return_true_when_everything_matches) {
  struct curl_slist *headers = NULL;
  headers = hmac_sign("GET", "/path", headers, &credentials);
  
  int authenticated = hmac_auth("GET", "/path", headers, &credentials);
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
