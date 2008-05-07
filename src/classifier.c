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
#include "misc.h"
#include "clue.h"

#define TINY_VAL_D 1e-200

/******************************************************************************************
 * The Peerworks implementation of a Bayesian Classifier.
 * 
 * 
 * This is inspired by SpamBayes with a few tweaks, see compute_n for the main enhancement.
 *
 * But first some history:
 *
 * This is basically a log of the development of the classifier.
 *
 * This section talks about 'seen' and 'unwanted' tags used in the background.
 * These tags are an artifact of our initial corpus tagging process. A user
 * was directed to tag an item with 'seen' if they felt ambivalent about it
 * and 'unwanted' if they definitely didn't want to see items like that.
 * 
 * These were partly used to handle tracking moderation completion, so we
 * knew we were done when all items were tagged, and partly with the idea
 * that we could use them in the classification of other tags.  However 
 * the idea is now obsolete and is only referenced below for historical
 * purposes.
 *
 * == Handling Backgrounds ==
 *
 * Initially the background was the combination of all other tags however,
 * when including tokens that are only in the background when computing
 * probabilities this produced a huge bias in favour of negatives, so for
 * a while we were ignoring tokens that only existed in the background.
 * This of course had the side effect of favouring positives but not as
 * dramatically as including them would favour negatives.
 *
 * I (Sean) had a theory that the reason that the bias would be so strongly
 * biased to negatives was because the variety of tokens in the background
 * is so huge, the chance of a token occuring in the background is so much
 * higher than in the foreground. This is directly related to another thing 
 * that was bugging me for a while.  Say you have three tags, 'politics', 
 * 'media' and 'right wing'; three different items are each tagged with one 
 * of those tags and each of these items has the word 'Bush'.  When you then 
 * compute the probability for the token 'Bush' in any of the three tags it 
 * becomes a negative token for any one of the pools because it appears in 
 * the background pool more than in the foreground because the background 
 * pool contains the tokens from other two tags.  Now clearly, Bush could 
 * probably be a strong indicator for any of those pools but by building the 
 * background pool out of all the other pools it becomes a negative token.
 *
 * So to counter this I did some experiments using just the unwanted tag as the
 * background.  This yielded pretty good results, with 57% true positives and
 * 99% true negatives for normal tags.
 *
 * To support this I have added a parameters to the initialize method:
 *
 *   :background_pool_name - The names of pools to use as the background.
 *
 * Currently only one pool name is supported.  This pool will be used as the
 * background for all other pools and the union of all other pools will be used
 * as the background for that pool.
 *
 * If no background pool is specified, the original behaviour of the union of all
 * other tags will be used.
 *
 * === Further work on backgrounds ===
 *
 * We then thought it would be interesting to try other backgrounds, such as 'seen'.
 * Initial testing using 'seen' as the background gave about a 20% increase in true positives
 * and only a 6% drop in true negatives.  
 *
 * 'unwanteds' suffered badly in this scheme though, so we required the addition of
 * another configuration parameter to define a list of pools that would use the entire
 * foreground union as their background instead of the background specified by
 * :background_pool_name.  This provided excellent performance for both normal and unwanted
 * tags.
 *
 * There was one more variation to try though. Using everything except 'seen' and 'unwanted'
 * as the background for normal tags.  I tried this it gave the worst results in terms of
 * true positives (around 30%).
 *
 * So the best solution seems to be using 'seen' as the background for normal tags and the
 * union of all normal tags as the background for 'unwanted' tags.
 *
 * === Update to background handling ==
 *
 * After making the changes to the Bayesian adjustment that better account for imbalances between
 * the foreground and background, the tests for different backgrounds were run again.  In this
 * case, the 'seen' as background solution gave the worst results and unwanted as background
 * and 'not tag' as background both gave fairly good results with unwanted slightly higher.  
 *
 * The outcome of this was that the special handling of backgrounds was removed and we went back
 * to using 'not tag' as the background.  Even though using 'unwanted' was slightly higher, using
 * 'not tag' is simpler and also less specific to individual tagging habits.
 *
 * I think you can assume that the use of 'seen' as background performed better originally because it
 * was a way of reducing the imbalance between the foreground and background tags. The modified 
 * Bayesian Adjustment clearly does a better job of fixing the imbalance and gives better results without
 * having to specifically handle background selection.
 *
 * === Yet another update to backgrounds ===
 *
 * After deploying the classifier at the last state described false negatives seemed awfully high. So we
 * added a random background that consisted of 5000 items randomly selected from the corpus.  This produced
 * much more consistent results that seemed a lot more stable. This is probably an analogue to the good
 * results we got using 'seen' and 'unwanted' in the fully moderated corpus.  'seen' and 'unwanted' are pretty
 * artifical, in the sense that a normal user wouldn't want to create these tags, i.e. you tag things you
 * like not things you don't like. But their size and generality provide a good flavour of the overall corpus,
 * similar to how the random background works I guess.
 *
 * Then after a while, I wondered how much affect including the rest of the user's tag in the background had.
 * It was clear that it created an undesirable affect that changing one tag would affect all other tags,
 * so we did some A-B comparison tests using just the negative examples and random examples as the background
 * versus the negative, random and all other positive tags for the user as the background.  This actually
 * favoured the test without the "all other positive" pools.
 *
 * So we removed the "not-tag" pool and ended up with a classifier for each tag where the foreground is made
 * up of the positive examples and the background is made of up the negative examples and the random background,
 * both treated as separate pools.  This version has worked well.  The random background seems to provide stability
 * when building a tag and the adjustment to the computation of 'n' in the bayesian adjustment balances out the
 * size differences between the positive and negative examples and the large random background.
 *
 */
 
 
