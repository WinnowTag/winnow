/* Copyright (c) 2008 The Kaphan Foundation
 *
 * Possession of a copy of this file grants no permission or license
 * to use, modify, or create derivate works.
 *
 * Please contact info@peerworks.org for further information.
 */
#include <config.h>
#include <time.h>
#include <stdlib.h>
#include <sqlite3.h>
#include <string.h>
#include <pthread.h>
#if HAVE_JUDY_H
#include <Judy.h>
#endif
#include "item_cache.h"
#include "logging.h"
#include "misc.h"

#define CURRENT_USER_VERSION 1
#define FETCH_ITEM_SQL "select id, strftime('%s', updated) from entries where id = ?"
#define FETCH_ITEM_TOKENS_SQL "select token_id, frequency from entry_tokens where entry_id = ?"
#define FETCH_ALL_ITEMS_SQL "select id, strftime('%s', updated) from entries"
#define FETCH_ALL_ITEMS_TOKENS_SQL "select entry_id, token_id, frequency from entry_tokens order by entry_id"
#define FETCH_RANDOM_BACKGROUND "select entry_id from random_backgrounds"

struct ITEM {
  /* The ID of the item */
  int id;
  /* The number of tokens in the item */
  int total_tokens;
  /* The timestamp the item was created or published */
  time_t time;
  /* The tokens of the item. This is a Judy array of token_id -> frequency. */
  Pvoid_t tokens;
}; 

/** This is the opaque type for the Item Cache */
struct ITEM_CACHE {
  const char *db_file;
  sqlite3 *db;  
  sqlite3_stmt *fetch_item_stmt;
  sqlite3_stmt *fetch_item_tokens_stmt;
  sqlite3_stmt *fetch_all_items_stmt;
  sqlite3_stmt *fetch_all_item_tokens_stmt;
  sqlite3_stmt *random_background_stmt;
  pthread_mutex_t *db_access_mutex; 
  
  int user_version;
  int version_mismatch;
  
  /* Flag for whether the item cache has been loaded. */
  int loaded;
  /* The Judy Array that stores each item keyed by their id. */
  Pvoid_t items_by_id;
  /* TODO A linked list of item ids in descending order of updated time. */
  /* The Random Background pool. */
  Pool *random_background;
};

static int get_user_version(ItemCache *item_cache) {
  int rc = CLASSIFIER_OK;
  sqlite3_stmt *stmt;
  
  if (SQLITE_OK == sqlite3_prepare_v2(item_cache->db, "PRAGMA user_version", -1, &stmt, NULL)) {
    if (SQLITE_ROW == sqlite3_step(stmt)) {
      item_cache->user_version = sqlite3_column_int(stmt, 0);
    } else {
      fatal("Could not fetch the user_version");
      rc = CLASSIFIER_FAIL;
    }
    
    sqlite3_reset(stmt);
    sqlite3_finalize(stmt);
  } else {
    fatal("Could not prepare statement: %s", sqlite3_errmsg(item_cache->db));
    rc = CLASSIFIER_FAIL;
  }
  
  return rc;
}

static int create_prepared_statements(ItemCache *item_cache) {
  int rc = CLASSIFIER_OK;
  
  if (SQLITE_OK != sqlite3_prepare_v2(item_cache->db, FETCH_ITEM_SQL, -1, &(item_cache->fetch_item_stmt), NULL) ||
      SQLITE_OK != sqlite3_prepare_v2(item_cache->db, FETCH_ITEM_TOKENS_SQL, -1, &(item_cache->fetch_item_tokens_stmt), NULL) ||
      SQLITE_OK != sqlite3_prepare_v2(item_cache->db, FETCH_ALL_ITEMS_SQL, -1, &(item_cache->fetch_all_items_stmt), NULL) ||
      SQLITE_OK != sqlite3_prepare_v2(item_cache->db, FETCH_ALL_ITEMS_TOKENS_SQL, -1, &(item_cache->fetch_all_item_tokens_stmt), NULL) ||
      SQLITE_OK != sqlite3_prepare_v2(item_cache->db, FETCH_RANDOM_BACKGROUND, -1, &(item_cache->random_background_stmt), NULL)) {
    fatal("Unable to prepare statment: %s", item_cache_errmsg(item_cache));
    rc = CLASSIFIER_FAIL;
  }
  
  return rc;
}

