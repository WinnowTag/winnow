// Copyright (c) 2007 The Kaphan Foundation
//
// Possession of a copy of this file grants no permission or license
// to use, modify, or create derivate works.
//
// Please contact info@peerworks.org for further information.

#include <stdlib.h>
#include <math.h>
#include <check.h>
#include "../src/classifier.h"
#include "../src/tag.h"
#include "../src/clue.h"
#include "assertions.h"
#include "mock_items.h"
#include "fixtures.h"

/*************************************************************
 *   Unit tests for trainer
 *
 *************************************************************/
 
ItemCacheOptions item_cache_options;
ItemCache *item_cache;

void setup_train(void) {
  item_cache_options.cache_update_wait_time = 1;
  setup_fixture_path();
  item_cache_create(&item_cache, "fixtures/valid.db", &item_cache_options);
}

void teardown_train(void) {
  teardown_fixture_path();
  free_item_cache(item_cache);
}

START_TEST (train_merges_examples_into_pools) {
  TagList *tags = load_tags_from_file("fixtures", "mock");
  assert_not_null(tags);
  const Tag *tag = tags->tags[0];
  assert_not_null(tag);
  
  TrainedClassifier *trained = train(tag, item_cache);
  assert_not_null(trained);
  
  const Pool *positive = tc_get_positive_pool(trained);
  assert_not_null(positive);
  assert_equal(196, pool_num_tokens(positive));
  assert_equal(323, pool_total_tokens(positive));
  
  const Pool *negative = tc_get_negative_pool(trained);
  assert_not_null(negative);
  assert_equal(754, pool_num_tokens(negative));
  assert_equal(1668, pool_total_tokens(negative));
  
  tc_free(trained);
} END_TEST


START_TEST (train_keeps_user_and_tag) {
  Tag *tag = create_tag("mock", "tag", 34, 56);
  assert_not_null(tag);
  tag->bias = 1.2;
  
  TrainedClassifier *trained = train(tag, item_cache);
  assert_not_null(trained);
  
  assert_equal_s("mock", tc_get_user(trained));
  assert_equal_s("tag",  tc_get_tag_name(trained));
  assert_equal(34, tc_get_user_id(trained));
  assert_equal(56, tc_get_tag_id(trained));
  assert_equal_f(1.2, tc_get_bias(trained));
  
  tc_free(trained);
} END_TEST

/*************************************************************
 *   Unit tests for precomputing
 *
 *************************************************************/
START_TEST (precompute_keeps_user_and_tag) {
  Pool *random_background = new_pool();
  TrainedClassifier tc;
  tc.user = "user";
  tc.tag_name = "tag";
  tc.user_id = 34;
  tc.tag_id = 56;
  tc.positive_pool = NULL;
  tc.negative_pool = NULL;
  Classifier *classifier = precompute(&tc, random_background);
  assert_not_null(classifier);
  assert_equal_s("user", cls_user(classifier));
  assert_equal_s("tag", cls_tag_name(classifier));
  assert_equal(34, cls_user_id(classifier));
  assert_equal(56, cls_tag_id(classifier));
  free_pool(random_background);
} END_TEST

START_TEST (precompute_creates_probabilities_for_each_token_in_tc) {
  Pool *random_background = new_pool();
  TrainedClassifier tc;
  tc.user = "user";
  tc.tag_name = "tag";
  tc.positive_pool = new_pool();
  tc.negative_pool = new_pool();
  pool_add_item(tc.positive_pool, item_1);
  pool_add_item(tc.negative_pool, item_2);
  Classifier *cls = precompute(&tc, random_background);
  
  assert_not_null(cls);
  assert_equal(3, cls_num_clues(cls));
  assert_between_ex(0.0, 1.0, cls_probability_for(cls, 1));
  assert_between_ex(0.0, 1.0, cls_probability_for(cls, 2));
  assert_between_ex(0.0, 1.0, cls_probability_for(cls, 3));
  assert_equal(0.5, cls_probability_for(cls, 4));
  
  free_classifier(cls);
  free_pool(random_background);
} END_TEST

