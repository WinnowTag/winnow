/* Copyright (c) 2007 The Kaphan Foundation
 *
 * Possession of a copy of this file grants no permission or license
 * to use, modify, or create derivate works.
 *
 * Please contact info@peerworks.org for further information.
 */


#ifndef TOKENIZER_H_
#define TOKENIZER_H_
#include "array.h"

typedef struct FEATURE {
	char *name;
	int frequency;
} Feature;

Array * html_tokenize(const char * html);
void free_feature(Feature *);

#endif /* TOKENIZER_H_ */
