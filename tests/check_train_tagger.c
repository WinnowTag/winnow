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

static void setup(void) {
  FILE *file;

  if (NULL != (file = fopen("fixtures/complete_tag.atom", "r"))) {
    fseek(file, 0, SEEK_END);
    int size = ftell(file);
    document = calloc(size, sizeof(char));
    fseek(file, 0, SEEK_SET);
    fread(document, sizeof(char), size, file);
    document[size] = 0;
    fclose(file);
  }
  
  item_cache_create(&item_cache, "fixtures/valid.db", &item_cache_options);
}

static void teardown(void) {
  if (document) {
    free(document);
  }
}

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

Suite *
tag_loading_suite(void) {
  Suite *s = suite_create("Tagger Training");  
  TCase *tc_complete_tag = tcase_create("complete_tag.atom");

  tcase_add_checked_fixture(tc_complete_tag, setup, teardown);
  tcase_add_test(tc_complete_tag, test_training_a_tag_with_all_items_in_the_cache_returns_TAGGER_TRAINED);
  tcase_add_test(tc_complete_tag, test_training_a_tag_with_all_items_in_the_cache_sets_state_to_TAGGER_TRAINED);
  tcase_add_test(tc_complete_tag, test_train_merges_examples_into_pools);

  suite_add_tcase(s, tc_complete_tag);
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
