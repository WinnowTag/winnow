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
#include <libxml/tree.h>
#include <libxml/xpath.h>
#include <libxml/xpathInternals.h>
#include <libxml/uri.h>

#if HAVE_JUDY_H
#include <Judy.h>
#endif
#include "item_cache.h"
#include "logging.h"
#include "misc.h"
#include "job_queue.h"
#include "xml.h"
#include "uri.h"

#define CURRENT_USER_VERSION 1
#define FETCH_ITEM_SQL "select id, strftime('%s', updated) from entries where id = ?"
#define FETCH_ITEM_TOKENS_SQL "select token_id, frequency from entry_tokens where entry_id = ?"
#define FETCH_ALL_ITEMS_SQL "select id, strftime('%s', updated) from entries where updated > (julianday('now') - ?) order by updated desc"
#define FETCH_RANDOM_BACKGROUND "select entry_id from random_backgrounds"
#define INSERT_ENTRY_SQL "insert or replace into entries (id, full_id, title, author, alternate, self, spider, content, updated, feed_id, created_at) \
                          VALUES (:id, :full_id, :title, :author, :alternate, :self, :spider, :content, julianday(:updated, 'unixepoch'), :feed_id, julianday(:created_at, 'unixepoch'))"
#define DELETE_ENTRY_SQL "delete from entries where id = ?"
#define INSERT_FEED_SQL "insert or replace into feeds VALUES (?, ?)"
#define DELETE_FEED_SQL "delete from feeds where id = ?"
#define FIND_ATOM_SQL "select id from tokens where token = ?"
#define INSERT_ATOM_SQL "insert into tokens (token) values (?)"
#define FIND_TOKEN_SQL "select token from tokens where id = ?"
#define INSERT_ENTRY_TOKENS "insert into entry_tokens (entry_id, token_id, frequency) values (?, ?, ?)"

typedef struct ORDERED_ITEM_LIST OrderedItemList;
struct ORDERED_ITEM_LIST {
  Item *item;
  OrderedItemList *next;
};

struct FEED {
  int id;
  char * title;
};

struct ITEM {
  /* The ID of the item */
  int id;
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
  int id; 
  char * full_id;
  char * title;
  char * author;
  char * alternate;
  char * self;
  char * spider;
  char * content;
  time_t updated;
  int feed_id;
  time_t created_at;
  char * atom;
};

/** This is the opaque type for the Item Cache */
struct ITEM_CACHE {
  const char *db_file;
  
  /* Item cache's copy of ItemCacheOptions */
  int cache_update_wait_time;
  int load_items_since;
  int min_tokens;
  
  sqlite3 *db;  
  sqlite3_stmt *fetch_item_stmt;
  sqlite3_stmt *fetch_item_tokens_stmt;
  sqlite3_stmt *fetch_all_items_stmt;
  sqlite3_stmt *random_background_stmt;
  sqlite3_stmt *insert_entry_stmt;
  sqlite3_stmt *delete_entry_stmt;
  sqlite3_stmt *insert_feed_stmt;
  sqlite3_stmt *delete_feed_stmt;
  sqlite3_stmt *find_token_stmt;
  sqlite3_stmt *find_atom_stmt;
  sqlite3_stmt *insert_atom_stmt;
  sqlite3_stmt *insert_entry_token_stmt;
  
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
  
  /* A linked list of item ids in descending order of updated time. */
  OrderedItemList *items_in_order;
  
  /* The Random Background pool. */
  Pool *random_background;
  
  /************************************
   *  Feature Extraction Members 
   */
  
  /* Queue for entries to get converted into items. */
  Queue *feature_extraction_queue;
  
  /* Function for feature extraction */
  FeatureExtractor feature_extractor;
  
  /* Additional data for the feature extractor */
  void *feature_extractor_memo;
  
  /* Thread which handles the feature extraction */
  pthread_t *feature_extraction_thread;
  
  /************************************
   *  In-memory cache updating members 
   */
  
  /* Queue for items to get added to the items_by_id and items_in_order lists. */
  Queue *update_queue;
  
  /* Thread which handles the cache updating */
  pthread_t *cache_updating_thread;
  
  /* R/W lock for accessing the in-memory item cache. */
  pthread_rwlock_t cache_lock;
  
  /* Callback for updates to the item cache */
  UpdateCallback update_callback;
  
  /* Additional arguments to the udpate callback */
  void *update_callback_memo;  
  
  /* Thread that purges the item cache */
  pthread_t *purge_thread;
  
