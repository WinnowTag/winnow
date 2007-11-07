/* Copyright (c) 2007 The Kaphan Foundation
 *
 * Possession of a copy of this file grants no permission or license
 * to use, modify, or create derivate works.
 *
 * Please contact info@peerworks.org for further information.
 */

#include "item_source.h"

/******************************************************************************
 * ItemSource functions 
 ******************************************************************************/

/** Fetch an item from and ItemSource.
 *
 *  @param is The item source.
 *  @param item_id The id of the item.
 *  @returns The item or NULL.
 */
Item * is_fetch_item(const ItemSource *is, const int item_id) {
  return is->fetch_func(is->state, item_id);
}

int is_alive(ItemSource *is) {
  return is->alive_func(is->state); 
}

/** Free's an ItemSource
 */
void free_item_source(ItemSource *is) {
  free(is);
}