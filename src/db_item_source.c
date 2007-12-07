/* Copyright (c) 2007 The Kaphan Foundation
 *
 * Possession of a copy of this file grants no permission or license
 * to use, modify, or create derivate works.
 *
 * Please contact info@peerworks.org for further information.
 */

#include <mysql.h>
#include <errmsg.h>
#include <string.h>
#include "db_item_source.h"
#include "time_helper.h"
#include "logging.h"
#include "misc.h"

#define FETCH_ITEM_SQL "select token_id, frequency, time from feed_item_tokens \
                        join feed_items on feed_items.id = feed_item_tokens.feed_item_id \
                        where feed_item_id = ?"
#define FETCH_ALL_ITEMS_SQL "select feed_items.id, token_id, frequency, time from feed_items \
                        join feed_item_tokens on feed_items.id = feed_item_tokens.feed_item_id \
                        order by time desc, feed_items.id"

typedef struct DB_ITEMSOURCE {
  /* DB connection configuration */
  DBConfig *config;
  /* MySQL connection */
  MYSQL *mysql;
  /* Statement used for selecting a specific item */
  MYSQL_STMT *fetch_item_stmt;
  /* Statement used for selecting all items */
  MYSQL_STMT *fetch_all_items_stmt;
} DBItemSource;

static Item *     fetch_item_func      (const void *state, int item_id);
static ItemList * fetch_all_items_func (const void *state);
static int        alive_func           (const void *state);
static void       close_func           (void *state);
static int        establish_connection (DBItemSource *is);

/** Creates an ItemSource for a MySQL database.
 *
 * The expected schema is the Winnow item - tokens schema, i.e. a single
 * table called feed_item_tokens with feed_item_id, token_id and
 * frequency columns, all being integers.
 */
ItemSource * create_db_item_source(DBConfig * config) {
  info("Creating item source (host='%s',user='%s',pass='%s',db='%s')", 
    config->host, config->user, config->password, config->database);
  ItemSource *is = create_item_source();
  
  if (NULL != is) {
    is->fetch_all_func = fetch_all_items_func;
    is->fetch_func = fetch_item_func;
    is->alive_func = alive_func;
    is->free_func  = free_db_item_source;
    is->close_func = close_func;
    
    DBItemSource *state = malloc(sizeof(DBItemSource));
    if (NULL == state) {
      free(is);
      fatal("Could not allocate DBItemSource");
    }
    
    is->state = state;
    state->config = config;
    state->mysql = NULL;
    state->fetch_all_items_stmt = NULL;
    state->fetch_item_stmt = NULL;
    
    if (!establish_connection(state)) {
      free_db_item_source(state);
      is = NULL;
      error("Could not connect to Item database");      
    }
    
  } else {
    fatal("Could not allocate ItemSource");
  }
  
  return is;
}

/** Fetch the item source from the DB.
 *
 *  NOT THREAD SAFE!
 *
 *  @param state The ItemSource state.
 *  @param item_id The id of the Item to fetch.
 *  @return The item or NULL if it doesn't exist.
 */
Item * fetch_item_func(const void *state_p, int item_id) {
  DBItemSource *state = (DBItemSource*) state_p;
  Item *item = NULL;
  MYSQL_STMT *stmt = state->fetch_item_stmt;
  
  if (alive_func(state)) {
    MYSQL_BIND bind[1];
    memset(bind, 0, sizeof(bind));
    bind[0].buffer_type = MYSQL_TYPE_LONG;
    bind[0].buffer = (char *) &item_id;
    
    if (mysql_stmt_bind_param(stmt, bind)) goto fetch_item_query_error;      
    if (mysql_stmt_execute(stmt))          goto fetch_item_query_error;
    
    int token_id;
    int frequency;
    MYSQL_TIME time;
    MYSQL_BIND results[3];
    memset(results, 0, sizeof(results));
    results[0].buffer_type = MYSQL_TYPE_LONG;
    results[0].buffer = (char *) &token_id;
    results[1].buffer_type = MYSQL_TYPE_LONG;
    results[1].buffer = (char *) &frequency;
    results[2].buffer_type = MYSQL_TYPE_DATETIME;
    results[2].buffer = (char *) &time;
    
    if (mysql_stmt_bind_result(stmt, results)) goto fetch_item_query_error;
    if (mysql_stmt_store_result(stmt))         goto fetch_item_query_error;
    
    if (!mysql_stmt_fetch(stmt)) {
      item = create_item(item_id);
      if (NULL != item) {
        convert_mysql_time(&time, &(item->time));
        item_add_token(item, token_id, frequency);
        while (!mysql_stmt_fetch(stmt)) {
          item_add_token(item, token_id, frequency);
        }
      }
    }
  }
 
exit:
  mysql_stmt_free_result(stmt);
  return item;
  
fetch_item_query_error:
  if (mysql_stmt_errno(stmt) == CR_OUT_OF_MEMORY) {
    fatal("Out of memory error fetching item: %s", mysql_stmt_error(stmt));
  } else {
    error("Error fetching item: %s", mysql_stmt_error(stmt));
  }
  item = NULL;
  goto exit;
}

