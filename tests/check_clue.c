// Copyright (c) 2007 The Kaphan Foundation
//
// Possession of a copy of this file grants no permission or license
// to use, modify, or create derivate works.
//
// Please contact info@peerworks.org for further information.

#include <stdlib.h>
#include <check.h>
#include "../src/clue.h"
#include "assertions.h"

START_TEST (create_clue_from_token_id_and_prob) {
  Clue *clue = new_clue(1234, 0.95);
  assert_not_null(clue);
  assert_equal(1234, clue_token_id(clue));
  assert_equal_f(0.95, clue_probability(clue));
  assert_equal_f(0.45, clue_strength(clue));
  free_clue(clue);
} END_TEST

START_TEST (test_create_clue_list_not_null) {
  ClueList *clues = new_clue_list();
  assert_not_null(clues);
} END_TEST

START_TEST (test_new_clue_list_size_is_zero) {
  ClueList *clues = new_clue_list();
  assert_equal(0, clues->size);
} END_TEST

START_TEST (test_adding_clue_to_list_increments_size) {
  ClueList *clues = new_clue_list();
  add_clue(clues, 1234, 0.95);
  assert_equal(1, clues->size);
} END_TEST

START_TEST (test_adding_same_clue_to_list_twice_increments_size_once) {
  ClueList *clues = new_clue_list();
  add_clue(clues, 1234, 0.95);
  add_clue(clues, 1234, 0.95);
  assert_equal(1, clues->size);
} END_TEST

START_TEST (test_can_get_clue_by_token_id) {
  ClueList *clues = new_clue_list();
  add_clue(clues, 1234, 0.95);
  Clue *clue = get_clue(clues, 1234);
  assert_not_null(clue);
  assert_equal(1234, clue->token_id);
  assert_equal_f(0.95, clue->probability);
  assert_equal_f(0.45, clue->strength);
} END_TEST

Suite *
clue_suite(void) {
  Suite *s = suite_create("Clues");  
  TCase *tc_clue = tcase_create("Clue");

// START_TESTS
  tcase_add_test(tc_clue, create_clue_from_token_id_and_prob);
  tcase_add_test(tc_clue, test_create_clue_list_not_null);
  tcase_add_test(tc_clue, test_new_clue_list_size_is_zero);
  tcase_add_test(tc_clue, test_adding_clue_to_list_increments_size);
  tcase_add_test(tc_clue, test_adding_same_clue_to_list_twice_increments_size_once);
  tcase_add_test(tc_clue, test_can_get_clue_by_token_id);  
// END_TESTS

  suite_add_tcase(s, tc_clue);
  return s;
}

int main(void) {
  initialize_logging("test.log");
  int number_failed;
  
  SRunner *sr = srunner_create(clue_suite());
  srunner_run_all(sr, CK_NORMAL);
  number_failed = srunner_ntests_failed(sr);
  srunner_free(sr);
  close_log();
  return (number_failed == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}