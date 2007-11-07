/* Copyright (c) 2007 The Kaphan Foundation
 *
 * Possession of a copy of this file grants no permission or license
 * to use, modify, or create derivate works.
 *
 * Please contact info@peerworks.org for further information.
 */

#ifndef _FILE_ITEM_SOURCE_H_
#define _FILE_ITEM_SOURCE_H_

#include "item.h"
#include "item_source.h"

extern ItemSource * create_file_item_source (const char * corpus_directory);
extern Item * create_item_from_file   (const void * state, const int item_id);

#endif /* _FILE_ITEM_SOURCE_H_ */
