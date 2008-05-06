/* Copyright (c) 2007 The Kaphan Foundation
 *
 * Possession of a copy of this file grants no permission or license
 * to use, modify, or create derivate works.
 *
 * Please contact info@peerworks.org for further information.
 */

#include "tagger.h"
#include "logging.h"

Tagging * create_tagging(const char * item_id, double strength) {
  Tagging *tagging = malloc(sizeof (struct TAGGING));

  if (tagging) {
    tagging->item_id = item_id;
    tagging->strength = strength;
  } else {
    fatal("Could not malloc Tagging");
  }

  return tagging;
}

TaggingList * create_tagging_list(int capacity) {
  TaggingList *list = malloc(sizeof(TaggingList));
  
  if (list) {
    list->size = 0;
    list->capacity = capacity;
    list->taggings = calloc(capacity, sizeof(Tagging *));

    if (!list->taggings) {
      free(list);
      list = NULL;
      fatal("Could not malloc TaggingList->taggings");
    }
  } else {
    fatal("Could not malloc TaggingList");
  }
  
  return list;
}

int add_to_tagging_list(TaggingList *list, Tagging * tagging) {
  int rc = 0;
  
  if (list && tagging) {
    if (list->size >= list->capacity) {
      int new_capacity = list->capacity * 2 + 1;
      Tagging **new_taggings = realloc(list->taggings, new_capacity * sizeof(Tagging *));
      
      if (!new_taggings) {
        fatal("Could not increase size of TaggingList->taggings");
        rc = 1;
      } else {
        list->taggings = new_taggings;
        list->capacity = new_capacity;
      }
    }
    
    if (rc == 0) {
      list->taggings[list->size] = tagging;
      list->size++;
    }
  }
  
  return rc;
}

void free_tagging_list(TaggingList *list) {
  if (list) {
    int i;
    
    for (i = 0; i < list->size; i++) {
      free(list->taggings[i]);
    }
    
    free(list->taggings);
    free(list);    
  }
}

