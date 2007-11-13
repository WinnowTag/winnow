// Copyright (c) 2007 The Kaphan Foundation
//
// Possession of a copy of this file grants no permission or license
// to use, modify, or create derivate works.
//
// Please contact info@peerworks.org for further information.

#ifndef CLASSIFIER_H
#define CLASSIFIER_H
#include "pool.h"
#include "tag.h"
#include "item.h"
#include "clue.h"
#include "item_source.h"
#include "tagging.h"
#include <Judy.h>

// Defines the probability assigned to unknown words.
//
// This was inspired by the SpamBayes option of the same name.
#define UNKNOWN_WORD_PROB 0.5

// Defines the weighting to give to the unknown word probability.
//
// This was inspired by the SpamBayes option of the same name.
#define UNKNOWN_WORD_STRENGTH 0.45
#define S_TIMES_X (UNKNOWN_WORD_STRENGTH * UNKNOWN_WORD_PROB)

// The minimum probability strength to use when building a probability cache for a Pool. 
// The probabilities that are less that min_prob_strength distance from 0.5 are ignored. 
// 
// This defaults to 0.1 
// 
// This was added as part of the distance thresholding change described in ticket 257. 
#define MIN_PROB_STRENGTH 0.1 

// The maximum number of discriminators to use when building a probability cache for a Pool. 
// The top max_discriminators probabilities ordered by distances from 0.5 are used for each pool. 
// 
// The default is 150. 
// 
// This was added as part of the distance thresholding change described in ticket 257. 
#define MAX_DISCRIMINATORS 150

// This is the ratio of clues in an items to use as the maximum clues.
//
// We get the max clues value by taking
//   
//   max(MAX_DISCRIMINATORS, MAX_CLUES_RATIO * item_length)
//
// This prevent large items from not having enough clues to have a balanced
// probability calculation which was creating tag magnets.
// 
#define MAX_CLUES_RATIO 0.5

typedef struct TRAINED_CLASSIFIER {
  const char * user;
  const char * tag_name;
  int user_id;
  int tag_id;
  Pool *positive_pool;
  Pool *negative_pool;
} TrainedClassifier;
  
typedef struct CLASSIFIER {
  const char * user;
  const char * tag_name;
  int user_id;
  int tag_id;
  Pvoid_t clues;
} Classifier;

typedef struct PROB_TOKEN {
  int token_count;
  int pool_size;
} ProbToken;



TrainedClassifier *  train       (const Tag *tag, const ItemSource *is);
Classifier        *  precompute  (const TrainedClassifier*, const Pool * random_background);
Tagging           *  classify    (const Classifier *classifier, const Item *item);
double                     chi2Q       (double x, int v);

/** Only in header for testing - shouldn't actual use it */
const Clue **              select_clues(const Classifier*, const Item*, int *num_clues);
double                     probability (const ProbToken *foreground[], int n_pos,
                                        const ProbToken *background[], int n_neg,
                                        int fg_size, int bg_size);

/**** Functions for handling TrainedClassifiers  ****/
const char *  tc_get_tag_name       (const TrainedClassifier *tc);
const char *  tc_get_user           (const TrainedClassifier *tc);
int           tc_get_tag_id         (const TrainedClassifier *tc);
int           tc_get_user_id        (const TrainedClassifier *tc);
const Pool *  tc_get_positive_pool  (const TrainedClassifier *tc);
const Pool *  tc_get_negative_pool  (const TrainedClassifier *tc);
void          tc_free               (TrainedClassifier *tc);

/**** Functions for handling Classifiers ****/
int           cls_num_clues         (const Classifier *c);
double        cls_probability_for   (const Classifier *c, int token_id);
const Clue *  cls_clue_for          (const Classifier *c, int token_id);
const char *  cls_tag_name          (const Classifier *c);
const char *  cls_user              (const Classifier *c);
int           cls_user_id           (const Classifier *c);
int           cls_tag_id            (const Classifier *c);
void          free_classifier       (Classifier *c);

#endif