/** Create an SQLite ItemSource.
 * 
 * @param item_cache A pointer to an pointer to an SQLiteItemSource. This
 *           will be initialized as an SQLiteItemSource.
 * @param db_file The filename for the database file.
 * @returns CLASSIFIER_OK if all went well, CLASSIFIER_FAIL if something went wrong.
 *          Providing that the function could allocate memory, item_cache will be an
 *          allocated item source in either case.  If the return value was
 *          CLASSIFIER_FAIL you can pass item_cache back to sqlite_item_source_errmsg
 *          to get a detailed error message.
 */
int item_cache_create(ItemCache **item_cache, const char * db_file) {
  int rc = CLASSIFIER_OK;
  
  *item_cache = malloc(sizeof(struct ITEM_CACHE));
  
  (*item_cache)->version_mismatch = 0;
  (*item_cache)->items_by_id = NULL;
  (*item_cache)->random_background = NULL;
  (*item_cache)->loaded = false;
  
  (*item_cache)->db_access_mutex = calloc(1, sizeof(pthread_mutex_t));
  if (!(*item_cache)->db_access_mutex) {
    fatal("MALLOC error for db_access_mutex");
    free(*item_cache);
    *item_cache = NULL;
    rc = CLASSIFIER_FAIL;
  } else {
    if (pthread_mutex_init((*item_cache)->db_access_mutex, NULL)) {
      fatal("pthread_mutex_init error for db_access_mutex");
      free(*item_cache);
      *item_cache = NULL;
      rc = CLASSIFIER_FAIL;
    }
  }
  
  if (*item_cache == NULL) {
    fatal("Unable to allocate memory for SQLiteItemSource");
    rc = CLASSIFIER_FAIL;
  } else {  
    if (SQLITE_OK != sqlite3_open_v2(db_file, &((*item_cache)->db), SQLITE_OPEN_READONLY, NULL)) { 
      rc = CLASSIFIER_FAIL;
    } else {
      if (CLASSIFIER_OK == get_user_version(*item_cache)) {
        if ((*item_cache)->user_version != CURRENT_USER_VERSION) {
          (*item_cache)->version_mismatch = 1;
          rc = CLASSIFIER_FAIL;
        } else if (CLASSIFIER_OK != create_prepared_statements(*item_cache)) {
          rc = CLASSIFIER_FAIL;
        }
      } else {
        rc = CLASSIFIER_FAIL;
      }
    }
  }
  
  return rc;
}

/** Free the memory and database connection associated with the item cache.
 */
void free_item_cache(ItemCache *item_cache) {
  
  if (item_cache) {
    if (item_cache->db) {
      sqlite3_finalize(item_cache->fetch_item_stmt);
      sqlite3_finalize(item_cache->fetch_item_tokens_stmt);
      sqlite3_finalize(item_cache->fetch_all_items_stmt);
      sqlite3_finalize(item_cache->fetch_all_item_tokens_stmt);
      sqlite3_close(item_cache->db);
    }
    
    if (item_cache->loaded) {
      Word_t index = 0;
      PWord_t item_pointer;
      JLF(item_pointer, item_cache->items_by_id, index);
      while (NULL != item_pointer) {
        free_item((Item*) (*item_pointer));
        JLN(item_pointer, item_cache->items_by_id, index);
      }
      JLFA(index, item_cache->items_by_id);  
      
      if (item_cache->random_background) {
        free_pool(item_cache->random_background);
      }
    }
    
    if (item_cache->db_access_mutex) {
      pthread_mutex_destroy(item_cache->db_access_mutex);
      free(item_cache->db_access_mutex);
    }
    
    memset(item_cache, 0, sizeof(struct ITEM_CACHE));
    free(item_cache);
  }
}

