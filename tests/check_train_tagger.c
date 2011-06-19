// General info: http://doc.winnowtag.org/open-source
// Source code repository: http://github.com/winnowtag
// Questions and feedback: contact@winnowtag.org
//
// Copyright (c) 2007-2011 The Kaphan Foundation
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in
// all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
// THE SOFTWARE.

// contact@winnowtag.org

#include <check.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "../src/tagger.h"
#include "assertions.h"
#include "fixtures.h"

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
  setup_fixture_path();
  read_document("fixtures/complete_tag.atom");
  system("rm -Rf /tmp/valid-copy && cp -Rf fixtures/valid /tmp/valid-copy && chmod -R 755 /tmp/valid-copy");
  item_cache_create(&item_cache, "/tmp/valid-copy", &item_cache_options);
}

static void setup_incomplete(void) {
  setup_fixture_path();
  read_document("fixtures/incomplete_tag.atom");
  system("rm -Rf /tmp/valid-copy && cp -Rf fixtures/valid /tmp/valid-copy && chmod -R 755 /tmp/valid-copy");
  item_cache_create(&item_cache, "/tmp/valid-copy", &item_cache_options);
}

static void teardown(void) {
  teardown_fixture_path();
  if (document) {
    free(document);
  }

  free_item_cache(item_cache);
}

/** Tests when all items are in the item cache */
START_TEST (test_training_a_tag_with_all_items_in_the_cache_returns_TAGGER_TRAINED) {
  Tagger *tagger = build_tagger(document, item_cache);
  TaggerState state = train_tagger(tagger, item_cache);
  assert_equal(TAGGER_TRAINED, state);
} END_TEST

START_TEST (test_training_a_tag_with_all_items_in_the_cache_sets_state_to_TAGGER_TRAINED) {
  Tagger *tagger = build_tagger(document, item_cache);
  train_tagger(tagger, item_cache);
  assert_equal(TAGGER_TRAINED, tagger->state);
} END_TEST

START_TEST (test_train_merges_examples_into_pools) {
  Tagger *tagger = build_tagger(document, item_cache);
  train_tagger(tagger, item_cache);

  assert_not_null(tagger->positive_pool);
  assert_equal(508, pool_num_tokens(tagger->positive_pool));
  assert_equal(965, pool_total_tokens(tagger->positive_pool));

  assert_not_null(tagger->negative_pool);
  assert_equal(54, pool_num_tokens(tagger->negative_pool));
  assert_equal(74, pool_total_tokens(tagger->negative_pool));
} END_TEST

START_TEST (test_training_twice_returns_SEQUENCE_ERROR) {
  Tagger *tagger = build_tagger(document, item_cache);
  train_tagger(tagger, item_cache);
  TaggerState state = train_tagger(tagger, item_cache);
  assert_equal(TAGGER_SEQUENCE_ERROR, state);
} END_TEST


/** Tests with missing items */

START_TEST (test_training_a_tag_with_missing_items_trains_items_exist) {
  Tagger *tagger = build_tagger(document, item_cache);
  train_tagger(tagger, item_cache);

  assert_not_null(tagger->positive_pool);
  assert_equal(180, pool_num_tokens(tagger->positive_pool));
  assert_equal(312, pool_total_tokens(tagger->positive_pool));

  assert_not_null(tagger->negative_pool);
  assert_equal(8, pool_num_tokens(tagger->negative_pool));
  assert_equal(8, pool_total_tokens(tagger->negative_pool));
} END_TEST

static ItemCache *missing_item_cache;

static void setup_item_cache_with_missing_items(void) {
  setup_fixture_path();
  item_cache_create(&missing_item_cache, "fixtures/valid-with-missing", &item_cache_options);
}

static void teardown_item_cache_with_missing_items(void) {
  teardown_fixture_path();
  free_item_cache(missing_item_cache);
}

START_TEST (test_retraining_with_item_cache_that_includes_missing_items_return_TAGGER_TRAINED) {
  Tagger *tagger = build_tagger(document, item_cache);
  int rc = train_tagger(tagger, item_cache);
  assert_equal(TAGGER_TRAINED, rc);
} END_TEST