/** Fetch all items from the database.
 *
 *  Returns an item list of all items in the database.
 */
ItemList * fetch_all_items_func(const void *state_p) {
  DBItemSource *state = (DBItemSource*) state_p;
  ItemList *item_list = create_item_list();
  MYSQL_STMT *stmt = state->fetch_all_items_stmt;
  
  if (alive_func(state)) {
    Item *item = NULL;
    if (mysql_stmt_execute(stmt)) goto fetch_all_items_query_error;
        
    int feed_item_id;
    int token_id;
    int frequency;
    MYSQL_TIME time;
    MYSQL_BIND results[4];
    memset(results, 0, sizeof(results));
    results[0].buffer_type = MYSQL_TYPE_LONG;
    results[1].buffer_type = MYSQL_TYPE_LONG;
    results[2].buffer_type = MYSQL_TYPE_LONG;
    results[3].buffer_type = MYSQL_TYPE_DATETIME;
    results[0].buffer      = (char *) &feed_item_id;
    results[1].buffer      = (char *) &token_id;
    results[2].buffer      = (char *) &frequency;
    results[3].buffer      = (char *) &time;
    
    if (mysql_stmt_bind_result(stmt, results)) goto fetch_all_items_query_error;
    
    while (!mysql_stmt_fetch(stmt)) {
      if (NULL == item || item_get_id(item) != feed_item_id) {
        item = create_item(feed_item_id);
        convert_mysql_time(&time, &(item->time));
        if (NULL == item) goto fetch_all_items_query_error;
        item_list_add_item(item_list, item);
      }
      
      item_add_token(item, token_id, frequency);
    }
  }
  
exit:
  mysql_stmt_free_result(stmt);
  return item_list;
  
fetch_all_items_query_error:
  if (mysql_stmt_errno(stmt) == CR_OUT_OF_MEMORY) {
    fatal("Out of memory error fetching all items: %s", mysql_stmt_error(stmt));
  } else {
    error("Error fetching all items: %s", mysql_stmt_error(stmt));
  }
  if(item_list) {
    free_item_list(item_list);
    item_list = NULL;
  }
  goto exit;
}

/** Checks whether the item source is alive.
 */
int alive_func(const void *state) {
  int alive = true;
  DBItemSource *db_is = (DBItemSource*) state;
  
  if (NULL != db_is) {
    if (!db_is->mysql) {
      alive = establish_connection(db_is);
    } else if (mysql_ping(db_is->mysql)) {
      info("MySQL server is down %s. Attempting reconnect.", mysql_error(db_is->mysql));
      alive = establish_connection(db_is);
      if (!alive) {
        error("Could not reconnect: %s.", mysql_error(db_is->mysql));
      }
    }
  } else {
    alive = false;
  }
  
  return alive;
}

void close_func(void *state) {
  DBItemSource *db_is = state;
  
  if (db_is) {
    if (db_is->fetch_item_stmt) {
      mysql_stmt_close(db_is->fetch_item_stmt);
    }
    
    if (db_is->fetch_all_items_stmt) {
      mysql_stmt_close(db_is->fetch_all_items_stmt);
    }
    
    if (db_is->mysql) {
      mysql_close(db_is->mysql);
    }
    
    db_is->fetch_all_items_stmt = NULL;
    db_is->fetch_item_stmt = NULL;
    db_is->mysql = NULL;
  }
}

void free_db_item_source(void *state) {
  DBItemSource *db_is = (DBItemSource*) state;
  if (NULL != db_is) {
    close_func(state);    
    free(db_is);
  }
}

int establish_connection(DBItemSource *state) {
  int success = true;
  
  if (!state) {
    success = false;
    error("Got null DBItemSource");
  } else {  
    DBConfig *config = state->config;
    state->mysql = mysql_init(NULL);
    if (NULL == state->mysql) {
      fatal("Could not allocate MYSQL");        
    } else if (!mysql_real_connect(state->mysql, config->host, config->user, 
                              config->password, config->database, config->port, NULL, 0)) {
      success = false;
      error("Failed to connect to item database: %s", mysql_error(state->mysql));      
    } else {
      state->fetch_item_stmt = mysql_stmt_init(state->mysql);
      state->fetch_all_items_stmt = mysql_stmt_init(state->mysql);

      if (NULL == state->fetch_item_stmt || 
          NULL == state->fetch_all_items_stmt) {
        success = false;
        error("Error creating prepared statement");
      }
      
      if (mysql_stmt_prepare(state->fetch_item_stmt, FETCH_ITEM_SQL, strlen(FETCH_ITEM_SQL))) {
        success = false;
        error("Error preparing hard coded statement (%s): %s", mysql_error(state->mysql), FETCH_ITEM_SQL);
      } else if (mysql_stmt_prepare(state->fetch_all_items_stmt, FETCH_ALL_ITEMS_SQL, strlen(FETCH_ALL_ITEMS_SQL))) {
        success = false;
        error("Error preparing hard coded statement (%s): %s", mysql_error(state->mysql), FETCH_ALL_ITEMS_SQL);
      }
    }
  }
  
  return success;
}
