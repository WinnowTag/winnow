// Copyright (c) 2007 The Kaphan Foundation
//
// Possession of a copy of this file grants no permission or license
// to use, modify, or create derivate works.
//
// Please contact info@peerworks.org for further information.

#include <stdlib.h>
#include <check.h>
#include "assertions.h"
#include "../src/random_background.h"
#include "../src/pool.h"
#include "mock_item_source.h"

START_TEST (load_random_background_from_file_test) {
  Pool random = create_random_background_from_file(is, "fixtures/random_background.txt");
  assert_not_null(random);
  assert_equal(3, pool_num_tokens(random));
  assert_equal(32, pool_total_tokens(random));
} END_TEST


Suite *
random_background_suite(void) {
  Suite *s = suite_create("Random_background");  
  TCase *tc_rb = tcase_create("Rb");
  tcase_add_checked_fixture (tc_rb, setup_mock_item_source, teardown_mock_item_source);
  
// START_TESTS
  tcase_add_test(tc_rb, load_random_background_from_file_test);
// END_TESTS

  suite_add_tcase(s, tc_rb);
  return s;
}
