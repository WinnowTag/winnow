/* Copyright (c) 2007 The Kaphan Foundation
 *
 * Possession of a copy of this file grants no permission or license
 * to use, modify, or create derivate works.
 *
 * Please contact info@peerworks.org for further information.
 */

#include <string.h>
#include <sys/types.h>
#include "misc.h"
#include "tagger.h"
#include "logging.h"
#include "classifier.h"
#include "fetch_url.h"

TaggerCache * create_tagger_cache(ItemCache * item_cache, TaggerCacheOptions *opts) {
  TaggerCache *tagger_cache = calloc(1, sizeof(struct TAGGER_CACHE));
  tagger_cache->item_cache = item_cache;
  tagger_cache->options = opts;
  tagger_cache->tag_urls = NULL;
  tagger_cache->failed_tags = NULL;
  tagger_cache->taggers = NULL;
  tagger_cache->tag_urls_last_updated = -1;
  
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
  if (tagger_cache->tag_retriever == NULL) {
    fatal("tagger_cache->tag_retriever not set");
  }
  
  Tagger *tagger = NULL;
  char *tag_document = NULL;
  int rc = tagger_cache->tag_retriever(tag_training_url, if_modified_since, &tag_document, errmsg);
  
  if (rc == URL_OK && tag_document != NULL) {
    tagger = build_tagger(tag_document);
    
    if (tagger) {
      /* Replace the training url with the url we actually used to fetch it,
       * i.e. don't trust the atom document to report this correctly.
       */
      free(tagger->training_url);
      tagger->training_url = strdup(tag_training_url);
      tagger->probability_function = &naive_bayes_probability;
      tagger->classification_function = &naive_bayes_classify;
      tagger->get_clues_function = &select_clues;
    
      if (tagger->state != TAGGER_LOADED) {
        tagger = NULL; // free_tagger
      }      
    } else {
      info("The tag document was badly formed");      
      if (errmsg) *errmsg = strdup("The tag document was badly formed");      
    }
    
    free(tag_document);    
  }
  
  return tagger;
}

#define TAGGER_NOT_CACHED 16

static int mark_as_checked_out(TaggerCache *tagger_cache, const char * tagger_identify) {
  debug("Checking out %s", tagger_identify);
  PWord_t tagger_pointer;
  
  JSLI(tagger_pointer, tagger_cache->checked_out_taggers, (const uint8_t*) tagger_identify);
  
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
      mark_as_checked_out(tagger_cache, (*tagger)->training_url);
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
       free_tagger((Tagger*) (*tagger_pointer));
      debug("Replacing %s in cache", tagger->training_url);
    } else {
      debug("Inserting %s into cache for the first time", tagger->training_url);
    }
    
    *tagger_pointer = (Word_t) tagger;
  } else {
    fatal("Out of memory in cache_tagger");
  }
  
  return 0;
}

