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

typedef struct ITEM_CACHE ItemCache;

extern int item_cache_create(ItemCache **is, const char *db_file);
extern int item_cache_load(ItemCache *item_cache);
extern int item_cache_loaded(const ItemCache *item_cache);
extern int item_cache_cached_size(const ItemCache *item_cache);
extern Item * item_cache_fetch_item(ItemCache *is, int item_id);  
extern const char * item_cache_errmsg(const ItemCache *is); 
extern void free_item_cache(ItemCache *is);



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

#endif /*SQLITE_ITEM_SOURCE_H_*/
