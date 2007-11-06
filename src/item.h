// Copyright (c) 2007 The Kaphan Foundation
//
// Possession of a copy of this file grants no permission or license
// to use, modify, or create derivate works.
//
// Please contact info@peerworks.org for further information.

#ifndef ITEM_H
#define ITEM_H 1

#include <config.h>
#if HAVE_JUDY_H
#include <Judy.h>
#endif

typedef struct ITEM {
  int id;
  int total_tokens;
  Pvoid_t tokens;
} Item, *PItem;

typedef struct TOKEN {
  int id;
  int frequency;
} Token, *Token_p;

/** An item source stores a reference to an item loading function
 *  and some state to pass as the first argument to the item 
 *  loading function.
 */
typedef struct ITEMSOURCE {
  Item* (*fetch_func)(const void*, const int item_id);
  const void* fetch_func_state;
} ItemSource;

/*** Item Source ****/
extern ItemSource * create_file_item_source (const char * corpus_directory);
extern Item *       is_fetch_item           (const ItemSource *is, const int item_id);
extern void        free_item_source         (ItemSource *is);

extern Item * create_item             (int id);
extern Item * create_item_with_tokens (int id, int tokens[][2], int num_tokens);
extern Item * create_item_from_file   (const void * state, const int item_id);
extern int    item_get_id             (const Item *item);
extern int    item_get_total_tokens   (const Item *item);
extern int    item_get_num_tokens     (const Item *item);
extern int    item_get_token          (const Item *item, int token_id, Token_p token);
extern int    item_next_token         (const Item *item, Token_p token);
extern void   free_item               (Item *item);

#endif