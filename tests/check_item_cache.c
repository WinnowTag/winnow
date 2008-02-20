/* Copyright (c) 2008 The Kaphan Foundation
 *
 * Possession of a copy of this file grants no permission or license
 * to use, modify, or create derivate works.
 *
 * Please contact info@peerworks.org for further information.
 */

#include <check.h>
#include "assertions.h"
#include "../src/sqlite_item_source.h"
#include "../src/misc.h"

START_TEST (creating_with_missing_db_file_fails) {
  SQLiteItemSource *is;
  int rc = sqlite_item_source_create(&is, "missing.db");
  assert_equal(CLASSIFIER_FAIL, rc);
  const char *msg = sqlite_item_source_errmsg(is);
  assert_equal_s("unable to open database file", msg);
} END_TEST

START_TEST (creating_with_empty_db_file_fails) {
  SQLiteItemSource *is;
  int rc = sqlite_item_source_create(&is, "fixtures/empty.db");
  assert_equal(CLASSIFIER_FAIL, rc);
  const char *msg = sqlite_item_source_errmsg(is); 
  assert_equal_s("Database file's user version does not match classifier version. Trying running classifier-db-migrate.", msg);
                 
} END_TEST

START_TEST (create_with_valid_db) {
  SQLiteItemSource *is;
  int rc = sqlite_item_source_create(&is, "fixtures/valid.db");
  assert_equal(CLASSIFIER_OK, rc);
} END_TEST

Suite *
sqlite_item_source_suite(void) {
  Suite *s = suite_create("sqlite_item_source");  
  TCase *tc_case = tcase_create("case");

// START_TESTS
  tcase_add_test(tc_case, creating_with_missing_db_file_fails);
  tcase_add_test(tc_case, creating_with_empty_db_file_fails);
  tcase_add_test(tc_case, create_with_valid_db);
    
// END_TESTS

  suite_add_tcase(s, tc_case);
  return s;
}

