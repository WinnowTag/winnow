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
#include <errno.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <libxml/tree.h>
#include <libxml/xpath.h>
#include <libxml/xpathInternals.h>
#include <libxml/uri.h>

#if HAVE_JUDY_H
#include <Judy.h>
#include <sqlite3.h>
#include <sys/types.h>
#endif
#include "item_cache.h"
#include "logging.h"
#include "misc.h"
#include "job_queue.h"
#include "xml.h"
#include "array.h"
#include "tokenizer.h"

#define CURRENT_USER_VERSION 5
#define FETCH_ITEM_SQL "select full_id, id, strftime('%s', updated) from entries where full_id = ?"
#define FETCH_ALL_ITEMS_SQL "select full_id, id, strftime('%s', updated) from entries where updated > (julianday('now') - ?) order by updated desc"
#define FETCH_RANDOM_BACKGROUND "select full_id, id from entries where id in (select entry_id from random_backgrounds)"
#define INSERT_ENTRY_SQL "insert into entries (full_id, updated, created_at) \
                          VALUES (:full_id, julianday(:updated, 'unixepoch'), julianday(:created_at, 'unixepoch'))"
#define UPDATE_ENTRY_SQL "update entries set updated = julianday(?, 'unixepoch') where full_id = ?"
#define DELETE_ENTRY_SQL "delete from entries where id = ?"
#define FIND_ATOM_SQL "select id from tokens where token = ?"
#define INSERT_ATOM_SQL "insert into tokens (token) values (?)"
#define FIND_TOKEN_SQL "select token from tokens where id = ?"
#define CORRUPT_TOKEN_FILE "Token file %s did not have a multiple of %i bytes, it has %i bytes and is possibly corrupt."
#define INSERT_ATOM_XML_SQL "insert into atom.entry_atom values (?, ?)"
#define DELETE_ATOM_XML_SQL "delete from atom.entry_atom where id = ?"
#define FETCH_ENTRY_TOKENS  "select tokens from token.entry_tokens where id = ?"
#define INSERT_ENTRY_TOKENS "insert into token.entry_tokens values (?, ?)"
#define DELETE_ENTRY_TOKENS "delete from token.entry_tokens where id = ?"
#define TOUCH_ITEM_SQL "update entries set last_used_at = julianday('now') where full_id = ?"
#define TOKEN_BYTES 6
#define PROCESSING_LIMIT 200

typedef struct ORDERED_ITEM_LIST OrderedItemList;
struct ORDERED_ITEM_LIST {
  Item *item;
  OrderedItemList *next;
};

struct ITEM {
  /* The ID of the item */
  unsigned char * id;
  /* Database surrogate key for the item */
  int key;
  /* The number of tokens in the item */
  int total_tokens;
  /* The timestamp the item. This is sort of overloaded, if the item is loaded from the database it is
   * the time the item was added to the database. If the item was added to a live cache via a REST operation
   * it will be the time the item was added to the cache.
   */
  time_t time;
  /* The tokens of the item. This is a Judy array of token_id -> frequency. */
  Pvoid_t tokens;
};

typedef enum UPDATE_TYPE {
  ADD,
  DELETE
} UpdateType;

typedef struct UPDATE_JOB {
  UpdateType type;
  Item *item;
} UpdateJob;

struct ITEM_CACHE_ENTRY {
  int id;          /* This is the DB id */
  char * full_id;  /* This is the "global" id */
  time_t updated;
  time_t created_at;
  char * atom;
};

/** This is the opaque type for the Item Cache */
struct ITEM_CACHE {
  char *cache_directory;

  /* Item cache's copy of ItemCacheOptions */
  int cache_update_wait_time;
  int load_items_since;
  int min_tokens;

  sqlite3 *db;
  sqlite3_stmt *fetch_item_stmt;
  sqlite3_stmt *fetch_all_items_stmt;
  sqlite3_stmt *random_background_stmt;
  sqlite3_stmt *insert_entry_stmt;
  sqlite3_stmt *update_entry_stmt;
  sqlite3_stmt *delete_entry_stmt;
  sqlite3_stmt *insert_atom_xml_stmt;
  sqlite3_stmt *delete_atom_xml_stmt;
  sqlite3_stmt *find_token_stmt;
  sqlite3_stmt *find_atom_stmt;
  sqlite3_stmt *insert_atom_stmt;
  sqlite3_stmt *insert_tokens_stmt;
  sqlite3_stmt *fetch_tokens_stmt;
  sqlite3_stmt *delete_tokens_stmt;
  sqlite3_stmt *touch_item_stmt;

  /* Mutex for database access.
   *
   * To prevent deadlocks, this should only be locked in the public API functions,
   * all static functions that require the lock to be held should expect the lock
   * to be acquired in the caller.
   */
  pthread_mutex_t db_access_mutex;

  int user_version;
  int version_mismatch;

  /************************************
   *  In-memory cache members
   */

  /* Flag for whether the item cache has been loaded. */
  int loaded;

  /* The Judy Array that stores each item keyed by their id. */
  Pvoid_t items_by_id;

  /* Number of items in the array */
  int cached_size;

  /* A linked list of item ids in descending order of updated time. */
  OrderedItemList *items_in_order;

  /* The Random Background pool. */
  Pool *random_background;

  /************************************
   *  In-memory cache updating members
   */

  /* Queue for items to get added to the items_by_id and items_in_order lists. */
  Queue *update_queue;

  /* Thread which handles the cache updating */
  pthread_t *cache_updating_thread;

  /* R/W lock for accessing the in-memory item cache.
   *
   * To prevent deadlocks, this should only be locked in the public API functions,
   * all static functions that require the lock to be held should expect the lock
   * to be acquired in the caller.
   */
  pthread_rwlock_t cache_lock;

  /* Callback for updates to the item cache */
  UpdateCallback update_callback;

  /* Additional arguments to the udpate callback */
  void *update_callback_memo;

  /* Thread that purges the item cache */
  pthread_t *purge_thread;

  /* Cache purging interval in seconds */
  int purge_interval;

  int shutting_down;
};

/******************************************************************************
 * Update job functions
 ******************************************************************************/
static UpdateJob * create_add_job(Item * item) {
  UpdateJob *job = calloc(1, sizeof(struct UPDATE_JOB));
  if (job) {
    job->item = item;
    job->type = ADD;
  }
  return job;
}


/******************************************************************************
 * ItemCacheEntry functions
 ******************************************************************************/
#define COPY_STRING(dest, s) if (s) {   \
 dest = strdup(s);                      \
 if (!dest) {                           \
   fatal("Malloc error copying string");\
 }                                      \
}

#define FREE_STRING(s) if (s) { \
  free(s);                      \
}

/* Creates an item cache entry.
 *
 * This maps to the entries table in the database.
 */
ItemCacheEntry * create_item_cache_entry( const char * full_id,
                                          time_t updated,
                                          time_t created_at,
                                          const char * atom) {
  ItemCacheEntry *entry = calloc(1, sizeof(struct ITEM_CACHE_ENTRY));

  if (entry) {
    debug("new entry %s", full_id);
    entry->updated = updated;
    entry->created_at = created_at;
    COPY_STRING(entry->full_id, full_id);
    COPY_STRING(entry->atom, atom);
  } else {
    fatal("Malloc failed in create_item_cache_entry");
  }

  return entry;
}

static ItemCacheEntry * copy_entry(const ItemCacheEntry * entry) {
  ItemCacheEntry *copy = create_item_cache_entry(entry->full_id, entry->updated, entry->created_at, entry->atom);
  copy->id = entry->id;
  return copy;
}

