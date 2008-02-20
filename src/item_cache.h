/* Copyright (c) 2008 The Kaphan Foundation
 *
 * Possession of a copy of this file grants no permission or license
 * to use, modify, or create derivate works.
 *
 * Please contact info@peerworks.org for further information.
 */

#ifndef SQLITE_ITEM_SOURCE_H_
#define SQLITE_ITEM_SOURCE_H_
#include "item_source.h"

typedef struct ITEM_CACHE ItemCache;
extern int item_cache_create(ItemCache **is, const char *db_file);
extern const char * item_cache_errmsg(const ItemCache *is); 
extern void free_item_cache(ItemCache *is);

#endif /*SQLITE_ITEM_SOURCE_H_*/