static int release_tagger_without_locks(TaggerCache *tagger_cache, uint8_t * tag_url) {
  int rc;
  JSLD(rc, tagger_cache->checked_out_taggers, tag_url);
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
      free_entry(entries[i]);
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
 *  @param do_fetch Boolean indicating whether the tag will be fetched or updated.
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
int get_tagger(TaggerCache * tagger_cache, const char * tag_training_url, int do_fetch, Tagger ** tagger, char ** errmsg) {
  int rc = -1;
  
  if (tagger_cache && tag_training_url) {
    int new_tagger = false;
    Tagger *temp_tagger = NULL;
    
    /* First lock the cache and attempt to checkout the tagger.
     * This will tell us if any one else has already checked it out,
     * if they have no further action is taken and we return
     * TAGGER_CHECKED_OUT. If it isn't checked out we check it out now.
     */
    pthread_mutex_lock(&tagger_cache->mutex);
    int cache_rc = checkout_tagger(tagger_cache, tag_training_url, &temp_tagger);
    if (cache_rc != TAGGER_CHECKED_OUT) {
      mark_as_checked_out(tagger_cache, tag_training_url);
    }
    pthread_mutex_unlock(&tagger_cache->mutex);
    
    debug("checkout complete");
    
    if (TAGGER_CHECKED_OUT != cache_rc) {
      /* Only fetch or update if do_fetch is true */
      if (do_fetch) {
        if (cache_rc == TAGGER_NOT_CACHED) {
          /* If we get here the tagger has not been cached so attempt to fetch it,
         * if we get it we need to store it and mark it as checked out.
         */
          temp_tagger = fetch_tagger(tagger_cache, tag_training_url, -1, errmsg);
          if (temp_tagger) {
            new_tagger = true;
          }
        } else if (temp_tagger && temp_tagger->state != TAGGER_PARTIALLY_TRAINED) {
          /* The tagger is cached, so we need to see if it has been updated, but only if it has no pending items. */
          Tagger *updated_tagger = fetch_tagger(tagger_cache, temp_tagger->training_url, temp_tagger->updated, errmsg);

          // TODO do we need to check if it is not found or there is a network error?
          if (updated_tagger) {
            new_tagger = true;
            temp_tagger = updated_tagger;          
          } else {
            debug("Tag %s not modified, using cached version", temp_tagger->training_url);
          }
        }
      }
      
      /* If we got a tagger try and get it to the precomputed state and update the tagger cache state for it. */
      if (temp_tagger) {        
        if (temp_tagger->state != TAGGER_PRECOMPUTED) {
          if (TAGGER_TRAINED == temp_tagger->state || TAGGER_TRAINED == train_tagger(temp_tagger, tagger_cache->item_cache)) {
            precompute_tagger(temp_tagger, tagger_cache->random_background);
          } 
        }
        
        /* Update the tagger cache state for this tag, need to lock for this */
        pthread_mutex_lock(&tagger_cache->mutex);
        
        if (new_tagger) {
          /* The tagger is new or updated so we need to store it the taggers cache */
          cache_tagger(tagger_cache, temp_tagger);
        }
        
        /* If the new tagger is not in the PRECOMPUTED state check it back in since we don't return it */
        if (temp_tagger->state != TAGGER_PRECOMPUTED) {
          release_tagger_without_locks(tagger_cache, tag_training_url);
        }
        
        pthread_mutex_unlock(&tagger_cache->mutex);
      }
            
      /* Figure out what state of tagger we have, if any, to determine what we return:
       *
       *  temp_tagger == NULL                     -> TAGGER_NOT_FOUND
       *  state       == TAGGER_PARTIALLY_TRAINED -> TAGGER_PENDING_ITEM_ADDITION
       *  state       == TAGGER_PRECOMPUTED       -> TAGGER_OK and the tagger set
       *  other                                   -> UNKNOWN (BUG!)
       */
      if (temp_tagger && temp_tagger->state == TAGGER_PRECOMPUTED) {
        *tagger = temp_tagger;
        rc = TAGGER_OK;
      } else if (temp_tagger && temp_tagger->state == TAGGER_PARTIALLY_TRAINED) {
        rc = TAGGER_PENDING_ITEM_ADDITION;
        if (errmsg) {
          *errmsg = strdup("Some items need to be cached");
        }
              
        /* Only add the items if this is a new tagger, otherwise we have already done it */
        if (new_tagger) {
          add_missing_entries(tagger_cache->item_cache, temp_tagger);
        }
      } else if (temp_tagger) {
        rc = UNKNOWN;
        error("Unaccounted for tagger state: %i", temp_tagger->state);
      } else {
        rc = TAG_NOT_FOUND;
        
        /* Make sure a missing tag is not checked out */
        pthread_mutex_lock(&tagger_cache->mutex);
        release_tagger_without_locks(tagger_cache, tag_training_url);
        pthread_mutex_unlock(&tagger_cache->mutex);
      }
    } else {
      if (errmsg) *errmsg = strdup("Tagger already being processed");        
      rc = cache_rc;
    }
  }
  
  return rc;
}

int release_tagger(TaggerCache *tagger_cache, Tagger * tagger) {
  int rc = 1;
  if (tagger_cache && tagger) {
    debug("releasing tagger %s", tagger->training_url);
    pthread_mutex_lock(&tagger_cache->mutex);
    rc = release_tagger_without_locks(tagger_cache, tagger->training_url);
    pthread_mutex_unlock(&tagger_cache->mutex);
  }
  
  return rc;
}

int is_cached(TaggerCache *cache, const char * tag) {
  int cached = 0;
  
  if (cache && tag) {
    pthread_mutex_lock(&cache->mutex);
    PWord_t tagger_pointer;
    
    JSLG(tagger_pointer, cache->taggers, (uint8_t*) tag);
    if (tagger_pointer) {
      cached = 1;
    }
    
    pthread_mutex_unlock(&cache->mutex);
  }
  
  return cached;
}

int is_error(TaggerCache *cache, const char * tag) {
  int _error = 0;
  
  if (cache && tag) {
    pthread_mutex_lock(&cache->mutex);
    PWord_t tagger_pointer;
    debug("is error for %s", tag);
    JSLG(tagger_pointer, cache->failed_tags, (uint8_t*) tag);
    if (tagger_pointer) {
      _error = 1;
    }
    pthread_mutex_unlock(&cache->mutex);
  }
  
  return _error;
}

struct background_fetch_data {
  TaggerCache *tagger_cache;
  char * tag;
};

static void *background_fetcher(void *memo) {
  struct background_fetch_data *data = (struct background_fetch_data*) memo;
  debug("background fetcher started for %s", data->tag);
  
  Tagger *tagger = NULL;
  int rc = get_tagger(data->tagger_cache, data->tag, true, &tagger, NULL);
  
  switch (rc) {
    case TAGGER_OK:
      release_tagger(data->tagger_cache, tagger);
      break;
    default:
      pthread_mutex_lock(&data->tagger_cache->mutex);  
      PWord_t tag_pointer;
      JSLI(tag_pointer, data->tagger_cache->failed_tags, (uint8_t*) data->tag);
      pthread_mutex_unlock(&data->tagger_cache->mutex);
  }
  
  free(data->tag);
  free(data);
  return 0;
}

int fetch_tagger_in_background(TaggerCache *cache, const char * tag) {
  if (cache && tag) {
    struct background_fetch_data *data = malloc(sizeof(struct background_fetch_data));
    if (data) {
      data->tag = strdup(tag);
      data->tagger_cache = cache;
      pthread_t background_thread;
      memset(&background_thread, 0, sizeof(pthread_t));
      
      if (pthread_create(&background_thread, NULL, background_fetcher, data)) {
        error("Could not create background fetcher thread");
        free(data->tag);
        free(data);
      }
    } else {
      fatal("Could not malloc memory for background_fetch_data");
    }
  }
  
  return 0;
}

/** Fetchs the tag urls from the tag index.
 *
 * @param tagger_cache The tagger cache that manages the tag index.
 * @param a The array which will be a pointer to an array of tag urls if the operation
 *          is successful.  The resulting array should not be modified externally.
 * @param errmsg Storage for any error messages.
 * @return TAG_INDEX_OK if operation is successfull, *a will point to the tag url array.
 *         TAG_INDEX_FAIL if operation failed, if non-null errmsg was provided it will contain the error.
 */
int fetch_tags(TaggerCache * tagger_cache, Array ** a, char ** errmsg) {
  int rc = TAG_INDEX_OK;
  
  if (tagger_cache && tagger_cache->options->tag_index_url && a) {
    char *tag_document = NULL;
    
    int urlrc = tagger_cache->tag_index_retriever(tagger_cache->options->tag_index_url, tagger_cache->tag_urls_last_updated, &tag_document, errmsg);
    
    if (urlrc == URL_OK && tag_document) {
      /* Tag Index updated or fetched for the first time */
      if (tagger_cache->tag_urls) {
        free_array(tagger_cache->tag_urls);
      }
      
      tagger_cache->tag_urls = create_array(100);
      tagger_cache->tag_urls_last_updated = time(NULL);
      rc = parse_tag_index(tag_document, tagger_cache->tag_urls, &tagger_cache->tag_urls_last_updated);
      
      if (rc == TAG_INDEX_FAIL && errmsg) {
        *errmsg = strdup("Parser error in tag index");
      } else {
        *a = tagger_cache->tag_urls;
      }      
    } else if (tagger_cache->tag_urls) {
      /* Return the cached version */
      debug("Returning cached version of tag index");
      *a = tagger_cache->tag_urls;
    } else {
      debug("urlrc = %i, tag_document = %s", urlrc, tag_document);
      rc = TAG_INDEX_FAIL;
      if (errmsg && *errmsg == NULL) {
        *errmsg = strdup("Could not find tag index");
      }
    }
    
    if (tag_document) {
      free(tag_document);
    }
  } else {
    rc = TAG_INDEX_FAIL;
    if (errmsg) {
      *errmsg = strdup("No tag index defined");
    }
  }
  
  return rc;
}

void free_tagger_cache(TaggerCache * tagger_cache) {
   
}
