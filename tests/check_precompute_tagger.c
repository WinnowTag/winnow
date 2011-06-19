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
#include <sqlite3.h>
#include "../src/tagger.h"
#include "../src/classifier.h"
#include "assertions.h"
#include "fixtures.h"

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
  setup_fixture_path();
  read_document("fixtures/complete_tag.atom");
  system("rm -Rf /tmp/valid-copy && cp -R fixtures/valid /tmp/valid-copy && chmod -R 755 /tmp/valid-copy");
  item_cache_create(&item_cache, "/tmp/valid-copy", &item_cache_options);
  tagger = build_tagger(document, item_cache);
  train_tagger(tagger, item_cache);
  tagger->probability_function = &probability_function;
  assert_equal(TAGGER_TRAINED, tagger->state);
  random_background = new_pool();
}

static void setup_with_random_background(void) {
  setup();
  int freeit;
  Item *item = item_cache_fetch_item(item_cache, (unsigned char*) "urn:peerworks.org:entry#709254", &freeit);
  pool_add_item(random_background, item);
}

static void teardown(void) {
  teardown_fixture_path();
  if (document) {
   free(document);
  }

  free_item_cache(item_cache);
}

START_TEST (test_precompute_with_trained_tagger_returns_TAGGER_PRECOMPUTED) {
  int rc = precompute_tagger(tagger, random_background);
  assert_equal(TAGGER_PRECOMPUTED, rc);
} END_TEST

START_TEST (test_precompute_with_trained_tagger_sets_state_to_TAGGER_PRECOMPUTED) {
  precompute_tagger(tagger, random_background);
  assert_equal(TAGGER_PRECOMPUTED, tagger->state);
} END_TEST

START_TEST (test_precomputing_a_tagger_with_no_probability_function_results_in_a_sequence_error) {
  tagger->probability_function = NULL;
  int rc = precompute_tagger(tagger, random_background);
  assert_equal(TAGGER_SEQUENCE_ERROR, rc);
} END_TEST

START_TEST (precompute_creates_probabilities_for_each_token_in_the_pools) {
  precompute_tagger(tagger, random_background);
  assert_not_null(tagger->clues);
  assert_equal(542, tagger->clues->size);
} END_TEST

// TODO START_TEST (test_after_precompute_there_are_clues_for_every_token_in_the_pool) {
//  precompute_tagger(tagger, random_background);
//
//  sqlite3 *db;
//  sqlite3_stmt *stmt;
//  sqlite3_open_v2("fixtures/valid.db", &db, SQLITE_OPEN_READONLY, NULL);
//  sqlite3_prepare_v2(db, "select distinct token_id from entry_tokens where entry_id in (753459, 880389, 886294, 888769, 884409)", -1, &stmt, NULL);
//
//  int clues = 0;
//  while (SQLITE_ROW == sqlite3_step(stmt)) {
//    int token = sqlite3_column_int(stmt, 0);
//    Clue *clue = get_clue(tagger->clues, token);
//    if (clue) {
//      clues++;
//      // should be what is returned by the probability function
//      assert_equal_f(0.75, clue->probability);
//    }
//  }
//
//  assert_equal(542, clues);
//} END_TEST

START_TEST (test_precompute_clears_out_training) {
  precompute_tagger(tagger, random_background);
  assert_null(tagger->positive_pool);
  assert_null(tagger->negative_pool);
} END_TEST

// TODO START_TEST (test_precompute_with_random_background_includes_tokens_in_the_random_background) {
//  precompute_tagger(tagger, random_background);
//
//  sqlite3 *db;
//  sqlite3_stmt *stmt;
//  sqlite3_open_v2("fixtures/valid.db", &db, SQLITE_OPEN_READONLY, NULL);
//  sqlite3_prepare_v2(db, "select distinct token_id from entry_tokens where entry_id in (709254, 753459, 880389, 886294, 888769, 884409)", -1, &stmt, NULL);
//
//  int clues = 0;
//  while (SQLITE_ROW == sqlite3_step(stmt)) {
//    int token = sqlite3_column_int(stmt, 0);
//    Clue *clue = get_clue(tagger->clues, token);
//    if (clue) {
//      clues++;
//      // should be what is returned by the probability function
//      assert_equal_f(0.75, clue->probability);
//    }
//  }
//
//  assert_equal(606, clues);
//} END_TEST

// TODO START_TEST (test_make_sure_it_works_with_naive_bayes_probability_function) {
//  tagger->probability_function = &naive_bayes_probability;
//  precompute_tagger(tagger, random_background);
//
//  sqlite3 *db;
//  sqlite3_stmt *stmt;
//  sqlite3_open_v2("fixtures/valid.db", &db, SQLITE_OPEN_READONLY, NULL);
//  sqlite3_prepare_v2(db, "select distinct token_id from entry_tokens where entry_id in (709254, 753459, 880389, 886294, 888769, 884409)", -1, &stmt, NULL);
//
//  int clues = 0;
//  while (SQLITE_ROW == sqlite3_step(stmt)) {
//    int token = sqlite3_column_int(stmt, 0);
//    Clue *clue = get_clue(tagger->clues, token);
//    if (clue) {
//      clues++;
//      assert_between_ex(0.0, 1.0, clue->probability);
//    }
//  }
//
//  assert_equal(606, clues);
//} END_TEST

Suite *
tag_precompute_suite(void) {
  Suite *s = suite_create("Tagger Precompute");

  TCase *tc_precomputer = tcase_create("Precomputer");
  tcase_add_checked_fixture(tc_precomputer, setup, teardown);
  tcase_add_test(tc_precomputer, test_precompute_with_trained_tagger_returns_TAGGER_PRECOMPUTED);
  tcase_add_test(tc_precomputer, test_precompute_with_trained_tagger_sets_state_to_TAGGER_PRECOMPUTED);
  tcase_add_test(tc_precomputer, precompute_creates_probabilities_for_each_token_in_the_pools);
  tcase_add_test(tc_precomputer, test_precompute_clears_out_training);
  //tcase_add_test(tc_precomputer, test_after_precompute_there_are_clues_for_every_token_in_the_pool);
  tcase_add_test(tc_precomputer, test_precomputing_a_tagger_with_no_probability_function_results_in_a_sequence_error);

  TCase *tc_precomputer_with_rnd = tcase_create("Precomputer with random background");
  tcase_add_checked_fixture(tc_precomputer_with_rnd, setup_with_random_background, teardown);
 // tcase_add_test(tc_precomputer_with_rnd, test_precompute_with_random_background_includes_tokens_in_the_random_background);
 // tcase_add_test(tc_precomputer_with_rnd, test_make_sure_it_works_with_naive_bayes_probability_function);

  suite_add_tcase(s, tc_precomputer);
  suite_add_tcase(s, tc_precomputer_with_rnd);
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
