// Copyright (c) 2007 The Kaphan Foundation
//
// Possession of a copy of this file grants no permission or license
// to use, modify, or create derivate works.
//
// Please contact info@peerworks.org for further information.

#ifndef MOCKITEMSOURCE_H
#define MOCKITEMSOURCE_H 1
#endif
#include "../src/item_cache.h"
#include "fixtures.h"

static char cwd[1024];

static Item *item_1;
static Item *item_2;
static Item *item_3;
static Item *item_4;

static void setup_mock_items(void) {
  setup_fixture_path();
   
  int tokens_1[][2] = {1, 10, 2, 3};
  int tokens_2[][2] = {1, 5, 3, 4};
  int tokens_3[][2] = {1, 6, 2, 4};
  int tokens_4[][2] = {1, 1, 2, 2, 3, 3};
  
  item_1 = create_item_with_tokens(1, tokens_1, 2);
  item_2 = create_item_with_tokens(2, tokens_2, 2);
  item_3 = create_item_with_tokens(3, tokens_3, 2);
  item_4 = create_item_with_tokens(4, tokens_4, 3);  
}

static void teardown_mock_items(void) {
  teardown_fixture_path();
  free_item(item_1);
  free_item(item_2);
  free_item(item_3);
  free_item(item_4);
}