  /* Cache purging interval in seconds */
  int purge_interval;
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
ItemCacheEntry * create_item_cache_entry(int id, 
                                          const char * full_id,
                                          const char * title,
                                          const char * author,
                                          const char * alternate,
                                          const char * self,
                                          const char * spider,
                                          const char * content,
                                          time_t updated,
                                          int feed_id,
                                          time_t created_at,
                                          const char * atom) {
  ItemCacheEntry *entry = calloc(1, sizeof(struct ITEM_CACHE_ENTRY));
  
  if (entry) {
    entry->id = id;
    entry->feed_id = feed_id;
    entry->updated = updated;
    entry->created_at = created_at;
    COPY_STRING(entry->full_id, full_id);
    COPY_STRING(entry->title, title);
    COPY_STRING(entry->author, author);
    COPY_STRING(entry->alternate, alternate);
    COPY_STRING(entry->self, self);
    COPY_STRING(entry->spider, spider);
    COPY_STRING(entry->content, content);
    COPY_STRING(entry->atom, atom);
  } else {
    fatal("Malloc failed in create_item_cache_entry");
  }
  
  return entry;
}

static ItemCacheEntry * copy_entry(const ItemCacheEntry * entry) {
  return create_item_cache_entry(entry->id, entry->full_id, entry->title, entry->author, entry->alternate,
                                 entry->self, entry->spider, entry->content, entry->updated, entry->feed_id,
                                 entry->created_at, entry->atom);
  
}

/** Create an entry from atom XML.
 */
ItemCacheEntry * create_entry_from_atom_xml_document(int feed_id, xmlDocPtr doc, const char * xml_source) {  
  ItemCacheEntry *entry = NULL;
  xmlXPathContextPtr context = xmlXPathNewContext(doc);    
  xmlXPathRegisterNs(context, BAD_CAST "atom", BAD_CAST "http://www.w3.org/2005/Atom");
  
  char *id = get_element_value(context, "/atom:entry/atom:id/text()");
  char *title = get_element_value(context, "/atom:entry/atom:title/text()");
  char *updated = get_element_value(context, "/atom:entry/atom:updated/text()");
  char *content = get_element_value(context, "/atom:entry/atom:content/text()");
  char *author = get_element_value(context, "/atom:entry/atom:author/atom:name/text()");
  char *alternate = get_attribute_value(context, "/atom:entry/atom:link[@rel = 'alternate']", "href");;
  char *self = get_attribute_value(context, "/atom:entry/atom:link[@rel = 'self']", "href");
  char *spider = get_attribute_value(context, "/atom:entry/atom:link[@rel = 'http://peerworks.org/rel/spider']", "href");
  
  // Must have all of these to create the item
  if (id && title && updated) {
    int id_i = uri_fragment_id(id);
    struct tm updated_tm;
    memset(&updated_tm, 0, sizeof(updated_tm));
    time_t updated_time = time(NULL);
    
    if (NULL != strptime(updated, "%Y-%m-%dT%H:%M:%S%Z", &updated_tm)) {
      updated_time = timegm(&updated_tm);
    } else if (NULL != strptime(updated, "%Y-%m-%dT%H:%M:%S", &updated_tm)) {
      updated_time = timegm(&updated_tm);
    } else {
      error("Couldn't parse datetime: %s", updated);
    }
   
    if (id_i > 0) {      
      entry = create_item_cache_entry(id_i, id, title, author, alternate, self, spider, content, updated_time, feed_id, time(NULL), xml_source);
    }
  }
   
  if (alternate) xmlFree(alternate);
  if (self) xmlFree(self);
  if (author) free(author);
  if (content) free(content);
  if (id) free(id);
  if (title) free(title);
  if (updated) free(updated);
  xmlXPathFreeContext(context);
  
  return entry;
}

int item_cache_entry_id(const ItemCacheEntry * entry) {
  return entry->id;
}

const char * item_cache_entry_atom(const ItemCacheEntry * entry) {
  return entry->atom;
}

void free_entry(ItemCacheEntry *entry) {
  if (entry) {
    FREE_STRING(entry->full_id);
    FREE_STRING(entry->title);
    FREE_STRING(entry->author);
    FREE_STRING(entry->alternate);
    FREE_STRING(entry->self);
    FREE_STRING(entry->content);
    FREE_STRING(entry->spider);
    FREE_STRING(entry->atom);
    free(entry);
  }
}

/******************************************************************************
 * Feed creation functions 
 ******************************************************************************/
Feed * create_feed(int id, const char * title) {
  Feed *feed = calloc(1, sizeof(struct FEED));
  
  if (NULL != feed) {
    feed->id = id;
    COPY_STRING(feed->title, title);
  }
  
  return feed;
}

void free_feed(Feed * feed) {
  if (feed) {
    FREE_STRING(feed->title);
    free(feed);
  }
}

/******************************************************************************
 * ItemCache functions 
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
      SQLITE_OK != sqlite3_prepare_v2( item_cache->db, FETCH_ITEM_TOKENS_SQL,      -1, &item_cache->fetch_item_tokens_stmt,     NULL) ||
      SQLITE_OK != sqlite3_prepare_v2( item_cache->db, FETCH_ALL_ITEMS_SQL,        -1, &item_cache->fetch_all_items_stmt,       NULL) ||
      SQLITE_OK != sqlite3_prepare_v2( item_cache->db, FETCH_RANDOM_BACKGROUND,    -1, &item_cache->random_background_stmt,     NULL) ||
      SQLITE_OK != sqlite3_prepare_v2( item_cache->db, INSERT_ENTRY_SQL,           -1, &item_cache->insert_entry_stmt,          NULL) ||
      SQLITE_OK != sqlite3_prepare_v2( item_cache->db, DELETE_ENTRY_SQL,           -1, &item_cache->delete_entry_stmt,          NULL) ||
      SQLITE_OK != sqlite3_prepare_v2( item_cache->db, INSERT_FEED_SQL,            -1, &item_cache->insert_feed_stmt,           NULL) ||
      SQLITE_OK != sqlite3_prepare_v2( item_cache->db, DELETE_FEED_SQL,            -1, &item_cache->delete_feed_stmt,           NULL) ||
      SQLITE_OK != sqlite3_prepare_v2( item_cache->db, FIND_TOKEN_SQL,             -1, &item_cache->find_token_stmt,            NULL) ||
      SQLITE_OK != sqlite3_prepare_v2( item_cache->db, FIND_ATOM_SQL,              -1, &item_cache->find_atom_stmt,             NULL) ||
      SQLITE_OK != sqlite3_prepare_v2( item_cache->db, INSERT_ATOM_SQL,            -1, &item_cache->insert_atom_stmt,           NULL) ||
      SQLITE_OK != sqlite3_prepare_v2( item_cache->db, INSERT_ENTRY_TOKENS,        -1, &item_cache->insert_entry_token_stmt,    NULL)) {
    fatal("Unable to prepare statment: %s", item_cache_errmsg(item_cache));
    rc = CLASSIFIER_FAIL;
  }
  
  return rc;
}

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
 * @param db_file The filename for the database file.
 * @returns CLASSIFIER_OK if all went well, CLASSIFIER_FAIL if something went wrong.
 *          Providing that the function could allocate memory, item_cache will be an
 *          allocated item source in either case.  If the return value was
 *          CLASSIFIER_FAIL you can pass item_cache back to sqlite_item_source_errmsg
 *          to get a detailed error message.
 */
int item_cache_create(ItemCache **item_cache, const char * db_file, const ItemCacheOptions * options) {
  int rc = CLASSIFIER_OK;
  
  *item_cache = calloc(1, sizeof(struct ITEM_CACHE));
  
  (*item_cache)->cache_update_wait_time = options->cache_update_wait_time;
  (*item_cache)->load_items_since = options->load_items_since;
  (*item_cache)->min_tokens = options->min_tokens;
  (*item_cache)->version_mismatch = 0;
  (*item_cache)->items_by_id = NULL;
  (*item_cache)->items_in_order = NULL;
  (*item_cache)->random_background = NULL;
  (*item_cache)->loaded = false;
  (*item_cache)->feature_extraction_queue = new_queue();
  (*item_cache)->update_queue = new_queue();
    
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
    fatal("Unable to allocate memory for SQLiteItemSource");
    rc = CLASSIFIER_FAIL;
  } else {  
    if (SQLITE_OK != sqlite3_open_v2(db_file, &((*item_cache)->db), SQLITE_OPEN_READWRITE, NULL)) { 
      rc = CLASSIFIER_FAIL;
    } else {
      if (CLASSIFIER_OK == get_user_version(*item_cache)) {
        if ((*item_cache)->user_version != CURRENT_USER_VERSION) {
          (*item_cache)->version_mismatch = 1;
          rc = CLASSIFIER_FAIL;
        } else if (CLASSIFIER_OK != create_prepared_statements(*item_cache)) {
          rc = CLASSIFIER_FAIL;
        } else {
          sqlite3_busy_timeout((*item_cache)->db, 1000);
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
      sqlite3_finalize(item_cache->random_background_stmt);
      sqlite3_finalize(item_cache->insert_entry_stmt);
      sqlite3_finalize(item_cache->delete_entry_stmt);
      sqlite3_finalize(item_cache->insert_feed_stmt);
      sqlite3_finalize(item_cache->delete_feed_stmt);
      sqlite3_finalize(item_cache->find_token_stmt);
      sqlite3_finalize(item_cache->find_atom_stmt);
      sqlite3_finalize(item_cache->insert_atom_stmt);
      sqlite3_finalize(item_cache->insert_entry_token_stmt);
      sqlite3_close(item_cache->db);
    }
    
    if (item_cache->items_in_order) {
      Word_t index = 0;
      PWord_t item_pointer;
      JLF(item_pointer, item_cache->items_by_id, index);
      while (NULL != item_pointer) {
        free_item((Item*) (*item_pointer));
        JLN(item_pointer, item_cache->items_by_id, index);
      }
      JLFA(index, item_cache->items_by_id);
          
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
    free_queue(item_cache->feature_extraction_queue);
    free_queue(item_cache->update_queue);    
    
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

/* Fetches the tokens for the given item.
 *
 * NOTE: The caller must hold the db_access_mutex.
 * 
 * @returns the number of tokens fetched for the the item.
 */
static int fetch_tokens_for(ItemCache * item_cache, Item * item) {
  int tokens_loaded = 0;
  
  if (item_cache && item) {
    int sqlite3_rc = sqlite3_bind_int(item_cache->fetch_item_tokens_stmt, 1, item->id);
    
    if (SQLITE_OK != sqlite3_rc) {
      fatal("fetch_item_tokens_stmt bind error = %s", item_cache_errmsg(item_cache));
    } else {
      while (SQLITE_ROW == sqlite3_step(item_cache->fetch_item_tokens_stmt)) {
        tokens_loaded++;
        item_add_token(item, sqlite3_column_int(item_cache->fetch_item_tokens_stmt, 0),
                             sqlite3_column_int(item_cache->fetch_item_tokens_stmt, 1));
      }
    }
    
    sqlite3_clear_bindings(item_cache->fetch_item_tokens_stmt);
    sqlite3_reset(item_cache->fetch_item_tokens_stmt);
  }
  
  return tokens_loaded;
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
  if (!item_cache) {
    fatal("item_cache_load got NULL for item_cache");
    return CLASSIFIER_FAIL;
  }
  
  info("item_cache_load from %i days ago", item_cache->load_items_since);
  pthread_mutex_lock(&item_cache->db_access_mutex);
  time_t start_time = time(NULL);
  int rc = CLASSIFIER_OK;
  OrderedItemList *ordered_list = NULL;
  OrderedItemList *last = NULL;
  Pvoid_t itemlist = NULL;  
  
  sqlite3_bind_int(item_cache->fetch_all_items_stmt, 1, item_cache->load_items_since);
    
  /* Load the item ids and timestamp indexing and order them by each. */
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
      
      if (!ordered_list) {
        ordered_list = malloc(sizeof(OrderedItemList));
        ordered_list->item = item;
        ordered_list->next = NULL;
        last = ordered_list;
      } else {
        last->next = malloc(sizeof(OrderedItemList));
        last->next->item = item;
        last->next->next = NULL;
        last = last->next;
      }
    }
  }
  
  sqlite3_clear_bindings(item_cache->fetch_all_items_stmt);
  sqlite3_reset(item_cache->fetch_all_items_stmt);
  
  /* Now load the tokens */
  OrderedItemList *current = ordered_list;
  OrderedItemList *previous = NULL;
  
  while (current) {
    if (item_cache->min_tokens > fetch_tokens_for(item_cache, current->item)) {
      OrderedItemList *next = current->next;
            
      if (!previous) {
        ordered_list = next;
      } else {
        previous->next = next;
      }
      
      int judyrc;      
      JLD(judyrc, itemlist, current->item->id);
      free_item(current->item);
      free(current);
      
      current = next;
    } else {
      previous = current;
      current = current->next;
    }
  }  
    
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
  
  pthread_rwlock_wrlock(&item_cache->cache_lock);
  item_cache->random_background = rndbg;
  item_cache->items_by_id = itemlist;
  item_cache->items_in_order = ordered_list;
  item_cache->loaded = true;
  time_t end_time = time(NULL);
  pthread_rwlock_unlock(&item_cache->cache_lock);
  
  pthread_mutex_unlock(&item_cache->db_access_mutex);
  
  info("loaded %i items in %i seconds", item_cache_cached_size(item_cache), end_time - start_time);
  return rc;
}

/** Returns the number of items in the in-memory cache.
 */
int item_cache_cached_size(ItemCache *item_cache) {
  int count;
  pthread_rwlock_rdlock(&item_cache->cache_lock);
  JLC(count, item_cache->items_by_id, 0, -1);
  pthread_rwlock_unlock(&item_cache->cache_lock);
  return count;
}

/** Set the FeatureExtractor to use for extracting features from an Entry.
 * 
 * @param item_cache The item cache to use the feature extractor with.
 * @param feature_extractor The FeatureExtractor to use to convert ItemCacheEntry instances to Item instances.
 */
int item_cache_set_feature_extractor(ItemCache * item_cache, FeatureExtractor feature_extractor, void *memo) {
  if (item_cache) {
    item_cache->feature_extractor = feature_extractor;
    item_cache->feature_extractor_memo = memo;
  }
  return CLASSIFIER_OK;
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
 * TODO Handle SQLITE_BUSY in case another process locks the database.
 */
Item * item_cache_fetch_item(ItemCache *item_cache, int id, int * free_when_done) {
  debug("fetching item %i", id);
  int sqlite3_rc;
  Item *item = NULL;
  
  if (!item_cache) {
    fatal("Got NULL item_cache in item_cache_fetch_item");
    return item;
  }
  
  pthread_rwlock_rdlock(&item_cache->cache_lock);
  PWord_t item_pointer;
  JLG(item_pointer, item_cache->items_by_id, id);
  if (NULL != item_pointer) {
    item = (Item*)(*item_pointer);
    *free_when_done = false;
  }
  pthread_rwlock_unlock(&item_cache->cache_lock);
  
  
  if (NULL == item) {
    *free_when_done = true;
    pthread_mutex_lock(&item_cache->db_access_mutex);
    trace("thread(%i) has locked db_access_mutex to fetch %i", pthread_self(), id);
    
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
    if (0 == fetch_tokens_for(item_cache, item)) {
      free_item(item);
      item = NULL;
    }      
    
    pthread_mutex_unlock(&item_cache->db_access_mutex);
    trace("thread(%i) has unlocked db_access_mutex after fetching %i", pthread_self(), id);
  }
  
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
const Pool * item_cache_random_background(const ItemCache * item_cache)  {
  const Pool * random_background = NULL;
  if (item_cache) {
    random_background = item_cache->random_background;
  }
  
  return random_background;
}

#define BIND_TEXT(stmt, txt, pos) if (txt) {               \
  if (SQLITE_OK != sqlite3_bind_text(stmt, pos, txt, -1, NULL)) {     \
    error("Error binding %s to parameter %i", txt, pos);              \
    rc = CLASSIFIER_FAIL;                                             \
    goto end;                                                         \
  }                                                                   \
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
  
  if (item_cache && entry) {
    int is_new_entry = true;
    pthread_mutex_lock(&item_cache->db_access_mutex);
    
    // Is it new?
    sqlite3_bind_int(item_cache->fetch_item_stmt, 1, entry->id);
    if (SQLITE_ROW == sqlite3_step(item_cache->fetch_item_stmt)) {
      is_new_entry = false;
    }
    
    sqlite3_clear_bindings(item_cache->fetch_item_stmt);
    sqlite3_reset(item_cache->fetch_item_stmt);
    
    // Do the insert or replace if it already exists
    sqlite3_bind_int(item_cache->insert_entry_stmt, 1, entry->id);
    BIND_TEXT( item_cache->insert_entry_stmt, entry->full_id,   2);
    BIND_TEXT( item_cache->insert_entry_stmt, entry->title,     3);
    BIND_TEXT( item_cache->insert_entry_stmt, entry->author,    4);
    BIND_TEXT( item_cache->insert_entry_stmt, entry->alternate, 5);
    BIND_TEXT( item_cache->insert_entry_stmt, entry->self,      6);
    BIND_TEXT( item_cache->insert_entry_stmt, entry->spider,    7);
    BIND_TEXT( item_cache->insert_entry_stmt, entry->content,   8);
    sqlite3_bind_double(item_cache->insert_entry_stmt, 9, entry->updated);
    sqlite3_bind_int(item_cache->insert_entry_stmt, 10, entry->feed_id);
    sqlite3_bind_double(item_cache->insert_entry_stmt, 11, entry->created_at);
    
    if (SQLITE_DONE != sqlite3_step(item_cache->insert_entry_stmt)) {
      error("Error inserting item %i: %s", entry->id, item_cache_errmsg(item_cache));
      rc = CLASSIFIER_FAIL;
    }
end:
    sqlite3_clear_bindings(item_cache->insert_entry_stmt);
    sqlite3_reset(item_cache->insert_entry_stmt);    
    pthread_mutex_unlock(&item_cache->db_access_mutex);
    
    // We don't want to extract features for items we already have.
    // TODO Handle updates to features for items somehow?
    if (is_new_entry && rc != CLASSIFIER_FAIL) {
      q_enqueue(item_cache->feature_extraction_queue, copy_entry(entry));
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
    
    if (SQLITE_DONE != sqlite3_rc) {
      if (SQLITE_CONSTRAINT == sqlite3_rc) {
        info("Constraint violated");
        rc = ITEM_CACHE_ENTRY_PROTECTED;
      } else {
        error("Error deleting ItemCache entry %i: %s", entry_id, item_cache_errmsg(item_cache));
        rc = CLASSIFIER_FAIL;        
      }      
    } else {
      info("Deleted ItemCache entry %i", entry_id);
    }
    
    sqlite3_clear_bindings(item_cache->delete_entry_stmt);
    sqlite3_reset(item_cache->delete_entry_stmt);
    pthread_mutex_unlock(&item_cache->db_access_mutex);    
  }
  
  return rc; 
}

/** Adds a feed to the item cache
 */
int item_cache_add_feed(ItemCache *item_cache, Feed * feed) {
  int rc = CLASSIFIER_OK;
  
  if (item_cache && feed) {
    pthread_mutex_lock(&item_cache->db_access_mutex);
    sqlite3_bind_int(item_cache->insert_feed_stmt, 1, feed->id);
    BIND_TEXT(item_cache->insert_feed_stmt, feed->title, 2);
    
    if (SQLITE_DONE != sqlite3_step(item_cache->insert_feed_stmt)) {
      error("Error inserting feed '%s' into item cache", feed->title, item_cache_errmsg(item_cache));
      rc = CLASSIFIER_FAIL;
    } else {
      info("Inserted feed '%s' into item cache", feed->title);
    }

  end:
    sqlite3_clear_bindings(item_cache->insert_feed_stmt);
    sqlite3_reset(item_cache->insert_feed_stmt);
    pthread_mutex_unlock(&item_cache->db_access_mutex);
  }
  
  return rc;
}

/** Remove a feed from the item cache.
 */
int item_cache_remove_feed(ItemCache *item_cache, int feed_id) {
  int rc = CLASSIFIER_OK;
  
  if (item_cache) {
    pthread_mutex_lock(&item_cache->db_access_mutex);    
    sqlite3_bind_int(item_cache->delete_feed_stmt, 1, feed_id);
    
    if (SQLITE_DONE != sqlite3_step(item_cache->delete_feed_stmt)) {
      error("Error deleting feed %i from item cache: %s", feed_id, item_cache_errmsg(item_cache));
      rc = CLASSIFIER_FAIL;
    } else {
      info("Deleted feed %i", feed_id);
    }
    
    sqlite3_clear_bindings(item_cache->delete_feed_stmt);
    sqlite3_reset(item_cache->delete_feed_stmt);
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

      PWord_t item_pointer;
      JLI(item_pointer, item_cache->items_by_id, item->id);
      if (item_pointer) {
        (*item_pointer) = (Word_t) item;
        OrderedItemList *new_node = calloc(1, sizeof(struct ORDERED_ITEM_LIST));

        if (new_node) {        
          new_node->item = item;
          // Is it the first one?
          if (item_cache->items_in_order == NULL) {
            item_cache->items_in_order = new_node;
          } else if (item_cache->items_in_order->item->time < item->time) {
            new_node->next = item_cache->items_in_order;
            item_cache->items_in_order = new_node;
          } else {
            OrderedItemList *current = item_cache->items_in_order;
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
    debug("Saving item %i", item->id);
    pthread_mutex_lock(&item_cache->db_access_mutex);    
    char *err;
    
    if (SQLITE_OK != sqlite3_exec(item_cache->db, "begin immediate transaction", NULL, NULL, &err)) {
      error("Could not start transaction to save item: %s", err);
      sqlite3_free(err);
      rc = CLASSIFIER_FAIL;
    } else {
      Token token;
      token.id = 0;
      
      while(item_next_token(item, &token)) {
        debug("save token %i", token.id);
        int stmt_rc;
        sqlite3_bind_int(item_cache->insert_entry_token_stmt, 1, item->id);
        sqlite3_bind_int(item_cache->insert_entry_token_stmt, 2, token.id);
        sqlite3_bind_int(item_cache->insert_entry_token_stmt, 3, token.frequency);
        
        stmt_rc = sqlite3_step(item_cache->insert_entry_token_stmt);
        sqlite3_clear_bindings(item_cache->insert_entry_token_stmt);
        sqlite3_reset(item_cache->insert_entry_token_stmt);
        
        if (SQLITE_DONE != stmt_rc) {
          error("Error inserting entry token: (%i, %i, %i), %s", 
                item->id, token.id, token.frequency, item_cache_errmsg(item_cache));
          rc = CLASSIFIER_FAIL;
          break;
        }
      }
      
      if (CLASSIFIER_FAIL == rc) {
        sqlite3_exec(item_cache->db, "rollback transaction", NULL, NULL, NULL);
      } else if (SQLITE_OK != sqlite3_exec(item_cache->db, "commit transaction", NULL, NULL, &err)) {
        error("Error commiting save item: %s", err);
        rc = CLASSIFIER_FAIL;
        sqlite3_free(err);
      }
    }
       
    pthread_mutex_unlock(&item_cache->db_access_mutex);
  }
  
  return rc;
}

static time_t get_purge_time(void) {
  time_t now = time(NULL);
  struct tm purge_time_tm;
  gmtime_r(&now, &purge_time_tm);
  purge_time_tm.tm_mon--;
  purge_time_tm.tm_mday--;
  return timegm(&purge_time_tm);
}

int item_cache_purge_old_items(ItemCache *item_cache) {
  info("Starting purge_old_items");
  int rc = CLASSIFIER_OK;
  
  if (item_cache) {    
    int number_purged = 0;
    int number_left = 0;
    pthread_rwlock_wrlock(&item_cache->cache_lock);
    time_t purge_time = get_purge_time();
        
    OrderedItemList *current;
    OrderedItemList *previous = NULL;
    
    /* Find the point in the list where items are older than purge_time and remove them. */
    for (current = item_cache->items_in_order; current != NULL; previous = current, current = current->next) {
      if (current->item->time <= purge_time) {
        if (!previous) {
          item_cache->items_in_order = NULL;         
        } else {
          previous->next = NULL;
        }
        
        while (current) {
          OrderedItemList *next = current->next;
          JLD(rc, item_cache->items_by_id, current->item->id);
          free_item(current->item);
          free(current);
          current = next;
          number_purged++;
        }
        
        break;
      }
      
      number_left++;
    }
     
    info("Purged %i items, %i items left", number_purged, number_left);   
    pthread_rwlock_unlock(&item_cache->cache_lock);
  } 
  
  return rc;
}


static void * item_cache_purge_thread_func(void *memo) {
  ItemCache *item_cache = (ItemCache *) memo;
  
  while (true) {
    sleep(item_cache->purge_interval);
    item_cache_purge_old_items(item_cache);
  }
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

/** Returns the number of ItemCacheEntry instances in the feature extraction queue.
 */
int item_cache_feature_extraction_queue_size(const ItemCache *item_cache) {
  int size = 0;
  if (item_cache) {
    size = q_size(item_cache->feature_extraction_queue);
  }
  return size;
}

/** Feature extraction thread function.
 * 
 * This handles taking an ItemCacheEntry off the feature_extraction queue,
 * calling the feature_extractor and putting the result on the cache_updating
 * queue.
 */
static void * feature_extraction_thread_func(void *memo) {
  ItemCache * item_cache = (ItemCache*) memo;
  info("feature extractor thread started");
  
  while (true) {
    ItemCacheEntry *entry = q_dequeue_or_wait(item_cache->feature_extraction_queue, 1);
    if (entry) {
      debug("Got entry off feature_extraction_queue");
      Item *item = item_cache->feature_extractor(item_cache, entry, item_cache->feature_extractor_memo);
      if (item) {
        UpdateJob *job = create_add_job(item);
        q_enqueue(item_cache->update_queue, job);
        debug("Update added to update_queue");
      }       
      free_entry(entry);
    }
  }
  
  info("feature extractor thread ended");
  return NULL;
}

/** Starts the item cache feature extractor.
 * 
 *  This requires a feature_extractor to be set on the item cache.
 *  This can not be called twice.
 * 
 */
int item_cache_start_feature_extractor(ItemCache *item_cache) {
  int rc = CLASSIFIER_OK;
  if (item_cache) {
    if (item_cache->feature_extraction_thread != NULL) {
      fatal("Tried to start feature extractor more than once.");
      rc = CLASSIFIER_FAIL;
    } else if (item_cache->feature_extractor == NULL) {
      fatal("Tried to start feature extractor without a feature extractor assigned.");
      rc = CLASSIFIER_FAIL;
    } else {
      item_cache->feature_extraction_thread = malloc(sizeof(pthread_t));
      if (item_cache->feature_extraction_thread) {
        if (pthread_create(item_cache->feature_extraction_thread, NULL, feature_extraction_thread_func, item_cache)) {
          fatal("Error creating feature_extraction_thread");
          rc = CLASSIFIER_FAIL;
        }
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

#define PROCESSING_LIMIT 200

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
void * cache_updating_func(void *memo) {
  ItemCache *item_cache = (ItemCache*) memo;
  
  while (true) {
    UpdateJob *job = q_dequeue_or_wait(item_cache->update_queue, item_cache->cache_update_wait_time);
    int items_added = 0;
    
    if (job) {      
      do {
        switch (job->type) {
          case ADD:
            job->item->time = time(NULL);
            if (CLASSIFIER_OK == item_cache_save_item(item_cache, job->item)) {              
              if (CLASSIFIER_OK == item_cache_add_item(item_cache, job->item)) {
                items_added++;
              } else {
                debug("No enough tokens to add to the classification item cache: %i", job->item->id);
                free_item(job->item);
              }
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

Item * item_from_xml(ItemCache * item_cache, const char * xml) {
  Item *item = NULL;
  
  if (item_cache && xml) {
    xmlDocPtr doc = xmlParseDoc(BAD_CAST xml);
    if (doc) {
      xmlXPathContextPtr context = xmlXPathNewContext(doc);
      xmlXPathRegisterNs(context, BAD_CAST "pw", BAD_CAST "http://peerworks.org/classifier");
      char *id_s = get_element_value(context, "/pw:item/pw:id/text()");
      int id = url_fragment_as_int(id_s);
      
      if (id > 0) {
        int i;
        item = create_item(id);
        xmlXPathObjectPtr xp = xmlXPathEvalExpression(BAD_CAST "/pw:item/pw:feature", context);
        
        for (i = 0; i < xp->nodesetval->nodeNr; i++) {
          int failed = false;
          char *key = (char *) xmlGetProp(xp->nodesetval->nodeTab[i], BAD_CAST "key");
          char *value = (char *) xmlGetProp(xp->nodesetval->nodeTab[i], BAD_CAST "value");
         
          if (!(key && value && *key != '\0' && *value != '\0')) {
            error("Got malformed features in item xml: %s", xml);
            failed = true;
          } else {
            int frequency = strtol(value, NULL, 10);            
            if (errno == EINVAL) {
              error("frequency was not an integer: %s in %s", value, xml);
              failed = true;
            }

            int token = item_cache_atomize(item_cache, key);
            if (token == 0) {
              failed = true;
            }
            
            if (!failed) {
              debug("Adding token: %i, %i", token, frequency);
              item_add_token(item, token, frequency);              
            }
          }
          
          if (key) xmlFree(key);
          if (value) xmlFree(value);
          if (failed) {
            free_item(item);
            item = NULL;
            break; 
          }
        }
        
        xmlXPathFreeObject(xp);
      } else {
        error("Got no id");
      }
      
      xmlFree(id_s);
      xmlXPathFreeContext(context);
      xmlFreeDoc(doc);
    }
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

/** Create an item from an array of tokens.
 *
 *  @param id The id of the item.
 *  @param tokens A 2D array of tokens where each row is {token_id, frequency}.
 *  @param num_tokens The length of the token array.
 *  @returns A new item initialized with the tokens.
 */
Item * create_item_with_tokens_and_time(int id, int tokens[][2], int num_tokens, time_t time) {
  Item *item = create_item(id);
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

