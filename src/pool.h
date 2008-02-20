// Copyright (c) 2007 The Kaphan Foundation
//
// Possession of a copy of this file grants no permission or license
// to use, modify, or create derivate works.
//
// Please contact info@peerworks.org for further information.

#ifndef POOL_H
#define POOL_H

#include <config.h>
#if HAVE_JUDY_H
#include <Judy.h>
#endif
#include "item_cache.h"
#include "item_source.h"

typedef struct POOL {
  int total_tokens;
  Pvoid_t tokens;
} Pool;

extern Pool * new_pool               (void);
extern int    pool_add_item          (Pool *pool, const Item *item);
extern int    pool_add_items         (Pool *pool, const int items[], int size, const ItemSource *is);
extern int    pool_num_tokens        (const Pool *pool);
extern int    pool_total_tokens      (const Pool *pool);
extern int    pool_token_frequency   (const Pool *pool, int token_id);
extern void   free_pool              (Pool *pool);
extern int    pool_next_token        (const Pool *pool, Token_p token);

#endif
