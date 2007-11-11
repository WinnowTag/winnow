// Copyright (c) 2007 The Kaphan Foundation
//
// Possession of a copy of this file grants no permission or license
// to use, modify, or create derivate works.
//
// Please contact info@peerworks.org for further information.

#include <mysql.h>
#include <stdio.h>

#include "random_background.h"
#include "misc.h"
#include "errno.h"

Pool * create_random_background_from_file (const ItemSource *is, const char * filename) {
  Pool *pool = new_pool();  
  FILE *file;

  file = fopen(filename, "r");
  if (NULL != file) {
    int item_id;
    while (EOF != fscanf(file, "%d\n", &item_id)) {
      Item *item = is_fetch_item(is, item_id);
      if (NULL != item) {
	      pool_add_item(pool, item);
      }
    }
  } else {
    error("Could not open %s: %s", filename, strerror(errno));
    free_pool(pool);
    pool = NULL;
  }
  
  return pool;
}

#define RND_BG_QUERY "select feed_item_id from random_backgrounds"

Pool * create_random_background_from_db (const ItemSource *is, const DBConfig *db) {
  MYSQL *mysql = NULL;
  MYSQL_RES *result = NULL;
  MYSQL_ROW row;
  
  Pool *pool = new_pool();
  if (NULL == pool) {
    fatal("Error malloc'ing pool for random background");
    return NULL;
  }
  
  mysql = mysql_init(NULL);
  if (NULL == mysql) goto query_error;
    
  
  if (!mysql_real_connect(mysql, db->host, db->user, db->password, db->database, db->port, NULL, 0)) {
    goto query_error;
  }
  
  if (mysql_query(mysql, RND_BG_QUERY)) goto query_error;
  
  result = mysql_use_result(mysql);
  if (NULL == result) goto query_error;
  
  while (row = mysql_fetch_row(result)) {
    Item *item;
    if (item = is_fetch_item(is, (int) strtol(row[0], NULL, 10))) {
      pool_add_item(pool, item);
    } else {
      error("Missing random background item %s", row[0]);
    }
  }

exit:
  if (result) {
    mysql_free_result(result);
  }
  
  if (mysql) {
    mysql_close(mysql);
  }
  
  return pool;
  
query_error:
  if (pool) {
    free_pool(pool);
    pool = NULL;
  }
  error("Error querying for random background: %s", mysql_error(mysql));
  goto exit;
}
