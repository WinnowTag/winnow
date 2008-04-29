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

START_TEST (test_fetching_from_a_file) {
  char path[1024];
  getcwd(path, 1024);
  char url[1024];
  sprintf(url, "file:%s/fixtures/entry.atom", path);
  
  char *data = NULL;
  int rc = fetch_url(url, 0, &data, NULL);
  assert_equal(URL_OK, rc);
  assert_not_null(data);
  assert_equal(920, strlen(data));
} END_TEST

START_TEST (test_fetching_non_existent_file_returns_null) {
  char *data = NULL;
  int rc = fetch_url("file:/foo/bar.txt", 0, &data, NULL);
  assert_equal(URL_FAIL, rc);
  assert_null(data);
} END_TEST

Suite *
url_fetching_suite(void) {
  Suite *s = suite_create("URL Fetching");  
  TCase *tc_case = tcase_create("Case");

// START_TESTS
  tcase_add_test(tc_case, test_fetching_from_a_file);
  tcase_add_test(tc_case, test_fetching_non_existent_file_returns_null);
// END_TESTS

  suite_add_tcase(s, tc_case);
  return s;
}

int main(void) {
  initialize_logging("test.log");
  int number_failed;
  
  SRunner *sr = srunner_create(url_fetching_suite());
  srunner_run_all(sr, CK_NORMAL);
  number_failed = srunner_ntests_failed(sr);
  srunner_free(sr);
  close_log();
  return (number_failed == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}
