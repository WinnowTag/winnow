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
#include "../src/fetch_url.h"
#include "assertions.h"
#include "fixtures.h"

START_TEST (test_signing_with_hmac_adds_date_header_if_missing) {
  
} END_TEST



Suite *
hmac_sign_suite(void) {
  Suite *s = suite_create("HMAC Signing");  
  TCase *tc_case = tcase_create("Case");

// START_TESTS
  tcase_add_test(tc_case, test_signing_with_hmac_adds_date_header_if_missing);
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
