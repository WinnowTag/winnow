/* Copyright (c) 2007 The Kaphan Foundation
 *
 * Possession of a copy of this file grants no permission or license
 * to use, modify, or create derivate works.
 *
 * Please contact info@peerworks.org for further information.
 */

#include <stdlib.h>
#include <mysql.h>
#include <string.h>
#include "misc.h"
#include "tagging.h"
#include "logging.h"

#define INS_TAGGING_STMT "insert into taggings                                                      \
                          (feed_item_id, tag_id, user_id, strength, classifier_tagging, created_on) \
                           VALUES (?, ?, ?, ?, 1, UTC_TIMESTAMP()) ON DUPLICATE KEY UPDATE          \
                           strength = VALUES(strength), created_on = VALUES(created_on)"
#define DEL_TAGGING_STMT "delete from taggings where feed_item_id = ? and tag_id = ? and user_id = ?\
                          and classifier_tagging = 1"
struct TAGGING_STORE {
  DBConfig config;
  MYSQL *mysql;
  MYSQL_STMT *insertion_stmt;
  MYSQL_STMT *delete_stmt;
  float insertion_threshold;
};

static int establish_connection(TaggingStore *store);

TaggingStore * create_db_tagging_store(const DBConfig *config, float insertion_threshold) {
  TaggingStore *store = malloc(sizeof(TaggingStore));
  if (NULL != store) {
    memcpy(&(store->config), config, sizeof(DBConfig));
    store->insertion_threshold = insertion_threshold;
    
    if (!establish_connection(store)) {
      free_tagging_store(store);
      store = NULL;
      fatal("Error creating tagging store");
    }
  }
  
  return store;
}

void free_tagging_store(TaggingStore *store) {
  if (store) {
    tagging_store_close(store);
    free(store);
  }
}

void tagging_store_close(TaggingStore *store) {
  if (store) {
    if (store->insertion_stmt) mysql_stmt_close(store->insertion_stmt);
    if (store->mysql) mysql_close(store->mysql);
    
    store->insertion_stmt = NULL;
    store->mysql = NULL;
  }
}

int tagging_store_is_alive(TaggingStore *store) {
  int alive = false;
  
  if (store) {
    if (!store->mysql) {
      alive = establish_connection(store);
    } else if (mysql_ping(store->mysql)) {
      error("TaggingStore connection is down: %s", mysql_error(store->mysql));
      alive = establish_connection(store);
      if (!alive) {
        fatal("Could not reconnect to mysql: %s", mysql_error(store->mysql));
      }
    } else {
      alive = true;
    }
  }
  
  return alive;
}

int tagging_store_store_taggings (TaggingStore *store, Tagging **taggings, int size) {
  int success = true;
  
  if (tagging_store_is_alive(store) && taggings) {
    if (mysql_autocommit(store->mysql, 0)) {
      error("Could not set autocommit = 0: %s", mysql_error(store->mysql));
      success = false;
    } else {
      int i;
      
      for (i = 0; i < size; i++) {
        success = tagging_store_store(store, taggings[i]);
        
        if (!success) {
          mysql_rollback(store->mysql);
          break;
        }
      }
      
      if (success && mysql_commit(store->mysql)) {
          error("Error committing tagging transaction: %s", mysql_error(store->mysql));
          success = false;
      }
        
      mysql_autocommit(store->mysql, 1);      
    }    
  }
  
  return success;
}

int tagging_store_store(TaggingStore *store, const Tagging *tagging) {
  int success = true;
  if (tagging_store_is_alive(store) && tagging) {
    MYSQL_STMT *stmt = tagging->strength >= store->insertion_threshold ?
                        store->insertion_stmt :
                        store->delete_stmt;
    MYSQL_BIND params[4];
    memset(params, 0, sizeof(params));
    params[0].buffer_type = MYSQL_TYPE_LONG;
    params[1].buffer_type = MYSQL_TYPE_LONG;
    params[2].buffer_type = MYSQL_TYPE_LONG;
    params[3].buffer_type = MYSQL_TYPE_DOUBLE;
    params[0].buffer = (char *) &(tagging->item_id);
    params[1].buffer = (char *) &(tagging->tag_id);
    params[2].buffer = (char *) &(tagging->user_id);
    params[3].buffer = (char *) &(tagging->strength);
   
    if (mysql_stmt_bind_param(stmt, params)) {
      error("Error binding variables for insertion statement: %s", mysql_stmt_error(stmt));
      success = false;
    } else if (mysql_stmt_execute(stmt)) {
      error("Error executing tagging insertion statement: %s", mysql_stmt_error(stmt));
      success = false;
    }
  }
  
  return success;
}

static int establish_connection(TaggingStore *store) {
  int success = true;
  const DBConfig *config = &(store->config);
  
  store->mysql = mysql_init(NULL);
      
  if (NULL == store->mysql) {
    success = false;
  } else if (!mysql_real_connect(store->mysql, config->host, config->user, config->password, config->database, config->port, NULL, 0)) {
    error("Error connecting to MySQL tagging store: %s", mysql_error(store->mysql));
    success = false;
  } else {
    store->insertion_stmt = mysql_stmt_init(store->mysql);
    store->delete_stmt = mysql_stmt_init(store->mysql);
    
    if (NULL == store->insertion_stmt || NULL == store->delete_stmt) {
      fatal("Error mallocing Prepared Statement: %s", mysql_error(store->mysql));
      success = false;
    } else if (mysql_stmt_prepare(store->insertion_stmt, INS_TAGGING_STMT, strlen(INS_TAGGING_STMT)) ||
               mysql_stmt_prepare(store->delete_stmt, DEL_TAGGING_STMT, strlen(DEL_TAGGING_STMT))) {
      fatal("Error preparing hard coded statement: %s", mysql_error(store->mysql));
      success = false;
    }
  }
    
  return success;
}
