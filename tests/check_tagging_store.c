/* Copyright (c) 2007 The Kaphan Foundation
 *
 * Possession of a copy of this file grants no permission or license
 * to use, modify, or create derivate works.
 *
 * Please contact info@peerworks.org for further information.
 */

#include <check.h>
#include <mysql.h>
#include <string.h>
#include <stdio.h>
#include "../src/tagging.h"
#include "../src/cls_config.h"
#include "assertions.h"

DBConfig config;
MYSQL *mysql;
MYSQL_STMT *tagging_stmt;

#define TAGGING_STMT "select id from taggings where feed_item_id = ? and user_id = ? and tag_id = ? and strength = ? and classifier_tagging = 1"

static int tagging_stored(Tagging *tagging) {
  float tagging_strength = (float) tagging->strength;
  MYSQL_BIND params[4];
  memset(params, 0, sizeof(params));
  params[0].buffer_type = params[1].buffer_type = params[2].buffer_type = MYSQL_TYPE_LONG;
  params[3].buffer_type = MYSQL_TYPE_FLOAT;
  params[0].buffer = (char *) &(tagging->item_id);
  params[1].buffer = (char *) &(tagging->user_id);
  params[2].buffer = (char *) &(tagging->tag_id);
  params[3].buffer = (char *) &(tagging_strength);

  int found = true;
  if (mysql_stmt_bind_param(tagging_stmt, params)) fail("Failed binding params %s", mysql_stmt_error(tagging_stmt));
  if (mysql_stmt_execute(tagging_stmt)) fail("Failed executing tagging query");
  if (mysql_stmt_fetch(tagging_stmt)) {
    found = false;
  }
  mysql_stmt_free_result(tagging_stmt);
  return found;
}

#define assert_tagging_stored(tagging) fail_unless(tagging_stored(tagging), "Tagging was not stored")
#define assert_tagging_not_stored(tagging) fail_unless(!tagging_stored(tagging), "Tagging was stored")

static void setup() {
  printf("setup\n");
  config.host = "localhost";
  config.user = "seangeo";
  config.password = "seangeo";
  config.database = "classifier_test";
  mysql = mysql_init(NULL);
  mysql_real_connect(mysql, config.host, config.user, config.password, config.database, 0, NULL, 0);
  if (mysql_query(mysql, "delete from taggings where classifier_tagging = 1")) {
    fail("Failed to clear out existing taggings %s", mysql_error(mysql));
  }
  tagging_stmt = mysql_stmt_init(mysql);
  if (mysql_stmt_prepare(tagging_stmt, TAGGING_STMT, strlen(TAGGING_STMT))) {
    fail("Failed preparing statement %s", mysql_stmt_error(tagging_stmt)); 
  } 
}

static void teardown() {
  mysql_close(mysql);
  mysql = NULL;
}

START_TEST(test_insert_tagging) {
  TaggingStore *tagging_store = create_db_tagging_store(&config, 0.9);
  assert_not_null(tagging_store);
  Tagging tagging;
  tagging.user_id = 23;
  tagging.tag_id = 45;
  tagging.item_id = 56;
  tagging.strength = 0.9;
  tagging_store_store(tagging_store, &tagging);
  assert_tagging_stored(&tagging);
} END_TEST

START_TEST(test_dont_insert_tagging_below_threshold) {
  TaggingStore *tagging_store = create_db_tagging_store(&config, 0.9);
  assert_not_null(tagging_store);
  Tagging tagging;
  tagging.user_id = 23;
  tagging.tag_id = 45;
  tagging.item_id = 57;
  tagging.strength = 0.89;
  tagging_store_store(tagging_store, &tagging);
  assert_tagging_not_stored(&tagging);
} END_TEST

START_TEST(test_insert_duplicate_updates_strength) {
  TaggingStore *tagging_store = create_db_tagging_store(&config, 0.9);
  assert_not_null(tagging_store);
  Tagging tagging1, tagging2;
  tagging1.user_id = tagging2.user_id = 23;
  tagging1.tag_id = tagging2.tag_id = 45;
  tagging1.item_id = tagging2.item_id = 56;
  tagging1.strength = 0.91;
  tagging2.strength = 0.95;
  
  tagging_store_store(tagging_store, &tagging1);
  tagging_store_store(tagging_store, &tagging2);
  assert_tagging_not_stored(&tagging1);
  assert_tagging_stored(&tagging2);
} END_TEST

Suite * tagging_store_suite(void) {
  Suite *s = suite_create("tagging_store");
  TCase *tc_case = tcase_create("case");
  tcase_add_checked_fixture (tc_case, setup, teardown);
  // START_TESTS
  tcase_add_test(tc_case, test_insert_tagging);
  tcase_add_test(tc_case, test_dont_insert_tagging_below_threshold);
  tcase_add_test(tc_case, test_insert_duplicate_updates_strength);
  // END_TESTS

  suite_add_tcase(s, tc_case);
  return s;
}
