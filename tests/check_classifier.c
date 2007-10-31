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
#include "assertions.h"
#include "mock_item_source.h"

/*************************************************************
 *   Unit tests for trainer
 *
 *************************************************************/
 
START_TEST (train_merges_examples_into_pools) {
  TagList tags = load_tags_from_file("fixtures", "mock");
  assert_not_null(tags);
  Tag tag = taglist_tag_at(tags, 1);
  assert_not_null(tag);
  
  TrainedClassifier trained = train(tag, is);
  assert_not_null(trained);
  
  const Pool positive = tc_get_positive_pool(trained);
  assert_not_null(positive);
  assert_equal(3, pool_num_tokens(positive));
  assert_equal(22, pool_total_tokens(positive));
  
  const Pool negative = tc_get_negative_pool(trained);
  assert_not_null(negative);
  assert_equal(3, pool_num_tokens(negative));
  assert_equal(16, pool_total_tokens(negative));
  
  tc_free(trained);
} END_TEST


START_TEST (train_keeps_user_and_tag) {
  TagList tags = load_tags_from_file("fixtures", "mock");
  assert_not_null(tags);
  Tag tag = taglist_tag_at(tags, 1);
  assert_not_null(tag);
  
  TrainedClassifier trained = train(tag, is);
  assert_not_null(trained);
  
  assert_equal_s("mock", tc_get_user(trained));
  assert_equal_s("tag",  tc_get_tag_name(trained));
  
  tc_free(trained);
} END_TEST

/*************************************************************
 *   Unit tests for precomputing
 *
 *************************************************************/
START_TEST (precompute_keeps_user_and_tag) {
  Pool random_background = new_pool();
  struct TRAINED_CLASSIFIER tc;
  tc.user = "user";
  tc.tag_name = "tag";
  Classifier classifier = precompute(&tc, random_background);
  assert_not_null(classifier);
  assert_equal_s("user", cls_user(classifier));
  assert_equal_s("tag", cls_tag_name(classifier));
} END_TEST

#define TOKEN_PROBS(pc, ps, nc, ns, bc, bs)         \
      struct PROB_TOKEN positive, negative, random; \
      positive.token_count = pc;                    \
      positive.pool_size = ps;                      \
      negative.token_count = nc;                    \
      negative.pool_size = ns;                      \
      random.token_count = bc;                      \
      random.pool_size = bs;                        \
      ProbToken foregrounds[] = {&positive};        \ 
      ProbToken backgrounds[] = {&negative, &random};\
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
  tcase_add_checked_fixture(tc_trainer, setup_mock_item_source, teardown_mock_item_source);
  tcase_add_test(tc_trainer, train_merges_examples_into_pools);
  tcase_add_test(tc_trainer, train_keeps_user_and_tag);
  suite_add_tcase(s, tc_trainer);
  
  TCase *tc_precomputer = tcase_create("Precomputer");
  tcase_add_checked_fixture(tc_precomputer, setup_mock_item_source, teardown_mock_item_source);
  tcase_add_test(tc_precomputer, precompute_keeps_user_and_tag);
  tcase_add_test(tc_precomputer, probability_1);
  tcase_add_test(tc_precomputer, probability_2);
  tcase_add_test(tc_precomputer, probability_3);
  tcase_add_test(tc_precomputer, probability_4);
  tcase_add_test(tc_precomputer, probability_5);
  tcase_add_test(tc_precomputer, probability_6);
  tcase_add_test(tc_precomputer, probability_7);
  suite_add_tcase(s, tc_precomputer);
  
  return s;
}
