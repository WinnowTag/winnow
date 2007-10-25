// Copyright (c) 2007 The Kaphan Foundation
//
// Possession of a copy of this file grants no permission or license
// to use, modify, or create derivate works.
//
// Please contact info@peerworks.org for further information.

#include <Judy.h>

typedef struct ITEM{
  int id;
  int count;
  Pvoid_t tokens;
} *Item;


Item load_item(char * corpus, int item_id);
extern void free_item(Item item);
