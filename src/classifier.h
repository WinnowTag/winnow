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
  
const TrainedClassifier    train   (const Tag tag, const ItemSource is);
double                     chi2Q   (double x, int v);

/**** Functions for handling TrainedClassifiers  ****/
const char *  tc_get_tag_name       (TrainedClassifier tc);
const char *  tc_get_user           (TrainedClassifier tc);
const Pool    tc_get_positive_pool  (TrainedClassifier tc);
const Pool    tc_get_negative_pool  (TrainedClassifier tc);
void          tc_free               (TrainedClassifier tc);
#endif