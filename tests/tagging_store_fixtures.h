/* Copyright (c) 2007 The Kaphan Foundation
 *
 * Possession of a copy of this file grants no permission or license
 * to use, modify, or create derivate works.
 *
 * Please contact info@peerworks.org for further information.
 */
#ifndef TAGGING_STORE_FIXTURES_H_
#define TAGGING_STORE_FIXTURES_H_
#include <mysql.h>
#include <string.h>
#include "assertions.h"
#include "../src/tagging.h"

DBConfig config;
MYSQL *mysql;
MYSQL_STMT *tagging_stmt;
MYSQL_STMT *tagging_count_stmt;

#define TAGGING_STMT "select id from taggings where feed_item_id = ? and user_id = ? and tag_id = ? and strength = ? and classifier_tagging = 1"
#define COUNT_TAGGINGS "select count(*) from taggings where classifier_tagging = 1"

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

static void assert_tagging_count_is(int n) {
  int inserted_count;
  MYSQL_BIND result[1];
  memset(result, 0, sizeof(result));
  result[0].buffer_type = MYSQL_TYPE_LONG;
  result[0].buffer = (char *) &inserted_count;
  
  if (mysql_stmt_execute(tagging_count_stmt)) fail("Failed executing tagging count query");
  if (mysql_stmt_bind_result(tagging_count_stmt, result)) fail("Failed binding tagging count results");
  if (!mysql_stmt_fetch(tagging_count_stmt)) {
    mark_point();
    assert_equal(n, inserted_count);
  } else {
    mark_point();
    fail("Count query did not return any results: %s", mysql_stmt_error(tagging_count_stmt));
  }
  mysql_stmt_free_result(tagging_count_stmt);
}

#define assert_tagging_stored(tagging) fail_unless(tagging_stored(tagging), "Tagging was not stored")
#define assert_tagging_not_stored(tagging) fail_unless(!tagging_stored(tagging), "Tagging was stored")

static void setup_tagging_store() {
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
  
  tagging_count_stmt = mysql_stmt_init(mysql);
  if (mysql_stmt_prepare(tagging_count_stmt, COUNT_TAGGINGS, strlen(COUNT_TAGGINGS))) {
    fail("Failed preparing statement %s", mysql_stmt_error(tagging_count_stmt)); 
  } 
  
  // Reset tag table
  if (mysql_query(mysql, "update tags set last_classified_at = NULL, bias = NULL")) fail(mysql_error(mysql));
  if (mysql_query(mysql, "update tags set updated_on = '2007-11-1 00:00:00'")) fail(mysql_error(mysql));
  if (mysql_query(mysql, "update tags set last_classified_at = '2007-10-31 00:00:00' where id = 38")) fail(mysql_error(mysql));
  if (mysql_query(mysql, "update tags set last_classified_at = '2007-11-1 00:00:00' where id = 39")) fail(mysql_error(mysql));   
  if (mysql_query(mysql, "update tags set bias = 1.2 where id = 38")) fail(mysql_error(mysql));
}

static void teardown_tagging_store() {
  mysql_stmt_close(tagging_stmt);
  mysql_stmt_close(tagging_count_stmt);
  mysql_close(mysql);
  mysql = NULL;
}
#endif /*TAGGING_STORE_FIXTURES_H_*/
