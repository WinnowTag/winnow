/* Copyright (c) 2007 The Kaphan Foundation
 *
 * Possession of a copy of this file grants no permission or license
 * to use, modify, or create derivate works.
 *
 * Please contact info@peerworks.org for further information.
 */

#include "misc.h"
#include "tagger.h"
#include "logging.h"
#include "classifier.h"

TaggerCache * create_tagger_cache(ItemCache * item_cache, TaggerCacheOptions *opts) {
  TaggerCache *tagger_cache = calloc(1, sizeof(struct TAGGER_CACHE));
  tagger_cache->item_cache = item_cache;
  tagger_cache->random_background = item_cache_random_background(item_cache);
  if (tagger_cache->random_background == NULL) {
    info("Operating with empty random background");
    tagger_cache->random_background = new_pool();
  }
  return tagger_cache;
}

static Tagger * fetch_tagger(TaggerCache * tagger_cache, const char * tag_training_url, char ** errmsg) {
  Tagger *tagger = NULL;
  char *tag_document = NULL;
  int rc = tagger_cache->tag_retriever(tag_training_url, -1, &tag_document, errmsg);
  
  if (rc == TAG_OK && tag_document != NULL) {
    tagger = build_tagger(tag_document);
    tagger->probability_function = &naive_bayes_probability;
    tagger->classification_function = &naive_bayes_classify;
    
    if (tagger->state != TAGGER_LOADED) {
      tagger = NULL;
    }
    
    free(tag_document);    
  }
  
  return tagger;
}

// TODO move to precompute tagger.c?
static void train_and_precompute(Tagger *tagger, ItemCache *item_cache, Pool * rndbg) {
  if (tagger->state != TAGGER_PRECOMPUTED) {
    train_tagger(tagger, item_cache);

    if (tagger->state == TAGGER_TRAINED) {
      precompute_tagger(tagger, rndbg);
    }
  }  
}

int get_tagger(TaggerCache * tagger_cache, const char * tag_training_url, Tagger ** tagger, char ** errmsg) {
  int rc = -1;
  
  if (tagger_cache && tag_training_url) {
    int tagger_updated = false;
    Tagger *temp_tagger = NULL;
    // lock_mutex
    // is_tagger_checked_out(tagger_cache, tag_training_url)
    // checkout_tagger
    // unlock
    //
    // if (tagger) {
    //    if (update_tagger(tagger_cache, tagger, err))
    //      store_tagger = true;
    // } else {
   
    temp_tagger = fetch_tagger(tagger_cache, tag_training_url, errmsg);
    if (temp_tagger) {
      tagger_updated = true;
    }
   
   
    
    /* Once we have a tagger we need to make sure it is trained and precomputed. */     
    if (!temp_tagger) {
      rc = TAG_NOT_FOUND;
    } else {
      if (temp_tagger->state != TAGGER_PRECOMPUTED) {
        train_and_precompute(temp_tagger, tagger_cache->item_cache, tagger_cache->random_background);
      }      
      
      /** Figure out what the caller gets back based on the state of the classifier.
       */
      if (temp_tagger->state == TAGGER_PRECOMPUTED) {
        /* All went well, we can return the tagger */
        *tagger = temp_tagger;
        rc = TAGGER_OK;
      } else if (temp_tagger->state == TAGGER_PARTIALLY_TRAINED) {
        /* If some items are missing and this is new a or updated
         * tagger then add the missing items.
         *
         * Always return TAGGER_PENDING_ITEM_ADDITION if it is only
         * partially trained.
         */
        rc = TAGGER_PENDING_ITEM_ADDITION;
        
        if (tagger_updated) {
          // Add missing items to item cache
        }
      }       
    }
    
    //
    // if (tagger_updated) 
    //    lock
    //    store_in_cache(tagger_cache, tagger);
    //    release_without_locks(tagger, tagger_url) if rc == TAGGER_PENDING_ITEM_ADDITION
    //    unlock
    //
  }
  
  return rc;
}

void release_tagger(TaggerCache *tagger_cache, Tagger * tagger) {
  
}

void free_tagger_cache(TaggerCache * tagger_cache) {
   
}
