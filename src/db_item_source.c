/* Copyright (c) 2007 The Kaphan Foundation
 *
 * Possession of a copy of this file grants no permission or license
 * to use, modify, or create derivate works.
 *
 * Please contact info@peerworks.org for further information.
 */

#include <mysql.h>
#include "db_item_source.h"

typedef struct DB_ITEMSOURCE {
  DBConfig *config;
  MYSQL *mysql;
} DBItemSource;

static Item * fetch_item_func(const void *state, int item_id);
static int    alive_func(const void *state);

ItemSource * create_db_item_source(DBConfig * config) {
  info("Creating item source (host='%s',user='%s',pass='%s',db='%s')", 
    config->host, config->user, config->password, config->database);
  ItemSource *is = malloc(sizeof(ItemSource));
  
  if (NULL != is) {
    is->fetch_func = fetch_item_func;
    is->alive_func = alive_func;
    
    DBItemSource *state = malloc(sizeof(DBItemSource));
    if (NULL == state) {
      free(is);
      fatal("Could not allocate DBItemSource");
    }
    
    state->config = config;
    state->mysql = mysql_init(NULL);
    is->state = state;
    
    if (!mysql_real_connect(state->mysql, config->host, config->user, 
                            config->password, config->database, config->port, NULL, 0)) {
      error("Failed to connect to item database: %s", mysql_error(state->mysql));
      free(state);
      free(is);
      is = NULL;
    }
  } else {
    fatal("Could not allocate ItemSource");
  }
  
  return is;
}

Item * fetch_item_func(const void *state, int item_id) {
  return NULL;
}

int alive_func(const void *state) {
  const DBItemSource *db_is = (DBItemSource*) state;
  if (NULL == db_is) {
    error("alive_func got null DBItemSource");
    return 0;
  } else if (db_is->mysql) {
    return !mysql_ping(db_is->mysql);    
  } else {
    return 0;
  }
}
