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

START_TEST (test_fetch_all_items) {
  ItemSource *is = create_db_item_source(&dbconfig);
  assert_true(is_alive(is));
  ItemList *item_list = is_fetch_all_items(is);
  assert_not_null(item_list);
  assert_equal(11, item_list_size(item_list));
  
  Item *item = item_list_item(item_list, 916470);
  assert_not_null(item);
  
  item = item_list_item_at(item_list, 2);
  assert_not_null(item);
  
  free_item_list(item_list);
  free_item_source(is);
} END_TEST

START_TEST (test_ordered_by_time) {
  ItemSource *is = create_db_item_source(&dbconfig);
  assert_true(is_alive(is));
  
  ItemList *item_list = is_fetch_all_items(is);
  assert_not_null(item_list);
  assert_equal(11, item_list_size(item_list));
  
  Item *item = item_list_item_at(item_list, 0);
  assert_not_null(item);
  assert_equal(916480, item_get_id(item));
  
  item = item_list_item_at(item_list, 10);
  assert_not_null(item);
  assert_equal(916479, item_get_id(item));
  
  free_item_list(item_list);
  free_item_source(is);
} END_TEST


START_TEST (test_fetch_item) {
  ItemSource *is = create_db_item_source(&dbconfig);
  assert_true(is_alive(is));
  Item *item = is_fetch_item(is, 916470);
  assert_not_null(item);
  assert_equal(563, item_get_num_tokens(item));
  assert_equal(916470, item_get_id(item));

  Token token;
  item_get_token(item, 85253, &token);
  assert_equal(85253, token.id);
  assert_equal(9, token.frequency);
  
  free_item(item);
  free_item_source(is);
} END_TEST

START_TEST (test_db_item_source_creation) {
  ItemSource *is = create_db_item_source(&dbconfig);
  assert_not_null(is);
  int alive = is_alive(is);
  assert_true(alive);
  free_item_source(is);
} END_TEST

START_TEST (broken_config) {
  dbconfig.host = "broken";
  ItemSource *is = create_db_item_source(&dbconfig);
  assert_null(is);
  free_item_source(is);
} END_TEST

Suite *
db_item_source_suite(void) {
  Suite *s = suite_create("Db_item_source");  
  TCase *tc_case = tcase_create("Case");
  tcase_add_checked_fixture (tc_case, setup_config, teardown_config);
// START_TESTS
  tcase_add_test(tc_case, test_db_item_source_creation);
  tcase_add_test(tc_case, broken_config);
  tcase_add_test(tc_case, test_fetch_item);
  tcase_add_test(tc_case, test_fetch_all_items);
  tcase_add_test(tc_case, test_ordered_by_time);
// END_TESTS

  suite_add_tcase(s, tc_case);
  return s;
}
