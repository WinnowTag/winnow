/* Copyright (c) 2007 The Kaphan Foundation
 *
 * Possession of a copy of this file grants no permission or license
 * to use, modify, or create derivate works.
 *
 * Please contact info@peerworks.org for further information.
 */

#include <check.h>
#include "../src/tag.h"
#include "../src/cls_config.h"
#include "assertions.h"

DBConfig config;

static void setup_config() {
  config.host = "localhost";
  config.user = "seangeo";
  config.password = "seangeo";
  config.database = "classifier_test";
}

static void teardown_config() {}

START_TEST (test_create_tag_db) {
  TagDB *tag_db = create_tag_db(&config);
  assert_not_null(tag_db);
  Tag *tag = tag_db_load_tag_by_id(tag_db, 39);
  assert_not_null(tag);
  assert_equal_s("Evilution", tag_tag_name(tag));
  assert_equal_s("", tag_user(tag));
  assert_equal(39, tag_tag_id(tag));
  assert_equal(2, tag_user_id(tag));
  assert_equal(20, tag_positive_examples_size(tag));
  assert_equal(84, tag_negative_examples_size(tag));
  free_tag(tag);
  free_tag_db(tag_db);
} END_TEST

Suite *
tag_db_suite(void) {
  Suite *s = suite_create("Tag_db");  
  TCase *tc_case = tcase_create("Case");
  tcase_add_checked_fixture (tc_case, setup_config, teardown_config);
  
// START_TESTS
  tcase_add_test(tc_case, test_create_tag_db);
// END_TESTS

  suite_add_tcase(s, tc_case);
  return s;
}
