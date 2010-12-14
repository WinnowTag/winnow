// Copyright (c) 2007-2010 The Kaphan Foundation
// 
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
// 
// The above copyright notice and this permission notice shall be included in
// all copies or substantial portions of the Software.
// 
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
// THE SOFTWARE.

// contact@winnowtag.org

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
  
  item_1 = create_item_with_tokens((unsigned char*) "1", tokens_1, 2);
  item_2 = create_item_with_tokens((unsigned char*) "2", tokens_2, 2);
  item_3 = create_item_with_tokens((unsigned char*) "3", tokens_3, 2);
  item_4 = create_item_with_tokens((unsigned char*) "4", tokens_4, 3);  
}

static void teardown_mock_items(void) {
  teardown_fixture_path();
  free_item(item_1);
  free_item(item_2);
  free_item(item_3);
  free_item(item_4);
}