ItemCacheEntry * create_entry_from_atom_xml(const char * xml) {
  ItemCacheEntry *entry = NULL;

  xmlDocPtr doc;

  if (NULL == (doc = xmlReadMemory(xml, strlen(xml), "", NULL, XML_PARSE_COMPACT))) {
    error("BAD DATA: %s", xml);
  } else {
    entry = calloc(1, sizeof(struct ITEM_CACHE_ENTRY));
    xmlXPathContextPtr ctx = xmlXPathNewContext(doc);
    xmlXPathRegisterNs(ctx, BAD_CAST "atom", BAD_CAST "http://www.w3.org/2005/Atom");

    entry->full_id = get_element_value(ctx, "/atom:entry/atom:id/text()");
    entry->updated = get_element_value_time(ctx, "/atom:entry/atom:updated/text()");
    entry->atom = strdup(xml);

    xmlXPathFreeContext(ctx);
    xmlFreeDoc(doc);
  }

  return entry;
}

/** Create an entry from atom XML.
 */
ItemCacheEntry * create_entry_from_atom_xml_document(xmlDocPtr doc, const char * xml_source) {
  ItemCacheEntry *entry = NULL;
  xmlXPathContextPtr context = xmlXPathNewContext(doc);
  xmlXPathRegisterNs(context, BAD_CAST "atom", BAD_CAST "http://www.w3.org/2005/Atom");

  char *id = get_element_value(context, "/atom:entry/atom:id/text()");
  char *updated = get_element_value(context, "/atom:entry/atom:updated/text()");

  // Must have an id to be able to create an item, everything else is optional.
  if (id) {
    struct tm updated_tm;
    memset(&updated_tm, 0, sizeof(updated_tm));
    time_t updated_time = time(NULL);

    if (updated && NULL != strptime(updated, "%Y-%m-%dT%H:%M:%S%Z", &updated_tm)) {
      updated_time = timegm(&updated_tm);
    } else if (updated && NULL != strptime(updated, "%Y-%m-%dT%H:%M:%S", &updated_tm)) {
      updated_time = timegm(&updated_tm);
    } else {
      error("Couldn't parse datetime: %s", updated);
    }

    entry = create_item_cache_entry(id, updated_time, time(NULL), xml_source);
  } else {
    error("Missing id or updated from atom (%s, %s)", id, updated);
  }

  if (id) free(id);
  if (updated) free(updated);
  xmlXPathFreeContext(context);

  return entry;
}

int item_cache_entry_id(const ItemCacheEntry * entry) {
  return entry->id;
}

const char * item_cache_entry_full_id(const ItemCacheEntry * entry) {
  return entry->full_id;
}

const char * item_cache_entry_atom(const ItemCacheEntry * entry) {
  return entry->atom;
}

void free_entry(ItemCacheEntry *entry) {
  if (entry) {
    FREE_STRING(entry->full_id);
    FREE_STRING(entry->atom);
    free(entry);
  }
}

/******************************************************************************
 * ItemCache functions
 ******************************************************************************/

/******************************************************************************
 * Static functions local to this source file.
 ******************************************************************************/

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

  if (SQLITE_OK != sqlite3_prepare_v2( item_cache->db, FETCH_ITEM_SQL,             -1, &item_cache->fetch_item_stmt,            NULL) ||
      SQLITE_OK != sqlite3_prepare_v2( item_cache->db, FETCH_ALL_ITEMS_SQL,        -1, &item_cache->fetch_all_items_stmt,       NULL) ||
      SQLITE_OK != sqlite3_prepare_v2( item_cache->db, FETCH_RANDOM_BACKGROUND,    -1, &item_cache->random_background_stmt,     NULL) ||
      SQLITE_OK != sqlite3_prepare_v2( item_cache->db, INSERT_ENTRY_SQL,           -1, &item_cache->insert_entry_stmt,          NULL) ||
      SQLITE_OK != sqlite3_prepare_v2( item_cache->db, UPDATE_ENTRY_SQL,           -1, &item_cache->update_entry_stmt,          NULL) ||
      SQLITE_OK != sqlite3_prepare_v2( item_cache->db, DELETE_ENTRY_SQL,           -1, &item_cache->delete_entry_stmt,          NULL) ||
      SQLITE_OK != sqlite3_prepare_v2( item_cache->db, FIND_TOKEN_SQL,             -1, &item_cache->find_token_stmt,            NULL) ||
      SQLITE_OK != sqlite3_prepare_v2( item_cache->db, FIND_ATOM_SQL,              -1, &item_cache->find_atom_stmt,             NULL) ||
      SQLITE_OK != sqlite3_prepare_v2( item_cache->db, INSERT_ATOM_SQL,            -1, &item_cache->insert_atom_stmt,           NULL) ||
      SQLITE_OK != sqlite3_prepare_v2( item_cache->db, INSERT_ATOM_XML_SQL,        -1, &item_cache->insert_atom_xml_stmt,       NULL) ||
      SQLITE_OK != sqlite3_prepare_v2( item_cache->db, DELETE_ATOM_XML_SQL,        -1, &item_cache->delete_atom_xml_stmt,       NULL) ||
      SQLITE_OK != sqlite3_prepare_v2( item_cache->db, INSERT_ENTRY_TOKENS,        -1, &item_cache->insert_tokens_stmt,         NULL) ||
      SQLITE_OK != sqlite3_prepare_v2( item_cache->db, FETCH_ENTRY_TOKENS,        -1, &item_cache->fetch_tokens_stmt,         NULL) ||
      SQLITE_OK != sqlite3_prepare_v2( item_cache->db, DELETE_ENTRY_TOKENS,        -1, &item_cache->delete_tokens_stmt,         NULL) ||
      SQLITE_OK != sqlite3_prepare_v2( item_cache->db, TOUCH_ITEM_SQL,						 -1, &item_cache->touch_item_stmt,            NULL)) {
    fatal("Unable to prepare statment: \"%s\"", item_cache_errmsg(item_cache));
    rc = CLASSIFIER_FAIL;
  }

  return rc;
}

static int check_user_version(ItemCache * item_cache) {
  int rc = CLASSIFIER_OK;
  if (CLASSIFIER_OK == get_user_version(item_cache)) {
    if (item_cache->user_version != CURRENT_USER_VERSION) {
      item_cache->version_mismatch = 1;
      rc = CLASSIFIER_FAIL;
    }
  } else {
    rc = CLASSIFIER_FAIL;
  }

  return rc;
}

static int attach_database(sqlite3 *db, const char * path, const char * alias) {
  int rc = CLASSIFIER_OK;
  char sql[MAXPATHLEN];

  if (MAXPATHLEN < snprintf(sql, MAXPATHLEN, "ATTACH DATABASE '%s' as %s", path, alias)) {
    fatal("Path name too long: %s", path);
    exit(1);
  }

  if (SQLITE_OK != sqlite3_exec(db, sql, NULL, NULL, NULL)) {
    rc = CLASSIFIER_FAIL;
  }

  return rc;
}

static int item_cache_open_database(ItemCache *item_cache) {
  int rc = CLASSIFIER_OK;
  char path[MAXPATHLEN];
  char atom_path[MAXPATHLEN];
  char token_path[MAXPATHLEN];

  if (MAXPATHLEN < snprintf(path, MAXPATHLEN, "%s/catalog.db", item_cache->cache_directory)) {
    fatal("Path to catalog.db too long: %s", item_cache->cache_directory);
    exit(1);
  }

  if (MAXPATHLEN < snprintf(atom_path, MAXPATHLEN, "%s/atom.db", item_cache->cache_directory)) {
    fatal("Path to atom.db too long: %s", item_cache->cache_directory);
    exit(1);
  }

  if (MAXPATHLEN < snprintf(token_path, MAXPATHLEN, "%s/tokens.db", item_cache->cache_directory)) {
    fatal("Path to tokens.db too long: %s", item_cache->cache_directory);
    exit(1);
  }

  info("Attempting to open item cache catalog file at %s", path);
  if (SQLITE_OK != sqlite3_open_v2(path, &item_cache->db, SQLITE_OPEN_READWRITE, NULL)) {
      rc = CLASSIFIER_FAIL;
  } else {
    if (CLASSIFIER_OK == (rc = check_user_version(item_cache)) &&
        CLASSIFIER_OK == (rc = attach_database(item_cache->db, atom_path, "atom")) &&
        CLASSIFIER_OK == (rc = attach_database(item_cache->db, token_path, "token"))) {

      rc = create_prepared_statements(item_cache);
      sqlite3_busy_timeout(item_cache->db, 1000);
    }
  }

  return rc;
}

