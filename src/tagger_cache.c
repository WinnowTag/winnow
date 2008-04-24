/* Copyright (c) 2007 The Kaphan Foundation
 *
 * Possession of a copy of this file grants no permission or license
 * to use, modify, or create derivate works.
 *
 * Please contact info@peerworks.org for further information.
 */

#include <string.h>
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
  
  if (pthread_mutex_init(&tagger_cache->mutex, NULL)) {
    fatal("pthread_mutex_init error for tagger_cache");
    free(tagger_cache);
    tagger_cache = NULL;
  }
  
  return tagger_cache;
}

static Tagger * fetch_tagger(TaggerCache * tagger_cache, const char * tag_training_url, time_t if_modified_since, char ** errmsg) {
  Tagger *tagger = NULL;
  char *tag_document = NULL;
  int rc = tagger_cache->tag_retriever(tag_training_url, if_modified_since, &tag_document, errmsg);
  
  if (rc == TAG_OK && tag_document != NULL) {
    tagger = build_tagger(tag_document);
    tagger->probability_function = &naive_bayes_probability;
    tagger->classification_function = &naive_bayes_classify;
    
    if (tagger->state != TAGGER_LOADED) {
      tagger = NULL;
    }
    
    free(tag_document);
    
    if (tagger->state == TAGGER_LOADED) {
      train_tagger(tagger, tagger_cache->item_cache);

      if (tagger->state == TAGGER_TRAINED) {
        precompute_tagger(tagger, tagger_cache->random_background);
      }
    } 
  }
  
  return tagger;
}

#define TAGGER_NOT_CACHED 16

static int mark_as_checked_out(TaggerCache *tagger_cache, const Tagger * tagger) {
  debug("Checking out %s", tagger->training_url);
  PWord_t tagger_pointer;
  
  JSLI(tagger_pointer, tagger_cache->checked_out_taggers, (const uint8_t*) tagger->training_url);
  
  if (tagger_pointer != NULL) {
    // Don't store the tagger here - don't need it and I don't want to have to update it.
    *tagger_pointer = 1;
  } else {
    fatal("Malloc error allocating element in checked_out_taggers");
  }
  
  return 0;
}

static int checkout_tagger(TaggerCache * tagger_cache, const char * tag_training_url, Tagger ** tagger) {
  int rc = TAGGER_OK;
  PWord_t tagger_pointer;
  
  JSLG(tagger_pointer, tagger_cache->checked_out_taggers, (const uint8_t*) tag_training_url);
  if (tagger_pointer != NULL) {
    rc = TAGGER_CHECKED_OUT;
  } else {
    JSLG(tagger_pointer, tagger_cache->taggers, (const uint8_t*) tag_training_url);
    
    if (tagger_pointer != NULL) {
      *tagger = (Tagger*) (*tagger_pointer);
      mark_as_checked_out(tagger_cache, *tagger);
    } else {
      rc = TAGGER_NOT_CACHED;
    }
  }
  
  return rc;
}

static int cache_tagger(TaggerCache * tagger_cache, Tagger * tagger) {
  PWord_t tagger_pointer;
  
  JSLI(tagger_pointer, tagger_cache->taggers, (uint8_t*) tagger->training_url);
  if (tagger_pointer != NULL) {
    if (*tagger_pointer != 0) {
       // we are replacing a tagger in the cache, so free the old one
       // TODO free_tagger((Tagger*) (*tagger_pointer));
    } 
    
    *tagger_pointer = (Word_t) tagger;
  } else {
    fatal("Out of memory in cache_tagger");
  }
  
  return 0;
}

static int release_tagger_without_locks(TaggerCache *tagger_cache, Tagger *tagger) {
  int rc;
  JSLD(rc, tagger_cache->checked_out_taggers, (uint8_t*) tagger->training_url);
  return rc;
}

static int add_missing_entries(ItemCache * item_cache, Tagger * tagger) {
  /* Only add missing entries if the tagger is new. */
  int number_of_missing_entries = tagger->missing_positive_example_count + tagger->missing_negative_example_count;
  ItemCacheEntry *entries[number_of_missing_entries];
  memset(entries, 0, sizeof(entries));
  
  if (!get_missing_entries(tagger, entries)) {
    int i;
    
    for (i = 0; i < number_of_missing_entries; i++) {
      item_cache_add_entry(item_cache, entries[i]);
    }
  }
  
  return 0;
}