/************************************************************************************************************
 *  The are the main functions in the classifier algorithm. For details on how
 *  these are hooked into the classification engine jump down to the API functions.
 ************************************************************************************************************/

/** This computes the ratios for a set of probability tokens.
 *  
 *  The ratio is token_count / pool_size.
 *
 *  The ratio of the ith token stored in the ith element of the ratios array.
 *  ratios must be large enough to hold size doubles.
 */
static void compute_ratios(const ProbToken *token_prob[], int size, double *ratios) {
  int i;
  for (i = 0; i < size; i++) {
    if (token_prob[i]->pool_size > 0) {
      ratios[i] = (double) token_prob[i]->token_count / token_prob[i]->pool_size;
    } else {
      ratios[i] = 0;
    }
  }
}

/** Averages an array of doubles ignoring elements less than or equal to 0. */
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

/* N provides a measure of confidence in a probability for a token.
 * It is used in the Bayesian Adjustment.  
 
 * Previously n was just pool.tokens[token] + other_token_count.
 * This has a built in assumption that the foreground and background
 * pools are roughly the same size and a token occuring a few times in
 * the foreground is equal in value to a token occuring a few times in 
 * the background.  This assumption doesn't hold for our purposes, 
 * or perhaps for multi-label classifiers in general.
 *
 * In our case the foreground pool is likely to be much smaller than
 * the background pool, just because people will tag less items than
 * they don't and we use a random background of 5000ish items.
 *
 * I did find a method for scaling n referenced in this article at
 * http://www.bgl.nu/bogofilter/param.html. It would scale n by the
 * ratio of the sizes between the foreground and background. However
 * it would scale the foreground token count by the same factor that
 * it will scale the background token count.  So it does still assume
 * some symmetricality, or at least that you want to treat rare tokens
 * in a large the same way as rare tokens in a smaller pool.
 *
 * We would like to be able to treat rare tokens in smaller pools as being
 * higher indicators of the token occuring in that pool that rare tokens
 * within large pools.  This is essentially preserving some of the information
 * in the pool_ratio and other_ratio that gets normalized by the initial
 * calculation of probability.
 *
 * So to achieve this I (Sean) came up with the idea of multiplying the token
 * count in the foreground by the ratio between the background and foreground 
 * and dividing the token count in the background by the same ratio then adding
 * these two figures together to get the value for n. This has the effect that
 * when the foreground and background are largely asymmetric in size a token 
 * appearing only in the smaller pool has a larger weight than a token appearing
 * only in the larger pool.
 *
 * From a results point of view, this pulled alot of the true positives
 * up from 0% in the lower training count bins and had no negative impact
 * on true negatives.
 *
 * With the move to arbitrary pools, n is calculated slightly differently
 * but with the same aim and results. It now takes an array of token counts
 * and an array total counts for number of pools. The ith element in the
 * token count array correponds to the ith element in the total count array.
 *
 * N is computed by summing each token count multiplied by the size of all the 
 * other pools, divided by the size of the pool the token count came from.
 */
