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
  
  return s;
}