static int serialize_tokens(Item * item, int *size, char ** token_data) {
  int rc = CLASSIFIER_OK;
  int num_tokens = item_get_num_tokens(item);
  *size = num_tokens * 6;

  if (NULL == (*token_data = calloc(num_tokens, 6))) {
    fatal("Could not allocate data for token array");
    rc = CLASSIFIER_FAIL;
  } else {
    int token = 0;
    short frequency = 0;
    char * position = token_data[0];

    while (item_next_token(item, &token, &frequency)) {
      /* Always write the tokens out in network byte order. */
      int out_token = htonl(token);
      short out_frequency = htons(frequency);

      // Make sure we haven't overflowed
      if (position - token_data[0] > *size) {
        error("Error allocating enough memory for tokens.");
        free(*token_data);
        rc = CLASSIFIER_FAIL;
        break;
      }

      memcpy(position, &out_token, 4);     position += 4;
      memcpy(position, &out_frequency, 2); position += 2;
    }
  }

  return rc;
}

static int save_tokens(ItemCache *item_cache, int entry_id, const char * token_data, int size) {
  int rc = CLASSIFIER_OK;

  if (SQLITE_OK != sqlite3_bind_int(item_cache->insert_tokens_stmt, 1, entry_id)) {
    error("Error binding entry id to %s", INSERT_ENTRY_TOKENS);
    rc = CLASSIFIER_FAIL;
  } else if (SQLITE_OK != sqlite3_bind_blob(item_cache->insert_tokens_stmt, 2, token_data, size, SQLITE_TRANSIENT)) {
    error("Error binding blob to %s", INSERT_ENTRY_TOKENS);
    rc = CLASSIFIER_FAIL;
  } else if (SQLITE_DONE != sqlite3_step(item_cache->insert_tokens_stmt)) {
    error("Error inserting token data for item %i: %s", entry_id, item_cache_errmsg(item_cache));
    rc = CLASSIFIER_FAIL;
  }

  sqlite3_clear_bindings(item_cache->insert_tokens_stmt);
  sqlite3_reset(item_cache->insert_tokens_stmt);

  return rc;
}

/* Fetches the tokens for the given item.
 *
 * @returns the number of tokens fetched for the the item.
 */
static int read_tokens(const char * token_data, int size, Item * item) {
  int tokens_read = 0;

  if (!token_data) {
    error("No token data for item");
    tokens_read = -1;
  } else if (0 != (size % TOKEN_BYTES)) {
    error("Token data is corrupt for item $i (size = %i)", item->key, size);
    tokens_read = -1;
  } else {
    int i, num_tokens = size / TOKEN_BYTES;
    const char *token_p = token_data;

    for (i = 0; i < num_tokens; i++) {
      int token;
      short frequency;
      memcpy(&token,     token_p, 4); token_p += 4;
      memcpy(&frequency, token_p, 2); token_p += 2;
      /* Tokens are stored in network byte order so switch them to host byte order */
      item_add_token(item, ntohl(token), ntohs(frequency));
      tokens_read++;
    }
  }

  return tokens_read;
}

static int fetch_tokens_for(ItemCache * item_cache, Item * item) {
  int tokens_loaded = 0;

  if (item_cache && item) {
    if (SQLITE_OK != sqlite3_bind_int(item_cache->fetch_tokens_stmt, 1, item->key)) {
      error("Could not bind item->key to stmt: %s", item_cache_errmsg(item_cache));
      tokens_loaded = -1;
    } else if (SQLITE_ROW != sqlite3_step(item_cache->fetch_tokens_stmt)) {
      tokens_loaded = -1;
    } else {
      int blob_size = sqlite3_column_bytes(item_cache->fetch_tokens_stmt, 0);
      const char *token_data = (char*) sqlite3_column_blob(item_cache->fetch_tokens_stmt, 0);
      tokens_loaded = read_tokens(token_data, blob_size, item);
    }

    sqlite3_clear_bindings(item_cache->fetch_tokens_stmt);
    sqlite3_reset(item_cache->fetch_tokens_stmt);
  }

  return tokens_loaded;
}

/* Fetches the item metadata from the catalog database.
 *
 * Caller must hold the db_access_mutex.
 */
static Item * fetch_item_from_catalog(ItemCache * item_cache, const char * id) {
	Item *item = NULL;
	trace("thread(%p) has locked db_access_mutex to fetch %s", pthread_self(), id);

	int sqlite3_rc = sqlite3_bind_text(item_cache->fetch_item_stmt, 1, id, -1, NULL);
	if (SQLITE_OK != sqlite3_rc) {
		fatal("fetch_item_stmt bind error = %s", item_cache_errmsg(item_cache));
		item = NULL;
	} else {
		sqlite3_rc = sqlite3_step(item_cache->fetch_item_stmt);

		if (SQLITE_ROW == sqlite3_rc) {
			item = create_item(sqlite3_column_text(item_cache->fetch_item_stmt, 0),
                         sqlite3_column_int(item_cache->fetch_item_stmt, 1),
                         sqlite3_column_int64(item_cache->fetch_item_stmt, 2));
		} else if (SQLITE_DONE != sqlite3_rc) {
			error("Error fetching item %d: %s", id, item_cache_errmsg(item_cache));
		}
	}

	sqlite3_clear_bindings(item_cache->fetch_item_stmt);
	sqlite3_reset(item_cache->fetch_item_stmt);

	trace("thread(%p) has unlocked db_access_mutex after fetching %s", pthread_self(), id);

	return item;
}

static int get_entry_key(ItemCache * item_cache, const char * entry_id) {
  int entry_key = -1;

  if (item_cache && entry_id) {
    if (SQLITE_OK != sqlite3_bind_text(item_cache->fetch_item_stmt, 1, entry_id, -1, NULL)) {
      error("Unable to bind %s to fetch_item_stmt: %s", entry_id, item_cache_errmsg(item_cache));
    } else if (SQLITE_ROW != sqlite3_step(item_cache->fetch_item_stmt)) {
      error("Entry does not exist: %s", entry_id);
    } else {
      entry_key = sqlite3_column_int(item_cache->fetch_item_stmt, 1);
    }

    sqlite3_clear_bindings(item_cache->fetch_item_stmt);
    sqlite3_reset(item_cache->fetch_item_stmt);
  }

  return entry_key;
}

/* This will also update the entry's id. But side-effecty I guess */
static int _is_new_entry(ItemCache * item_cache, ItemCacheEntry * entry) {
  int is_new_entry = true;

  sqlite3_bind_text(item_cache->fetch_item_stmt, 1, entry->full_id, -1, NULL);
  if (SQLITE_ROW == sqlite3_step(item_cache->fetch_item_stmt)) {
    is_new_entry = false;
    entry->id = sqlite3_column_int(item_cache->fetch_item_stmt, 1);
  }

  sqlite3_clear_bindings(item_cache->fetch_item_stmt);
  sqlite3_reset(item_cache->fetch_item_stmt);
  return is_new_entry;
}

