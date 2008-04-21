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
#include "../src/tagger.h"
#include "assertions.h"

static char * document;
static ItemCacheOptions item_cache_options = {1, 3650, 2};
static ItemCache *item_cache;

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

static void setup_complete(void) {
  read_document("fixtures/complete_tag.atom");
  item_cache_create(&item_cache, "fixtures/valid.db", &item_cache_options);
}

static void setup_incomplete(void) {
  read_document("fixtures/incomplete_tag.atom");
  item_cache_create(&item_cache, "fixtures/valid.db", &item_cache_options);
}

static void teardown(void) {
  if (document) {
    free(document);
  }
}

/** Tests when all items are in the item cache */
START_TEST (test_training_a_tag_with_all_items_in_the_cache_returns_TAGGER_TRAINED) {
  Tagger *tagger = build_tagger(document);
  TaggerState state = train_tagger(tagger, item_cache);
  assert_equal(TAGGER_TRAINED, state);
} END_TEST

START_TEST (test_training_a_tag_with_all_items_in_the_cache_sets_state_to_TAGGER_TRAINED) {
  Tagger *tagger = build_tagger(document);
  train_tagger(tagger, item_cache);
  assert_equal(TAGGER_TRAINED, tagger->state);
} END_TEST

START_TEST (test_train_merges_examples_into_pools) {
  Tagger *tagger = build_tagger(document);
  train_tagger(tagger, item_cache);
  
  assert_not_null(tagger->positive_pool);
  assert_equal(508, pool_num_tokens(tagger->positive_pool));
  assert_equal(965, pool_total_tokens(tagger->positive_pool));
  
  assert_not_null(tagger->negative_pool);
  assert_equal(54, pool_num_tokens(tagger->negative_pool));
  assert_equal(74, pool_total_tokens(tagger->negative_pool));  
} END_TEST

START_TEST (test_training_twice_returns_SEQUENCE_ERROR) {
  Tagger *tagger = build_tagger(document);
  train_tagger(tagger, item_cache);
  TaggerState state = train_tagger(tagger, item_cache);
  assert_equal(TAGGER_SEQUENCE_ERROR, state);
} END_TEST

START_TEST (test_training_a_tagger_with_all_items_in_the_cache_has_zero_missing_item_counts) {
  Tagger *tagger = build_tagger(document);
  train_tagger(tagger, item_cache);
  assert_equal(0, tagger->missing_positive_example_count);
  assert_equal(0, tagger->missing_negative_example_count);
} END_TEST

START_TEST (test_completely_trained_tag_returns_no_missing_items) {
  Tagger *tagger = build_tagger(document);
  train_tagger(tagger, item_cache);
  ItemCacheEntry *entries[3];
  memset(entries, 0, sizeof(entries));
  int rc = get_missing_entries(tagger, entries);
  assert_equal(1, rc);
  assert_null(entries[0]);
  assert_null(entries[1]);
  assert_null(entries[2]);
} END_TEST

/** Tests with missing items */
START_TEST (test_training_a_tag_with_missing_items_returns_TAGGER_PARTIALLY_TRAINED) {
  Tagger *tagger = build_tagger(document);
  TaggerState state = train_tagger(tagger, item_cache);
  assert_equal(TAGGER_PARTIALLY_TRAINED, state);
} END_TEST

START_TEST (test_training_a_tag_with_missing_items_sets_state_to_TAGGER_PARTIALLY_TRAINED) {
  Tagger *tagger = build_tagger(document);
  train_tagger(tagger, item_cache);
  assert_equal(TAGGER_PARTIALLY_TRAINED, tagger->state);
} END_TEST

START_TEST (test_training_a_tag_with_missing_items_trains_items_exist) {
  Tagger *tagger = build_tagger(document);
  train_tagger(tagger, item_cache);
  
  assert_not_null(tagger->positive_pool);
  assert_equal(171, pool_num_tokens(tagger->positive_pool));
  assert_equal(303, pool_total_tokens(tagger->positive_pool));
  
  assert_not_null(tagger->negative_pool);
  assert_equal(0, pool_num_tokens(tagger->negative_pool));
  assert_equal(0, pool_total_tokens(tagger->negative_pool));
} END_TEST

START_TEST (test_training_a_tag_with_missing_items_stores_missing_positives) {
  Tagger *tagger = build_tagger(document);
  train_tagger(tagger, item_cache);
  assert_equal(2, tagger->missing_positive_example_count);
  assert_equal_s("urn:peerworks.org:entry#2", tagger->missing_positive_examples[0]);
  assert_equal_s("urn:peerworks.org:entry#3", tagger->missing_positive_examples[1]);
} END_TEST

