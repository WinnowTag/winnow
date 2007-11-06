// Copyright (c) 2007 The Kaphan Foundation
//
// Possession of a copy of this file grants no permission or license
// to use, modify, or create derivate works.
//
// Please contact info@peerworks.org for further information.

#include <stdlib.h>
#include <check.h>
#include <string.h>
#include "../src/item.h"
#include "assertions.h"

#ifndef CORPUS
#define CORPUS "fixtures"
#endif

START_TEST (check_item_creation) {
  Item *item;
  Token token;
  int tokens[][2] = {1,2,3,4};
  
  item = create_item_with_tokens(1, tokens, 2);
  assert_not_null(item);
  assert_equal(1, item_get_id(item));
  assert_equal(6, item_get_total_tokens(item));
  assert_equal(2, item_get_num_tokens(item));

  item_get_token(item, 1, &token);
  assert_equal(1, token.id);
  assert_equal(2, token.frequency);
  
  item_get_token(item, 3, &token);
  assert_equal(3, token.id);
  assert_equal(4, token.frequency);
  
  free_item(item);
} END_TEST

START_TEST (check_item_id) {
  Item *item = create_item_from_file(CORPUS, 1234);
  assert_equal(1234, item_get_id(item));
  free_item(item);
} END_TEST

START_TEST (check_item_total_tokens) {
  Item *item = create_item_from_file(CORPUS, 1234);
  assert_equal(125, item_get_total_tokens(item));
  free_item(item);
} END_TEST

START_TEST (check_token_count) {
  Item *item;
  Token token;
  int get_token_return;
  
  item = create_item_from_file(CORPUS, 1234);
  get_token_return = item_get_token(item, 253866, &token);
  
  assert_equal(0, get_token_return);
  assert_equal(253866, token.id);
  assert_equal(2, token.frequency);
  
  free_item(item);
} END_TEST

START_TEST (check_nonexistant_token_returns_0) {
  Item *item;
  Token token;
  int get_token_return;
  
  item = create_item_from_file(CORPUS, 1234);
  get_token_return = item_get_token(item, 1, &token);
  
  assert_equal(0, get_token_return);
  assert_equal(1, token.id);
  assert_equal(0, token.frequency);
  
  free_item(item);
} END_TEST

START_TEST (item_path_too_long) {
  Item *item;
  int i;
  char corpus[1024];
  corpus[0] = '\0';
  for (i = 0; i < 1022; i++) {
    strncat(corpus, "a", 1024);
  }
  
  item = create_item_from_file(corpus, 12345);
  assert_null(item);
} END_TEST

START_TEST (missing_item) {
  Item *item = create_item_from_file(CORPUS, 123456);
  assert_null(item);
} END_TEST

START_TEST (check_item_path) {
  Item *item = create_item_from_file(CORPUS, 1234);
  assert_equal_s("fixtures/1234.tokens", item_get_path(item));
  free_item(item);
} END_TEST

START_TEST (passing_null_to_free_item_doesnt_crash) {
  free_item(NULL);
} END_TEST

START_TEST (check_token_iterator) {
  Item *item;
  Token token;
  token.id = 0;
  int tokens[][2] = {1,2,3,4};
  int ret_val;
  
  item = create_item_with_tokens(1, tokens, 2);
  ret_val = item_next_token(item, &token);
  assert_equal(1, ret_val);
  assert_equal(1, token.id);
  assert_equal(2, token.frequency);
  
  ret_val = item_next_token(item, &token);
  assert_equal(1, ret_val);
  assert_equal(3, token.id);
  assert_equal(4, token.frequency);
  
  ret_val = item_next_token(item, &token);
  assert_equal(0, ret_val);
  assert_equal(0, token.id);
  assert_equal(0, token.frequency);
  
  free_item(item);
} END_TEST

START_TEST (iterate_over_null_item_doesnt_crash) {
  Token token;
  assert_equal(0, item_next_token(NULL, &token));
  assert_equal(0, token.id);
  assert_equal(0, token.frequency);
} END_TEST

/***** Item Source Test *****/
START_TEST (setup_item_source) {
  ItemSource *source = create_file_item_source("fixtures");
  assert_not_null(source);
  Item *item = is_fetch_item(source, 1234);
  assert_not_null(item);
  assert_equal(1234, item_get_id(item));
} END_TEST


Suite *
item_suite(void) {
  Suite *s = suite_create("ItemLoader");
  
  TCase *tc_item_loader = tcase_create("Item Loader");
  tcase_add_test(tc_item_loader, check_token_count);
  tcase_add_test(tc_item_loader, check_item_total_tokens);
  tcase_add_test(tc_item_loader, check_item_id);
  tcase_add_test(tc_item_loader, item_path_too_long);
  tcase_add_test(tc_item_loader, missing_item);
  tcase_add_test(tc_item_loader, check_item_path);
  tcase_add_test(tc_item_loader, check_item_creation);
  tcase_add_test(tc_item_loader, check_nonexistant_token_returns_0);
  tcase_add_test(tc_item_loader, passing_null_to_free_item_doesnt_crash);
  tcase_add_test(tc_item_loader, check_token_iterator);
  tcase_add_test(tc_item_loader, iterate_over_null_item_doesnt_crash);
  suite_add_tcase(s, tc_item_loader);  
  
  TCase *tc_item_source = tcase_create("Item Source");
  tcase_add_test(tc_item_source, setup_item_source);
  suite_add_tcase(s, tc_item_source);
  
  return s;
}
