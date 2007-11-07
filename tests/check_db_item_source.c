/* Copyright (c) 2007 The Kaphan Foundation
 *
 * Possession of a copy of this file grants no permission or license
 * to use, modify, or create derivate works.
 *
 * Please contact info@peerworks.org for further information.
 */

#include <check.h>
#include <stdlib.h>
#include "assertions.h"
#include "../src/cls_config.h"
#include "../src/db_item_source.h"

DBConfig dbconfig;

static void setup_config(void) {
  dbconfig.host = "localhost";
  dbconfig.user = "seangeo";
  dbconfig.password = "seangeo";
  dbconfig.database = "classifier_test";
  dbconfig.port = 0;  
}

static void teardown_config(void) { }

START_TEST (test_db_item_source_creation) {
  ItemSource *is = create_db_item_source(&dbconfig);
  assert_not_null(is);
  int alive = is_alive(is);
  assert_true(alive);
} END_TEST

START_TEST (broken_config) {
  dbconfig.host = "broken";
  ItemSource *is = create_db_item_source(&dbconfig);
  assert_null(is);
} END_TEST

Suite *
db_item_source_suite(void) {
  Suite *s = suite_create("Db_item_source");  
  TCase *tc_case = tcase_create("Case");
  tcase_add_checked_fixture (tc_case, setup_config, teardown_config);
// START_TESTS
  tcase_add_test(tc_case, test_db_item_source_creation);
  tcase_add_test(tc_case, broken_config);
// END_TESTS

  suite_add_tcase(s, tc_case);
  return s;
}
