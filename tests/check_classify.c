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
#include "assertions.h"
#include "fixtures.h"
#include "../src/tagger.h"
#include "../src/classifier.h"

static Tagger *tagger;
static char * document;
static ItemCacheOptions item_cache_options = {1, 3650, 2};
static ItemCache *item_cache;
static Tagger *tagger;
static Pool *random_background;
static const Item *classified_item = NULL;
static Item *item = NULL;

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

static double mock_classify(const ClueList *clues, const Item *item) {
  classified_item = item;
  return 0.75;
}

static void setup(void) {
  setup_fixture_path();
  read_document("fixtures/complete_tag.atom");
  random_background = new_pool();
  item_cache_create(&item_cache, "fixtures/valid.db", &item_cache_options);
  
  tagger = build_tagger(document);
  train_tagger(tagger, item_cache);
  tagger->probability_function = &naive_bayes_probability;
  tagger->classification_function = &mock_classify;
  precompute_tagger(tagger, random_background);
  assert_equal(TAGGER_PRECOMPUTED, tagger->state);
  
  classified_item = NULL;
  int freeit;
  item = item_cache_fetch_item(item_cache, (unsigned char*) "urn:peerworks.org:entry#709254", &freeit);
}

static void teardown(void) {
  teardown_fixture_path();
  if (document) {
   free(document);
  }

  free_item_cache(item_cache);
}

START_TEST (test_classify_item_when_not_in_precompute_state_returns_SEQUENCE_ERROR) {
  double prob;
  tagger->state = TAGGER_TRAINED;
  int rc = classify_item(tagger, item, &prob);
  assert_equal(TAGGER_SEQUENCE_ERROR, rc);
} END_TEST

START_TEST (test_classify_item_when_not_in_precompute_state_returns_TAGGER_OK) {
  double prob;
  int rc = classify_item(tagger, item, &prob);
  assert_equal(TAGGER_OK, rc);
} END_TEST

START_TEST (test_classify_item_when_prob_is_NULL_does_nothing) {
  classify_item(tagger, item, NULL);
  assert_null(classified_item);
} END_TEST

START_TEST (test_classify_passed_item_to_the_classifcation_function) {
  double prob;
  classify_item(tagger, item, &prob);
  assert_equal(item, classified_item);
} END_TEST

START_TEST (test_classify_item_sets_prob_to_the_return_value_of_the_classify_function) {
  double prob;
  classify_item(tagger, item, &prob);
  assert_equal_f(0.75, prob);
} END_TEST

START_TEST (test_classify_item_with_no_classification_function_returns_SEQUENCE_ERROR) {
  tagger->classification_function = NULL;
  double prob;
  TaggerState state = classify_item(tagger, item, &prob);
  assert_equal(TAGGER_SEQUENCE_ERROR, state);
} END_TEST

Suite *
check_classify_suite(void) {
  Suite *s = suite_create("check classify");  
  TCase *tc_mock_classification = tcase_create("mock classification");

  tcase_add_checked_fixture(tc_mock_classification, setup, teardown);
  tcase_add_test(tc_mock_classification, test_classify_item_sets_prob_to_the_return_value_of_the_classify_function);
  tcase_add_test(tc_mock_classification, test_classify_item_when_prob_is_NULL_does_nothing);
  tcase_add_test(tc_mock_classification, test_classify_item_when_not_in_precompute_state_returns_SEQUENCE_ERROR);
  tcase_add_test(tc_mock_classification, test_classify_passed_item_to_the_classifcation_function);
  tcase_add_test(tc_mock_classification, test_classify_item_with_no_classification_function_returns_SEQUENCE_ERROR);
  
  suite_add_tcase(s, tc_mock_classification);
  return s;
}

int main(void) {
  initialize_logging("test.log");
  int number_failed;

  SRunner *sr = srunner_create(check_classify_suite());
  srunner_run_all(sr, CK_NORMAL);
  number_failed = srunner_ntests_failed(sr);
  srunner_free(sr);
  close_log();
  return (number_failed == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}