static double compute_n(const ProbToken *foregrounds[], int n_fg, 
                 const ProbToken *backgrounds[], int n_bg, 
                 double fg_total_tokens, double bg_total_tokens) {
  int i;
  double fg_ns[n_fg];
  double bg_ns[n_bg];
  
  // This formula is more cleary expressed as:
  //  
  //  n +=  token count * sum of total tokens for all opposite pools 
  //                      -------------------------------------------
  //                             the pool_size for this pool
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

/** Calculates the probability that the ProbToken appears in the given list of
 *  foreground pools, as opposed to the background pool.
 *
 *  Current foregounds will just be a single ProbToken representing the token in
 *  the positive examples provided by the user, and backgrounds will the token in
 *  negative examples and the random background.
 *
 *  For reference, a ProbToken contains the frequency of the token in a pool and
 *  the size of the pool.
 *
 * 
 *  @param foregrounds The token's occurrence in each foreground pool.
 *  @param backgrounds The token's occurrence in each background pool.
 *  @param n_fg Number of foreground pools.
 *  @param n_bg Number of background pools.
 *  @param fg_total_tokens The size of all the foreground pools.
 *  @param bg_total_tokens The size of all the background pools.
 */
double probability(const ProbToken *foregrounds[], int n_fg,
                  const ProbToken *backgrounds[], int n_bg,
                  int fg_total_tokens, int bg_total_tokens) {
  
  double probability = UNKNOWN_WORD_PROB;
  
  /* Guard against empty pool.
   *
   * We don't need to guard against zero tokens because a classifier's
   * token probabilities are pre-computed based on the tokens that appear
   * in the training, not the tokens that appear in items to be classified.
   * So the token frequency will never be zero.
   */
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
    
    /* Do the Bayesian adjustment found in SpamBayes.
    *
    * See also:
    * SpamBayes code
    *  (http://spambayes.cvs.sourceforge.net/spambayes/spambayes/spambayes/classifier.py?view=markup)
    * Essay by Gary Robinson
    *  (http://radio.weblogs.com/0101454/stories/2002/09/16/spamDetection.html)
    */
    probability = (S_TIMES_X + n * ratio) / (UNKNOWN_WORD_STRENGTH + n);
  }
  
  return probability;
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

/** Used by qsort to sort clues in order of strength.
 */
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

/** Selects the clues from a classifier to use when classifying an item.
 */
const Clue ** select_clues(const ClueList * clues, const Item *item, int *num_clues) {
  Token token;
  int i = 0;
  int num_item_tokens = item_get_num_tokens(item);
  int max_clues = MAX(MAX_DISCRIMINATORS, MAX_CLUES_RATIO * num_item_tokens);
  
  // This is an array that can hold the maximum number of clues
  // which is one per item token. Use calloc so it is effectively
  // NULL terminating the array. This will just hold pointers to
  // the actual clues that are stored in the classifier.
  const Clue **selected_clues = calloc(num_item_tokens, sizeof(Clue*));
  
  token.id = 0;
  while (item_next_token(item, &token)) {
    const Clue *clue = get_clue(clues, token.id);
    if (NULL != clue && MIN_PROB_STRENGTH <= clue_strength(clue)) {      
      selected_clues[i++] = clue;
    }
  }
  
  qsort(selected_clues, i, sizeof(Clue*), compare_clues); 
  *num_clues = MIN(i, max_clues);
  return selected_clues;
}

/** Combines the probabilities for each individual token into a single probability for the item.
 *
 * This method is based on the chi2_spamprob method in SpamBayes. To make it easier to compare
 * this method with the original method, the ham and spam variable names have been maintained.
 * In the context of this classifier, spam is belonging to the pool identified by pool_name and
 * ham is belonging to the other pools.
 *
 * This method will follow the algorithm pretty closely, for detail see
 * http://spambayes.cvs.sourceforge.net/spambayes/spambayes/spambayes/classifier.py?revision=1.31&view=markup
 */
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

/*****************************************************************************
 * These functions provide the API to the classifier for the outside world.
 */

/** This function is used to provide a hook to calculate the probability for a token given
 *  a positive, negative and random background pool.  This function fulfils the interface
 *  defined by the Tagger module.
 */
double naive_bayes_probability(const Pool * positive_pool, const Pool * negative_pool, const Pool * random_bg, int token_id, double bias) {
  ProbToken positive_token   = {pool_token_frequency(positive_pool, token_id), pool_total_tokens(positive_pool) / bias};
  ProbToken negative_token   = {pool_token_frequency(negative_pool, token_id), pool_total_tokens(negative_pool) * bias};
  ProbToken background_token = {pool_token_frequency(random_bg, token_id), pool_total_tokens(random_bg) * bias};
  
  const int fg_total_tokens = positive_token.pool_size;
  const int bg_total_tokens = negative_token.pool_size + background_token.pool_size;
  const ProbToken *foregrounds[] = {&positive_token};
  const ProbToken *backgrounds[] = {&negative_token, &background_token};

  return probability(foregrounds, 1, backgrounds, 2, fg_total_tokens, bg_total_tokens);
}

/** Classifies the item using the given classifier.
 *
 * Returns the probability (0..1) that the item is in 
 * the tag represented by the classifier.
 */
double naive_bayes_classify(const ClueList *clues, const Item * item) {
  if (NULL == clues || NULL == item) {
    fatal("classify received NULL classifier(%x) or item(%x)", clues, item);
    return 0.5;
  }
  
  double prob = 0.5;
  int num_clues;
  const Clue **selected_clues = select_clues(clues, item, &num_clues);
    
  if (num_clues > 0) {
    prob = chi2_combine(selected_clues, num_clues);
    free(selected_clues);
  }
    
  return prob;
}
