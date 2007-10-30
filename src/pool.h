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
#include "item.h"

typedef struct POOL {
  int total_tokens;
  Pvoid_t tokens;
} *Pool;

extern Pool   new_pool               (void);
extern int    pool_add_item          (Pool pool, Item item);
extern int    pool_num_tokens        (Pool pool);
extern int    pool_total_tokens      (Pool pool);
extern int    pool_token_frequency   (Pool pool, int token_id);
extern void   free_pool              (Pool pool);
extern int    pool_next_token        (Pool pool, Token_p token);

#endif