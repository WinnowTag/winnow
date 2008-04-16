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
  /***** Meta data for the tagger *****/
  
  /* The ID of the tag */
  char *tag_id;
  
  /* The URL to fetch training information from */
  char *training_url;
  
  /* The URL to send classifier taggings to */
  char *classifier_taggings_url;
  
  /* The time the user last updated the tag */
  time_t updated;
  
  /* The time the tag was last classified */
  time_t last_classified;
  
  /* The bias of the tag. */
  double bias;
  
  /**** Tag examples *****/
  
  /* The number of positive examples */
  int positive_example_count;
  
  /* The number of negative examples */
  int negative_example_count;
  
  /* The item ids for the positive examples */
  char ** positive_examples;
  
  /* The item ids for the negative examples */
  char ** negative_examples;
  
  /* Hold on to the latest atom document, in case we need it? */
  char *atom;
} Tagger;

extern Tagger * build_tagger(const char * atom);

#endif /* _TAGGER_H_ */