/** Gets the latest error message.
 */
const char * item_cache_errmsg(const ItemCache *item_cache) {
  const char *msg = "No Error";
  
  if (item_cache && item_cache->db) {
    if (item_cache->version_mismatch) {
      msg = "Database file's user version does not match classifier version. Trying running classifier-db-migrate.";
    } else {
      msg = sqlite3_errmsg(item_cache->db);
    }
  }
  
  return msg;
}

/** Load the items from the database into an in-memory cache.
 * 
 * The in memory cache has two structures, the first indexes each 
 * item by their id's which provides fast retrieval via id, and the
 * second orders ids by time from newest to oldest.
 * 
 * TODO Add locking around item loading in item_cache_load.
 */
int item_cache_load(ItemCache *item_cache) {
  info("item_cache_load");
  if (!item_cache) {
    fatal("item_cache_load got NULL for item_cache");
    return CLASSIFIER_FAIL;
  }
  
  int rc = CLASSIFIER_OK;
  Pvoid_t itemlist = NULL;  
    
  while (SQLITE_ROW == sqlite3_step(item_cache->fetch_all_items_stmt)) {
    int id = sqlite3_column_int(item_cache->fetch_all_items_stmt, 0);
    
    if (id < 1) {
      continue;
    } else {
      PWord_t item_pointer;
      Item *item = create_item(id);
      if (NULL == item) {
        fatal("Malloc error creating item");
        rc = CLASSIFIER_FAIL;
        break;
      }
      item->time = sqlite3_column_int64(item_cache->fetch_all_items_stmt, 1);
      
      JLI(item_pointer, itemlist, id);
      if (item_pointer != NULL) {
        *item_pointer = (Word_t) item;
      } else {
        fatal("Malloc error creating item");
        rc = CLASSIFIER_FAIL;
        break;
      }            
    }
  }
  
  sqlite3_reset(item_cache->fetch_all_items_stmt);
  
  /* Now load the tokens */
  while (SQLITE_ROW == sqlite3_step(item_cache->fetch_all_item_tokens_stmt)) {
    PWord_t item_pointer = NULL;
    int id = sqlite3_column_int(item_cache->fetch_all_item_tokens_stmt, 0);
    int token_id = sqlite3_column_int(item_cache->fetch_all_item_tokens_stmt, 1);
    int frequency = sqlite3_column_int(item_cache->fetch_all_item_tokens_stmt, 2);
    
    JLG(item_pointer, itemlist, id);
    if (NULL != item_pointer) {
      Item *item = (Item*) (*item_pointer);
      if (item) {
        item_add_token(item, token_id, frequency);
      }
    } else {
      error("Got token for missing item: %d", id);
    }
  }
  
  sqlite3_reset(item_cache->fetch_all_item_tokens_stmt);
  
  /* Now load the random background */
  Pool *rndbg = new_pool();
  while (SQLITE_ROW == sqlite3_step(item_cache->random_background_stmt)) {
    int id = sqlite3_column_int(item_cache->random_background_stmt, 0);
    PWord_t item_pointer = NULL;
    JLG(item_pointer, itemlist, id);
    if (NULL != item_pointer) {
      Item *item = (Item*) (*item_pointer);
      if (item) {
        pool_add_item(rndbg, item);
      }      
    }
  }
  
  sqlite3_reset(item_cache->random_background_stmt);
  
  item_cache->random_background = rndbg;
  item_cache->items_by_id = itemlist;
  item_cache->loaded = true;
  
  return rc;
}

/** Returns the number of items in the in-memory cache.
 */
int item_cache_cached_size(const ItemCache *item_cache) {
  int count;
  JLC(count, item_cache->items_by_id, 0, -1);
  return count;
}

/** Returns true if the item cache has been loaded into memory.
 */
