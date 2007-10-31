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
#include "../src/item.h"
#include "../src/pool.h"

ItemSource is;
Item item_1;
Item item_2;
Item item_3;

Item item_1;
Item item_2;
Item item_3;

Item fake_item_source(const void* ignore, const int item_id) {
  switch (item_id) {
    case 1:
      return item_1;
    case 2:
      return item_2;
    case 3:
      return item_3;
    default:
      return NULL;
  }
}

static void setup(void) {
  int tokens_1[][2] = {1, 10, 2, 3};
  int tokens_2[][2] = {1, 5, 3, 4};
  int tokens_3[][2] = {1, 6, 2, 4};
  
  item_1 = create_item_with_tokens(1, tokens_1, 2);
  item_2 = create_item_with_tokens(2, tokens_2, 2);
  item_3 = create_item_with_tokens(3, tokens_3, 2);
  
  is = malloc(sizeof(struct ITEMSOURCE));
  is->fetch_func = fake_item_source;
  is->fetch_func_state = NULL;
}

static void teardown(void) {
  free(is);
  free_item(item_1);
  free_item(item_2);
  free_item(item_3);
}

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
  tcase_add_checked_fixture (tc_rb, setup, teardown);
  
// START_TESTS
  tcase_add_test(tc_rb, load_random_background_from_file_test);
// END_TESTS

  suite_add_tcase(s, tc_rb);
  return s;
}
