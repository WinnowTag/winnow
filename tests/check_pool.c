// Copyright (c) 2007-2010 The Kaphan Foundation
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

#include <stdlib.h>
#include <math.h>
#include <check.h>
#include "fixtures.h"
#include "../src/item_cache.h"
#include "../src/misc.h"
#include "assertions.h"

static ItemCacheOptions item_cache_options;
static ItemCache *item_cache;
static int free_when_done;

static void setup(void) {
  item_cache_options.cache_update_wait_time = 1;
  setup_fixture_path();
  system("rm -Rf /tmp/valid-copy && cp -R fixtures/valid /tmp/valid-copy && chmod -R 755 /tmp/valid-copy");
  item_cache_create(&item_cache, "/tmp/valid-copy", &item_cache_options);
  item_cache_load(item_cache);
}

static void teardown(void) {
  teardown_fixture_path();
  free_item_cache(item_cache);
}

START_TEST (create_pool) {
  Pool *pool = new_pool();
  assert_not_null(pool);
  free_pool(pool);
} END_TEST

START_TEST (new_pool_is_empty) {
  Pool *pool = new_pool();
  assert_equal(0, pool_num_tokens(pool));
  assert_equal(0, pool_total_tokens(pool));
  free_pool(pool);
} END_TEST

START_TEST (add_1_item) {
  Pool *pool = new_pool();
  pool_add_item(pool, item_cache_fetch_item(item_cache, (unsigned char *) "urn:peerworks.org:entry#878944", &free_when_done));
  assert_equal(186, pool_num_tokens(pool));
  assert_equal(336, pool_total_tokens(pool));
  assert_equal(9, pool_token_frequency(pool, 7982));
  assert_equal(1, pool_token_frequency(pool, 7967));
  assert_equal(0, pool_token_frequency(pool, 10));
  free_pool(pool);
} END_TEST

START_TEST (add_2_items_with_same_tokens) {
  Pool *pool = new_pool();
  pool_add_item(pool, item_cache_fetch_item(item_cache, (unsigned char *) "urn:peerworks.org:entry#709254", &free_when_done));
  pool_add_item(pool, item_cache_fetch_item(item_cache, (unsigned char *) "urn:peerworks.org:entry#753459", &free_when_done));
  assert_equal(196, pool_num_tokens(pool));
  assert_equal(323, pool_total_tokens(pool));
  assert_equal(3, pool_token_frequency(pool, 665));
  assert_equal(3, pool_token_frequency(pool, 1179));
  assert_equal(1, pool_token_frequency(pool, 1222));
  assert_equal(0, pool_token_frequency(pool, 10));
  free_pool(pool);
} END_TEST

START_TEST (token_iteration) {
  int ret_val;
  Pool *pool = new_pool();
  pool_add_item(pool, item_cache_fetch_item(item_cache, (unsigned char *) "urn:peerworks.org:entry#709254", &free_when_done));
  pool_add_item(pool, item_cache_fetch_item(item_cache, (unsigned char *) "urn:peerworks.org:entry#753459", &free_when_done));

  Token token;
  token.id = 0;

  ret_val = pool_next_token(pool, &token);
  assert_true(ret_val);
  assert_equal(1, token.id);
  assert_equal(1, token.frequency);

  ret_val = pool_next_token(pool, &token);
  assert_true(ret_val);
  assert_equal(13, token.id);
  assert_equal(2, token.frequency);

  free_pool(pool);
} END_TEST

START_TEST (token_iteration_with_null_pool_doesnt_crash) {
  Token token;
  token.id = 0;
  int ret_val;

  ret_val = pool_next_token(NULL, &token);
  assert_false(ret_val);
} END_TEST

Suite *
pool_suite(void) {
  Suite *s = suite_create("Pool");

  TCase *tc_pool = tcase_create("Pool");
  tcase_add_checked_fixture (tc_pool, setup, teardown);
  tcase_add_test(tc_pool, create_pool);
  tcase_add_test(tc_pool, new_pool_is_empty);
  tcase_add_test(tc_pool, add_1_item);
  tcase_add_test(tc_pool, add_2_items_with_same_tokens);
  tcase_add_test(tc_pool, token_iteration);
  tcase_add_test(tc_pool, token_iteration_with_null_pool_doesnt_crash);
  suite_add_tcase(s, tc_pool);

  return s;
}

int main(void) {
  initialize_logging("test.log");
  int number_failed;

  SRunner *sr = srunner_create(pool_suite());
  srunner_run_all(sr, CK_NORMAL);
  number_failed = srunner_ntests_failed(sr);
  srunner_free(sr);
  close_log();
  return (number_failed == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}