static int save_entry_xml(ItemCache *item_cache, ItemCacheEntry *entry) {
  int rc = CLASSIFIER_OK;

  if (!(entry->atom && entry->id > 0)) {
    error("No xml or id for entry %s (%i)", entry->full_id, entry->id);
    rc = CLASSIFIER_FAIL;
  } else {
    int size = strlen(entry->atom);
    if (SQLITE_OK != sqlite3_bind_int(item_cache->insert_atom_stmt, 1, entry->id)) {
      error("Unable to bind atom id: %s", item_cache_errmsg(item_cache));
      rc = CLASSIFIER_FAIL;
    } else if (SQLITE_OK != sqlite3_bind_blob(item_cache->insert_atom_xml_stmt, 2, entry->atom, size, SQLITE_TRANSIENT)) {
      error("Unable to bind atom xml: %s", item_cache_errmsg(item_cache));
      rc = CLASSIFIER_FAIL;
    } else if (SQLITE_DONE != sqlite3_step(item_cache->insert_atom_xml_stmt)) {
      error("Unable to insert atom xml: %s", item_cache_errmsg(item_cache));
      rc = CLASSIFIER_FAIL;
    }

    sqlite3_clear_bindings(item_cache->insert_atom_xml_stmt);
    sqlite3_reset(item_cache->insert_atom_xml_stmt);
  }

  return rc;
}

static int insert_entry(ItemCache *item_cache, ItemCacheEntry *entry) {
  int rc = CLASSIFIER_OK;

  if (SQLITE_OK != sqlite3_bind_text(item_cache->insert_entry_stmt, 1, entry->full_id, -1, NULL)) {
    error("Error binding %s to parameter %i", entry->full_id, 1);
    rc = CLASSIFIER_FAIL;
  } else {
    sqlite3_bind_double(item_cache->insert_entry_stmt, 2, entry->updated);
    sqlite3_bind_double(item_cache->insert_entry_stmt, 3, entry->created_at);

    if (SQLITE_DONE != sqlite3_step(item_cache->insert_entry_stmt)) {
      error("Error inserting item %s: %s", entry->full_id, item_cache_errmsg(item_cache));
      rc = CLASSIFIER_FAIL;
    } else {
      entry->id = sqlite3_last_insert_rowid(item_cache->db);
    }
  }

  sqlite3_clear_bindings(item_cache->insert_entry_stmt);
  sqlite3_reset(item_cache->insert_entry_stmt);
  return rc;
}

static int update_entry(ItemCache *item_cache, ItemCacheEntry *entry) {
  int rc = CLASSIFIER_OK;
  sqlite3_bind_double(item_cache->update_entry_stmt, 1, entry->updated);
  sqlite3_bind_text(item_cache->update_entry_stmt, 2, entry->full_id, -1, NULL);

  if (SQLITE_DONE != sqlite3_step(item_cache->update_entry_stmt)) {
    error("Error update item %s: %s", entry->full_id, item_cache_errmsg(item_cache));
    rc = CLASSIFIER_FAIL;
  }

  sqlite3_clear_bindings(item_cache->update_entry_stmt);
  sqlite3_reset(item_cache->update_entry_stmt);
  return rc;
}

static int entry_has_tokens(ItemCache *item_cache, ItemCacheEntry *entry) {
  int has_tokens = false;

  if (SQLITE_OK != sqlite3_bind_int(item_cache->fetch_tokens_stmt, 1, entry->id)) {
    fatal("Error bind int: %s", item_cache_errmsg(item_cache));
  } else {
    has_tokens = SQLITE_ROW == sqlite3_step(item_cache->fetch_tokens_stmt);
    sqlite3_clear_bindings(item_cache->fetch_tokens_stmt);
    sqlite3_reset(item_cache->fetch_tokens_stmt);
  }

  return has_tokens;
}

static time_t get_purge_time(int days_to_keep) {
  time_t now = time(NULL);
  struct tm purge_time_tm;
  gmtime_r(&now, &purge_time_tm);
  purge_time_tm.tm_mon -= (days_to_keep / 30);
  purge_time_tm.tm_mday -= (days_to_keep % 30);
  return timegm(&purge_time_tm);
}

static void * item_cache_purge_thread_func(void *memo) {
  ItemCache *item_cache = (ItemCache *) memo;

  while (!item_cache->shutting_down) {
    sleep(item_cache->purge_interval);
    item_cache_purge_old_items(item_cache);
  }

  return NULL;
}

/** The cache updating thread.
 *
 * This is one of the hairier functions in the Item cache.  It is responsible
 * for saving new tokenized items in the database and adding new tokenized items
 * into the in-memory cache that is used for classification.
 *
 * UpdateJob are first placed into the update_queue by the feature extraction thread.
 * The updating thread then gets the jobs of the queue. The jobs contain an item that
 * is to be saved into the database and, if that is successful, it is saved into the
 * in-memory cache.  We use a write lock on the in-memory cache to ensure that
 * nothing is reading or writing to the in-memory cache at this time.  The lock is around
 * the smallest possible section, the item_cache_add_item function, to ensure we hold
 * the lock for as short a time as possible.
 *
 * One particularly trick aspect to this is that new classification jobs are triggered
 * by the addition of new items to the cache. This is handled by the calling of the
 * update_callback when new items are added.  However item can come in spurts and we
 * don't want to be creating a new classification job for every item if we are getting
 * say 100-200 items at a rate of one every second or so, this will flood the classifier
 * with thousands of small jobs that each classify a single item and then end.
 *
 * So what we want to try and do here is to add as many items as we can before calling
 * the callback, but we also want to make sure the callback is called fairly regularly
 * and you do don't have a small number of items waiting a long time to be classified.
 * To do this we keep track of the number of items processed in a "run", when that reaches
 * PROCESSING_LIMIT (default: 200) the callback is called and we continue processing new
 * items.  Before that limit is reached we will try an get another item off the queue,
 * if there are no items we wait 1 second, if there are still no items we then call the callback.
 *
 * So we have a couple of parameters that might need tuning:
 *
 *   PROCESSING_LIMIT: The number of items that will be processed before the update_callback is triggered.
 *   QUEUE_WAIT: The time we wait for a new item in the queue before the update_callback is triggered.
 */
static void * cache_updating_func(void *memo) {
  ItemCache *item_cache = (ItemCache*) memo;

  while (!item_cache->shutting_down) {
    UpdateJob *job = q_dequeue_or_wait(item_cache->update_queue, item_cache->cache_update_wait_time);
    int items_added = 0;

    if (job) {
      do {

        switch (job->type) {
          case ADD:
            job->item->time = time(NULL);
						if (CLASSIFIER_OK == item_cache_add_item(item_cache, job->item)) {
							items_added++;
						} else {
							debug("No enough tokens to add to the classification item cache: %s\n", job->item->id);
							free_item(job->item);
						}
						break;
					default:
						fatal("Got unknown cache updating job type");
						break;
				}

        free(job);

        /* When we have added 200 items to the queue
         * skip to calling the callback. Otherwise
         * try an get another job, possibly waiting
         * one second for it to arrive.
         */
        if (items_added >= PROCESSING_LIMIT) {
          debug("Hit PROCESSING_LIMIT(%i)", PROCESSING_LIMIT);
          break;
        } else {
          job = q_dequeue_or_wait(item_cache->update_queue, item_cache->cache_update_wait_time);
        }

        /* We keep doing this until we don't get any more jobs after a 1 second wait. */
        /* Is one second long enough? This might need to be a parameter. */
      } while (job != NULL);

      if (item_cache->update_callback && items_added > 0) {
        debug("Trigger update callback for %i items", items_added);
        item_cache->update_callback(item_cache, item_cache->update_callback_memo);
      }
    }
  }

  return NULL;
}

