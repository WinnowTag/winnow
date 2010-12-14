/* Copyright (c) 2007-2010 The Kaphan Foundation
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */

/*
 * contact@winnowtag.org
 */

#ifndef SQLITE_ITEM_SOURCE_H_
#define SQLITE_ITEM_SOURCE_H_

#include <time.h>
#include <libxml/tree.h>

#define ITEM_CACHE_ENTRY_PROTECTED 2

typedef struct TOKEN {
  int id;
  short frequency;
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

typedef int (*ItemIterator) (const Item *item, void *memo);
typedef void (*UpdateCallback) (ItemCache * item_cache, void *memo);

extern int          item_cache_initialize         (const char *dbfile, char *error);
extern int          item_cache_create             (ItemCache **is, const char *db_file, const ItemCacheOptions * options);
extern int          item_cache_load               (ItemCache *item_cache);
extern int          item_cache_loaded             (const ItemCache *item_cache);
extern int          item_cache_cached_size        (ItemCache *item_cache);
extern Item *       item_cache_fetch_item         (ItemCache *item_cache,  const unsigned char * item_id, int * free_when_done);  
extern const char * item_cache_errmsg             (const ItemCache *is);
extern int          item_cache_each_item          (ItemCache *item_cache, ItemIterator iterator, void *memo);
extern const Pool * item_cache_random_background  (ItemCache *item_cache);
extern int          item_cache_add_entry          (ItemCache *item_cache, ItemCacheEntry *entry);
extern int          item_cache_remove_entry       (ItemCache *item_cache, int entry_id);
extern int          item_cache_add_item           (ItemCache *item_cache, Item *item);
extern int          item_cache_save_item          (ItemCache *item_cache, Item *item);
extern int          item_cache_start_purger       (ItemCache *item_cache, int purge_interval);
extern int          item_cache_purge_old_items    (ItemCache *item_cache);
extern int          item_cache_start_cache_updater     (ItemCache *item_cache);
extern int          item_cache_update_queue_size  (const ItemCache * item_cache);
extern int          item_cache_set_update_callback(ItemCache *item_cache, UpdateCallback callback, void *memo);
extern int          item_cache_atomize            (ItemCache *item_cache, const char *s);
extern char *       item_cache_globalize          (ItemCache *item_cache, int atom);
extern void         free_item_cache               (ItemCache *is);

extern ItemCacheEntry * create_item_cache_entry( const char * full_id,
                                                 time_t updated,
                                                 time_t created_at,
                                                 const char * atom);
extern ItemCacheEntry * create_entry_from_atom_xml_document(xmlDocPtr doc, const char * xml_source);
extern ItemCacheEntry * create_entry_from_atom_xml(const char * xml);
extern int item_cache_entry_id(const ItemCacheEntry *entry);
extern const char * item_cache_entry_full_id(const ItemCacheEntry *entry);
extern const char * item_cache_entry_title(const ItemCacheEntry *entry);
extern void free_entry(ItemCacheEntry *entry);
extern const char * item_cache_entry_atom(const ItemCacheEntry *entry);

extern Item * create_item             (const unsigned char * id, int key, time_t time);
extern Item * create_item_with_tokens (const unsigned char * id, int tokens[][2], int num_tokens);
extern Item * create_item_with_tokens_and_time (const unsigned char * id, int tokens[][2], int num_tokens, time_t time);
extern const unsigned char * item_get_id             (const Item *item);
extern int    item_get_total_tokens   (const Item *item);
extern int    item_get_num_tokens     (const Item *item);
extern time_t item_get_time           (const Item *item);
extern short  item_get_token_frequency(const Item *item, int token_id);
extern int    item_next_token         (const Item *item, int * token_id, short * token_frequency);
extern void   free_item               (Item *item);
/* This should only be called by item loaders */
extern int    item_add_token          (Item *item, int id, short frequency);

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
