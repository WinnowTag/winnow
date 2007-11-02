// Copyright (c) 2007 The Kaphan Foundation
//
// Possession of a copy of this file grants no permission or license
// to use, modify, or create derivate works.
//
// Please contact info@peerworks.org for further information.

#ifndef MOCKITEMSOURCE_H
#define MOCKITEMSOURCE_H 1
#endif
#include "../src/item.h"

static ItemSource is;

static Item item_1;
static Item item_2;
static Item item_3;
static Item item_4;

static Item fake_item_source(const void* ignore, const int item_id) {
  switch (item_id) {
    case 1:
      return item_1;
    case 2:
      return item_2;
    case 3:
      return item_3;
    case 4:
      return item_4;
    default:
      return NULL;
  }
}

static void setup_mock_item_source(void) {
  int tokens_1[][2] = {1, 10, 2, 3};
  int tokens_2[][2] = {1, 5, 3, 4};
  int tokens_3[][2] = {1, 6, 2, 4};
  int tokens_4[][2] = {1, 1, 2, 2, 3, 3};
  
  item_1 = create_item_with_tokens(1, tokens_1, 2);
  item_2 = create_item_with_tokens(2, tokens_2, 2);
  item_3 = create_item_with_tokens(3, tokens_3, 2);
  item_4 = create_item_with_tokens(4, tokens_4, 3);
  
  is = malloc(sizeof(struct ITEMSOURCE));
  is->fetch_func = fake_item_source;
  is->fetch_func_state = NULL;
}

static void teardown_mock_item_source(void) {
  free(is);
  free_item(item_1);
  free_item(item_2);
  free_item(item_3);
  free_item(item_4);
}