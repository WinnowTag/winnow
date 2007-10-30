// Copyright (c) 2007 The Kaphan Foundation
//
// Possession of a copy of this file grants no permission or license
// to use, modify, or create derivate works.
//
// Please contact info@peerworks.org for further information.

#include <stdlib.h>
#include <math.h>
#include <check.h>
#include "../src/pool.h"
#include "../src/item.h"
#include "../src/misc.h"
#include "assertions.h"

Item item_1;
Item item_2;
Item item_3;

void setup(void) {
  int tokens_1[][2] = {1, 10, 2, 3};
  int tokens_2[][2] = {1, 5, 3, 4};
  int tokens_3[][2] = {1, 6, 2, 4};
  
  item_1 = create_item_with_tokens(1, tokens_1, 2);
  item_2 = create_item_with_tokens(2, tokens_2, 2);
  item_3 = create_item_with_tokens(3, tokens_3, 2);
}

void teardown(void) {
  free_item(item_1);
  free_item(item_2);
  free_item(item_3);
}

START_TEST (create_pool) {
  Pool pool = new_pool();
  assert_not_null(pool);
  free_pool(pool);
} END_TEST

START_TEST (new_pool_is_empty) {
  Pool pool = new_pool();
  assert_equal(0, pool_num_tokens(pool));
  assert_equal(0, pool_total_tokens(pool));
  free_pool(pool);
} END_TEST

START_TEST (add_1_item) {
  Pool pool = new_pool();
  pool_add_item(pool, item_1);
  assert_equal(2, pool_num_tokens(pool));
  assert_equal(13, pool_total_tokens(pool));
  assert_equal(10, pool_token_frequency(pool, 1));
  assert_equal(3, pool_token_frequency(pool, 2));
  assert_equal(0, pool_token_frequency(pool, 10));
  free_pool(pool);
} END_TEST

START_TEST (add_2_items_with_same_tokens) {
  Pool pool = new_pool();
  pool_add_item(pool, item_1);
  pool_add_item(pool, item_3);
  assert_equal(2, pool_num_tokens(pool));
  assert_equal(23, pool_total_tokens(pool));
  assert_equal(16, pool_token_frequency(pool, 1));
  assert_equal(7, pool_token_frequency(pool, 2));
  assert_equal(0, pool_token_frequency(pool, 3));
  assert_equal(0, pool_token_frequency(pool, 10));
  free_pool(pool);
} END_TEST

START_TEST (add_2_items_with_1_overlapping_token) {
  Pool pool = new_pool();
  int ret_val;
  
  ret_val = pool_add_item(pool, item_1);
  assert_equal(true, ret_val);  
  ret_val = pool_add_item(pool, item_2);
  assert_equal(true, ret_val);
  
  assert_equal(3, pool_num_tokens(pool));
  assert_equal(22, pool_total_tokens(pool));
  assert_equal(15, pool_token_frequency(pool, 1));
  assert_equal(3, pool_token_frequency(pool, 2));
  assert_equal(4, pool_token_frequency(pool, 3));
  assert_equal(0, pool_token_frequency(pool, 10));
  free_pool(pool);
} END_TEST

START_TEST (token_iteration) {
  int ret_val;
  Pool pool = new_pool();
  pool_add_item(pool, item_1);
  pool_add_item(pool, item_2);
  pool_add_item(pool, item_3);
  
  Token token;
  token.id = 0;
  
  ret_val = pool_next_token(pool, &token);
  assert_true(ret_val);
  assert_equal(1, token.id);
  assert_equal(21, token.frequency);

  ret_val = pool_next_token(pool, &token);  
  assert_true(ret_val);
  assert_equal(2, token.id);
  assert_equal(7, token.frequency);
  
  ret_val = pool_next_token(pool, &token);
  assert_true(ret_val);
  assert_equal(3, token.id);
  assert_equal(4, token.frequency);
  
  ret_val = pool_next_token(pool, &token);
  assert_false(ret_val);
  assert_equal(0, token.id);
  assert_equal(0, token.frequency);
  
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
classifier_suite(void) {
  Suite *s = suite_create("Pool");
  
  TCase *tc_pool = tcase_create("Pool");
  tcase_add_checked_fixture (tc_pool, setup, teardown);
  tcase_add_test(tc_pool, create_pool);
  tcase_add_test(tc_pool, new_pool_is_empty);
  tcase_add_test(tc_pool, add_1_item);
  tcase_add_test(tc_pool, add_2_items_with_same_tokens);
  tcase_add_test(tc_pool, add_2_items_with_1_overlapping_token);
  tcase_add_test(tc_pool, token_iteration);
  tcase_add_test(tc_pool, token_iteration_with_null_pool_doesnt_crash);
  suite_add_tcase(s, tc_pool);  
  
  return s;
}

int main(void) {
  int number_failed;
  Suite *s = classifier_suite();
  SRunner *sr = srunner_create(s);
  srunner_run_all(sr, CK_NORMAL);
  number_failed = srunner_ntests_failed(sr);
  srunner_free(sr);
  return (number_failed == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}