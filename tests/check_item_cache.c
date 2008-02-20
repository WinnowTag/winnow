/* Copyright (c) 2008 The Kaphan Foundation
 *
 * Possession of a copy of this file grants no permission or license
 * to use, modify, or create derivate works.
 *
 * Please contact info@peerworks.org for further information.
 */

#include <check.h>
#include "assertions.h"
#include "../src/item_cache.h"
#include "../src/misc.h"
#include "../src/item_cache.h"

START_TEST (creating_with_missing_db_file_fails) {
  ItemCache *item_cache;
  int rc = item_cache_create(&item_cache, "missing.db");
  assert_equal(CLASSIFIER_FAIL, rc);
  const char *msg = item_cache_errmsg(item_cache);
  assert_equal_s("unable to open database file", msg);
} END_TEST

START_TEST (creating_with_empty_db_file_fails) {
  ItemCache *item_cache;
  int rc = item_cache_create(&item_cache, "fixtures/empty.db");
  assert_equal(CLASSIFIER_FAIL, rc);
  const char *msg = item_cache_errmsg(item_cache); 
  assert_equal_s("Database file's user version does not match classifier version. Trying running classifier-db-migrate.", msg);           
} END_TEST

START_TEST (create_with_valid_db) {
  ItemCache *item_cache;
  int rc = item_cache_create(&item_cache, "fixtures/valid.db");
  assert_equal(CLASSIFIER_OK, rc);
} END_TEST

/* Tests for fetching an item */
ItemCache *item_cache;

static void setup_fetch_item(void) {
  item_cache_create(&item_cache, "fixtures/valid.db");
}

static void teardown_fetch_item(void) {
  free_item_cache(item_cache);
}

START_TEST (test_fetch_item_returns_null_when_item_doesnt_exist) {
  Item *item = item_cache_fetch_item(item_cache, 111);
  assert_null(item);
  free_item(item);
} END_TEST

START_TEST (test_fetch_item_contains_item_id) {
  Item *item = item_cache_fetch_item(item_cache, 890806);
  assert_not_null(item);
  assert_equal(890806, item_get_id(item));
  free_item(item);
} END_TEST

START_TEST (test_fetch_item_contains_item_time) {
  Item *item = item_cache_fetch_item(item_cache, 890806);
  assert_not_null(item);
  assert_equal(1178551672, item_get_time(item));
  free_item(item);
} END_TEST

START_TEST (test_fetch_item_contains_the_right_number_of_tokens) {
 Item *item = item_cache_fetch_item(item_cache, 890806);
 assert_not_null(item);
 assert_equal(76, item_get_num_tokens(item));
 free_item(item);
} END_TEST

START_TEST (test_fetch_item_contains_the_right_frequency_for_a_given_token) {
  Item *item = item_cache_fetch_item(item_cache, 890806);
  assert_not_null(item);
  Token token;
  item_get_token(item, 9949, &token);
  assert_equal(9949, token.id);
  assert_equal(3, token.frequency);
} END_TEST

Suite *
sqlite_item_source_suite(void) {
  Suite *s = suite_create("ItemCache");  
  TCase *tc_case = tcase_create("case");

// START_TESTS
  tcase_add_test(tc_case, creating_with_missing_db_file_fails);
  tcase_add_test(tc_case, creating_with_empty_db_file_fails);
  tcase_add_test(tc_case, create_with_valid_db);
    
// END_TESTS
  
  TCase *fetch_item_case = tcase_create("fetch_item");
  tcase_add_checked_fixture(fetch_item_case, setup_fetch_item, teardown_fetch_item);
  tcase_add_test(fetch_item_case, test_fetch_item_returns_null_when_item_doesnt_exist);
  tcase_add_test(fetch_item_case, test_fetch_item_contains_item_id);
  tcase_add_test(fetch_item_case, test_fetch_item_contains_item_time);
  tcase_add_test(fetch_item_case, test_fetch_item_contains_the_right_number_of_tokens);
  tcase_add_test(fetch_item_case, test_fetch_item_contains_the_right_frequency_for_a_given_token);
  
  suite_add_tcase(s, tc_case);
  suite_add_tcase(s, fetch_item_case);
  return s;
}

