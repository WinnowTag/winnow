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

#ifndef CORPUS
#define CORPUS "fixtures"
#endif

#ifndef assert_equal
#define assert_equal(expected, actual) fail_unless(expected == actual, "expected %d but got %d", expected, actual)
#endif

#ifndef assert_equal_s
#define assert_equal_s(e, a) fail_unless(strcmp(e,a) == 0, "expected %s but got %d", e, a)
#endif

START_TEST (check_item_id) {
  Item item;
  
  item = load_item(CORPUS, 1234);
  assert_equal(1234, item_get_id(item));
  free_item(item);
} END_TEST

START_TEST (check_item_token_list_length) {
  Item item;
  
  item = load_item(CORPUS, 1234);
  assert_equal(82, item_get_count(item));
  free_item(item);
} END_TEST

START_TEST (check_token_count) {
  Item item;
  Token token;
  int get_token_return;
  
  item = load_item(CORPUS, 1234);
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
  
  item = load_item(corpus, 12345);
  assert_equal(NULL, item);
} END_TEST

START_TEST (missing_item) {
  Item item;
  item = load_item(CORPUS, 123456);
  assert_equal(NULL, item);
} END_TEST

START_TEST (check_item_path) {
  Item item;
  item = load_item(CORPUS, 1234);
  assert_equal_s("fixtures/1234.tokens", item_get_path(item));
} END_TEST

Suite *
item_loader_suite(void) {
  Suite *s = suite_create("ItemLoader");
  
  TCase *tc_item_loader = tcase_create("Item Loader");
  tcase_add_test(tc_item_loader, check_token_count);
  tcase_add_test(tc_item_loader, check_item_token_list_length);
  tcase_add_test(tc_item_loader, check_item_id);
  tcase_add_test(tc_item_loader, item_path_too_long);
  tcase_add_test(tc_item_loader, missing_item);
  tcase_add_test(tc_item_loader, check_item_path);
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