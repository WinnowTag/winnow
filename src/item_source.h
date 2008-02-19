/* Copyright (c) 2007 The Kaphan Foundation
 *
 * Possession of a copy of this file grants no permission or license
 * to use, modify, or create derivate works.
 *
 * Please contact info@peerworks.org for further information.
 */

#ifndef _ITEM_SOURCE_H_
#define _ITEM_SOURCE_H_
#include <Judy.h>
#include "item.h"

typedef struct ITEM_LIST {
  int size;
  /* Stores items by item id */
  Pvoid_t items;
  /* Stores items in order of insertion */
  Pvoid_t ordered_items;
} ItemList;

/** An item source stores a reference to an item loading function
 *  and some state to pass as the first argument to the item 
 *  loading function.
 * 
 *  An ItemSource is different from an item cache since it is specifically
 *  used for fetching items for the purpose of classification.
 */
typedef struct ITEMSOURCE {
  Item* (*fetch_func)(const void*, const int item_id);
  ItemList* (*fetch_all_func)(const void*);
  int   (*flush_func)(const void*);
  int   (*alive_func)(const void*);
  void  (*free_func)(void*);
  void  (*close_func)(void*);
  void* state;
} ItemSource;

/*** Item Source ****/
extern Item *       is_fetch_item           (const ItemSource *is, const int item_id);
extern ItemList *   is_fetch_all_items      (const ItemSource *is);
extern int          is_flush                (const ItemSource *is);
extern void         free_item_source        (ItemSource *is);
extern int          is_alive                (const ItemSource *is);
extern void         is_close                (const ItemSource *is);

/**** ItemList ****/
extern ItemList  *  create_item_list        ();
extern int          item_list_size          (const ItemList *item_list);
extern Item      *  item_list_item_at       (const ItemList *item_list, int index);
extern Item      *  item_list_item          (const ItemList *item_list, int item_id);
extern void         item_list_add_item      (ItemList *item_list, const Item *item);
extern void         free_item_list          (ItemList *item_list);

extern ItemSource * create_item_source      (void);
extern ItemSource * create_caching_item_source (ItemSource * is);
#endif /* _ITEM_SOURCE_H_ */