START_TEST (test_with_bias) {
  int tokens_1[][2] = {1, 5, 2, 15};
  int tokens_2[][2] = {1, 5, 2, 5};
  Item *i1 = create_item_with_tokens(1, tokens_1, 2);
  Item *i2 = create_item_with_tokens(2, tokens_2, 2);
  
  Pool *rb = new_pool();
  TrainedClassifier tc;
  tc.user = "user";
  tc.tag_name = "tag";
  tc.bias = 1.0;
  tc.positive_pool = new_pool();
  tc.negative_pool = new_pool();
  pool_add_item(tc.positive_pool, i1);
  pool_add_item(tc.negative_pool, i2);
  
  Classifier *cls = precompute(&tc, rb);
  tc.bias = 1.1;
  Classifier *cls_biased = precompute(&tc, rb);
    
  assert_equal_f(0.33912483912, cls_probability_for(cls, 1));
  assert_equal_f(0.383957, cls_probability_for(cls_biased, 1));
  
  free_classifier(cls);
  free_classifier(cls_biased);
  free_pool(rb);
  free_pool(tc.positive_pool);
  free_pool(tc.negative_pool);
  free_item(i1);
  free_item(i2);
} END_TEST

#define TOKEN_PROBS(pc, ps, nc, ns, bc, bs)         \
      ProbToken positive, negative, random; \
      positive.token_count = pc;                    \
      positive.pool_size = ps;                      \
      negative.token_count = nc;                    \
      negative.pool_size = ns;                      \
      random.token_count = bc;                      \
      random.pool_size = bs;                        \
      const ProbToken *foregrounds[] = {&positive};        \
      const ProbToken *backgrounds[] = {&negative, &random};\
      int foreground_size = ps;                      \
      int background_size = ns + bs;                 \

START_TEST (probability_1) {
  TOKEN_PROBS(5,20,5,10,0,0)  
  float prob = probability(foregrounds, 1, backgrounds, 2, foreground_size, background_size);
  assert_equal_f(0.33912483912, prob);
} END_TEST

START_TEST (probability_2) {
  TOKEN_PROBS(5,20,5,10,0,15)  
  float prob = probability(foregrounds, 1, backgrounds, 2, foreground_size, background_size);
  assert_equal_f(0.33782435130, prob);
} END_TEST

START_TEST (probability_3) {
  TOKEN_PROBS(5,20,5,10,10,80)  
  float prob = probability(foregrounds, 1, backgrounds, 2, foreground_size, background_size);
  assert_equal_f(0.44530060883, prob);
} END_TEST

START_TEST (probability_4) {
  TOKEN_PROBS(0,0,5,10,0,0)  
  float prob = probability(foregrounds, 1, backgrounds, 2, foreground_size, background_size);
  assert_equal_f(0.23684210526, prob);
} END_TEST

START_TEST (probability_5) {
  TOKEN_PROBS(5,20,0,0,0,0)  
  float prob = probability(foregrounds, 1, backgrounds, 2, foreground_size, background_size);
  assert_equal_f(0.67857142857, prob);
} END_TEST

START_TEST (probability_6) {
  TOKEN_PROBS(5,20,5,20,0,0)  
  float prob = probability(foregrounds, 1, backgrounds, 2, foreground_size, background_size);
  assert_equal_f(0.5, prob);
} END_TEST

START_TEST (probability_7) {
  TOKEN_PROBS(0,0,0,0,0,0)  
  float prob = probability(foregrounds, 1, backgrounds, 2, foreground_size, background_size);
  assert_equal_f(0.5, prob);
} END_TEST

/*************************************************************
 *   Unit tests for classification
 *
 *************************************************************/
struct CLASSIFIER classifier;
#define assert_tagging(u, t, uid, tid, s, tagging)  \
      assert_not_null(tagging);                     \
      assert_equal(uid, tagging_user_id(tagging));  \
      assert_equal(tid, tagging_tag_id(tagging));   \
      assert_equal_s(u, tagging_user(tagging));     \
      assert_equal_s(t, tagging_tag_name(tagging)); \
      assert_equal_f(s, tagging_strength(tagging));

