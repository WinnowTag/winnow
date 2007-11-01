// Copyright (c) 2007 The Kaphan Foundation
//
// Possession of a copy of this file grants no permission or license
// to use, modify, or create derivate works.
//
// Please contact info@peerworks.org for further information.

#include "random_background.h"
#include "misc.h"
#include "errno.h"
#include <stdio.h>

Pool create_random_background_from_file (const ItemSource is, const char * filename) {
  Pool pool = new_pool();  
  FILE *file;

  file = fopen(filename, "r");
  if (NULL != file) {
    int item_id;
    while (EOF != fscanf(file, "%d\n", &item_id)) {
      Item item = is_fetch_item(is, item_id);
      if (NULL != item) {
	      pool_add_item(pool, item);
      }
    }
  } else {
    error("Could not open %s: %s", filename, strerror(errno));
    free_pool(pool);
    pool = NULL;
  }
  
  return pool;
}
