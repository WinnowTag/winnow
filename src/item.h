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
  char * path;
  Pvoid_t tokens;
} *Item;

typedef struct TOKEN {
  int id;
  int frequency;
} Token, *Token_p;

/** An item source stores a reference to an item loading function
 *  and some state to pass as the first argument to the item 
 *  loading function.
 */
typedef struct ITEMSOURCE {
  Item (*fetch_func)(const void*, const int item_id);
  const void* fetch_func_state;
} *ItemSource;

/*** Item Source ****/
extern ItemSource create_file_item_source (const char * corpus_directory);
extern Item       is_fetch_item           (const ItemSource is, const int item_id);

extern Item   create_item             (int id);
extern Item   create_item_with_tokens (int id, int tokens[][2], int num_tokens);
extern Item   create_item_from_file   (char * corpus, int item_id);
extern int    item_get_id             (Item item);
extern int    item_get_total_tokens   (Item item);
extern int    item_get_num_tokens     (Item item);
extern int    item_get_token          (Item item, int token_id, Token_p token);
extern char * item_get_path           (Item item);
extern int    item_next_token         (Item item, Token_p token);
extern void   free_item               (Item item);

#endif