/** Gets a tagger from the Tagger Cache.
 *
 *  This has a few different outcomes, I'll try and list them all.
 *
 *  See doc/TaggerCacheFlowChart.graffle for a flow chart on this function.
 *
 *  @param tagger_cache The cache to get the tagger from.
 *  @param tag_training_url Used as the URL for the tag training document and the key into the tagger cache.
 *  @param tagger A pointer to a Tagger pointer. This will be intitalized to the tagger if the call is
 *                successful. Don't free it when you are done with it, instead call release_tagger.
 *  @param errmsg Will be allocated and filled with an error message if an error occurs. The caller must free
 *                the error message when done. Can be NULL in which case you won't get any error messages.
 *  @return TAGGER_OK -> Got a valid trained and precomputed tagger in **tagger. We done with the tagger
 *                       you must release it usingl release_tagger(TaggerCache, Tagger)
 *          TAGGER_NOT_FOUND -> Could not find the tagger in either the cache or the URL. **tagger is NULL.
 *          TAGGER_CHECKED_OUT -> Someone else has the tagger. **tagger is NULL.
 *          TAGGER_PENDING_ITEM_ADDITION -> The tagger requires items that are missing from the cache.
 *                                          The items have been added and are scheduled for feature extract.
 *                                          Call get_tagger again later to see if it is ready. **tagger is NULL.
 *
 *  TODO Double check this locking
 */
int get_tagger(TaggerCache * tagger_cache, const char * tag_training_url, Tagger ** tagger, char ** errmsg) {
  int rc = -1;
  
  if (tagger_cache && tag_training_url) {
    int new_tagger = false;
    Tagger *temp_tagger = NULL;
    
    /* First lock the cache and attempt to checkout the tagger.
     * This will tell us if any one else has already checked it out,
     * if they have no further action is taken and we return
     * TAGGER_CHECKED_OUT.
     */
    pthread_mutex_lock(&tagger_cache->mutex);
    int cache_rc = checkout_tagger(tagger_cache, tag_training_url, &temp_tagger);
    pthread_mutex_unlock(&tagger_cache->mutex);
    
    if (cache_rc == TAGGER_CHECKED_OUT) {
      if (errmsg) {
        *errmsg = strdup("Tagger already being processed");        
      }
      rc = cache_rc;
    } else {
      /* If we get here the tagger has not been cached so attempt to fetch it,
       * if we get it we need to store it and mark it as checked out.
       */
      if (cache_rc == TAGGER_NOT_CACHED) {
        temp_tagger = fetch_tagger(tagger_cache, tag_training_url, -1, errmsg);
        if (temp_tagger) {
          new_tagger = true;
          pthread_mutex_lock(&tagger_cache->mutex);
          cache_tagger(tagger_cache, temp_tagger);          

          // Only checkout the tagger if it is in the PRECOMPUTED state since we don't return it otherwise
          if (temp_tagger->state == TAGGER_PRECOMPUTED) {
            mark_as_checked_out(tagger_cache, temp_tagger);          
          }

          pthread_mutex_unlock(&tagger_cache->mutex);
        }
      } else if (tagger) {
        /* The tagger is cached, so we need to see if it has been updated. */
        Tagger *updated_tagger = fetch_tagger(tagger_cache, temp_tagger->training_url, temp_tagger->updated, errmsg);
        
        if (updated_tagger) {
          new_tagger = true;
          temp_tagger = updated_tagger;
          pthread_mutex_lock(&tagger_cache->mutex);
          cache_tagger(tagger_cache, temp_tagger); 
          
          /* If the new tagger is not in the PRECOMPUTED state check it back it since we don't return it */
          if (updated_tagger->state != TAGGER_PRECOMPUTED) {
            release_tagger_without_locks(tagger_cache, temp_tagger);
          } 
                  
          pthread_mutex_unlock(&tagger_cache->mutex);
        }
      }
      
      if (temp_tagger && temp_tagger->state == TAGGER_PRECOMPUTED) {
        *tagger = temp_tagger;
        rc = TAGGER_OK;
      } else if (temp_tagger && temp_tagger->state == TAGGER_PARTIALLY_TRAINED) {
        rc = TAGGER_PENDING_ITEM_ADDITION;
        if (errmsg) {
          *errmsg = strdup("Some items need to be cached");
        }
              
        if (new_tagger) {
          add_missing_entries(tagger_cache->item_cache, temp_tagger);
        }
      } else if (temp_tagger) {
        rc = UNKNOWN;
        error("Unaccounted for tagger state: %i", temp_tagger->state);
      } else {
        rc = TAG_NOT_FOUND;
      }
    }
  }
  
  return rc;
}

int release_tagger(TaggerCache *tagger_cache, Tagger * tagger) {
  int rc = 1;
  if (tagger_cache && tagger) {
    pthread_mutex_lock(&tagger_cache->mutex);
    rc = release_tagger_without_locks(tagger_cache, tagger);
    pthread_mutex_unlock(&tagger_cache->mutex);
  }
  
  return rc;
}

void free_tagger_cache(TaggerCache * tagger_cache) {
   
}
