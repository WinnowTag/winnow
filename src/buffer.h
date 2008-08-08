/* Copyright (c) 2007 The Kaphan Foundation
 *
 * Possession of a copy of this file grants no permission or license
 * to use, modify, or create derivate works.
 *
 * Please contact info@peerworks.org for further information.
 */

#ifndef _BUFFER_H
#define	_BUFFER_H

#ifdef	__cplusplus
extern "C" {
#endif

typedef struct BUFFER {
  char *buf;
  int capacity;
  int length;
} Buffer;

extern Buffer * new_buffer(int size);
extern void buffer_in(Buffer *b, const char * data, int in_size);
        
#ifdef	__cplusplus
}
#endif

#endif	/* _BUFFER_H */

