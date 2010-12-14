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
#include "../src/fetch_url.h"
#include "assertions.h"
#include "fixtures.h"

START_TEST (test_fetching_from_a_file) {
  setup_fixture_path();
  char path[1024];
  getcwd(path, 1024);
  char url[1024];
  sprintf(url, "file:%s/fixtures/entry.atom", path);
  
  char *data = NULL;
  int rc = fetch_url(url, 0, NULL, &data, NULL);
  assert_equal(URL_OK, rc);
  assert_not_null(data);
  assert_equal(928, strlen(data));
} END_TEST

START_TEST (test_fetching_non_existent_file_returns_null) {
  char *data = NULL;
  int rc = fetch_url("file:/foo/bar.txt", 0, NULL, &data, NULL);
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
