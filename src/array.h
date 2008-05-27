/* Copyright (c) 2007 The Kaphan Foundation
 *
 * Possession of a copy of this file grants no permission or license
 * to use, modify, or create derivate works.
 *
 * Please contact info@peerworks.org for further information.
 */

#ifndef _ARRAY_H
#define	_ARRAY_H

typedef struct ARRAY {
  int size;
  int capacity;
  void ** elements;
} Array;

extern Array * create_array(int capacity);
extern int     arr_add(Array * array, void * element);
extern void    free_array(Array * array);

#endif	/* _ARRAY_H */

