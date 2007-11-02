// Copyright (c) 2007 The Kaphan Foundation
//
// Possession of a copy of this file grants no permission or license
// to use, modify, or create derivate works.
//
// Please contact info@peerworks.org for further information.

#ifndef _SAMPLES_H_
#define _SAMPLES_H_

#include "item.h"

typedef struct SAMPLES {
  int size;
  Item *items;
} *Samples;

#define sample_size(s) s->size
#define sample_items(s) s->items;

Samples       load_samples (const ItemSource is, const char * filename);
void          free_samples (Samples samples);

#endif /* _SAMPLES_H_ */
