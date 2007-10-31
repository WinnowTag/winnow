// Copyright (c) 2007 The Kaphan Foundation
//
// Possession of a copy of this file grants no permission or license
// to use, modify, or create derivate works.
//
// Please contact info@peerworks.org for further information.

#include <stdlib.h>
#include <math.h>
#include "classifier.h"
#include "logging.h"
#include "pool.h"
#include "tag.h"

/** Functions for transitioning classifiers between various states */
const TrainedClassifier train(Tag tag, ItemSource is) {
  TrainedClassifier tc = malloc(sizeof(struct TRAINED_CLASSIFIER));
  
  if (NULL != tc) {
    tc->user = tag_user(tag);
    tc->tag_name = tag_tag_name(tag);
    tc->positive_pool = new_pool();
    tc->negative_pool = new_pool();
    
    if (tc->positive_pool == NULL || tc->negative_pool == NULL) {
      goto train_malloc_error;
    }
    
    int size = tag_positive_examples_size(tag);
    
    if (0 < size) {
      const int *examples = tag_positive_examples(tag);
    
      if (NULL != examples) {
        pool_add_items(tc->positive_pool, examples, size, is);
        free(examples);
      } else {
        goto train_malloc_error;
      }
    }
    
    size = tag_negative_examples_size(tag);
    
    if (0 < size) {
      const int *examples = tag_negative_examples(tag);
      
      if (NULL != examples) {
        pool_add_items(tc->negative_pool, examples, size, is);
        free(examples);
      } else {
        goto train_malloc_error;
      }      
    }    
  }
  
  return tc;
  train_malloc_error:
    error("Malloc error in train");
    tc_free(tc);
    return NULL;
}

/**** Trained classifier functions ****/
const Pool tc_get_positive_pool(TrainedClassifier tc) {
  return tc->positive_pool;
}

const Pool tc_get_negative_pool(TrainedClassifier tc) {
  return tc->negative_pool;
}

const char * tc_get_user(TrainedClassifier tc) {
  return tc->user;
}

const char * tc_get_tag_name(TrainedClassifier tc) {
  return tc->tag_name;
}

void tc_free(TrainedClassifier tc) {
  
}

/* Returns prob(chisq >= x2, with v degrees of freedom)
 *
 * Algorithm taken from http://spambayes.cvs.sourceforge.net/spambayes/spambayes/spambayes/chi2.py?view=markup
 */
double chi2Q(double x2, int v) {
  double chi2;
  
  if (v <= 0 || v % 2 != 0) {
    chi2 = -1.0;
  } else {
    int i;
    int max_i;
    double m;
    double sum;
    double term;
    
    m = x2 / 2;
    max_i = v / 2;
    sum = exp(-m);
    term = sum;

    for (i = 1; i <= max_i; i++) {
      term *= m / i;
      sum += term;
    }

    if (sum > 1.0) {
      sum = 1.0;
    }
    
    chi2 = sum;
  }
  
  return chi2;
}