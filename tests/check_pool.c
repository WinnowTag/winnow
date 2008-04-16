// Copyright (c) 2007 The Kaphan Foundation
//
// Possession of a copy of this file grants no permission or license
// to use, modify, or create derivate works.
//
// Please contact info@peerworks.org for further information.

#include <stdlib.h>
#include <math.h>
#include <check.h>
#include "fixtures.h"
#include "../src/item_cache.h"
#include "../src/misc.h"
#include "assertions.h"

static ItemCacheOptions item_cache_options;
static ItemCache *item_cache;
static int free_when_done;

static void setup(void) {
  item_cache_options.cache_update_wait_time = 1;
  setup_fixture_path();
  item_cache_create(&item_cache, "fixtures/valid.db", &item_cache_options);
  item_cache_load(item_cache);
}

static void teardown(void) {
  teardown_fixture_path();
  free_item_cache(item_cache);
}

START_TEST (create_pool) {
  Pool *pool = new_pool();
  assert_not_null(pool);
  free_pool(pool);
} END_TEST

START_TEST (new_pool_is_empty) {
  Pool *pool = new_pool();
  assert_equal(0, pool_num_tokens(pool));
  assert_equal(0, pool_total_tokens(pool));
  free_pool(pool);
} END_TEST

START_TEST (add_1_item) {
  Pool *pool = new_pool();
  pool_add_item(pool, item_cache_fetch_item(item_cache, (unsigned char *) "urn:peerworks.org:entry#878944", &free_when_done));
  assert_equal(186, pool_num_tokens(pool));
  assert_equal(336, pool_total_tokens(pool));
  assert_equal(9, pool_token_frequency(pool, 7982));
  assert_equal(1, pool_token_frequency(pool, 7967));
  assert_equal(0, pool_token_frequency(pool, 10));
  free_pool(pool);
} END_TEST

START_TEST (add_2_items_with_same_tokens) {
  Pool *pool = new_pool();
  pool_add_item(pool, item_cache_fetch_item(item_cache, (unsigned char *) "urn:peerworks.org:entry#709254", &free_when_done));
  pool_add_item(pool, item_cache_fetch_item(item_cache, (unsigned char *) "urn:peerworks.org:entry#753459", &free_when_done));
  assert_equal(196, pool_num_tokens(pool));
  assert_equal(323, pool_total_tokens(pool));
  assert_equal(3, pool_token_frequency(pool, 665));
  assert_equal(3, pool_token_frequency(pool, 1179));
  assert_equal(1, pool_token_frequency(pool, 1222));
  assert_equal(0, pool_token_frequency(pool, 10));
  free_pool(pool);
} END_TEST

START_TEST (token_iteration) {
  int ret_val;
  Pool *pool = new_pool();
  pool_add_item(pool, item_cache_fetch_item(item_cache, (unsigned char *) "urn:peerworks.org:entry#709254", &free_when_done));
  pool_add_item(pool, item_cache_fetch_item(item_cache, (unsigned char *) "urn:peerworks.org:entry#753459", &free_when_done));
  
  Token token;
  token.id = 0;
  
  ret_val = pool_next_token(pool, &token);
  assert_true(ret_val);
  assert_equal(1, token.id);
  assert_equal(1, token.frequency);

  ret_val = pool_next_token(pool, &token);  
  assert_true(ret_val);
  assert_equal(13, token.id);
  assert_equal(2, token.frequency);
   
  free_pool(pool);
} END_TEST

START_TEST (token_iteration_with_null_pool_doesnt_crash) {
  Token token;
  token.id = 0;
  int ret_val;
  
  ret_val = pool_next_token(NULL, &token);
  assert_false(ret_val);
} END_TEST

Suite *
pool_suite(void) {
  Suite *s = suite_create("Pool");
  
  TCase *tc_pool = tcase_create("Pool");
  tcase_add_checked_fixture (tc_pool, setup, teardown);
  tcase_add_test(tc_pool, create_pool);
  tcase_add_test(tc_pool, new_pool_is_empty);
  tcase_add_test(tc_pool, add_1_item);
  tcase_add_test(tc_pool, add_2_items_with_same_tokens);
  tcase_add_test(tc_pool, token_iteration);
  tcase_add_test(tc_pool, token_iteration_with_null_pool_doesnt_crash);
  suite_add_tcase(s, tc_pool);  
  
  return s;
}

int main(void) {
  initialize_logging("test.log");
  int number_failed;
  
  SRunner *sr = srunner_create(pool_suite());
  srunner_run_all(sr, CK_NORMAL);
  number_failed = srunner_ntests_failed(sr);
  srunner_free(sr);
  close_log();
  return (number_failed == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}