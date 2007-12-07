/* Copyright (c) 2007 The Kaphan Foundation
 *
 * Possession of a copy of this file grants no permission or license
 * to use, modify, or create derivate works.
 *
 * Please contact info@peerworks.org for further information.
 */

#include <stdlib.h>
#include <stdio.h>

int create_file(const char * filename) {
  int failed = 0;
  FILE *file = fopen(filename, "w");
  if (NULL == file) {
    failed = 1;
  } else {
    fclose(file);
  }
  
  return failed;
}