static void setup_classifier_test(void) {
  classifier.user = "user";
  classifier.tag_name = "tag";
  classifier.user_id = 12;
  classifier.tag_id = 123;
  classifier.clues = NULL;
  
  PWord_t clue_p;
  JLI(clue_p, classifier.clues, 1);
  *clue_p = (Word_t)new_clue(1, 0.75);
  
  JLI(clue_p, classifier.clues, 2);
  *clue_p = (Word_t)new_clue(2, 0.51);
  
  JLI(clue_p, classifier.clues, 3);
  *clue_p = (Word_t)new_clue(3, 0.1);
  
  JLI(clue_p, classifier.clues, 4);
  *clue_p = (Word_t)new_clue(4, 0.95);
}

static void teardown_classifier_test(void) {
  Word_t bytes;
  JLFA(bytes, classifier.clues);
}

START_TEST (clue_selection_filters_out_weak_clues) {
  int tokens[][2] = {1, 1, 2, 1};
  Item *item = create_item_with_tokens(1, tokens, 2);
  int num_clues;
  const Clue **clues = select_clues(&classifier, item, &num_clues);
  assert_not_null(clues);
  assert_equal(1, num_clues);
  assert_equal(1, clue_token_id(clues[0]));
  free_item(item);
  free(clues);
} END_TEST

START_TEST (clue_selection_sorted_by_strength) {
  int tokens[][2] = {1, 1, 2, 1, 4, 1};
  Item *item = create_item_with_tokens(1, tokens, 3);
  int num_clues;
  const Clue **clues = select_clues(&classifier, item, &num_clues);
  assert_not_null(clues);
  assert_equal(2, num_clues);
  assert_equal(4, clue_token_id(clues[0]));
  assert_equal(1, clue_token_id(clues[1]));
  free_item(item);
  free(clues);
} END_TEST

START_TEST (classify_1) {
  int tokens[][2] = {10, 10};
  Item *item = create_item_with_tokens(1, tokens, 1);
  Tagging *tagging = classify(&classifier, item);
  assert_tagging("user", "tag", 12, 123, 0.5, tagging);
  free_item(item);
} END_TEST

START_TEST (classify_2) {
  int tokens[][2] = {2, 10};
  Item *item = create_item_with_tokens(1, tokens, 1);
  Tagging *tagging = classify(&classifier, item);
  assert_tagging("user", "tag", 12, 123, 0.5, tagging);
  free_item(item);
} END_TEST

START_TEST (classify_3) {
  int tokens[][2] = {4, 1};
  Item *item = create_item_with_tokens(1, tokens, 1);
  Tagging *tagging = classify(&classifier, item);
  assert_tagging("user", "tag", 12, 123, 0.89947100800, tagging);
  free_item(item);
} END_TEST

START_TEST (classify_4) {
  int tokens[][2] = {4, 100};
  Item *item = create_item_with_tokens(1, tokens, 1);
  Tagging *tagging = classify(&classifier, item);
  assert_tagging("user", "tag", 12, 123, 0.89947100800, tagging);
  free_item(item);
} END_TEST

START_TEST (classify_5) {
  int tokens[][2] = {4, 1, 2, 1};
  Item *item = create_item_with_tokens(1, tokens, 2);
  Tagging *tagging = classify(&classifier, item);
  assert_tagging("user", "tag", 12, 123, 0.89947100800, tagging);
  free_item(item);
} END_TEST

START_TEST (classify_6) {
  int tokens[][2] = {4, 1, 1, 1};
  Item *item = create_item_with_tokens(1, tokens, 2);
  Tagging *tagging = classify(&classifier, item);
  assert_tagging("user", "tag", 12, 123, 0.90383289433, tagging);
  free_item(item);
} END_TEST

START_TEST (classify_7) {
  int tokens[][2] = {4, 1, 3, 1};
  Item *item = create_item_with_tokens(1, tokens, 2);
  Tagging *tagging = classify(&classifier, item);
  assert_tagging("user", "tag", 12, 123, 0.59043855740, tagging);
  free_item(item);
} END_TEST

START_TEST (classify_8) {
  int tokens[][2] = {3, 1, 4, 1};
  Item *item = create_item_with_tokens(1, tokens, 2);
  Tagging *tagging = classify(&classifier, item);
  assert_tagging("user", "tag", 12, 123, 0.59043855740, tagging);
  free_item(item);
} END_TEST


START_TEST (classify_9) {
  int tokens[][2] = {3, 1};
  Item *item = create_item_with_tokens(1, tokens, 1);
  Tagging *tagging = classify(&classifier, item);
  assert_tagging("user", "tag", 12, 123, 0.16771702260, tagging);
  free_item(item);
} END_TEST