/* Insert an item in the items by id cache.
 *
 * Caller must hold a write lock on the cache.
 */
static int items_by_id_insert(ItemCache * item_cache, Item * item) {
  int rc = CLASSIFIER_OK;

  PWord_t item_pointer;
  JSLI(item_pointer, item_cache->items_by_id, item->id);
  if (NULL != item_pointer) {
    *item_pointer = (Word_t) item;
    item_cache->cached_size++;
  } else {
    fatal("Error malloc'ing item by id");
    rc = CLASSIFIER_FAIL;
  }

  return rc;
}

static Item * items_by_id_get(ItemCache * item_cache, const unsigned char * id) {
	Item *item = NULL;

	PWord_t item_pointer;
	JSLG(item_pointer, item_cache->items_by_id, id);
	if (NULL != item_pointer) {
		item = (Item*)(*item_pointer);
	}

	return item;
}

static int items_by_id_remove(ItemCache * item_cache, Item * item) {
  int judyrc;

  JSLD(judyrc, item_cache->items_by_id, item->id);
  if (judyrc) {
    item_cache->cached_size--;
  }

  return judyrc == 1 ? CLASSIFIER_OK : CLASSIFIER_FAIL;
}


static OrderedItemList * ordered_item_list_insert_after(OrderedItemList * insert_after, Item * item) {
  OrderedItemList * new = malloc(sizeof(OrderedItemList));
  if (!new) {
    fatal("Could not malloc OrderedItemList");
  } else {
    new->item = item;
    new->next = NULL;

    if (insert_after) {
      new->next = insert_after->next;
      insert_after->next = new;
    }
  }

  return new;
}

/* Inserts item in list and returns the head of the list.
 *
 * Order is descending item time.
 */
static OrderedItemList * ordered_item_list_insert_in_order(OrderedItemList * list, Item * item) {
  OrderedItemList *head = list;
  OrderedItemList *new_node = calloc(1, sizeof(struct ORDERED_ITEM_LIST));

  if (new_node) {
    new_node->item = item;
    new_node->next = NULL;

    if (head == NULL) {
      head = new_node;
    } else if (head->item->time < new_node->item->time) {
      new_node->next = head;
      head = new_node;
    } else {
      OrderedItemList *current = head;

      while (current->next != NULL) {
        if (current->next->item->time < item->time) {
          new_node->next = current->next;
          current->next = new_node;
          break;
        } else {
          current = current->next;
        }
      }

      if (current->next == NULL) {
        current->next = new_node;
      }
    }
  }

  return head;
}

static OrderedItemList * ordered_item_list_split(OrderedItemList * list, time_t split_time) {
  OrderedItemList *leftovers = NULL;
  OrderedItemList *current = list;
  OrderedItemList *previous = NULL;

  /* Find the point in the list where items are older than purge_time and remove them. */
  for ( ; current != NULL; previous = current, current = current->next) {
    if (current->item->time < split_time) {
      if (previous) {
        previous->next = NULL;
      }

      leftovers = current;
      break;
    }
  }

  return leftovers;
}

/* Loads all the items into the cache.
 *
 * Caller must hold the db_access mutex and a write lock on the cache.
 */
static int load_all_items(ItemCache * item_cache) {
  int rc = CLASSIFIER_OK;
  OrderedItemList * last = item_cache->items_in_order;

  sqlite3_bind_int(item_cache->fetch_all_items_stmt, 1, item_cache->load_items_since);

  /* Load the item ids and timestamp indexing and order them by each. */
  while (SQLITE_ROW == sqlite3_step(item_cache->fetch_all_items_stmt)) {
    const unsigned char * id = sqlite3_column_text(item_cache->fetch_all_items_stmt, 0);
    int key = sqlite3_column_int(item_cache->fetch_all_items_stmt, 1);
    time_t item_time = sqlite3_column_int64(item_cache->fetch_all_items_stmt, 2);

    Item *item = create_item(id, key, item_time);
    if (NULL == item) {
      rc = CLASSIFIER_FAIL;
      break;
    }

    if (item_cache->min_tokens > fetch_tokens_for(item_cache, item)) {
      free_item(item);
      continue;
    }

    if (items_by_id_insert(item_cache, item)) {
      rc = CLASSIFIER_FAIL;
      free_item(item);
      break;
    }

    if (NULL == (last = ordered_item_list_insert_after(last, item))) {
      free_item(item);
      break;
    }

    if (!item_cache->items_in_order) {
      item_cache->items_in_order = last;
    }
  }

  sqlite3_clear_bindings(item_cache->fetch_all_items_stmt);
  sqlite3_reset(item_cache->fetch_all_items_stmt);

  return rc;
}

static int load_random_background(ItemCache * item_cache) {
  int rndbg_item_count = 0;
  item_cache->random_background = new_pool();

  while (SQLITE_ROW == sqlite3_step(item_cache->random_background_stmt)) {
    const unsigned char * id = sqlite3_column_text(item_cache->random_background_stmt, 0);
    int key = sqlite3_column_int(item_cache->random_background_stmt, 1);
    Item *item = create_item(id, key, -1);

    if (item) {
      if (fetch_tokens_for(item_cache, item)) {
        pool_add_item(item_cache->random_background, item);
      }

      free_item(item);
      rndbg_item_count++;
    } else {
      error("Could not create item: %s (%i)", id, key);
    }
  }
  sqlite3_reset(item_cache->random_background_stmt);
  info("Randombackground contains %i items", rndbg_item_count);

  return CLASSIFIER_OK;
}

/*****************************************************************************
 * External API functions for the item cache.
 *****************************************************************************/

/** Initialize an SQLite database with the ItemCache schema.
 *
 * @param db_file File to initialize.
 * @param error Any error message is stored here.
 */
int item_cache_initialize(const char *db_file, char * error) {
  int rc = CLASSIFIER_OK;
  sqlite3 *db;
  FILE *file;

  if (NULL != (file = fopen(db_file, "r"))) {
    snprintf(error, 512, "Database file %s already exists, no action taken.", db_file);
    fclose(file);
    rc = CLASSIFIER_FAIL;
  } else if (SQLITE_OK != sqlite3_open_v2(db_file, &db, SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE, NULL)) {
    snprintf(error, 512, "Error creating database file %s: %s.", db_file, sqlite3_errmsg(db));
    sqlite3_close(db);
    rc = CLASSIFIER_FAIL;
  } else {
    FILE *schema_file;
    char *schema;
    char schema_file_name[512];
    snprintf(schema_file_name, 512, "%s/%s/%s", DATADIR, PACKAGE,"initial_schema.sql");

    if (NULL != (schema_file = fopen(schema_file_name, "r"))) {
      fseek(schema_file, 0, SEEK_END);
      int size = ftell(schema_file);
      schema = malloc(size + 1);
      fseek(schema_file, 0, SEEK_SET);

      if (size != fread(schema, sizeof(char), size, schema_file)) {
        rc = CLASSIFIER_FAIL;
        snprintf(error, 512, "Error reading schema file %s: %s", schema_file_name, strerror(errno));
      } else {
        schema[size] = 0;
        if (SQLITE_OK != sqlite3_exec(db, schema, NULL, NULL, NULL)) {
          snprintf(error, 512, "Error loading schema into database file: %s", sqlite3_errmsg(db));
          rc = CLASSIFIER_FAIL;
        }
      }

      free(schema);
      fclose(schema_file);
    } else {
      snprintf(error, 512, "Could not find the database schema file at %s. Perhaps it is missing from the install.", schema_file_name);
      rc = CLASSIFIER_FAIL;
    }

    sqlite3_close(db);
  }

  return rc;
}

