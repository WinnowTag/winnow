// General info: http://doc.winnowtag.org/open-source
// Source code repository: http://github.com/winnowtag
// Questions and feedback: contact@winnowtag.org
//
// Copyright (c) 2007-2011 The Kaphan Foundation
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

#ifndef _TAGGER_H_
#define _TAGGER_H_

#include <time.h>
#include <pthread.h>
#include <Judy.h>
#include "item_cache.h"
#include "clue.h"
#include "array.h"
#include "hmac_credentials.h"

typedef enum TAGGER_STATE {
  TAGGER_LOADED,
  TAGGER_TRAINED,
  TAGGER_PRECOMPUTED,
  UNKNOWN,
  TAGGER_SEQUENCE_ERROR
} TaggerState;

#define TAG_INDEX_OK 0
#define TAG_INDEX_FAIL 1
#define TAGGER_OK 0
#define TAG_OK 0
#define TAG_NOT_FOUND 1
#define TAG_NOT_MODIFIED 2
#define TAGGER_CHECKED_OUT 4

#define ATOM "http://www.w3.org/2005/Atom"
#define CLASSIFIER "http://peerworks.org/classifier"

typedef struct TAGGING {
    /* Id of the tagged item */
    const unsigned char *item_id;
    /* Strength of the tagging */
    double strength;
} Tagging;

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
  
  /* The atom:category 'term' for the tagger */
  char * term;
  
  /* The atom:category 'scheme' for the tagger */
  char * scheme;
  
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
  
  Clue ** (*get_clues_function)(const ClueList *clues, const Item *item, int *num);
  
  /**** Tag examples *****/
  
  /* The number of positive examples */
  int positive_example_count;
  
  /* The number of negative examples */
  int negative_example_count;
  
  /* The item ids for the positive examples */
  char ** positive_examples;
  
  /* The item ids for the negative examples */
  char ** negative_examples;
  
  /**** Trained Pools ****/
  
  Pool *positive_pool;
  Pool *negative_pool;
  
  /**** Precomputed classifier state ****/
  ClueList *clues;
  
  /* Hold on to the latest atom document, in case we need it? */
  char *atom;
} Tagger;

typedef struct TAGGER_CACHE_OPTIONS {
  /* URL for the index of tags which will be handled by the classifier. */
  const char * tag_index_url;
  const Credentials * credentials;
} TaggerCacheOptions;

typedef int (*TagRetriever)(const char * tag_training_url, time_t last_updated, 
                            const Credentials * credentials, 
                            char ** tag_document, char ** errmsg);

typedef struct TAGGER_CACHE {
  /* URL for the index of tags which will be handled by the classifier. */
  const char * tag_index_url;
  const Credentials * credentials;
  
  /* Tagger cache mutex.  Only one thread can access the internal arrays of the tagger cache at one time. */
  pthread_mutex_t mutex;
  
  /* The item cache to get items for training taggers */
  ItemCache *item_cache;
    
  /* Function used to fetch tag documents. This is really just a function pointer to help testing. */
  TagRetriever tag_retriever;
  
  /* Function used to fetch tag index documents. This is really just a function pointer to help testing. */
  int (*tag_index_retriever)(const char * tag_index_url, time_t last_updated, 
                            const Credentials * credentials, 
                            char ** tag_document, char ** errmsg);
    
  /* Array of tag urls fetched from the tag index */
  Array *tag_urls;
  
  /* Time the tag urls were last updated */
  time_t tag_urls_last_updated;
  
  /* Array of taggers that are checked out.  A checked out tagger cannot be 'gotten' by anyone else. */
  Pvoid_t checked_out_taggers;
  
  /* Array of taggers indexed by training url. */
  Pvoid_t taggers;
  
  /* Array of tagger ids that could not be fetched */
  Pvoid_t failed_tags;
} TaggerCache;

extern Tagging *     create_tagging      (const char * item_id, double strength);

extern Tagger *      build_tagger        (const char * atom, ItemCache * item_cache);
extern TaggerState   train_tagger        (Tagger * tagger, ItemCache * item_cache);
extern TaggerState   precompute_tagger   (Tagger * tagger, const Pool * random_background);
extern TaggerState   prepare_tagger      (Tagger * tagger, ItemCache * item_cache);
extern int           classify_item       (const Tagger * tagger, const Item * item, double * probability);
extern Clue **       get_clues           (const Tagger * tagger, const Item * item, int * num);
extern int           update_taggings     (const Tagger * tagger, Array *list, const Credentials * credentials, char ** errmsg);
extern int           replace_taggings    (const Tagger * tagger, Array *list, const Credentials * credentials, char ** errmsg);
extern int           get_missing_entries (Tagger * tagger, ItemCacheEntry ** entries);
extern void          free_tagger         (Tagger * tagger);

extern TaggerCache * create_tagger_cache (ItemCache * item_cache, TaggerCacheOptions * options);
extern void          free_tagger_cache   (TaggerCache * tagger_cache);
extern int           get_tagger          (TaggerCache * tagger_cache, const char * tag_training_url, Tagger ** tagger, char ** errmsg);
extern int           get_tagger_without_fetching (TaggerCache *tagger_cache, const char * tag_training_url, Tagger ** tagger, char ** errmsg);
extern int           release_tagger      (TaggerCache * tagger_cache, Tagger * tagger);
extern int           fetch_tags          (TaggerCache * tagger_cache, Array **a, char ** errmsg);
extern int           is_cached           (TaggerCache * tagger_cache, const char * tag_training_url);
extern int           is_failed_tag            (TaggerCache * tagger_cache, const char * tag_training_url);
extern int           clear_error         (TaggerCache * tagger_cache, const char * tag_training_url);
extern int           fetch_tagger_in_background(TaggerCache *cache, const char * tag);

/* Only in the header for testing. */
extern int           parse_tag_index     (const char * document, Array * a, time_t * updated);
#endif /* _TAGGER_H_ */