int item_cache_loaded(const ItemCache *item_cache) {
  if (!item_cache) {
    fatal("Got NULL item_cache in item_cache_loaded");
    return false;
  }
  
  return item_cache->loaded;
}

/** Fetch an item from the cache.
 * 
 * @param item_cache The ItemCache to get the item from.
 * @param id The id of the item to get.
 * @returns The Item with the id matching id. It is the responsibility of the
 *          caller to free the item.
 * TODO Add locks around item_cache_fetch_item so only a single thread can do the DB access.
 * TODO Add r/w locks around in-memory item cache.
 * TODO Handle SQLITE_BUSY in case another process locks the database.
 */
Item * item_cache_fetch_item(ItemCache *item_cache, int id) {
  int sqlite3_rc;
  Item *item = NULL;
  
  if (!item_cache) {
    fatal("Got NULL item_cache in item_cache_fetch_item");
    return item;
  }
  
  if (item_cache->loaded) {
    PWord_t item_pointer;
    JLG(item_pointer, item_cache->items_by_id, id);
    if (NULL != item_pointer) {
      item = (Item*)(*item_pointer);
    }
  }
  
  if (NULL == item) {
    pthread_mutex_lock(item_cache->db_access_mutex);
    debug("thread(%i) has locked db_access_mutex to fetch %i", pthread_self(), id);
    
    sqlite3_rc = sqlite3_bind_int(item_cache->fetch_item_stmt, 1, id);
    if (SQLITE_OK != sqlite3_rc) {
      fatal("fetch_item_stmt bind error = %s", item_cache_errmsg(item_cache));
      return item;
    } else {  
      sqlite3_rc = sqlite3_step(item_cache->fetch_item_stmt);
      
      if (SQLITE_ROW == sqlite3_rc) {
        item = create_item(sqlite3_column_int(item_cache->fetch_item_stmt, 0));
        if (NULL != item) {
          item->time = (time_t) sqlite3_column_int64(item_cache->fetch_item_stmt, 1);      
        }
      } else if (SQLITE_DONE != sqlite3_rc) {
        error("Error fetching item %d: %s", id, item_cache_errmsg(item_cache));
      }
    }
    
    sqlite3_clear_bindings(item_cache->fetch_item_stmt);
    sqlite3_reset(item_cache->fetch_item_stmt);
    
    /* Load up the tokens */
    if (item) {
      sqlite3_rc = sqlite3_bind_int(item_cache->fetch_item_tokens_stmt, 1, id);
      
      if (SQLITE_OK != sqlite3_rc) {
        fatal("fetch_item_tokens_stmt bind error = %s", item_cache_errmsg(item_cache));
        free_item(item); item = NULL;
      } else {
        while (SQLITE_ROW == sqlite3_step(item_cache->fetch_item_tokens_stmt)) {
          item_add_token(item, sqlite3_column_int(item_cache->fetch_item_tokens_stmt, 0),
                               sqlite3_column_int(item_cache->fetch_item_tokens_stmt, 1));
        }
      }
      
      sqlite3_clear_bindings(item_cache->fetch_item_tokens_stmt);
      sqlite3_reset(item_cache->fetch_item_tokens_stmt);
    }
    
    pthread_mutex_unlock(item_cache->db_access_mutex);
    debug("thread(%i) has unlocked db_access_mutex after fetching %i", pthread_self(), id);
  }
  
  return item;
}

/** Iterates over each item.
 * 
 *  TODO Add read locking to item_cache_each_item.
 */
int item_cache_each_item(const ItemCache *item_cache, ItemIterator iterator, void *memo) {
  if (item_cache->loaded) {
    Word_t index = 0;
    PWord_t item_pointer;
    
    JLF(item_pointer, item_cache->items_by_id, index);
    while (item_pointer != NULL) {
      Item *item = (Item*) (*item_pointer);
      if (CLASSIFIER_OK != iterator(item, memo)) {
        break;
      }
      JLN(item_pointer, item_cache->items_by_id, index);
    }
  }
  return 0;
}

