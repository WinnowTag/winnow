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
  Item item;
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
  Item item;
  
  item = create_item_from_file(CORPUS, 1234);
  assert_equal(1234, item_get_id(item));
  free_item(item);
} END_TEST

START_TEST (check_item_total_tokens) {
  Item item;
  
  item = create_item_from_file(CORPUS, 1234);
  assert_equal(125, item_get_total_tokens(item));
  free_item(item);
} END_TEST

START_TEST (check_token_count) {
  Item item;
  Token token;
  int get_token_return;
  
  item = create_item_from_file(CORPUS, 1234);
  get_token_return = item_get_token(item, 253866, &token);
  
  assert_equal(1, get_token_return);
  assert_equal(253866, token.id);
  assert_equal(2, token.frequency);
  
  free_item(item);
} END_TEST

START_TEST (item_path_too_long) {
  Item item;
  int i;
  char corpus[1024];
  corpus[0] = '\0';
  for (i = 0; i < 1022; i++) {
    strncat(corpus, "a", 1024);
  }
  
  item = create_item_from_file(corpus, 12345);
  assert_equal(NULL, item);
} END_TEST

START_TEST (missing_item) {
  Item item;
  item = create_item_from_file(CORPUS, 123456);
  assert_equal(NULL, item);
} END_TEST

START_TEST (check_item_path) {
  Item item;
  item = create_item_from_file(CORPUS, 1234);
  assert_equal_s("fixtures/1234.tokens", item_get_path(item));
} END_TEST

Suite *
item_loader_suite(void) {
  Suite *s = suite_create("ItemLoader");
  
  TCase *tc_item_loader = tcase_create("Item Loader");
  tcase_add_test(tc_item_loader, check_token_count);
  tcase_add_test(tc_item_loader, check_item_total_tokens);
  tcase_add_test(tc_item_loader, check_item_id);
  tcase_add_test(tc_item_loader, item_path_too_long);
  tcase_add_test(tc_item_loader, missing_item);
  tcase_add_test(tc_item_loader, check_item_path);
  tcase_add_test(tc_item_loader, check_item_creation);
  suite_add_tcase(s, tc_item_loader);  
  
  return s;
}

int main(void) {
  int number_failed;
  Suite *s = item_loader_suite();
  SRunner *sr = srunner_create(s);
  srunner_run_all(sr, CK_NORMAL);
  number_failed = srunner_ntests_failed(sr);
  srunner_free(sr);
  return (number_failed == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}