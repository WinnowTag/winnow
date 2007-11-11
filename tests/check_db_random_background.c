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
#include "mock_item_source.h"
#include "../src/random_background.h"

DBConfig db_config;

static void setup(void) {
  setup_mock_item_source();
  db_config.host = "localhost";
  db_config.user = "seangeo";
  db_config.password = "seangeo";
  db_config.database = "classifier_test";
}

static void teardown(void) {
  teardown_mock_item_source();
}

START_TEST(test_load_random_background_from_db) {
  Pool *pool = create_random_background_from_db(is, &db_config);
  assert_not_null(pool);
  assert_equal(3, pool_num_tokens(pool));
  assert_equal(32, pool_total_tokens(pool));
  free_pool(pool);
}
END_TEST


Suite * db_random_background_suite(void) {
  Suite *s = suite_create("db_random_background");
  TCase *tc_case = tcase_create("case");
  tcase_add_unchecked_fixture(tc_case, setup, teardown);
  
  // START_TESTS
  tcase_add_test(tc_case, test_load_random_background_from_db);
  // END_TESTS

  suite_add_tcase(s, tc_case);
  return s;
}