/** Create an SQLite ItemCache.
 *
 * @param item_cache A pointer to an pointer to an SQLiteItemSource. This
 *           will be initialized as an SQLiteItemSource.
 * @param cache_directory The directory containing the item cache.
 * @returns CLASSIFIER_OK if all went well, CLASSIFIER_FAIL if something went wrong.
 *          Providing that the function could allocate memory, item_cache will be an
 *          allocated item source in either case.  If the return value was
 *          CLASSIFIER_FAIL you can pass item_cache back to sqlite_item_source_errmsg
 *          to get a detailed error message.
 */
int item_cache_create(ItemCache **item_cache, const char * cache_directory, const ItemCacheOptions * options) {
  int rc = CLASSIFIER_OK;

  *item_cache = calloc(1, sizeof(struct ITEM_CACHE));

  (*item_cache)->cache_directory = strdup(cache_directory);
  (*item_cache)->cache_update_wait_time = options->cache_update_wait_time;
  (*item_cache)->load_items_since = options->load_items_since;
  (*item_cache)->min_tokens = options->min_tokens;
  (*item_cache)->version_mismatch = 0;
  (*item_cache)->items_by_id = NULL;
  (*item_cache)->items_in_order = NULL;
  (*item_cache)->random_background = NULL;
  (*item_cache)->loaded = false;
  (*item_cache)->update_queue = new_queue();
  (*item_cache)->shutting_down = 0;

  if (pthread_mutex_init(&(*item_cache)->db_access_mutex, NULL)) {
    fatal("pthread_mutex_init error for db_access_mutex");
    free(*item_cache);
    *item_cache = NULL;
    rc = CLASSIFIER_FAIL;
  }

  if (pthread_rwlock_init(&(*item_cache)->cache_lock, NULL)) {
    fatal("Could not allocate item cache");
    rc = CLASSIFIER_FAIL;
    free(*item_cache);
    *item_cache = NULL;
  }

  if (*item_cache == NULL) {
    fatal("Unable to allocate memory for Item Cache");
    rc = CLASSIFIER_FAIL;
  } else {
    rc = item_cache_open_database(*item_cache);
  }

  return rc;
}

/** Free the memory and database connection associated with the item cache.
 */
