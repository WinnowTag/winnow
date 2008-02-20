/* Copyright (c) 2008 The Kaphan Foundation
 *
 * Possession of a copy of this file grants no permission or license
 * to use, modify, or create derivate works.
 *
 * Please contact info@peerworks.org for further information.
 */

#include <stdlib.h>
#include <sqlite3.h>
#include <string.h>
#include "item_cache.h"
#include "logging.h"
#include "misc.h"

#define CURRENT_USER_VERSION 1

/** This is the opaque type for the SQLite Item Source */
struct ITEM_CACHE {
  sqlite3 *db;  
  const char *db_file;
  int user_version;
  int last_rc;
};

static int get_user_version(ItemCache *is) {
  int rc = CLASSIFIER_OK;
  sqlite3_stmt *stmt;
  
  is->last_rc = sqlite3_prepare_v2(is->db, "PRAGMA user_version", -1, &stmt, NULL);
  if (SQLITE_OK == is->last_rc) {
    if (SQLITE_ROW == sqlite3_step(stmt)) {
      is->user_version = sqlite3_column_int(stmt, 0);       
    } else {
      fatal("Could not fetch the user_version");
      rc = CLASSIFIER_FAIL;
    }
    
    sqlite3_reset(stmt);
    sqlite3_finalize(stmt);
  } else {
    fatal("Could not prepare statement: %s", sqlite3_errmsg(is->db));
    rc = CLASSIFIER_FAIL;
  }
  
  return rc;
}

/** Create an SQLite ItemSource.
 * 
 * @param is A pointer to an pointer to an SQLiteItemSource. This
 *           will be initialized as an SQLiteItemSource.
 * @param db_file The filename for the database file.
 * @returns CLASSIFIER_OK if all went well, CLASSIFIER_FAIL if something went wrong.
 *          Providing that the function could allocate memory, is will be an
 *          allocated item source in either case.  If the return value was
 *          CLASSIFIER_FAIL you can pass is back to sqlite_item_source_errmsg
 *          to get a detailed error message.
 */
int item_cache_create(ItemCache **is, const char * db_file) {
  int rc = CLASSIFIER_OK;
  
  *is = malloc(sizeof(struct ITEM_CACHE));
  if (*is == NULL) {
    fatal("Unable to allocate memory for SQLiteItemSource");
    rc = CLASSIFIER_FAIL;
  } else {  
    (*is)->last_rc = sqlite3_open_v2(db_file, &((*is)->db), SQLITE_OPEN_READONLY, NULL);
    if (SQLITE_OK != (*is)->last_rc) { 
      rc = CLASSIFIER_FAIL;
    } else {
      if (CLASSIFIER_OK == get_user_version(*is)) {
        if ((*is)->user_version != CURRENT_USER_VERSION) {
          rc = CLASSIFIER_FAIL;
        }     
      } else {
        rc = CLASSIFIER_FAIL;
      }
    }
  }
  
  return rc;
}

void free_item_cache(ItemCache *is) {
  if (is) {
    if (is->db) {
      sqlite3_close(is->db);
    }
    
    memset(is, 0, sizeof(struct ITEM_CACHE));
    free(is);
  }
}

const char * item_cache_errmsg(const ItemCache *is) {
  const char *msg = "No Error";
  
  if (is && is->db) {
    if (is->last_rc != SQLITE_OK) {
      msg = sqlite3_errmsg(is->db);
    } else if (is->user_version != CURRENT_USER_VERSION) {
      msg = "Database file's user version does not match classifier version. Trying running classifier-db-migrate.";
    }
  }
  
  return msg;
}
