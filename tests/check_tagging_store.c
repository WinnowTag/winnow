/* Copyright (c) 2007 The Kaphan Foundation
 *
 * Possession of a copy of this file grants no permission or license
 * to use, modify, or create derivate works.
 *
 * Please contact info@peerworks.org for further information.
 */

#include <check.h>
#include <mysql.h>
#include <string.h>
#include <stdio.h>
#include "../src/tagging.h"
#include "../src/cls_config.h"
#include "assertions.h"
#include "tagging_store_fixtures.h"

START_TEST(test_insert_tagging) {
  TaggingStore *tagging_store = create_db_tagging_store(&config, 0.9);
  assert_not_null(tagging_store);
  Tagging tagging;
  tagging.user_id = 23;
  tagging.tag_id = 45;
  tagging.item_id = 56;
  tagging.strength = 0.9;
  tagging_store_store(tagging_store, &tagging);
  assert_tagging_stored(&tagging);
} END_TEST

START_TEST(test_dont_insert_tagging_below_threshold) {
  TaggingStore *tagging_store = create_db_tagging_store(&config, 0.9);
  assert_not_null(tagging_store);
  Tagging tagging;
  tagging.user_id = 23;
  tagging.tag_id = 45;
  tagging.item_id = 57;
  tagging.strength = 0.89;
  tagging_store_store(tagging_store, &tagging);
  assert_tagging_not_stored(&tagging);
} END_TEST

START_TEST(test_insert_duplicate_updates_strength) {
  TaggingStore *tagging_store = create_db_tagging_store(&config, 0.9);
  assert_not_null(tagging_store);
  Tagging tagging1, tagging2;
  tagging1.user_id = tagging2.user_id = 23;
  tagging1.tag_id = tagging2.tag_id = 45;
  tagging1.item_id = tagging2.item_id = 56;
  tagging1.strength = 0.91;
  tagging2.strength = 0.95;
  
  tagging_store_store(tagging_store, &tagging1);
  tagging_store_store(tagging_store, &tagging2);
  assert_tagging_not_stored(&tagging1);
  assert_tagging_stored(&tagging2);
} END_TEST

START_TEST (reopen_after_close) {
  TaggingStore *tagging_store = create_db_tagging_store(&config, 0.9);
  assert_not_null(tagging_store);
  assert_true(tagging_store_is_alive(tagging_store));
  tagging_store_close(tagging_store);
  memset(&config, 0, sizeof(DBConfig));
  assert_true(tagging_store_is_alive(tagging_store));
} END_TEST

START_TEST (store_after_close) {
  TaggingStore *tagging_store = create_db_tagging_store(&config, 0.9);
  assert_not_null(tagging_store);
  assert_true(tagging_store_is_alive(tagging_store));
  tagging_store_close(tagging_store);
  memset(&config, 0, sizeof(DBConfig));
  
  Tagging tagging;
  tagging.user_id = 23;
  tagging.tag_id = 45;
  tagging.item_id = 56;
  tagging.strength = 0.99;
  
  tagging_store_store(tagging_store, &tagging);
  assert_tagging_stored(&tagging);
  free_tagging_store(tagging_store);
} END_TEST

Suite * tagging_store_suite(void) {
  Suite *s = suite_create("tagging_store");
  TCase *tc_case = tcase_create("case");
  tcase_add_checked_fixture (tc_case, setup_tagging_store, teardown_tagging_store);
  // START_TESTS
  tcase_add_test(tc_case, test_insert_tagging);
  tcase_add_test(tc_case, test_dont_insert_tagging_below_threshold);
  tcase_add_test(tc_case, test_insert_duplicate_updates_strength);
  tcase_add_test(tc_case, reopen_after_close);
  tcase_add_test(tc_case, store_after_close);
  // END_TESTS

  suite_add_tcase(s, tc_case);
  return s;
}