void free_item_cache(ItemCache *item_cache) {

  if (item_cache) {
    item_cache->shutting_down = 1;

    if (item_cache->cache_updating_thread) {
      info("Stopping cache updater");
      pthread_detach(*item_cache->cache_updating_thread);
      pthread_cancel(*item_cache->cache_updating_thread);
      free(item_cache->cache_updating_thread);
    }

    if (item_cache->purge_thread) {
      info("Stopping cache purger");
      pthread_detach(*item_cache->purge_thread);
      pthread_cancel(*item_cache->purge_thread);
      free(item_cache->purge_thread);
    }

    if (item_cache->db) {
      sqlite3_finalize(item_cache->fetch_item_stmt);
      sqlite3_finalize(item_cache->fetch_all_items_stmt);
      sqlite3_finalize(item_cache->random_background_stmt);
      sqlite3_finalize(item_cache->insert_entry_stmt);
      sqlite3_finalize(item_cache->update_entry_stmt);
      sqlite3_finalize(item_cache->delete_entry_stmt);
      sqlite3_finalize(item_cache->find_token_stmt);
      sqlite3_finalize(item_cache->find_atom_stmt);
      sqlite3_finalize(item_cache->insert_atom_stmt);
      sqlite3_finalize(item_cache->insert_atom_xml_stmt);
      sqlite3_finalize(item_cache->delete_atom_xml_stmt);
      sqlite3_finalize(item_cache->delete_tokens_stmt);
      sqlite3_finalize(item_cache->insert_tokens_stmt);
      sqlite3_finalize(item_cache->touch_item_stmt);
      sqlite3_finalize(item_cache->fetch_tokens_stmt);
      sqlite3_close(item_cache->db);
    }

    if (item_cache->items_in_order) {
      int freed_bytes;
      uint8_t index[256];
      index[0] = '\0';
      PWord_t item_pointer;

      JSLF(item_pointer, item_cache->items_by_id, index);
      while (NULL != item_pointer) {
        free_item((Item*) (*item_pointer));
        JSLN(item_pointer, item_cache->items_by_id, index);
      }
      JSLFA(freed_bytes, item_cache->items_by_id);

      OrderedItemList *current = item_cache->items_in_order;
      while (current) {
        OrderedItemList *to_free = current;
        current = current->next;
        free(to_free);
      }

      if (item_cache->random_background) {
        free_pool(item_cache->random_background);
      }
    }

    pthread_mutex_destroy(&item_cache->db_access_mutex);
    pthread_rwlock_destroy(&item_cache->cache_lock);
    free_queue(item_cache->update_queue);

    free(item_cache->cache_directory);
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
      msg = "Database file's user version does not match classifier version."
          " Trying running classifier-db-migrate from the classifier-tools package.";
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
 */
int item_cache_load(ItemCache *item_cache) {
  if (!item_cache) {
    fatal("item_cache_load got NULL for item_cache");
    return CLASSIFIER_FAIL;
  }

  info("item_cache_load from %i days ago", item_cache->load_items_since);
  time_t start_time = time(NULL);

  pthread_rwlock_wrlock(&item_cache->cache_lock);
  pthread_mutex_lock(&item_cache->db_access_mutex);

  int rc = load_all_items(item_cache);

  if (CLASSIFIER_OK == rc) {
    rc = load_random_background(item_cache);
  }

  item_cache->loaded = true;
  pthread_mutex_unlock(&item_cache->db_access_mutex);
  pthread_rwlock_unlock(&item_cache->cache_lock);
  time_t end_time = time(NULL);

  info("loaded %i items in %i seconds", item_cache_cached_size(item_cache), end_time - start_time);
  return rc;
}

/** Returns the number of items in the in-memory cache.
 */
int item_cache_cached_size(ItemCache *item_cache) {
  return item_cache->cached_size;
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

void touch_item(ItemCache *item_cache, const unsigned char * id) {
	if (item_cache && id) {
		pthread_mutex_lock(&item_cache->db_access_mutex);
		sqlite3_bind_text(item_cache->touch_item_stmt, 1, id, -1, NULL);
		sqlite3_step(item_cache->touch_item_stmt);
		sqlite3_reset(item_cache->touch_item_stmt);
		pthread_mutex_unlock(&item_cache->db_access_mutex);
	}
}

/** Fetch an item from the cache.
 *
 * @param item_cache The ItemCache to get the item from.
 * @param id The id of the item to get.
 * @returns The Item with the id matching id. It is the responsibility of the
 *          caller to free the item.
 * TODO Handle SQLITE_BUSY in case another process locks the database.
 */
Item * item_cache_fetch_item(ItemCache *item_cache, const unsigned char * id, int * free_when_done) {
  debug("fetching item %s", id);
  Item *item = NULL;
  *free_when_done = false;

  if (!item_cache) {
    fatal("Got NULL item_cache in item_cache_fetch_item");
    return item;
  }

  pthread_rwlock_rdlock(&item_cache->cache_lock);
  item = items_by_id_get(item_cache, id);
  pthread_rwlock_unlock(&item_cache->cache_lock);

  if (NULL == item) {
    *free_when_done = true;
    pthread_mutex_lock(&item_cache->db_access_mutex);
    item = fetch_item_from_catalog(item_cache, (char *) id);

    if (item && fetch_tokens_for(item_cache, item) <= 0) {
      // TODO No tokens for the item, should probably add it to the tokenizer queue
    	free_item(item);
    	item = NULL;
    }

    pthread_mutex_unlock(&item_cache->db_access_mutex);
  }

  touch_item(item_cache, id);

  return item;
}

/** Iterates over each item.
 *
 */
int item_cache_each_item(ItemCache *item_cache, ItemIterator iterator, void *memo) {
  if (item_cache->loaded) {
    pthread_rwlock_rdlock(&item_cache->cache_lock);
    OrderedItemList *current = item_cache->items_in_order;

    while (current) {
      Item *item = current->item;
      if (CLASSIFIER_OK != iterator(item, memo)) {
        break;
      }
      current = current->next;
    }

    pthread_rwlock_unlock(&item_cache->cache_lock);
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
const Pool * item_cache_random_background(ItemCache * item_cache)  {
  const Pool * random_background = NULL;
  if (item_cache) {
    if (!item_cache->random_background) {
      item_cache->random_background = new_pool();
    }

    random_background = item_cache->random_background;
  }

  return random_background;
}

#include <sys/time.h>
#include <time.h>

static float tdiff(struct timeval from, struct timeval to) {
  double from_d = from.tv_sec + (from.tv_usec / 1000000.0);
  double to_d = to.tv_sec + (to.tv_usec / 1000000.0);
  return to_d - from_d;
}

/** Adds an entry to the item cache.
 *
 * This immediately stores the item in the database.
 *
 * TODO Add SQLITE_BUSY handling for add_entry
 */
// :id, :full_id, :title, :author, :alternate, :self, :spider, :content, :updated, :feed_id, :created_at
int item_cache_add_entry(ItemCache *item_cache, ItemCacheEntry *entry) {
  int rc = CLASSIFIER_OK;
  struct timeval start;
  gettimeofday(&start, NULL);
  if (item_cache && entry) {
	pthread_mutex_lock(&item_cache->db_access_mutex);
	int is_new_entry = _is_new_entry(item_cache, entry);

	if (is_new_entry) {
	  insert_entry(item_cache, entry);
	} else {
	  update_entry(item_cache, entry);
	}

	if (save_entry_xml(item_cache, entry)) {
	  rc = CLASSIFIER_FAIL;
	}

	pthread_mutex_unlock(&item_cache->db_access_mutex);

	struct timeval inserted;
	gettimeofday(&inserted, NULL);
	debug("insertion: %.7fs", tdiff(start, inserted));

	// We don't want to extract features for items we already have.
	// TODO Handle updates to features for items somehow?

	if (rc == CLASSIFIER_OK && (is_new_entry || !entry_has_tokens(item_cache, entry))) {
		debug("tokenizing entry %s", entry->full_id);
		if (entry->atom) {
			Pvoid_t features = atom_tokenize(entry->atom);
			if (features) {
				Item *item = create_item(entry->full_id, entry->id, entry->updated);
				item->tokens = NULL;

				struct timeval tokenized;
				gettimeofday(&tokenized, NULL);
				debug("tokenized %.7fs", tdiff(inserted, tokenized));

				PWord_t PValue;
				uint8_t token[512];
				token[0] = '\0';

				JSLF(PValue, features, token);
				while (PValue != NULL) {
					int atomizedId = item_cache_atomize(item_cache, token);
					item_add_token(item, atomizedId, *PValue);
					JSLN(PValue, features, token);
				}

				struct timeval atomized;
				gettimeofday(&atomized, NULL);
				debug("atomized %.7fs", tdiff(tokenized, atomized));
				     
				Word_t rc;
				JSLFA(rc, features);

				if (CLASSIFIER_OK == item_cache_save_item(item_cache, item)) {
					UpdateJob *job = create_add_job(item);
					q_enqueue(item_cache->update_queue, job);
					debug("Added to update queue");
				}

				struct timeval complete;
				gettimeofday(&complete, NULL);
				debug("complete %.7fs", tdiff(atomized, complete));
			}
		}
	}

  }

  return rc;
}

/** Removes an entry from the item cache.
 *
 * TODO Add SQLITE_BUSY handling to remove_entry.
 * TODO Queue up job to remove entry from in-memory queues.
 */
int item_cache_remove_entry(ItemCache *item_cache, int entry_id) {
  int rc = CLASSIFIER_OK;

  if (item_cache) {
    pthread_mutex_lock(&item_cache->db_access_mutex);
    int sqlite3_rc;
    sqlite3_bind_int(item_cache->delete_entry_stmt, 1, entry_id);
    sqlite3_rc = sqlite3_step(item_cache->delete_entry_stmt);
    sqlite3_clear_bindings(item_cache->delete_entry_stmt);
    sqlite3_reset(item_cache->delete_entry_stmt);

    if (SQLITE_DONE != sqlite3_rc) {
      if (SQLITE_CONSTRAINT == sqlite3_rc) {
        info("Constraint violated");
        rc = ITEM_CACHE_ENTRY_PROTECTED;
      } else {
        error("Error deleting ItemCache entry %i: %s", entry_id, item_cache_errmsg(item_cache));
        rc = CLASSIFIER_FAIL;
      }
    } else {
      sqlite3_bind_int(item_cache->delete_atom_xml_stmt, 1, entry_id);
      sqlite3_step(item_cache->delete_atom_xml_stmt);
      sqlite3_reset(item_cache->delete_atom_xml_stmt);
      sqlite3_bind_int(item_cache->delete_tokens_stmt, 1, entry_id);
      sqlite3_step(item_cache->delete_tokens_stmt);
      sqlite3_reset(item_cache->delete_tokens_stmt);
      info("Deleted ItemCache entry %i", entry_id);
    }

    pthread_mutex_unlock(&item_cache->db_access_mutex);
  }

  return rc;
}

/** Adds an item to the in memory item cache.
 *
 * This doesn't touch the database at all.
 *
 * NO ONE SHOULD CALL THIS: it is available only for testing!!
 *
 */
int item_cache_add_item(ItemCache *item_cache, Item *item) {
  int rc = CLASSIFIER_OK;

  if (item_cache && item) {
    if (item_get_num_tokens(item) < item_cache->min_tokens) {
      rc = CLASSIFIER_FAIL;
    } else {
      pthread_rwlock_wrlock(&item_cache->cache_lock);

      if (CLASSIFIER_OK == items_by_id_insert(item_cache, item)) {
        item_cache->items_in_order = ordered_item_list_insert_in_order(item_cache->items_in_order, item);
      } else {
        fatal("Malloc error inserting into items_by_id");
        rc = CLASSIFIER_FAIL;
      }

      pthread_rwlock_unlock(&item_cache->cache_lock);
    }
  }

  return rc;
}

/** Saves the item in the database.
 *
 * This save the tokenized representation of an entry in the database.
 * The item must have an entry in the DB or it will not be saved.
 *
 * @param item_cache The item cache to save the item in.
 * @param item The tokenized item to save in the cache's database.
 */
int item_cache_save_item(ItemCache * item_cache, Item *item) {
  int rc = CLASSIFIER_OK;

  if (item_cache && item) {
    debug("Saving item %s", item->id);
    pthread_mutex_lock(&item_cache->db_access_mutex);
    int entry_key = get_entry_key(item_cache, (char*) item->id);

    if (entry_key <= 0) {
      rc = CLASSIFIER_FAIL;
    } else {
      int size;
      char *token_data;
      if (CLASSIFIER_OK == (rc = serialize_tokens(item, &size, &token_data))) {
        rc = save_tokens(item_cache, entry_key, token_data, size);
        free(token_data);
      }
    }

    pthread_mutex_unlock(&item_cache->db_access_mutex);
  }

  return rc;
}

int item_cache_purge_old_items(ItemCache *item_cache) {
  info("Starting purge_old_items");
  int rc = CLASSIFIER_OK;

  if (item_cache) {
    int number_purged = 0;
    pthread_rwlock_wrlock(&item_cache->cache_lock);

    OrderedItemList *purge_list = ordered_item_list_split(item_cache->items_in_order, get_purge_time(item_cache->load_items_since));

    if (purge_list == item_cache->items_in_order) {
      item_cache->items_in_order = NULL;
    }

    while (purge_list) {
      OrderedItemList *next = purge_list->next;
      items_by_id_remove(item_cache, purge_list->item);
      free_item(purge_list->item);
      free(purge_list);
      purge_list = next;
      number_purged++;
    }

    pthread_rwlock_unlock(&item_cache->cache_lock);
    info("Purged %i items", number_purged);
  }

  return rc;
}

int item_cache_start_purger(ItemCache * item_cache, int purge_interval) {
  int rc = CLASSIFIER_OK;

  if (item_cache) {
    item_cache->purge_interval = purge_interval;
    item_cache->purge_thread = malloc(sizeof(pthread_t));
    if (item_cache->purge_thread == NULL) {
      fatal("Could not malloc purge_thread");
      rc = CLASSIFIER_FAIL;
    } else {
      if (pthread_create(item_cache->purge_thread,NULL, item_cache_purge_thread_func, item_cache)) {
        fatal("Could not start purge thread");
        rc = CLASSIFIER_FAIL;
      }
    }
  }

  return rc;
}

/** Gets the number of updates to in-memory cache waiting in the update queue. */
int item_cache_update_queue_size(const ItemCache * item_cache) {
  int size = -1;
  if (item_cache) {
    size = q_size(item_cache->update_queue);
  }
  return size;
}

int item_cache_start_cache_updater(ItemCache * item_cache) {
  int rc = CLASSIFIER_OK;

  if (item_cache) {
    if (item_cache->cache_updating_thread) {
      fatal("Tried to start the updating thread twice");
      rc = CLASSIFIER_FAIL;
    } else {
      item_cache->cache_updating_thread = malloc(sizeof(pthread_t));
      if (pthread_create(item_cache->cache_updating_thread, NULL, cache_updating_func, item_cache)) {
        fatal("Error creating item cache updating thread");
        rc = CLASSIFIER_FAIL;
      }
    }
  }

  return rc;
}

int item_cache_set_update_callback(ItemCache *item_cache, UpdateCallback callback, void *memo) {
  if (item_cache) {
    item_cache->update_callback = callback;
    item_cache->update_callback_memo = memo;
  }
  return CLASSIFIER_OK;
}

/******************************************************************************
 * Atomization functions.
 ******************************************************************************/

/** Converts a string token into it's atomized form.
 *
 *  If no atom for the string exists this will create one.
 *
 *  @return The integer atom for the token or -1 if it failed.
 */
int item_cache_atomize(ItemCache * item_cache, const char * s) {
  int atom = -1;

  if (item_cache && s) {
    pthread_mutex_lock(&item_cache->db_access_mutex);

    if (SQLITE_OK != sqlite3_bind_text(item_cache->find_atom_stmt, 1, s, -1, NULL)) {
      error("Error binding %s to parameter 1", s);
    } else {

      if (SQLITE_ROW == sqlite3_step(item_cache->find_atom_stmt)) {
        atom = sqlite3_column_int(item_cache->find_atom_stmt, 0);
      } else {
        if (SQLITE_OK != sqlite3_bind_text(item_cache->insert_atom_stmt, 1, s, -1, NULL)) {
          error("Error bind %s to parameter 1", s);
        } else if (SQLITE_DONE != sqlite3_step(item_cache->insert_atom_stmt)) {
          error("Error executing atom insertion: %s", item_cache_errmsg(item_cache));
        } else {
          atom = sqlite3_last_insert_rowid(item_cache->db);
        }

        sqlite3_clear_bindings(item_cache->insert_atom_stmt);
        sqlite3_reset(item_cache->insert_atom_stmt);
      }

      sqlite3_clear_bindings(item_cache->find_atom_stmt);
      sqlite3_reset(item_cache->find_atom_stmt);
    }


    pthread_mutex_unlock(&item_cache->db_access_mutex);
  }

  return atom;
}

char * item_cache_globalize(ItemCache * item_cache, int atom) {
  char * s = NULL;

  if (item_cache) {
    pthread_mutex_lock(&item_cache->db_access_mutex);

    sqlite3_bind_int(item_cache->find_token_stmt, 1, atom);

    if (SQLITE_ROW == sqlite3_step(item_cache->find_token_stmt)) {
      const char *token = (char*) sqlite3_column_text(item_cache->find_token_stmt, 0);
      if (token) {
        s = strdup(token);
      }
    }

    sqlite3_clear_bindings(item_cache->find_token_stmt);
    sqlite3_reset(item_cache->find_token_stmt);
    pthread_mutex_unlock(&item_cache->db_access_mutex);
  }
  return s;
}

/******************************************************************************
 * Static item functions
 ******************************************************************************/

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

/******************************************************************************
 * Item creation functions
 ******************************************************************************/

/** Create a empty item.
 *
 */
Item * create_item(const unsigned char *id, int key, time_t item_time) {
  Item *item = NULL;

  if (!id) {
    fatal("Missing id for create_item");
  } else if (NULL != (item = malloc(sizeof(Item)))) {
    item->id = (unsigned char*) strdup((char*) id);
    item->total_tokens = 0;
    item->time = item_time;
    item->key = key;
    item->tokens = NULL;
  } else {
    fatal("Malloc Error allocating item %d", id);
  }

  return item;
}

/** Create an item from an array of tokens.
 *
 *  @param id The id of the item.
 *  @param tokens A 2D array of tokens where each row is {token_id, frequency}.
 *  @param num_tokens The length of the token array.
 *  @returns A new item initialized with the tokens.
 */
Item * create_item_with_tokens(const unsigned char *id, int tokens[][2], int num_tokens) {
  Item *item = create_item(id, -1, -1);
  if (NULL != item) {
    if (load_tokens_from_array(item, tokens, num_tokens)) {
      free_item(item);
      item = NULL;
    }
  }

  return item;
}

/** Create an item from an array of tokens.
 *
 *  @param id The id of the item.
 *  @param tokens A 2D array of tokens where each row is {token_id, frequency}.
 *  @param num_tokens The length of the token array.
 *  @returns A new item initialized with the tokens.
 */
Item * create_item_with_tokens_and_time(const unsigned char *id, int tokens[][2], int num_tokens, time_t time) {
  Item *item = create_item(id, -1, time);
  if (NULL != item) {
    item->time = time;
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

const unsigned char * item_get_id(const Item * item) {
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

short item_get_token_frequency(const Item * item, int token_id) {
  short return_frequency;
  Word_t * frequency;
  JLG(frequency, item->tokens, token_id);

  if (NULL == frequency) {
    return_frequency = 0;
  } else {
    return_frequency = *frequency;
  }

  return return_frequency;
}

int item_next_token(const Item * item, int * token_id, short * token_frequency) {
  int success = true;
  PWord_t frequency = NULL;
  Word_t index = (Word_t) *token_id;

  if (NULL != item) {
    if (0 == token_id) {
      JLF(frequency, item->tokens, index);
    } else {
      JLN(frequency, item->tokens, index);
    }
  }

  if (NULL == frequency) {
    success = false;
    *token_frequency = 0;
  } else {
    *token_id = index;
    *token_frequency = *frequency;
  }

  return success;
}

void free_item(Item *item) {
  if (NULL != item) {
    free(item->id);
    int freed_bytes;
    if (item->tokens) {
      JLFA(freed_bytes, item->tokens);
      item->tokens = NULL;
    }
    free(item);
  }
}

int item_add_token(Item *item, int id, short token_frequency) {
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
    *token_frequency_p = (int) token_frequency;
  }

  return return_code;
}
