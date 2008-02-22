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
#include "clue.h"

#define TINY_VAL_D 1e-200

/** Some  function prototypes */
static void compute_ratios(const ProbToken *token_prob[], int size, double *ratios);
static double filtered_average(double arr[], int size);
static double compute_n(const ProbToken *foregrounds[], int n_fg, 
                 const ProbToken *backgrounds[], int n_bg, 
                 double fg_total_tokens, double bg_total_tokens);
static double chi2_combine(const Clue **clues, int num_clues);

/** Functions for transitioning classifiers between various states */
TrainedClassifier * train(const Tag *tag, const ItemCache *item_cache) {
  TrainedClassifier *tc = malloc(sizeof(struct TRAINED_CLASSIFIER));
  
  if (NULL != tc) {
    tc->user_id = tag_user_id(tag);
    tc->user = tag_user(tag);
    tc->tag_id = tag_tag_id(tag);
    tc->tag_name = tag_tag_name(tag);
    tc->bias = tag_bias(tag);
    tc->positive_pool = new_pool();
    tc->negative_pool = new_pool();
    
    if (tc->positive_pool == NULL || tc->negative_pool == NULL) {
      goto train_malloc_error;
    }
    
    int size = tag_positive_examples_size(tag);
    
    if (0 < size) {
      int *examples = tag_positive_examples(tag);
    
      if (NULL != examples) {
        pool_add_items(tc->positive_pool, examples, size, item_cache);
        free(examples);
      } else {
        goto train_malloc_error;
      }
    }
    
    size = tag_negative_examples_size(tag);
    
    if (0 < size) {
      int *examples = tag_negative_examples(tag);
      
      if (NULL != examples) {
        pool_add_items(tc->negative_pool, examples, size, item_cache);
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

Classifier * precompute(const TrainedClassifier *tc, const Pool *background) {
  Classifier *classifier = malloc(sizeof(struct CLASSIFIER));
  if (NULL != classifier) {
    classifier->user = tc_get_user(tc);
    classifier->tag_name = tc_get_tag_name(tc);
    classifier->tag_id = tc_get_tag_id(tc);
    classifier->user_id = tc_get_user_id(tc);
    classifier->bias = tc_get_bias(tc);
    classifier->clues = NULL;
    
    struct PROB_TOKEN positive_token, negative_token, background_token;
    const Pool *positive_pool = tc_get_positive_pool(tc);
    const Pool *negative_pool = tc_get_negative_pool(tc);
    const ProbToken *foregrounds[] = {&positive_token};
    const ProbToken *backgrounds[] = {&negative_token, &background_token};
    positive_token.pool_size = pool_total_tokens(positive_pool) / classifier->bias;
    negative_token.pool_size = pool_total_tokens(negative_pool) * classifier->bias;
    background_token.pool_size = pool_total_tokens(background)  * classifier->bias;
    const int fg_total_tokens = positive_token.pool_size;
    const int bg_total_tokens = negative_token.pool_size + background_token.pool_size;
    Token working_token;
    working_token.id = 0;
    
    // Start with the background as it is probably the biggest
    while (pool_next_token(background, &working_token)) {
      PWord_t clue_p;
      background_token.token_count = working_token.frequency;
      positive_token.token_count = pool_token_frequency(positive_pool, working_token.id);
      negative_token.token_count = pool_token_frequency(negative_pool, working_token.id);
      double prob = probability(foregrounds, 1, backgrounds, 2, fg_total_tokens, bg_total_tokens);
      const Clue *clue = new_clue(working_token.id, prob);

      JLI(clue_p, classifier->clues, working_token.id);
      if (NULL == clue_p) goto classifier_malloc_error;
      *clue_p = (Word_t) clue;
    }
    
    // Now do the foreground
    working_token.id = 0;
    while (pool_next_token(positive_pool, &working_token)) {
      PWord_t clue_p;
      // already done it?
      JLG(clue_p, classifier->clues, working_token.id);
      if (NULL == clue_p) {
        positive_token.token_count = working_token.frequency;
        negative_token.token_count = pool_token_frequency(negative_pool, working_token.id);
        background_token.token_count = pool_token_frequency(background, working_token.id);
      
        double prob = probability(foregrounds, 1, backgrounds, 2, fg_total_tokens, bg_total_tokens);
        const Clue *clue = new_clue(working_token.id, prob);
       
        JLI(clue_p, classifier->clues, working_token.id);
        if (NULL == clue_p) goto classifier_malloc_error;
        *clue_p = (Word_t) clue;
      }      
    }
    
    // Now do the background
    working_token.id = 0;
    while (pool_next_token(negative_pool, &working_token)) {
      PWord_t clue_p;
      // already done it?
      JLG(clue_p, classifier->clues, working_token.id);
      if (NULL == clue_p) {
        negative_token.token_count = working_token.frequency;
        positive_token.token_count = pool_token_frequency(positive_pool, working_token.id);
        background_token.token_count = pool_token_frequency(background, working_token.id);
      
        double prob = probability(foregrounds, 1, backgrounds, 2, fg_total_tokens, bg_total_tokens);
        const Clue *clue = new_clue(working_token.id, prob);
       
        JLI(clue_p, classifier->clues, working_token.id);
        if (NULL == clue_p) goto classifier_malloc_error;
        *clue_p = (Word_t) clue;
      }      
    }
  }
  
  return classifier;
  classifier_malloc_error:
    free_classifier(classifier);
    return NULL;
}

/* Computes the probability that the tokens belong to the pool identified by pool_name.
 *
 * This method is based on the chi2_spamprob method in SpamBayes. To make it easier to compare
 * this method with the original method, the ham and spam variable names have been maintained.
 * In the context of this classifier, spam is belonging to the pool identified by pool_name and
 * ham is belonging to the other pools.
 *
 * This method will follow the algorithm pretty closely, for detail see
 * http://spambayes.cvs.sourceforge.net/spambayes/spambayes/spambayes/classifier.py?revision=1.31&view=markup
 */
Tagging * classify(const Classifier *classifier, const Item * item) {
  if (NULL == classifier || NULL == item) {
    error("classify received NULL classifier(%x) or item(%x)", classifier, item);
    return NULL;
  }
  
  Tagging *tagging = malloc(sizeof(Tagging));
  if (NULL != tagging) {
    tagging->item_id = item_get_id(item);
    tagging->user = cls_user(classifier);
    tagging->tag_name = cls_tag_name(classifier);
    tagging->tag_id = cls_tag_id(classifier);
    tagging->user_id = cls_user_id(classifier);
    
    int num_clues;
    const Clue **clues = select_clues(classifier, item, &num_clues);
    
    if (num_clues > 0) {
      tagging->strength = chi2_combine(clues, num_clues);
    } else {
      tagging->strength = UNKNOWN_WORD_PROB;
    }
    
    free(clues);
  }
  return tagging;
}

static double chi2_combine(const Clue **clues, int num_clues) {
  // Now we can combine all token scores into an item score
  double h, s;
  int hExp, sExp, i;
  h = s = 1.0;
  hExp = sExp = 0;

  for (i = 0; i < num_clues; i++) {
    s *= (1.0 - clue_probability(clues[i]));
    h *= clue_probability(clues[i]);
  
    // Check for underflow
    if (s < TINY_VAL_D) {
      int e = 0;
      s = frexp(s, &e);
      sExp += e;
    }
  
    if (h < TINY_VAL_D) {
      int e = 0;
      h = frexp(h, &e);
      hExp += e;
    }
  }

  s = log(s) + sExp * M_LN2;
  h = log(h) + hExp * M_LN2;
  s = 1.0 - chi2Q(-2.0 * s, num_clues * 2);
  h = 1.0 - chi2Q(-2.0 * h, num_clues * 2);
  return (s - h + 1.0) / 2.0;
}

static int compare_clues(const void *clue1_p, const void *clue2_p) {
  Clue *clue1 = *((Clue**)clue1_p);
  Clue *clue2 = *((Clue**)clue2_p);
  double strength_1 = clue_strength(clue1);
  double strength_2 = clue_strength(clue2);
  
  if (strength_1 < strength_2) {
    return 1;
  } else if (strength_2 < strength_1) {
    return -1;
  } else {
    return 0;
  }
}

const Clue ** select_clues(const Classifier * classifier, const Item *item, int *num_clues) {
  Token token;
  int i = 0;
  int num_item_tokens = item_get_num_tokens(item);
  int max_clues = MAX(MAX_DISCRIMINATORS, MAX_CLUES_RATIO * num_item_tokens);
  
  // This is an array that can hold the maximum number of clues
  // which is one per item token. Use calloc so it is effectively
  // NULL terminating the array. This will just hold pointers to
  // the actual clues that are stored in the classifier.
  const Clue **clues = calloc(num_item_tokens, sizeof(Clue*));
  
  token.id = 0;
  while (item_next_token(item, &token)) {
    const Clue *clue = cls_clue_for(classifier, token.id);
    if (NULL != clue && MIN_PROB_STRENGTH <= clue_strength(clue)) {      
      clues[i++] = clue;
    }
  }
  
  qsort(clues, i, sizeof(Clue*), compare_clues); 
  *num_clues = MIN(i, max_clues);
  return clues;
}

/**** Trained classifier functions ****/
float tc_get_bias(const TrainedClassifier *tc) {
  return tc->bias;
}

const Pool * tc_get_positive_pool(const TrainedClassifier *tc) {
  return tc->positive_pool;
}

const Pool * tc_get_negative_pool(const TrainedClassifier *tc) {
  return tc->negative_pool;
}

const char * tc_get_user(const TrainedClassifier *tc) {
  return tc->user;
}

int tc_get_user_id(const TrainedClassifier *tc) {
  return tc->user_id;
}

const char * tc_get_tag_name(const TrainedClassifier *tc) {
  return tc->tag_name;
}

int tc_get_tag_id(const TrainedClassifier *tc) {
  return tc->tag_id;
}

void tc_free(TrainedClassifier *tc) {
  free_pool(tc->positive_pool);
  free_pool(tc->negative_pool);
  free(tc);
}

/***** Classifier Functions *****/
const char * cls_tag_name(const Classifier *cls) {
  return cls->tag_name;
}

int cls_tag_id(const Classifier *cls) {
  return cls->tag_id;
}

const char * cls_user(const Classifier *cls) {
  return cls->user;
}

int cls_user_id(const Classifier *cls) {
  return cls->user_id;
}

int cls_num_clues(const Classifier *cls) {
  Word_t count;
  JLC(count, cls->clues, 0, -1);
  return count;
}

const Clue * cls_clue_for(const Classifier *cls, int token_id) {
  Clue *clue = NULL;
  PWord_t clue_p;
  JLG(clue_p, cls->clues, token_id);
  if (NULL != clue_p) {
    clue = (Clue*)(*clue_p);
  }
  return clue;
}

double cls_probability_for(const Classifier *cls, int token_id) {
  double probability = UNKNOWN_WORD_PROB;
  PWord_t clue_p;
  JLG(clue_p, cls->clues, token_id);
  if (NULL != clue_p) {
    Clue *clue = (Clue*)(*clue_p);
    probability = clue_probability(clue);
  }
  return probability;
}

void free_classifier(Classifier *cls) {
  if (NULL != cls) {
    if (cls->clues) {
      PWord_t clue;
      Word_t index = 0;
      Word_t bytes_freed;
      JLF(clue, cls->clues, index);
      while (NULL != clue) {
        free((Clue*)(*clue));
        JLN(clue, cls->clues, index);
      }
      JLFA(bytes_freed, cls->clues);
    }
    free(cls);
  }
}

/*** "Private" functions ***/
double probability(const ProbToken *foregrounds[], int n_fg,
                  const ProbToken *backgrounds[], int n_bg,
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


void compute_ratios(const ProbToken *token_prob[], int size, double *ratios) {
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
double compute_n(const ProbToken *foregrounds[], int n_fg, 
                 const ProbToken *backgrounds[], int n_bg, 
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
