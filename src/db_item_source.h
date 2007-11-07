/* Copyright (c) 2007 The Kaphan Foundation
 *
 * Possession of a copy of this file grants no permission or license
 * to use, modify, or create derivate works.
 *
 * Please contact info@peerworks.org for further information.
 */

#ifndef _DB_ITEM_SOURCE_H_
#define _DB_ITEM_SOURCE_H_

#include "cls_config.h"
#include "item_source.h"

extern ItemSource * create_db_item_source (DBConfig *db_config);
extern void         free_db_item_source   (void *state);

#endif /* _DB_ITEM_SOURCE_H_ */
