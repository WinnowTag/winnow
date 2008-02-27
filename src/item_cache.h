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
typedef struct TOKEN {
  int id;
  int frequency;
} Token, *Token_p;

typedef struct ITEM Item;
typedef struct POOL Pool;
typedef struct ITEM_CACHE ItemCache;
typedef struct ITEM_CACHE_ENTRY ItemCacheEntry;

typedef int (*ItemIterator) (const Item *item, void *memo);

extern int          item_cache_initialize         (const char *dbfile, char *error);
extern int          item_cache_create             (ItemCache **is, const char *db_file);
extern int          item_cache_load               (ItemCache *item_cache);
extern int          item_cache_loaded             (const ItemCache *item_cache);
extern int          item_cache_cached_size        (const ItemCache *item_cache);
extern Item *       item_cache_fetch_item         (ItemCache *is, int item_id);  
extern const char * item_cache_errmsg             (const ItemCache *is);
extern int          item_cache_each_item          (const ItemCache *item_cache, ItemIterator iterator, void *memo);
extern const Pool * item_cache_random_background  (const ItemCache *item_cache);
extern int          item_cache_add_entry          (ItemCache *item_cache, ItemCacheEntry *entry);
extern void         free_item_cache               (ItemCache *is);

extern ItemCacheEntry * create_item_cache_entry(int id, 
                                                 const char * full_id,
                                                 const char * title,
                                                 const char * author,
                                                 const char * alternate,
                                                 const char * self,
                                                 const char * content,
                                                 double updated,
                                                 int feed_id,
                                                 double created_at);

extern Item * create_item             (int id);
extern Item * create_item_with_tokens (int id, int tokens[][2], int num_tokens);
extern int    item_get_id             (const Item *item);
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
