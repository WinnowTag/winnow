/* Copyright (c) 2007 The Kaphan Foundation
 *
 * Possession of a copy of this file grants no permission or license
 * to use, modify, or create derivate works.
 *
 * Please contact info@peerworks.org for further information.
 */

#ifndef _ITEM_SOURCE_H_
#define _ITEM_SOURCE_H_
#include "item.h"
/** An item source stores a reference to an item loading function
 *  and some state to pass as the first argument to the item 
 *  loading function.
 */
typedef struct ITEMSOURCE {
  Item* (*fetch_func)(const void*, const int item_id);
  const void* fetch_func_state;
} ItemSource;

/*** Item Source ****/
extern Item *       is_fetch_item           (const ItemSource *is, const int item_id);
extern void         free_item_source         (ItemSource *is);


#endif /* _ITEM_SOURCE_H_ */
