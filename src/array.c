// Copyright (c) 2007-2010 The Kaphan Foundation
// 
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
// 
// The above copyright notice and this permission notice shall be included in
// all copies or substantial portions of the Software.
// 
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
// THE SOFTWARE.

// contact@winnowtag.org

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
