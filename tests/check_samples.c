// Copyright (c) 2007 The Kaphan Foundation
//
// Possession of a copy of this file grants no permission or license
// to use, modify, or create derivate works.
//
// Please contact info@peerworks.org for further information.

#include <check.h>
#include "../src/samples.h"
#include "mock_item_source.h"
#include "assertions.h"

START_TEST (load_samples_test) {
  Samples samples = load_samples(is, "fixtures/samples.txt");
  assert_not_null(samples);
  assert_equal(4, sample_size(samples));
  
  const Item *items = sample_items(samples);
  assert_equal(item_1, items[0]);
  assert_equal(item_2, items[1]);
  assert_equal(item_3, items[2]);
  assert_equal(item_4, items[3]);
  free_samples(samples);
} END_TEST

Suite *
samples_suite(void) {
  Suite *s = suite_create("Samples");  
  TCase *tc_sample = tcase_create("Sample");
  tcase_add_checked_fixture (tc_sample, setup_mock_item_source, teardown_mock_item_source);

// START_TESTS
  tcase_add_test(tc_sample, load_samples_test);
// END_TESTS

  suite_add_tcase(s, tc_sample);
  return s;
}
