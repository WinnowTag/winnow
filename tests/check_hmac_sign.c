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
#include "assertions.h"
#include "fixtures.h"

static Credentials credentials = {"access_id", "secret"};

START_TEST (test_signing_with_hmac_adds_date_header_if_missing) {
  struct curl_slist *headers = NULL;
  headers = hmac_sign(GET, "/path", headers, &credentials);
  assert_not_null(headers);
  assert_match("^Date: [[:alpha:]]{3}, [[:digit:]]{2} [[:alpha:]]{3} [[:digit:]]{4} [[:digit:]]{2}:[[:digit:]]{2}:[[:digit:]]{2} GMT$", headers->data);
  curl_slist_free_all(headers);
} END_TEST

START_TEST (test_signing_with_hmac_adds_authorization_header) {
  struct curl_slist *headers = NULL;
  headers = hmac_sign(GET, "/path", headers, &credentials);
  assert_not_null(headers->next);
  assert_match("^Authorization: ", headers->next->data);
  curl_slist_free_all(headers);
} END_TEST

START_TEST (test_signing_with_hmac_prefixes_the_authorization_header_with_AuthHMAC) {
    struct curl_slist *headers = NULL;
  headers = hmac_sign(GET, "/path", headers, &credentials);
  assert_not_null(headers->next);
  assert_match("^Authorization: AuthHMAC ", headers->next->data);
  curl_slist_free_all(headers);
} END_TEST

START_TEST (test_signing_with_hmac_includes_the_access_id_as_the_first_part_of_the_signature) {
  struct curl_slist *headers = NULL;
  headers = hmac_sign(GET, "/path", headers, &credentials);
  assert_not_null(headers->next);
  assert_match("^Authorization: AuthHMAC access_id:", headers->next->data);
  curl_slist_free_all(headers);
} END_TEST

START_TEST (test_signing_with_hmac_creates_a_complete_key) {
  struct curl_slist *headers = NULL;
  headers = curl_slist_append(headers, "Content-Type: text/plain");
  headers = curl_slist_append(headers, "Content-MD5: blahblah");
  headers = curl_slist_append(headers, "Date: Thu, 10 Jul 2008 03:29:56 GMT");
  headers = hmac_sign(PUT, "/path/to/put", headers, &credentials);
  
  struct curl_slist *last = headers;
  while(last->next) { last = last->next; }
  assert_not_null(last);
  
  assert_equal_s("Authorization: AuthHMAC access_id:71wAJM4IIu/3o6lcqx/tw7XnAJs=", last->data);
  curl_slist_free_all(headers);
  
} END_TEST        

Suite *
hmac_sign_suite(void) {
  Suite *s = suite_create("HMAC Signing");  
  TCase *tc_case = tcase_create("signing");

// START_TESTS
  tcase_add_test(tc_case, test_signing_with_hmac_adds_date_header_if_missing);
  tcase_add_test(tc_case, test_signing_with_hmac_adds_authorization_header);
  tcase_add_test(tc_case, test_signing_with_hmac_prefixes_the_authorization_header_with_AuthHMAC);
  tcase_add_test(tc_case, test_signing_with_hmac_includes_the_access_id_as_the_first_part_of_the_signature);
  tcase_add_test(tc_case, test_signing_with_hmac_creates_a_complete_key);
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
