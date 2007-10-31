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
#include "misc.h"

#define UNKNOWN_WORD_STRENGTH 0.45
#define UNKNOWN_WORD_PROB     0.5
#define S_TIMES_X   UNKNOWN_WORD_PROB * UNKNOWN_WORD_STRENGTH

static void compute_ratios(const ProbToken token_prob[], int size, double *ratios);
static double filtered_average(double arr[], int size);
static double compute_n(const ProbToken foregrounds[], int n_fg, 
                 const ProbToken backgrounds[], int n_bg, 
                 double fg_total_tokens, double bg_total_tokens);

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
      int *examples = tag_positive_examples(tag);
    
      if (NULL != examples) {
        pool_add_items(tc->positive_pool, examples, size, is);
        free(examples);
      } else {
        goto train_malloc_error;
      }
    }
    
    size = tag_negative_examples_size(tag);
    
    if (0 < size) {
      int *examples = tag_negative_examples(tag);
      
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

const Classifier precompute(const TrainedClassifier tc, const Pool background) {
  Classifier classifier = malloc(sizeof(struct CLASSIFIER));
  if (NULL != classifier) {
    classifier->user = tc_get_user(tc);
    classifier->tag_name = tc_get_tag_name(tc);
  }
  
  return classifier;
}


float probability(const ProbToken foregrounds[], int n_fg,
                  const ProbToken backgrounds[], int n_bg,
                  int fg_total_tokens, int bg_total_tokens) {
  
  double probability = UNKNOWN_WORD_PROB;
  if (fg_total_tokens > 0 || bg_total_tokens > 0) {
    // Protect from divide by zero 
    fg_total_tokens = MAX(1, fg_total_tokens);
    bg_total_tokens = MAX(1, bg_total_tokens);
  
    // This is gcc only stuff
    double fg_ratios[n_fg];
    double bg_ratios[n_bg];
  
    compute_ratios(foregrounds, n_fg, fg_ratios);
    compute_ratios(backgrounds, n_bg, bg_ratios);  
    double fg_ratio = filtered_average(fg_ratios, n_fg);
    double bg_ratio = filtered_average(bg_ratios, n_bg);  
    double ratio = fg_ratio / (fg_ratio + bg_ratio);  
    double n = compute_n(foregrounds, n_fg, backgrounds, n_bg, fg_total_tokens, bg_total_tokens);
    probability = (S_TIMES_X + n * ratio) / (UNKNOWN_WORD_STRENGTH + n);
  }
  
  return probability;
}

/*** "Private" functions ***/

void compute_ratios(const ProbToken token_prob[], int size, double *ratios) {
  int i;
  for (i = 0; i < size; i++) {
    if (token_prob[i]->pool_size > 0) {
      ratios[i] = (double) token_prob[i]->token_count / token_prob[i]->pool_size;
    } else {
      ratios[i] = 0;
    }
  }
}

/* N provides a measure of confidence in a probability for a token.
 * It is used in the Bayesian Adjustment.  For more info on what is
 * does and how we got to the current calculation for it see the 
 * class level comments.
 *
 * With the move to arbitrary pools, n is calculated slightly differently
 * but with the same aim and results. It now takes an array of token counts
 * and an array total counts for number of pools. The ith element in the
 * token count array correponds to the ith element in the total count array.
 *
 * N is computed by summing each token count multiplied by the size of all the 
 * other pools, divided by the size of the pool the token count came from.
 */
double compute_n(const ProbToken foregrounds[], int n_fg, 
                 const ProbToken backgrounds[], int n_bg, 
                 double fg_total_tokens, double bg_total_tokens) {
  int i;
  double fg_ns[n_fg];
  double bg_ns[n_bg];
  
  // This formula is more cleary expressed as:
  //  
  //  n +=  token count * sum of total tokens for all opposite pools 
  //                      -------------------------------------------
  //                             the pool_size for this
  //
  for (i = 0; i < n_fg; i++) {
    if (foregrounds[i]->pool_size > 0) {
      fg_ns[i] = foregrounds[i]->token_count * bg_total_tokens / foregrounds[i]->pool_size;
    } else {
      fg_ns[i] = 0;
    }
  }
  
  for (i = 0; i <n_bg; i++) {
    if (backgrounds[i]->pool_size > 0) {
      bg_ns[i] = backgrounds[i]->token_count * fg_total_tokens / backgrounds[i]->pool_size;
    } else {
      bg_ns[i] = 0;
    }
  }
  
  return filtered_average(fg_ns, n_fg) + filtered_average(bg_ns, n_bg);
}

double filtered_average(double arr[], int size) {             
  double sum = 0;
  int i, denominator = 0;
  
  for (i = 0; i < size; i++) {
    if (arr[i] > 0) {
      sum += arr[i];
      denominator++;
    }
  }
  
  if (denominator == 0) {
    return 0;
  } else {
    return sum / denominator;            
  }
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
  free_pool(tc->positive_pool);
  free_pool(tc->negative_pool);
  free(tc);
}

/***** Classifier Functions *****/
const char * cls_tag_name(Classifier cls) {
  return cls->tag_name;
}

const char * cls_user(Classifier cls) {
  return cls->user;
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

