/* Copyright (c) 2007 The Kaphan Foundation
 *
 * Possession of a copy of this file grants no permission or license
 * to use, modify, or create derivate works.
 *
 * Please contact info@peerworks.org for further information.
 */
 
#include <time.h>
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
      is->state = NULL;
    }
    free(is);
  }
}

/********************************************************************************
 *  Caching ItemSource Functions
 ********************************************************************************/
static Item * cis_fetch_item(const void *state, const int item_id);
static ItemList * cis_fetch_all_items(const void *state);
static void cis_free(void *state);
  

typedef struct CACHING_ITEM_SOURCE_STATE {
  ItemSource *is;
  ItemList *items;
  time_t    loaded_at;
} CachingItemSourceState;

ItemSource * create_caching_item_source(ItemSource * is) {
  ItemSource *cis = create_item_source();
  if (cis) {
    CachingItemSourceState *state = malloc(sizeof(CachingItemSourceState));
    if (!state) {
      free_item_source(cis);
      cis = NULL;
      fatal("Error malloc'ing CachingItemSourceState");
    }
    
    state->is = is;
    state->items = is_fetch_all_items(is);
    state->loaded_at = time(0);
    info("Loaded %i items into cached item state at %s", item_list_size(state->items), ctime(&(state->loaded_at)));
    
    cis->fetch_func = cis_fetch_item;
    cis->fetch_all_func = cis_fetch_all_items;
    cis->free_func = cis_free;
    cis->state = state;
  }
  return cis;
}

Item * cis_fetch_item(const void *state_vp, const int item_id) {
  Item *item = NULL;
  CachingItemSourceState *state = (CachingItemSourceState*) state_vp;
  if (state) {
    item = item_list_item(state->items, item_id);
  }
  return item;
}

ItemList * cis_fetch_all_items(const void *state_vp) {
  ItemList *items = NULL;
  CachingItemSourceState *state = (CachingItemSourceState*) state_vp;
  if (state) {
    items = state->items;
  }
  return items;
}

void cis_free(void * state_vp) {
  CachingItemSourceState *state = (CachingItemSourceState*) state_vp;
  if (state) {
    if (state->items) {
      free_item_list(state->items);
      state->items = NULL;
    }
    
    if (state->is) {
      free_item_source(state->is);
      state->is = NULL;
    }
    
    free(state);
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
  
  if (item_list) {
    JLG(item_pointer, item_list->items, item_id);
    if (NULL != item_pointer) {
      item = (Item*)(*item_pointer);
    }  
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