START_TEST (test_retraining_with_item_cache_that_includes_missing_items_sets_state_to_TAGGER_TRAINED) {
  Tagger *tagger = build_tagger(document, item_cache);
  train_tagger(tagger, item_cache);
  assert_equal(TAGGER_TRAINED, tagger->state);
} END_TEST

START_TEST (test_retraining_with_item_cache_that_includes_missing_items_adds_those_items_to_the_pools) {
  Tagger *tagger = build_tagger(document, item_cache);
  train_tagger(tagger, item_cache);
  train_tagger(tagger, missing_item_cache);

  assert_not_null(tagger->positive_pool);
  assert_equal(180, pool_num_tokens(tagger->positive_pool));
  assert_equal(312, pool_total_tokens(tagger->positive_pool));

  assert_not_null(tagger->negative_pool);
  assert_equal(8, pool_num_tokens(tagger->negative_pool));
  assert_equal(8, pool_total_tokens(tagger->negative_pool));
} END_TEST

static void setup_item_cache_with_some_missing_items(void) {
  setup_fixture_path();
  item_cache_create(&missing_item_cache, "fixtures/valid-with-some-missing", &item_cache_options);
}

static void teardown_item_cache_with_some_missing_items(void) {
  teardown_fixture_path();
  free_item_cache(missing_item_cache);
}

START_TEST (test_retraining_with_item_cache_that_includes_some_missing_items_adds_those_items_to_the_pools) {
  Tagger *tagger = build_tagger(document, item_cache);
  train_tagger(tagger, item_cache);

  assert_not_null(tagger->positive_pool);
  assert_equal(180, pool_num_tokens(tagger->positive_pool));
  assert_equal(312, pool_total_tokens(tagger->positive_pool));

  assert_not_null(tagger->negative_pool);
  assert_equal(8, pool_num_tokens(tagger->negative_pool));
  assert_equal(8, pool_total_tokens(tagger->negative_pool));
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

  TCase *tc_incomplete_tag = tcase_create("incomplete_tag.atom");
  tcase_add_checked_fixture(tc_incomplete_tag, setup_incomplete, teardown);
  tcase_add_test(tc_incomplete_tag, test_training_a_tag_with_missing_items_trains_items_exist);

  TCase *tc_incomplete_tag_after_adding_items = tcase_create("incomplete_tag.atom after adding missing items");
  tcase_add_checked_fixture(tc_incomplete_tag_after_adding_items, setup_incomplete, teardown);
  tcase_add_checked_fixture(tc_incomplete_tag_after_adding_items, setup_item_cache_with_missing_items, teardown_item_cache_with_missing_items);
  tcase_add_test(tc_incomplete_tag_after_adding_items, test_retraining_with_item_cache_that_includes_missing_items_return_TAGGER_TRAINED);
  tcase_add_test(tc_incomplete_tag_after_adding_items, test_retraining_with_item_cache_that_includes_missing_items_sets_state_to_TAGGER_TRAINED);
  tcase_add_test(tc_incomplete_tag_after_adding_items, test_retraining_with_item_cache_that_includes_missing_items_adds_those_items_to_the_pools);

  TCase *tc_incomplete_tag_after_adding_some_items = tcase_create("incomplete_tag.atom after adding some missing items");
  tcase_add_checked_fixture(tc_incomplete_tag_after_adding_some_items, setup_incomplete, teardown);
  tcase_add_checked_fixture(tc_incomplete_tag_after_adding_some_items, setup_item_cache_with_some_missing_items, teardown_item_cache_with_some_missing_items);
  tcase_add_test(tc_incomplete_tag_after_adding_some_items, test_retraining_with_item_cache_that_includes_some_missing_items_adds_those_items_to_the_pools);

  suite_add_tcase(s, tc_complete_tag);
  suite_add_tcase(s, tc_incomplete_tag);
  suite_add_tcase(s, tc_incomplete_tag_after_adding_items);
  suite_add_tcase(s, tc_incomplete_tag_after_adding_some_items);
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
