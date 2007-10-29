// Copyright (c) 2007 The Kaphan Foundation
//
// Possession of a copy of this file grants no permission or license
// to use, modify, or create derivate works.
//
// Please contact info@peerworks.org for further information.

#include <config.h>

typedef struct ITEM *Item;

typedef struct TOKEN {
  int id;
  int frequency;
} Token, *Token_p;

extern Item   load_item        (char * corpus, int item_id);
extern int    item_get_id      (Item item);
extern int    item_get_count   (Item item);
extern int    item_get_token   (Item item, int token_id, Token_p token);
extern char * item_get_path    (Item item);
extern void   free_item        (Item item);
