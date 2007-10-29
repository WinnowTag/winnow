// Copyright (c) 2007 The Kaphan Foundation
//
// Possession of a copy of this file grants no permission or license
// to use, modify, or create derivate works.
//
// Please contact info@peerworks.org for further information.

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

extern Item   create_item             (int id);
extern Item   create_item_with_token  (int id, int tokens[][2], int num_tokens);
extern Item   create_item_from_file   (char * corpus, int item_id);
extern int    item_get_id             (Item item);
extern int    item_get_total_tokens   (Item item);
extern int    item_get_num_tokens     (Item item);
extern int    item_get_token          (Item item, int token_id, Token_p token);
extern char * item_get_path           (Item item);
extern void   free_item               (Item item);
