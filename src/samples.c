// Copyright (c) 2007 The Kaphan Foundation
//
// Possession of a copy of this file grants no permission or license
// to use, modify, or create derivate works.
//
// Please contact info@peerworks.org for further information.

#include "samples.h"
#include "stdio.h"
#include "logging.h"

Samples load_samples(const ItemSource is, const char *filename) {
  Samples samples = malloc(sizeof(struct SAMPLES));
  if (NULL != samples) {
    FILE *file = fopen(filename, "r");
    if (NULL != file) {
      Pvoid_t item_list = NULL;
      Word_t item_id;
      Word_t rc_word;
      
      while (EOF != fscanf(file, "%d\n", &item_id)) {
        J1S(rc_word, item_list, item_id);
      }
      
      J1C(samples->size, item_list, 0, -1);
      
      samples->items = malloc(sizeof(Item) * samples->size);
      int i = 0;
      item_id = 0;
      J1F(rc_word, item_list, item_id);
      while (rc_word) {
        samples->items[i++] = is_fetch_item(is, item_id);
        J1N(rc_word, item_list, item_id);
      }
      
      J1FA(rc_word, item_list);
    }
  }
  return samples;
}

void free_samples(Samples samples) {
  free(samples->items);
  free(samples);
}
