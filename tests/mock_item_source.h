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
#include "../src/item_source.h"
#include "fixtures.h"

#define MOCK_IS_SIZE 4 

static char cwd[1024];
static ItemSource *is;

static Item *item_1;
static Item *item_2;
static Item *item_3;
static Item *item_4;
static ItemList *item_list;

static Item * fake_item_source(const void* ignore, const int item_id) {
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

static ItemList * fake_fetch_all(const void *ignore) {
  return item_list;
}

static void setup_mock_item_source(void) {
  setup_fixture_path();
   
  int tokens_1[][2] = {1, 10, 2, 3};
  int tokens_2[][2] = {1, 5, 3, 4};
  int tokens_3[][2] = {1, 6, 2, 4};
  int tokens_4[][2] = {1, 1, 2, 2, 3, 3};
  
  item_1 = create_item_with_tokens(1, tokens_1, 2);
  item_2 = create_item_with_tokens(2, tokens_2, 2);
  item_3 = create_item_with_tokens(3, tokens_3, 2);
  item_4 = create_item_with_tokens(4, tokens_4, 3);
  
  item_list = create_item_list();
  item_list_add_item(item_list, item_1);
  item_list_add_item(item_list, item_2);
  item_list_add_item(item_list, item_3);
  item_list_add_item(item_list, item_4);
  
  is = create_item_source();
  is->fetch_func = fake_item_source;
  is->fetch_all_func = fake_fetch_all;  
}

static void teardown_mock_item_source(void) {
  teardown_fixture_path();
  free_item_source(is);
  free_item_list(item_list);
}
