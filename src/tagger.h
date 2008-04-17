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
#include "item_cache.h"

typedef enum TAGGER_STATE {
  TAGGER_LOADED,
  TAGGER_PARTIALLY_TRAINED,
  TAGGER_TRAINED,
  UNKNOWN,
  TAGGER_SEQUENCE_ERROR
} TaggerState;

typedef struct TAGGER {
  /***** Meta data for the tagger *****/
  
  /* The state of the tagger */
  TaggerState state;
  
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
  
  /** Number of positive examples that could not be found while training. */
  int missing_positive_example_count;
  
  /** Number of negative examples that could not be found while training. */
  int missing_negative_example_count;
  
  /** The ids of the positive examples that could not be found while training. */
  char ** missing_positive_examples;
  
  /** The ids of the negative examples that could not be found while training. */
  char ** missing_negative_examples;
  
  /**** Trained Pools ****/
  
  Pool *positive_pool;
  Pool *negative_pool;
  
  /* Hold on to the latest atom document, in case we need it? */
  char *atom;
} Tagger;

extern Tagger *    build_tagger(const char * atom);
extern TaggerState train_tagger(Tagger * tagger, ItemCache * item_cache);

#endif /* _TAGGER_H_ */
