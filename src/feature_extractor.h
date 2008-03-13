/* Copyright (c) 2007 The Kaphan Foundation
 *
 * Possession of a copy of this file grants no permission or license
 * to use, modify, or create derivate works.
 *
 * Please contact info@peerworks.org for further information.
 */

#ifndef _FEATURE_EXTRACTOR_H_
#define _FEATURE_EXTRACTOR_H_
#include "item_cache.h"

Item * tokenize_entry(ItemCache * item_cache, const ItemCacheEntry *entry, void *memo); 

#endif /* _FEATURE_EXTRACTOR_H_ */
