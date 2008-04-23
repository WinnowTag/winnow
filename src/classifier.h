// Copyright (c) 2007 The Kaphan Foundation
//
// Possession of a copy of this file grants no permission or license
// to use, modify, or create derivate works.
//
// Please contact info@peerworks.org for further information.

#ifndef CLASSIFIER_H
#define CLASSIFIER_H
#include "tag.h"
#include "item_cache.h"
#include "clue.h"
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

typedef struct PROB_TOKEN {
  int token_count;
  int pool_size;
} ProbToken;

extern double naive_bayes_classify    (const ClueList *clues, const Item *item);
extern double naive_bayes_probability (const Pool * positive_pool, const Pool * negative_pool, const Pool * random_bg, int token_id, double bias);

/** Only in header for testing - shouldn't actual use it */
extern double          chi2Q        (double x, int v);
extern const  Clue **  select_clues (const ClueList*, const Item*, int *num_clues);
extern double          probability  (const ProbToken *foreground[], int n_pos,
                                        const ProbToken *background[], int n_neg,
                                        int fg_size, int bg_size);

#endif
