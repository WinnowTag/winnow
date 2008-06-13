/* Copyright (c) 2008 The Kaphan Foundation
 *
 * Possession of a copy of this file grants no permission or license
 * to use, modify, or create derivate works.
 *
 * Please contact info@peerworks.org for further information.
 */

#ifndef SQLITE_ITEM_SOURCE_H_
#define SQLITE_ITEM_SOURCE_H_

#include <time.h>
#include <libxml/tree.h>

#define ITEM_CACHE_ENTRY_PROTECTED 2

typedef struct TOKEN {
  int id;
  int frequency;
} Token, *Token_p;

typedef struct ITEM_CACHE_OPTIONS {
  int cache_update_wait_time;
  int load_items_since;
  int min_tokens;
} ItemCacheOptions;

typedef struct ITEM Item;
typedef struct POOL Pool;
typedef struct ITEM_CACHE ItemCache;
typedef struct ITEM_CACHE_ENTRY ItemCacheEntry;
typedef struct FEED Feed;

typedef int (*ItemIterator) (const Item *item, void *memo);
typedef Item* (*FeatureExtractor) (ItemCache *item_cache, const ItemCacheEntry * entry, void *memo);
typedef void (*UpdateCallback) (ItemCache * item_cache, void *memo);

extern int          item_cache_initialize         (const char *dbfile, char *error);
extern int          item_cache_create             (ItemCache **is, const char *db_file, const ItemCacheOptions * options);
extern int          item_cache_set_feature_extractor (ItemCache * item_cache, FeatureExtractor feature_extractor, void *memo);
extern int          item_cache_load               (ItemCache *item_cache);
extern int          item_cache_loaded             (const ItemCache *item_cache);
extern int          item_cache_cached_size        (ItemCache *item_cache);
extern Item *       item_cache_fetch_item         (ItemCache *item_cache,  const unsigned char * item_id, int * free_when_done);  
extern const char * item_cache_errmsg             (const ItemCache *is);
extern int          item_cache_each_item          (ItemCache *item_cache, ItemIterator iterator, void *memo);
extern const Pool * item_cache_random_background  (ItemCache *item_cache);
extern int          item_cache_add_entry          (ItemCache *item_cache, ItemCacheEntry *entry);
extern int          item_cache_remove_entry       (ItemCache *item_cache, int entry_id);
extern int          item_cache_add_feed           (ItemCache *item_cache, Feed *feed);
extern int          item_cache_remove_feed        (ItemCache *item_cache, int feed_id);
extern int          item_cache_add_item           (ItemCache *item_cache, Item *item);
extern int          item_cache_save_item          (ItemCache *item_cache, Item *item);
extern int          item_cache_start_purger       (ItemCache *item_cache, int purge_interval);
extern int          item_cache_purge_old_items    (ItemCache *item_cache);
extern int          item_cache_feature_extraction_queue_size(const ItemCache *item_cache);
extern int          item_cache_start_feature_extractor (ItemCache *item_cache);
extern int          item_cache_start_cache_updater     (ItemCache *item_cache);
extern int          item_cache_update_queue_size  (const ItemCache * item_cache);
extern int          item_cache_set_update_callback(ItemCache *item_cache, UpdateCallback callback, void *memo);
extern int          item_cache_atomize            (ItemCache *item_cache, const char *s);
extern char *       item_cache_globalize          (ItemCache *item_cache, int atom);
extern void         free_item_cache               (ItemCache *is);

extern ItemCacheEntry * create_item_cache_entry( const char * full_id,
                                                 time_t updated,
                                                 int feed_id,
                                                 time_t created_at,
                                                 const char * atom);
extern ItemCacheEntry * create_entry_from_atom_xml_document(int feed_id, xmlDocPtr doc, const char * xml_source);
extern ItemCacheEntry * create_entry_from_atom_xml(const char * xml, int feed_id);
extern int item_cache_entry_id(const ItemCacheEntry *entry);
extern const char * item_cache_entry_full_id(const ItemCacheEntry *entry);
extern const char * item_cache_entry_title(const ItemCacheEntry *entry);
extern void free_entry(ItemCacheEntry *entry);
extern const char * item_cache_entry_atom(const ItemCacheEntry *entry);
extern Feed * create_feed(int id, const char * title);
extern void   free_feed(Feed * feed);

extern Item * item_from_xml           (ItemCache * item_cache, const char * xml);
extern Item * create_item             (const unsigned char * id);
extern Item * create_item_with_tokens (const unsigned char * id, int tokens[][2], int num_tokens);
extern Item * create_item_with_tokens_and_time (const unsigned char * id, int tokens[][2], int num_tokens, time_t time);
extern const unsigned char * item_get_id             (const Item *item);
extern int    item_get_total_tokens   (const Item *item);
extern int    item_get_num_tokens     (const Item *item);
extern time_t item_get_time           (const Item *item);
extern int    item_get_token          (const Item *item, int token_id, Token_p token);
extern int    item_next_token         (const Item *item, Token_p token);
extern void   free_item               (Item *item);
/* This should only be called by item loaders */
extern int    item_add_token          (Item *item, int id, int frequency);

/* Prototypes for pools */
extern Pool * new_pool               (void);
extern int    pool_add_item          (Pool *pool, const Item *item);
extern int    pool_add_items         (Pool *pool, const int items[], int size, const ItemCache *is);
extern int    pool_num_tokens        (const Pool *pool);
extern int    pool_total_tokens      (const Pool *pool);
extern int    pool_token_frequency   (const Pool *pool, int token_id);
extern void   free_pool              (Pool *pool);
extern int    pool_next_token        (const Pool *pool, Token_p token);

#endif /*SQLITE_ITEM_SOURCE_H_*/
