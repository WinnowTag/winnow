/* Copyright (c) 2007 The Kaphan Foundation
 *
 * Possession of a copy of this file grants no permission or license
 * to use, modify, or create derivate works.
 *
 * Please contact info@peerworks.org for further information.
 */

#include <check.h>
#include "assertions.h"
#include "../src/cls_config.h"

START_TEST (null_item_db_configuration) {
  Config *config = load_config("fixtures/no-db.conf");
  assert_not_null(config);
  DBConfig db_config;
  int rc = cfg_item_db_config(config, &db_config);
  assert_false(rc);
  
  free_config(config);
} END_TEST

START_TEST (item_db_configuration) {
  Config *config = load_config("fixtures/db.conf");
  DBConfig db_config;
  int rc = cfg_item_db_config(config, &db_config);
  assert_true(rc);
  
  assert_equal_s("localhost", db_config.host);
  assert_equal_s("bob", db_config.user);
  assert_equal_s("password", db_config.password);
  assert_equal_s("collector", db_config.database);
  assert_equal(3307, db_config.port); 
  free_config(config);
} END_TEST

START_TEST (tag_db_configuration) {
  Config *config = load_config("fixtures/db.conf");
  DBConfig db_config;
  memset(&db_config, 0, sizeof(db_config));
  int rc = cfg_tag_db_config(config, &db_config);
  assert_true(rc);
  
  assert_equal_s("localhost", db_config.host);
  assert_equal_s("tag", db_config.user);
  assert_equal_s("password", db_config.password);
  assert_equal_s("tag", db_config.database);
  assert_equal(3307, db_config.port); 
  free_config(config);
} END_TEST

Suite *
config_suite(void) {
  Suite *s = suite_create("Config");  
  TCase *tc_case = tcase_create("Case");

// START_TESTS
  tcase_add_test(tc_case, null_item_db_configuration);
  tcase_add_test(tc_case, item_db_configuration);
  tcase_add_test(tc_case, tag_db_configuration);
// END_TESTS

  suite_add_tcase(s, tc_case);
  return s;
}
