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

typedef struct SQLITE_ITEM_SOURCE SQLiteItemSource;
extern int sqlite_item_source_create(SQLiteItemSource **is, const char *db_file);
extern const char * sqlite_item_source_errmsg(const SQLiteItemSource *is); 

#endif /*SQLITE_ITEM_SOURCE_H_*/
