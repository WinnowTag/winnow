/* Copyright (c) 2007 The Kaphan Foundation
 *
 * Possession of a copy of this file grants no permission or license
 * to use, modify, or create derivate works.
 *
 * Please contact info@peerworks.org for further information.
 */
#ifndef TAGGING_STORE_FIXTURES_H_
#define TAGGING_STORE_FIXTURES_H_

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

static void setup_tagging_store() {
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

static void teardown_tagging_store() {
  mysql_close(mysql);
  mysql = NULL;
}
#endif /*TAGGING_STORE_FIXTURES_H_*/
