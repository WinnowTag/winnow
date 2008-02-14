/* Copyright (c) 2007 The Kaphan Foundation
 *
 * Possession of a copy of this file grants no permission or license
 * to use, modify, or create derivate works.
 *
 * Please contact info@peerworks.org for further information.
 */

#include <stdlib.h>
#include <mysql.h>
#include <mysqld_error.h>
#include <string.h>
#include "misc.h"
#include "tagging.h"
#include "tag.h"
#include "logging.h"

#define INS_TAGGING_STMT "insert into taggings                                                      \
                          (feed_item_id, tag_id, user_id, strength, classifier_tagging, created_on) \
                           VALUES (?, ?, ?, ?, 1, UTC_TIMESTAMP()) ON DUPLICATE KEY UPDATE          \
                           strength = VALUES(strength), created_on = VALUES(created_on)"
#define DEL_TAGGINGS_STMT "delete from taggings where tag_id = ? and classifier_tagging = 1"

struct TAGGING_STORE {
  DBConfig config;
  MYSQL *mysql;
  MYSQL_STMT *insertion_stmt;
  MYSQL_STMT *delete_by_tag_stmt;
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

int tagging_store_clear_for_tag(TaggingStore *store, const Tag *tag) {
  int success = true;
  
  if (tagging_store_is_alive(store)) {
    MYSQL_BIND params[1];
    memset(params, 0, sizeof(params));
    params[0].buffer_type = MYSQL_TYPE_LONG;
    params[0].buffer = (char *) &(tag->tag_id);
    
    if (mysql_stmt_bind_param(store->delete_by_tag_stmt, params)) {
      error("Error binding variables for delete_by_tag statement: %s", mysql_stmt_error(store->delete_by_tag_stmt));
      success = false;
    } else if (mysql_stmt_execute(store->delete_by_tag_stmt)) {
      error("Error executing delete_by_tag statement: %s", mysql_stmt_error(store->delete_by_tag_stmt));
      success = false;
    }    
  } else {
    success = false;
  }
  
  return success;
}

/** Replaces the taggings for a given tag with the provided taggings.
 * 
 * @param store The tagging store in which to store the taggings.
 * @param taglist A list of tags to replace. All the classifier taggings for the 
 *                tags in this TagList will be deleted before the new taggings
 *                are inserted.  This can be NULL, in which case now classifier
 *                taggings are deleted.
 * @param taggings An array of new taggings. All taggings with strength higher than
 *                 store->insertion_threshold will be inserted into the database.
 * @param size    The size of the taggings array.
 * @param progress A pointer to a float. If not NULL this will be updated with progress
 *                 as taggings are inserted.
 */
int tagging_store_replace_taggings (TaggingStore *store, const TagList *taglist, Tagging **taggings, int size, float *progress) {
  int success = true;
  
  if (tagging_store_is_alive(store) && taggings) {
    if (mysql_autocommit(store->mysql, 0)) {
      error("Could not set autocommit = 0: %s", mysql_error(store->mysql));
      success = false;
    } else {
      int increment_base = size;
      if (taglist) {
        increment_base += taglist->size;
      }
      
      float progress_increment = 20.0 / increment_base;
      int i;
      
      if (taglist) {
        for (i = 0; i < taglist->size; i++) {
          success = tagging_store_clear_for_tag(store, taglist->tags[i]);
          
          if (!success) {
            mysql_rollback(store->mysql);
            break;
          } else if (NULL != progress) {
            *progress += progress_increment;
          }
        }
      }
      
      if (success) {      
        for (i = 0; i < size; i++) {
          success = tagging_store_store(store, taggings[i]);
          
          if (!success) {
            mysql_rollback(store->mysql);
            break;
          } else if (NULL != progress) {
            *progress += progress_increment;
          }
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
  if (tagging_store_is_alive(store) && tagging && tagging->strength >= store->insertion_threshold) {
    MYSQL_STMT *stmt = store->insertion_stmt;
                        
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
    } else if (mysql_stmt_execute(stmt) && mysql_stmt_errno(stmt) != ER_NO_REFERENCED_ROW_2) {
      error("Error executing tagging insertion statement: %s (%d)", mysql_stmt_error(stmt), mysql_stmt_errno(stmt));
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
    store->delete_by_tag_stmt = mysql_stmt_init(store->mysql);
    
    if (NULL == store->insertion_stmt || NULL == store->delete_by_tag_stmt) {
      fatal("Error mallocing Prepared Statement: %s", mysql_error(store->mysql));
      success = false;
    } else if (mysql_stmt_prepare(store->insertion_stmt, INS_TAGGING_STMT, strlen(INS_TAGGING_STMT)) ||
               mysql_stmt_prepare(store->delete_by_tag_stmt, DEL_TAGGINGS_STMT, strlen(DEL_TAGGINGS_STMT))) {
      fatal("Error preparing hard coded statement: %s", mysql_error(store->mysql));
      success = false;
    }
  }
    
  return success;
}
