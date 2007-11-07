/* Copyright (c) 2007 The Kaphan Foundation
 *
 * Possession of a copy of this file grants no permission or license
 * to use, modify, or create derivate works.
 *
 * Please contact info@peerworks.org for further information.
 */
 
#include <string.h>
#include "item_source.h"

/******************************************************************************
 * ItemSource functions 
 ******************************************************************************/

/** Creates a blank item source.
 *
 *  This should only be used by concrete item sources to create a blank source
 *  to fill in.
 */
ItemSource * create_item_source() {
  ItemSource *is = calloc(1, sizeof(ItemSource));
  if (NULL == is) {
    fatal("Malloc error in create_item_source");
  }
  
  return is;
}
/** Fetch an item from and ItemSource.
 *
 *  @param is The item source.
 *  @param item_id The id of the item.
 *  @returns The item or NULL.
 */
Item * is_fetch_item(const ItemSource *is, const int item_id) {
  return is->fetch_func(is->state, item_id);
}

ItemList * is_fetch_all_items(const ItemSource *is) {
  ItemList *items = NULL;
  if (NULL != is->fetch_all_func) {
    items = is->fetch_all_func(is->state);    
  }
  return items;
}

int is_alive(const ItemSource *is) {
  return is->alive_func(is->state); 
}

/** Free's an ItemSource
 */
void free_item_source(ItemSource *is) {
  if (NULL != is) {
    if (NULL != is->free_func) {
      is->free_func(is->state);
    }
    free(is);
  }
}

/********************************************************************************
 *  Item List Functions
 ********************************************************************************/

ItemList * create_item_list() {
  ItemList * item_list = malloc(sizeof(ItemList));
  if (NULL != item_list) {
    item_list->size = 0;
    item_list->items = NULL;
  } else {
    fatal("Error allocation memory for ItemList");
  }
  
  return item_list;
}

int item_list_size(const ItemList *item_list) {
  return item_list->size;
}

Item * item_list_item(const ItemList *item_list, int item_id) {
  Item *item = NULL;
  PWord_t item_pointer;
  
  JLG(item_pointer, item_list->items, item_id);
  if (NULL != item_pointer) {
    item = (Item*)(*item_pointer);
  }  
  
  return item;
}

Item * item_list_item_at(const ItemList *item_list, int index) {
  return NULL;
}

void item_list_add_item(ItemList *item_list, const Item *item) {
  PWord_t item_pointer;
  int item_id = item_get_id(item);
  JLI(item_pointer, item_list->items, item_id);
  if (NULL != item_pointer) {
    *item_pointer = (Word_t) item;
    item_list->size++;
  } else {
    fatal("Error inserting item %i into Judy array", item_id);
  }
}

void free_item_list(ItemList *item_list) {
  
}
