// Copyright (c) 2007 The Kaphan Foundation
//
// Possession of a copy of this file grants no permission or license
// to use, modify, or create derivate works.
//
// Please contact info@peerworks.org for further information.

#ifndef CLASSIFIER
#define CLASSIFIER_H
#include "pool.h"
#include "tag.h"
#include "item.h"

typedef struct TRAINED_CLASSIFIER {
  const char * user;
  const char * tag_name;
  Pool positive_pool;
  Pool negative_pool;
} *TrainedClassifier;
  
typedef struct CLASSIFIER {
  const char * user;
  const char * tag_name;
} *Classifier;

typedef struct PROB_TOKEN {
  int token_count;
  int pool_size;
} *ProbToken;

const TrainedClassifier    train       (const Tag tag, const ItemSource is);
const Classifier           precompute  (const TrainedClassifier, const Pool random_background);
double                     chi2Q       (double x, int v);
float                      probability (const ProbToken foreground[], int n_pos,
                                        const ProbToken background[], int n_neg,
                                        int fg_size, int bg_size);

/**** Functions for handling TrainedClassifiers  ****/
const char *  tc_get_tag_name       (TrainedClassifier tc);
const char *  tc_get_user           (TrainedClassifier tc);
const Pool    tc_get_positive_pool  (TrainedClassifier tc);
const Pool    tc_get_negative_pool  (TrainedClassifier tc);
void          tc_free               (TrainedClassifier tc);

/**** Functions for handling Classifiers ****/
const char * cls_tag_name            (Classifier c);
const char * cls_user                (Classifier c);
#endif