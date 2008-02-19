/* Copyright (c) 2008 The Kaphan Foundation
 *
 * Possession of a copy of this file grants no permission or license
 * to use, modify, or create derivate works.
 *
 * Please contact info@peerworks.org for further information.
 */

#include <stdlib.h>
#include <sqlite3.h>
#include "sqlite_item_source.h"
#include "logging.h"
#include "misc.h"

#define CURRENT_USER_VERSION 1

struct SQLITE_ITEM_SOURCE {
  sqlite3 *db;  
  const char *db_file;
  int user_version;
  int last_rc;
};

static int get_user_version(SQLiteItemSource *is) {
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

int sqlite_item_source_create(SQLiteItemSource **is, const char * db_file) {
  int rc = CLASSIFIER_OK;
  
  *is = malloc(sizeof(struct SQLITE_ITEM_SOURCE));
  if (*is == NULL) {
    fatal("Unable to allocate memory for SQLiteItemSource");
    rc = CLASSIFIER_FAIL;
  }
  
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
  
  return rc;
}

const char * sqlite_item_source_errmsg(const SQLiteItemSource *is) {
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
