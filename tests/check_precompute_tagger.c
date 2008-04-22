/* Copyright (c) 2007 The Kaphan Foundation
 *
 * Possession of a copy of this file grants no permission or license
 * to use, modify, or create derivate works.
 *
 * Please contact info@peerworks.org for further information.
 */
 
#include <check.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sqlite3.h>
#include "../src/tagger.h"
#include "assertions.h"

static char * document;
static ItemCacheOptions item_cache_options = {1, 3650, 2};
static ItemCache *item_cache;
static Tagger *tagger;
static Pool *random_background;

static void read_document(const char * filename) {
  FILE *file;

  if (NULL != (file = fopen(filename, "r"))) {
    fseek(file, 0, SEEK_END);
    int size = ftell(file);
    document = calloc(size, sizeof(char));
    fseek(file, 0, SEEK_SET);
    fread(document, sizeof(char), size, file);
    document[size] = 0;
    fclose(file);
  }
}

static double probability_function(const Pool *positive, const Pool *negative, const Pool *random_bg, int token_id, double bias) {
  return 0.75;
}

static void setup(void) {
  read_document("fixtures/complete_tag.atom");
  item_cache_create(&item_cache, "fixtures/valid.db", &item_cache_options);
  tagger = build_tagger(document);
  train_tagger(tagger, item_cache);
  tagger->probability_function = &probability_function;
  assert_equal(TAGGER_TRAINED, tagger->state);
  random_background = new_pool();  
}

static void teardown(void) {
  if (document) {
   free(document);
  }

  free_item_cache(item_cache);
}

// START_TEST (precompute_keeps_user_and_tag) {
//   Pool *random_background = new_pool();
//   TrainedClassifier tc;
//   tc.user = "user";
//   tc.tag_name = "tag";
//   tc.user_id = 34;
//   tc.tag_id = 56;
//   tc.positive_pool = NULL;
//   tc.negative_pool = NULL;
//   Classifier *classifier = precompute(&tc, random_background);
//   assert_not_null(classifier);
//   assert_equal_s("user", cls_user(classifier));
//   assert_equal_s("tag", cls_tag_name(classifier));
//   assert_equal(34, cls_user_id(classifier));
//   assert_equal(56, cls_tag_id(classifier));
//   free_pool(random_background);
// } END_TEST

START_TEST (test_precompute_with_trained_tagger_returns_TAGGER_PRECOMPUTED) {
  int rc = precompute_tagger(tagger, random_background);
  assert_equal(TAGGER_PRECOMPUTED, rc);
} END_TEST

START_TEST (test_precompute_with_trained_tagger_sets_state_to_TAGGER_PRECOMPUTED) {
  precompute_tagger(tagger, random_background);
  assert_equal(TAGGER_PRECOMPUTED, tagger->state);
} END_TEST

START_TEST (test_precomputing_a_non_trained_tagger_results_in_a_sequence_error) {
  tagger->state = TAGGER_PARTIALLY_TRAINED;
  int rc = precompute_tagger(tagger, random_background);
  assert_equal(TAGGER_SEQUENCE_ERROR, rc);
} END_TEST


START_TEST (precompute_creates_probabilities_for_each_token_in_the_pools) {  
  precompute_tagger(tagger, random_background);
  assert_not_null(tagger->clues);
  assert_equal(542, tagger->clues->size);
} END_TEST

START_TEST (test_after_precompute_there_are_clues_for_every_token_in_the_pool) {
  precompute_tagger(tagger, random_background);
  
  sqlite3 *db;
  sqlite3_stmt *stmt;
  sqlite3_open_v2("/tmp/valid-copy.db", &db, SQLITE_OPEN_READONLY, NULL);
  sqlite3_prepare_v2(db, "select distinct token_id from entry_tokens where entry_id in (753459, 880389, 886294, 888769, 884409)", -1, &stmt, NULL);
  
  int clues = 0;
  while (SQLITE_ROW == sqlite3_step(stmt)) {
    int token = sqlite3_column_int(stmt, 0);
    Clue *clue = get_clue(tagger->clues, token);
    if (clue) {
      clues++;
      // should be what is returned by the probability function
      assert_equal_f(0.75, clue->probability);
    }
  }
  
  assert_equal(542, clues);
} END_TEST

  // assert_between_ex(0.0, 1.0, cls_probability_for(cls, 1));
  // assert_between_ex(0.0, 1.0, cls_probability_for(cls, 2));
  // assert_between_ex(0.0, 1.0, cls_probability_for(cls, 3));
  // assert_equal(0.5, cls_probability_for(cls, 4));
  // 
  // free_classifier(cls);
  // free_pool(random_background);

START_TEST (test_precompute_clears_out_training) {
  precompute_tagger(tagger, random_background);
  assert_null(tagger->positive_pool);
  assert_null(tagger->negative_pool);
} END_TEST

Suite *
tag_precompute_suite(void) {
  Suite *s = suite_create("Tagger Precompute");  

  TCase *tc_precomputer = tcase_create("Precomputer");
  tcase_add_checked_fixture(tc_precomputer, setup, teardown);
  tcase_add_test(tc_precomputer, test_precompute_with_trained_tagger_returns_TAGGER_PRECOMPUTED);
  tcase_add_test(tc_precomputer, test_precompute_with_trained_tagger_sets_state_to_TAGGER_PRECOMPUTED);
  tcase_add_test(tc_precomputer, test_precomputing_a_non_trained_tagger_results_in_a_sequence_error);
  tcase_add_test(tc_precomputer, precompute_creates_probabilities_for_each_token_in_the_pools);
  tcase_add_test(tc_precomputer, test_precompute_clears_out_training);
  tcase_add_test(tc_precomputer, test_after_precompute_there_are_clues_for_every_token_in_the_pool);

  suite_add_tcase(s, tc_precomputer);
  return s;
}

int main(void) {
  initialize_logging("test.log");
  int number_failed;

  SRunner *sr = srunner_create(tag_precompute_suite());
  srunner_run_all(sr, CK_NORMAL);
  number_failed = srunner_ntests_failed(sr);
  srunner_free(sr);
  close_log();
  return (number_failed == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}
