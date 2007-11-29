/* Copyright (c) 2007 The Kaphan Foundation
 *
 * Possession of a copy of this file grants no permission or license
 * to use, modify, or create derivate works.
 *
 * Please contact info@peerworks.org for further information.
 */

#include <check.h>
#include "../src/item_source.h"
#include "../src/item.h"
#include "assertions.h"
#include "mock_item_source.h"

ItemList *replacement = NULL;

static ItemList * replacement_fake_fetch_all(const void *ignore) {
  if (!replacement) {
    replacement = create_item_list();
    item_list_add_item(replacement, item_1);
    item_list_add_item(replacement, item_2);    
  }
  
  return replacement;
}

START_TEST(test_all_items_accessible_by_id) {
  ItemSource *cis = create_caching_item_source(is);
  assert_not_null(cis);
  Item *item = is_fetch_item(cis, _i);
  assert_not_null(item);
  assert_equal(_i, item_get_id(item));
  free_item_source(cis);
} END_TEST

START_TEST(test_all_items_accessible_by_index) {
  ItemSource *cis = create_caching_item_source(is);
  assert_not_null(cis);
  ItemList *items = is_fetch_all_items(cis);
  assert_not_null(items);
  Item *item = item_list_item_at(items, _i);
  assert_not_null(item);
  assert_equal(_i + 1, item_get_id(item));
  free_item_source(cis);
} END_TEST

START_TEST(test_sizeof_item_list) {
  ItemSource *cis = create_caching_item_source(is);
  assert_not_null(cis);
  ItemList *items = is_fetch_all_items(cis);
  assert_not_null(items);
  assert_equal(MOCK_IS_SIZE, item_list_size(items));
  free_item_source(cis);
} END_TEST

START_TEST(test_flushing) {
  ItemSource *cis = create_caching_item_source(is);
  assert_not_null(cis);
  ItemList *items = is_fetch_all_items(cis);
  assert_not_null(items);
  assert_equal(MOCK_IS_SIZE, item_list_size(items));
  
  is->fetch_all_func = replacement_fake_fetch_all;
  is_flush(cis);
  items = is_fetch_all_items(cis);
  assert_not_null(items);
  assert_equal(2, item_list_size(items));
} END_TEST


Suite * caching_item_source_suite(void) {
  Suite *s = suite_create("caching_item_source");
  TCase *tc_case = tcase_create("case");
  tcase_add_unchecked_fixture(tc_case, setup_mock_item_source, teardown_mock_item_source);
  
  // START_TESTS
  tcase_add_loop_test(tc_case, test_all_items_accessible_by_id, 1, 5);
  tcase_add_loop_test(tc_case, test_all_items_accessible_by_index, 0, 4);
  tcase_add_test(tc_case, test_sizeof_item_list);
  tcase_add_test(tc_case, test_flushing);
  // END_TESTS

  suite_add_tcase(s, tc_case);
  return s;
}
