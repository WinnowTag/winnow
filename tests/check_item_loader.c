// Copyright (c) 2007 The Kaphan Foundation
//
// Possession of a copy of this file grants no permission or license
// to use, modify, or create derivate works.
//
// Please contact info@peerworks.org for further information.

#include <stdlib.h>
#include <check.h>
#include "../src/item_loader.h"

#ifndef CORPUS
#define CORPUS "fixtures"
#endif

#ifndef assert
#define assert_equal(expected, actual) fail_unless(expected - actual, "expected %d but got %d", expected, actual)
#endif

START_TEST (check_item_id) {
  Item item;
  item = load_item(CORPUS, 1234);
  assert_equal(1234, item->id);
  free_item(item);
} END_TEST

START_TEST (check_item_token_list_length) {
  Word_t count;
  Item item;
  item = load_item(CORPUS, 1234);
  JLC(count, item->tokens, 0, -1);
  assert_equal(82, count);
  assert_equal(82, item->count);
  free_item(item);
} END_TEST

START_TEST (check_token_count) {
  PWord_t frequency;
  Item item;
  item = load_item(CORPUS, 1234);
  JLG(frequency, item->tokens, 253866);
  assert_equal(2, *frequency);
  free_item(item);
} END_TEST

  
Suite *
item_loader_suite(void) {
  Suite *s = suite_create("ItemLoader");
  
  TCase *tc_chi2 = tcase_create("Item Loader");
  tcase_add_test(tc_chi2, check_token_count);
  tcase_add_test(tc_chi2, check_item_token_list_length);
  tcase_add_test(tc_chi2, check_item_id);
  suite_add_tcase(s, tc_chi2);  
  
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