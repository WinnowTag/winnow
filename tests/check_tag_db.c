/* Copyright (c) 2007 The Kaphan Foundation
 *
 * Possession of a copy of this file grants no permission or license
 * to use, modify, or create derivate works.
 *
 * Please contact info@peerworks.org for further information.
 */

#include <check.h>
#include "../src/tag.h"
#include "../src/cls_config.h"
#include "assertions.h"
#include "mysql.h"

DBConfig config;
TagDB *tag_db;
MYSQL *mysql;

static void setup() {
  config.host = "localhost";
  config.user = "seangeo";
  config.password = "seangeo";
  config.database = "classifier_test";
  tag_db = create_tag_db(&config);
  assert_not_null(tag_db);
  
  mysql = mysql_init(NULL);
  mysql_real_connect(mysql, config.host, config.user, config.password, config.database, 0, NULL, 0);
   
  if (mysql_query(mysql, "update tags set last_classified_at = NULL")) fail(mysql_error(mysql));
  if (mysql_query(mysql, "update tags set updated_on = '2007-11-1 00:00:00'")) fail(mysql_error(mysql));
  if (mysql_query(mysql, "update tags set last_classified_at = '2007-11-1 00:00:00' where id = 39")) fail(mysql_error(mysql));   
}

static void teardown() {
  free_tag_db(tag_db);
  mysql_close(mysql);
  mysql = NULL;
}

static void assert_last_classified_updated(int tag_id) {
  char q[256];
  sprintf(q, "select * from tags where last_classified_at is not null and id = %i", tag_id);
  if (mysql_query(mysql, q)) fail(mysql_error(mysql));
  MYSQL_RES *result = mysql_store_result(mysql);
  int size = (int) mysql_num_rows(result);
  mysql_free_result(result);
  fail_unless(size == 1, "last classified not updated for tag %i", tag_id);  
}

START_TEST (test_create_tag_db) {
  Tag *tag = tag_db_load_tag_by_id(tag_db, 39);
  assert_not_null(tag);
  assert_equal_s("Evilution", tag_tag_name(tag));
  assert_equal_s("", tag_user(tag));
  assert_equal(39, tag_tag_id(tag));
  assert_equal(2, tag_user_id(tag));
  assert_equal(20, tag_positive_examples_size(tag));
  assert_equal(84, tag_negative_examples_size(tag));
  free_tag(tag);
} END_TEST

START_TEST(test_update_last_classified_time_for_a_tag) {
  TagDB *tag_db = create_tag_db(&config);
  assert_not_null(tag_db);
  Tag *tag = tag_db_load_tag_by_id(tag_db, 48);
  assert_not_null(tag);
  int ret = tag_db_update_last_classified_at(tag_db, tag);
  assert_false(ret);
  assert_last_classified_updated(48);
} END_TEST

Suite *
tag_db_suite(void) {
  Suite *s = suite_create("Tag_db");  
  TCase *tc_case = tcase_create("Case");
  tcase_add_checked_fixture (tc_case, setup, teardown);
  
// START_TESTS
  tcase_add_test(tc_case, test_create_tag_db);
  tcase_add_test(tc_case, test_update_last_classified_time_for_a_tag);
// END_TESTS

  suite_add_tcase(s, tc_case);
  return s;
}
