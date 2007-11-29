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
   
  if (mysql_query(mysql, "update tags set last_classified_at = NULL, bias = NULL")) fail(mysql_error(mysql));
  if (mysql_query(mysql, "update tags set updated_on = '2007-11-1 00:00:00'")) fail(mysql_error(mysql));
  if (mysql_query(mysql, "update tags set last_classified_at = '2007-10-31 00:00:00' where id = 38")) fail(mysql_error(mysql));
  if (mysql_query(mysql, "update tags set last_classified_at = '2007-11-1 00:00:00' where id = 39")) fail(mysql_error(mysql));   
  if (mysql_query(mysql, "update tags set bias = 1.2 where id = 38")) fail(mysql_error(mysql));   
  if (mysql_query(mysql, "insert ignore into taggings(user_id, tag_id,feed_item_id,strength,classifier_tagging) VALUES (2, 39, 1, 1.0, 1)")) fail(mysql_error(mysql));   
}

static void teardown() {
  if (mysql_query(mysql, "delete from taggings where classifier_tagging = 1")) fail(mysql_error(mysql));   
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
  assert_null(tag_user(tag));
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

START_TEST(test_get_tags_for_user) {
  TagDB *tag_db = create_tag_db(&config);
  assert_not_null(tag_db);
  TagList *tag_list = tag_db_load_tags_to_classify_for_user(tag_db, 2);
  assert_not_null(tag_list);
  assert_equal(6, tag_list->size);
  assert_not_null(tag_list->tags);
  
  assert_equal(38, tag_tag_id(tag_list->tags[0]));
  assert_equal(40, tag_tag_id(tag_list->tags[1]));
  assert_equal(41, tag_tag_id(tag_list->tags[2]));
  assert_equal(44, tag_tag_id(tag_list->tags[3]));
  assert_equal(45, tag_tag_id(tag_list->tags[4]));
  assert_equal(47, tag_tag_id(tag_list->tags[5]));
  
  free_taglist(tag_list);
  free_tag_db(tag_db);
} END_TEST

START_TEST(test_get_tags_for_user_loads_examples) {
  TagDB *tag_db = create_tag_db(&config);
  assert_not_null(tag_db);
  TagList *tag_list = tag_db_load_tags_to_classify_for_user(tag_db, 2);
  assert_not_null(tag_list);
  
  Tag *tag = tag_list->tags[0];
  assert_not_null(tag);
  assert_equal(11, tag_positive_examples_size(tag));
  assert_equal(11, tag_negative_examples_size(tag));
  
  free_taglist(tag_list);
  free_tag_db(tag_db);
} END_TEST

START_TEST (test_get_tags_for_user_loads_last_classified_time) {
  TagDB *tag_db = create_tag_db(&config);
  assert_not_null(tag_db);
  TagList *tag_list = tag_db_load_tags_to_classify_for_user(tag_db, 2);
  assert_not_null(tag_list);
  
  Tag *tag = tag_list->tags[0];
  assert_not_null(tag);
  
  struct tm tm;
  tm.tm_year = 2007 - 1900;
  tm.tm_mon = 9;
  tm.tm_mday = 31;
  tm.tm_hour = 0;
  tm.tm_min = 0;
  tm.tm_sec = 0;
  time_t t = timegm(&tm);
  assert_equal(t, tag_last_classified_at(tag));
  
  free_taglist(tag_list);
  free_tag_db(tag_db);
} END_TEST

START_TEST (test_get_tags_for_user_loads_updated_at_time) {
  TagDB *tag_db = create_tag_db(&config);
  assert_not_null(tag_db);
  TagList *tag_list = tag_db_load_tags_to_classify_for_user(tag_db, 2);
  assert_not_null(tag_list);
  
  Tag *tag = tag_list->tags[0];
  assert_not_null(tag);
  
  struct tm tm;
  tm.tm_year = 2007 - 1900;
  tm.tm_mon = 10;
  tm.tm_mday = 1;
  tm.tm_hour = 0;
  tm.tm_min = 0;
  tm.tm_sec = 0;
  time_t t = timegm(&tm);
  assert_equal(t, tag_updated_at(tag));
  
  free_taglist(tag_list);
  free_tag_db(tag_db);
} END_TEST

START_TEST (test_load_tag_by_id_loads_last_classified_time) {
  TagDB *tag_db = create_tag_db(&config);
  assert_not_null(tag_db);
  
  Tag *tag = tag_db_load_tag_by_id(tag_db, 38);
  assert_not_null(tag);
  
  struct tm tm;
  tm.tm_year = 2007 - 1900;
  tm.tm_mon = 9;
  tm.tm_mday = 31;
  tm.tm_hour = 0;
  tm.tm_min = 0;
  tm.tm_sec = 0;
  time_t t = timegm(&tm);
  assert_equal(t, tag_last_classified_at(tag));
  
  free_tag(tag);
  free_tag_db(tag_db);
} END_TEST

START_TEST (test_load_tag_by_id_loads_updated_at_time) {
  TagDB *tag_db = create_tag_db(&config);
  assert_not_null(tag_db);
  Tag *tag = tag_db_load_tag_by_id(tag_db, 38);
  assert_not_null(tag);
  
  struct tm tm;
  tm.tm_year = 2007 - 1900;
  tm.tm_mon = 10;
  tm.tm_mday = 1;
  tm.tm_hour = 0;
  tm.tm_min = 0;
  tm.tm_sec = 0;
  time_t t = timegm(&tm);
  assert_equal(t, tag_updated_at(tag));
  
  free_tag(tag);
  free_tag_db(tag_db);
} END_TEST

START_TEST(test_default_bias_should_be_1) {
  TagDB *tag_db = create_tag_db(&config);
  assert_not_null(tag_db);
  Tag *tag = tag_db_load_tag_by_id(tag_db, 48);
  assert_not_null(tag);
  assert_equal_f(1.0, tag_bias(tag));
  free_tag(tag);
  free_tag_db(tag_db);
} END_TEST

START_TEST (test_loads_bias_for_tag) {
  TagDB *tag_db = create_tag_db(&config);
  assert_not_null(tag_db);
  Tag *tag = tag_db_load_tag_by_id(tag_db, 38);
  assert_not_null(tag);
  assert_equal_f(1.2, tag_bias(tag));
  free_tag(tag);
  free_tag_db(tag_db);
} END_TEST

START_TEST (test_load_bias_for_tag_list) {
  TagDB *tag_db = create_tag_db(&config);
  assert_not_null(tag_db);
  TagList *tag_list = tag_db_load_tags_to_classify_for_user(tag_db, 2);
  assert_not_null(tag_list);
    
  Tag *tag = tag_list->tags[0];
  assert_not_null(tag);
  assert_equal(38, tag_tag_id(tag));
  assert_equal_f(1.2, tag_bias(tag));
  
  free_taglist(tag_list);
  free_tag_db(tag_db);
} END_TEST


Suite *
tag_db_suite(void) {
  Suite *s = suite_create("Tag_db");  
  TCase *tc_case = tcase_create("Case");
  tcase_add_checked_fixture (tc_case, setup, teardown);
  
// START_TESTS
  tcase_add_test(tc_case, test_create_tag_db);
  tcase_add_test(tc_case, test_update_last_classified_time_for_a_tag);
  tcase_add_test(tc_case, test_get_tags_for_user);
  tcase_add_test(tc_case, test_get_tags_for_user_loads_examples);
  tcase_add_test(tc_case, test_default_bias_should_be_1);
  tcase_add_test(tc_case, test_loads_bias_for_tag);
  tcase_add_test(tc_case, test_load_bias_for_tag_list);
  tcase_add_test(tc_case, test_get_tags_for_user_loads_last_classified_time);
  tcase_add_test(tc_case, test_get_tags_for_user_loads_updated_at_time);
  tcase_add_test(tc_case, test_load_tag_by_id_loads_last_classified_time);
  tcase_add_test(tc_case, test_load_tag_by_id_loads_updated_at_time);
// END_TESTS

  suite_add_tcase(s, tc_case);
  return s;
}
