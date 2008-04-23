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
#include "clue.h"

typedef enum TAGGER_STATE {
  TAGGER_LOADED,
  TAGGER_PARTIALLY_TRAINED,
  TAGGER_TRAINED,
  TAGGER_PRECOMPUTED,
  UNKNOWN,
  TAGGER_SEQUENCE_ERROR
} TaggerState;

#define TAGGER_OK 0
#define TAG_OK 0
#define TAG_NOT_FOUND 1
#define TAG_NOT_MODIFIED 2
#define TAGGER_PENDING_ITEM_ADDITION 3
#define TAGGER_CHECKED_OUT 4

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
  
  /* The function that is used for calculating the probability for a token. */
  double (*probability_function)(const Pool *positive, const Pool *negative, const Pool *random_bg, int token_id, double bias);
  
  /* The function that is used to classify a item */
  double (*classification_function)(const ClueList *clues, const Item *item);
  
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
  
  /**** Precomputed classifier state ****/
  ClueList *clues;
  
  /* Hold on to the latest atom document, in case we need it? */
  char *atom;
} Tagger;

typedef struct TAGGER_CACHE_OPTIONS {
  
} TaggerCacheOptions;

typedef struct TAGGER_CACHE {
  ItemCache *item_cache;
  Pool * random_background;
  /* Function used to fetch tag documents. This is really just a function pointer to help testing. */
  int (*tag_retriever)(const char * tag_training_url, time_t last_updated, char ** tag_document, char ** errmsg);
} TaggerCache;

extern Tagger *      build_tagger        (const char * atom);
extern TaggerState   train_tagger        (Tagger * tagger, ItemCache * item_cache);
extern TaggerState   precompute_tagger   (Tagger * tagger, const Pool * random_background);
extern int           classify_item       (const Tagger * tagger, const Item * item, double * probability);
extern int           get_missing_entries (Tagger * tagger, ItemCacheEntry ** entries);

extern TaggerCache * create_tagger_cache (ItemCache * item_cache, TaggerCacheOptions * options);
extern void          free_tagger_cache   (TaggerCache * tagger_cache);
extern int           get_tagger          (TaggerCache * tagger_cache, const char * tag_training_url, Tagger ** tagger, char ** errmsg);
extern void          release_tagger      (TaggerCache * tagger_cache, Tagger * tagger);

#endif /* _TAGGER_H_ */
