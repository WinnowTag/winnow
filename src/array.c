/* Copyright (c) 2007 The Kaphan Foundation
 *
 * Possession of a copy of this file grants no permission or license
 * to use, modify, or create derivate works.
 *
 * Please contact info@peerworks.org for further information.
 */

#include <stdlib.h>
#include "array.h"
#include "logging.h"


Array * create_array(int capacity) {
  Array * array = calloc(1, sizeof(struct ARRAY));
  
  if (array) {
    array->size = 0;
    array->capacity = capacity;
    array->elements = calloc(capacity, sizeof(void*));
            
    if (!array->elements) {
      free(array);
      array = NULL;
      fatal("Could not malloc array->elements");
    }
  } else {
    fatal("Could not allocate memory for the array");
  }
  
  return array;
}

int arr_add(Array * array, void * e) {
  int rc = 0;
  
  if (array && e) {
    if (array->size >= array->capacity) {
      int new_capacity = array->capacity * 2 + 1;
      void **new_elements = realloc(array->elements, new_capacity * sizeof(void *));
      
      if (!new_elements) {
        fatal("Could not increase size of array->element");
        rc = 1;
      } else {
        array->elements = new_elements;
        array->capacity = new_capacity;
      }
    }
    
    if (rc == 0) {
      array->elements[array->size] = e;
      array->size++;
    }
  }
  
  return rc;
}

void free_array(Array *array) {
  if (array) {
    int i;
    
    for (i = 0; i < array->size; i++) {
      free(array->elements[i]);
    }
    
    free(array->elements);
    free(array);    
  }
}