/** Gets the RandomBackground pool.
 * 
 *  This only returns the pool if the item cache has been loaded.
 *  Otherwise it returns NULL.
 * 
 *  The items to use in the random background are defined in the random_backgrounds
 *  table in the database. The pool returned has each of these items added to it.
 *  
 * @param item_cache The ItemCache to get the random background pool for.
 * @return The random background pool.
 */
const Pool * item_cache_random_background(const ItemCache * item_cache)  {
  const Pool * random_background = NULL;
  if (item_cache) {
    random_background = item_cache->random_background;
  }
  
  return random_background;
}

/******************************************************************************
 * Item creation functions 
 ******************************************************************************/

/** Create a empty item.
 *
 */
Item * create_item(int id) {
  Item *item;
  
  item = malloc(sizeof(Item));
  if (NULL != item) {
    item->id = id;
    item->total_tokens = 0;
    item->time = 0;
    item->tokens = NULL;
  } else {
    fatal("Malloc Error allocating item %d", id);
  }
  
  return item;
}

static int load_tokens_from_array(Item *item, int tokens[][2], int num_tokens) {
  int i;
  int return_code = 0;
  for (i = 0; i < num_tokens; i++) {
    int token_id = tokens[i][0];
    int token_frequency = tokens[i][1];
    
    if (item_add_token(item, token_id, token_frequency)) {
      return_code = 1;
      break;
    }      
  }
  
  return return_code;
}

/** Create an item from an array of tokens.
 *
 *  @param id The id of the item.
 *  @param tokens A 2D array of tokens where each row is {token_id, frequency}.
 *  @param num_tokens The length of the token array.
 *  @returns A new item initialized with the tokens.
 */
Item * create_item_with_tokens(int id, int tokens[][2], int num_tokens) {
  Item *item = create_item(id);
  if (NULL != item) {  
    if (load_tokens_from_array(item, tokens, num_tokens)) {
      free_item(item);
      item = NULL;
    }
  }
  
  return item;
}

/***************************************************************************
 * Item functions 
 ***************************************************************************/

int item_get_id(const Item * item) {
  return item->id;
}

time_t item_get_time(const Item * item) {
  return item->time;
}

int item_get_num_tokens(const Item * item) {
  Word_t count;
  JLC(count, item->tokens, 0, -1);
  return (int) count;
}

int item_get_total_tokens(const Item * item) {
  return item->total_tokens;
}

int item_get_token(const Item * item, int token_id, Token_p token) {
  Word_t * frequency;
  JLG(frequency, item->tokens, token_id);
  
  token->id = token_id;
  if (NULL == frequency) {
    token->frequency = 0;
  } else {
    token->frequency = *frequency;
  }
  
  return 0;
}

int item_next_token(const Item * item, Token_p token) {
  int success = true;  
  PWord_t frequency = NULL;
  Word_t  token_id = token->id;
  
  if (NULL != item) {
    if (0 == token_id) {    
      JLF(frequency, item->tokens, token_id);
    } else {
      JLN(frequency, item->tokens, token_id);
    }  
  }
 
  if (NULL == frequency) {
    success = false;
    token->id = 0;
    token->frequency = 0;
  } else {
    token->id = token_id;
    token->frequency = *frequency;
  }
  
  return success;
}

void free_item(Item *item) {
  if (NULL != item) {
    int freed_bytes;
    if (item->tokens) {
      JLFA(freed_bytes, item->tokens);
      item->tokens = NULL;
    }
    free(item);    
  }
}



int item_add_token(Item *item, int id, int token_frequency) {  
  int return_code = 0;  
  Word_t token_id;
  Word_t * token_frequency_p;
  
  item->total_tokens += token_frequency;
  token_id = (Word_t) id;
  
  JLI(token_frequency_p, item->tokens, token_id);
  if (PJERR == token_frequency_p) {
    error("Could not malloc memory for token array");
    return_code = ERR;
  } else {
    *token_frequency_p = token_frequency;
  }
  
  return return_code;
}

