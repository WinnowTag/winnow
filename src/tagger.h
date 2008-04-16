/* Copyright (c) 2007 The Kaphan Foundation
 *
 * Possession of a copy of this file grants no permission or license
 * to use, modify, or create derivate works.
 *
 * Please contact info@peerworks.org for further information.
 */

#ifndef _TAGGER_H_
#define _TAGGER_H_

#include <time.h>

typedef struct TAGGER {
  char *tag_id;
  char *training_url;
  char *classifier_taggings_url;
  time_t updated;
  time_t last_classified;
  double bias;
} Tagger;

extern Tagger * build_tagger(const char * atom);

#endif /* _TAGGER_H_ */