START_TEST (classify_10) {
  int tokens[][2] = {1, 1, 2, 1, 3, 1, 4, 1};
  Item *item = create_item_with_tokens(1, tokens, 4);
  Tagging *tagging = classify(&classifier, item);
  assert_tagging("user", "tag", 12, 123, 0.69125149517, tagging);
  free_item(item);
} END_TEST

/*************************************************************
 *   Unit tests for chi2q(double, int)
 *
 *************************************************************/
 
START_TEST (test_chi2_degrees_of_freedom_is_multiple_of_2) {
  assert_equal_f(-1.0, chi2Q(10, 11));
} END_TEST

START_TEST (degress_of_freedom_must_be_greater_than_0) {
  assert_equal_f(-1.0, chi2Q(10, 0));
} END_TEST
  
START_TEST (chi2_test1) {
  assert_equal_f(1.0, chi2Q(100, 300));
} END_TEST

START_TEST (chi2_test2) {
  assert_equal_f(0.0, chi2Q(1000, 300));
} END_TEST

START_TEST (chi2_test3) {
  assert_equal_f(0.82913752732, chi2Q(375, 400));
} END_TEST

START_TEST (chi2_test4) {
  assert_equal_f(0.52169717971, chi2Q(300, 300));
} END_TEST
  
Suite *
classifier_suite(void) {
  Suite *s = suite_create("Classifier");
  
  TCase *tc_chi2 = tcase_create("Chi2");
  tcase_add_test(tc_chi2, test_chi2_degrees_of_freedom_is_multiple_of_2);
  tcase_add_test(tc_chi2, degress_of_freedom_must_be_greater_than_0);
  tcase_add_test(tc_chi2, chi2_test1);
  tcase_add_test(tc_chi2, chi2_test2);
  tcase_add_test(tc_chi2, chi2_test3);
  tcase_add_test(tc_chi2, chi2_test4);
  suite_add_tcase(s, tc_chi2);
  
  TCase *tc_trainer = tcase_create("Trainer");
  tcase_add_checked_fixture(tc_trainer, setup_train, teardown_train);
  tcase_add_test(tc_trainer, train_merges_examples_into_pools);
  tcase_add_test(tc_trainer, train_keeps_user_and_tag);
  suite_add_tcase(s, tc_trainer);
  
  TCase *tc_precomputer = tcase_create("Precomputer");
  tcase_add_checked_fixture(tc_precomputer, setup_mock_items, teardown_mock_items);
  tcase_add_test(tc_precomputer, precompute_keeps_user_and_tag);
  tcase_add_test(tc_precomputer, precompute_creates_probabilities_for_each_token_in_tc);
  tcase_add_test(tc_precomputer, test_with_bias);
  tcase_add_test(tc_precomputer, probability_1);
  tcase_add_test(tc_precomputer, probability_2);
  tcase_add_test(tc_precomputer, probability_3);
  tcase_add_test(tc_precomputer, probability_4);
  tcase_add_test(tc_precomputer, probability_5);
  tcase_add_test(tc_precomputer, probability_6);
  tcase_add_test(tc_precomputer, probability_7);
  suite_add_tcase(s, tc_precomputer);
  
  TCase *tc_classifier = tcase_create("Classifier");
  tcase_add_checked_fixture(tc_classifier, setup_classifier_test, teardown_classifier_test);
  tcase_add_test(tc_classifier, clue_selection_filters_out_weak_clues);
  tcase_add_test(tc_classifier, clue_selection_sorted_by_strength);
  tcase_add_test(tc_classifier, classify_1);
  tcase_add_test(tc_classifier, classify_2);
  tcase_add_test(tc_classifier, classify_3);
  tcase_add_test(tc_classifier, classify_4);
  tcase_add_test(tc_classifier, classify_5);
  tcase_add_test(tc_classifier, classify_6);
  tcase_add_test(tc_classifier, classify_7);
  tcase_add_test(tc_classifier, classify_8);
  tcase_add_test(tc_classifier, classify_9);
  tcase_add_test(tc_classifier, classify_10);
  suite_add_tcase(s, tc_classifier);
  
  return s;
}