START_TEST (test_training_a_tag_with_missing_items_stores_missing_negatives) {
  Tagger *tagger = build_tagger(document);
  train_tagger(tagger, item_cache);
  assert_equal(1, tagger->missing_negative_example_count);
  assert_equal_s("urn:peerworks.org:entry#1", tagger->missing_negative_examples[0]);
} END_TEST

START_TEST (test_can_get_the_missing_item_cache_entries) {
  Tagger *tagger = build_tagger(document);
  train_tagger(tagger, item_cache);
  ItemCacheEntry *entries[3];
  memset(entries, 0, sizeof(entries));
  get_missing_entries(tagger, entries);
  assert_not_null(entries[0]);
  assert_not_null(entries[1]);
  assert_not_null(entries[2]);
} END_TEST

START_TEST (test_missing_item_cache_entries_include_the_ids) {
  Tagger *tagger = build_tagger(document);
  train_tagger(tagger, item_cache);
  ItemCacheEntry *entries[3];
  memset(entries, 0, sizeof(entries));
  get_missing_entries(tagger, entries);
  assert_equal_s("urn:peerworks.org:entry#2", item_cache_entry_full_id(entries[0]));
  assert_equal_s("urn:peerworks.org:entry#3", item_cache_entry_full_id(entries[1]));
  assert_equal_s("urn:peerworks.org:entry#1", item_cache_entry_full_id(entries[2]));
} END_TEST

START_TEST (test_missing_item_cache_entries_include_the_titles) {
  Tagger *tagger = build_tagger(document);
  train_tagger(tagger, item_cache);
  ItemCacheEntry *entries[3];
  memset(entries, 0, sizeof(entries));
  get_missing_entries(tagger, entries);
  assert_equal_s("a more secularized religion", item_cache_entry_title(entries[0]));
  assert_equal_s("Secular Faith", item_cache_entry_title(entries[1]));
  assert_equal_s("Sensible Priors for Sparse Bayesian Learning", item_cache_entry_title(entries[2]));
} END_TEST

Suite *
tag_loading_suite(void) {
  Suite *s = suite_create("Tagger Training");  
  
  TCase *tc_complete_tag = tcase_create("complete_tag.atom");
  tcase_add_checked_fixture(tc_complete_tag, setup_complete, teardown);
  tcase_add_test(tc_complete_tag, test_training_a_tag_with_all_items_in_the_cache_returns_TAGGER_TRAINED);
  tcase_add_test(tc_complete_tag, test_training_a_tag_with_all_items_in_the_cache_sets_state_to_TAGGER_TRAINED);
  tcase_add_test(tc_complete_tag, test_train_merges_examples_into_pools);
  tcase_add_test(tc_complete_tag, test_training_twice_returns_SEQUENCE_ERROR);
  tcase_add_test(tc_complete_tag, test_training_a_tagger_with_all_items_in_the_cache_has_zero_missing_item_counts);
  tcase_add_test(tc_complete_tag, test_completely_trained_tag_returns_no_missing_items);

  TCase *tc_incomplete_tag = tcase_create("incomplete_tag.atom");
  tcase_add_checked_fixture(tc_incomplete_tag, setup_incomplete, teardown);
  tcase_add_test(tc_incomplete_tag, test_training_a_tag_with_missing_items_returns_TAGGER_PARTIALLY_TRAINED);
  tcase_add_test(tc_incomplete_tag, test_training_a_tag_with_missing_items_sets_state_to_TAGGER_PARTIALLY_TRAINED);
  tcase_add_test(tc_incomplete_tag, test_training_a_tag_with_missing_items_trains_items_exist);
  tcase_add_test(tc_incomplete_tag, test_training_a_tag_with_missing_items_stores_missing_positives);
  tcase_add_test(tc_incomplete_tag, test_training_a_tag_with_missing_items_stores_missing_negatives);
  tcase_add_test(tc_incomplete_tag, test_can_get_the_missing_item_cache_entries);
  tcase_add_test(tc_incomplete_tag, test_missing_item_cache_entries_include_the_ids);
  tcase_add_test(tc_incomplete_tag, test_missing_item_cache_entries_include_the_titles);  
  
  suite_add_tcase(s, tc_complete_tag);
  suite_add_tcase(s, tc_incomplete_tag);
  return s;
}

int main(void) {
  initialize_logging("test.log");
  int number_failed;
  
  SRunner *sr = srunner_create(tag_loading_suite());
  srunner_run_all(sr, CK_NORMAL);
  number_failed = srunner_ntests_failed(sr);
  srunner_free(sr);
  close_log();
  return (number_failed == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}
