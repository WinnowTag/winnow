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

#define CHECKED_OUT_MSG "Tagger already being processed"
#define TAGGER_NOT_CACHED 16

/** Creates a new TaggerCache with an item cache and some options.
 *
 *  @param item_cache The item cache that will be used in training taggers.
 *  @param opts Some additional options for the TaggerCache.
 *  @return The new TaggerCache or NULL if something goes wrong.
 */
TaggerCache * create_tagger_cache(ItemCache * item_cache, TaggerCacheOptions *opts) {
  TaggerCache *tagger_cache = calloc(1, sizeof(struct TAGGER_CACHE));
  
  if (tagger_cache) {
    tagger_cache->item_cache = item_cache;
    tagger_cache->options = opts;
    tagger_cache->tag_urls = NULL;
    tagger_cache->failed_tags = NULL;
    tagger_cache->taggers = NULL;
    tagger_cache->tag_urls_last_updated = -1;

    if (pthread_mutex_init(&tagger_cache->mutex, NULL)) {
      fatal("pthread_mutex_init error for tagger_cache");
      free(tagger_cache);
      tagger_cache = NULL;
    }
  } else {
    fatal("Could not allocate TaggerCache");
  }
  
  return tagger_cache;
}

static void setup_classification_functions(Tagger *tagger) {
  tagger->probability_function    = &naive_bayes_probability;
  tagger->classification_function = &naive_bayes_classify;
  tagger->get_clues_function      = &select_clues;
}

static Tagger * fetch_tagger(TagRetriever tag_retriever, const char * tag_training_url, time_t if_modified_since, char ** errmsg) {
  Tagger *tagger = NULL;
  
  if (tag_retriever == NULL) {
    fatal("tagger_cache->tag_retriever not set");
  } else {
    char *tag_document = NULL;
    int fetch_rc = tag_retriever(tag_training_url, if_modified_since, &tag_document, errmsg);

    if (fetch_rc == URL_OK && tag_document != NULL) {
      tagger = build_tagger(tag_document);

      if (tagger && tagger->state == TAGGER_LOADED) {
        /* Replace the training url with the url we actually used to fetch it,
         * i.e. don't trust the atom document to report this correctly.
         */
        free(tagger->training_url);
        tagger->training_url = strdup(tag_training_url);
        setup_classification_functions(tagger);
      } else if (tagger) {
        free_tagger(tagger);
        tagger = NULL;          
      } else {
        info("The tag document was badly formed");      
        if (errmsg) *errmsg = strdup("The tag document was badly formed");      
      }

      free(tag_document);    
    }
  }
  
  return tagger;
}

static int mark_as_checked_out(TaggerCache *tagger_cache, const char * tag_training_url) {
  debug("Checking out %s", tag_training_url);
  PWord_t tagger_pointer;
  
  JSLI(tagger_pointer, tagger_cache->checked_out_taggers, (const uint8_t*) tag_training_url);
  
  if (tagger_pointer != NULL) {
    *tagger_pointer = 1;
  } else {
    fatal("Malloc error allocating element in checked_out_taggers");
  }
  
  return 0;
}

static int is_checked_out(TaggerCache *tagger_cache, const char * tag_training_url) {
  int checked_out = false;
  PWord_t tagger_pointer = NULL;
  
  JSLG(tagger_pointer, tagger_cache->checked_out_taggers, (uint8_t*) tag_training_url);
  if (tagger_pointer) {
    checked_out = true;
  }
  
  return checked_out;
}

static Tagger * get_cached_tagger(TaggerCache *tagger_cache, const char * tag_training_url) {
  Tagger *tagger = NULL;
  PWord_t tagger_pointer = NULL;
  
  JSLG(tagger_pointer, tagger_cache->taggers, (uint8_t*) tag_training_url);
  if (tagger_pointer) {
    tagger = (Tagger*) (*tagger_pointer);
  }
  
  return tagger;
}

static int checkout_tagger(TaggerCache * tagger_cache, const char * tag_training_url, Tagger ** tagger) {
  int rc = TAGGER_OK;
  
  pthread_mutex_lock(&tagger_cache->mutex);
  if (is_checked_out(tagger_cache, tag_training_url)) {
    rc = TAGGER_CHECKED_OUT;
  } else {
    mark_as_checked_out(tagger_cache, tag_training_url);
    if (NULL == (*tagger = get_cached_tagger(tagger_cache, tag_training_url))) {
      rc = TAGGER_NOT_CACHED;
    }
  }
  pthread_mutex_unlock(&tagger_cache->mutex);
  
  return rc;
}

static int cache_tagger(TaggerCache * tagger_cache, Tagger * tagger) {
  PWord_t tagger_pointer;
  
  pthread_mutex_lock(&tagger_cache->mutex);
  JSLI(tagger_pointer, tagger_cache->taggers, (uint8_t*) tagger->training_url);
  pthread_mutex_unlock(&tagger_cache->mutex);
  
  if (tagger_pointer != NULL) {
    if (*tagger_pointer != 0) {
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

static int release_tagger_by_url(TaggerCache *tagger_cache, uint8_t * tag_url) {
  int rc;
  pthread_mutex_lock(&tagger_cache->mutex);
  rc = release_tagger_without_locks(tagger_cache, tag_url);
  pthread_mutex_unlock(&tagger_cache->mutex);
  return rc;
}

static int add_missing_entries_if_needed(ItemCache * item_cache, Tagger * tagger) {
  if (tagger && tagger->state == TAGGER_PARTIALLY_TRAINED) {
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
  }
  
  return 0;
}

/* Figure out what state of tagger we have, if any, to determine what we return:
 *
 *  temp_tagger == NULL                     -> TAGGER_NOT_FOUND
 *  state       == TAGGER_PARTIALLY_TRAINED -> TAGGER_PENDING_ITEM_ADDITION
 *  state       == TAGGER_PRECOMPUTED       -> TAGGER_OK and the tagger set
 *  other                                   -> UNKNOWN (BUG!)
 */
int determine_return_state(Tagger *tagger, char ** errmsg) {
  if (!tagger) {
    return TAG_NOT_FOUND;
  } else if (tagger->state == TAGGER_PRECOMPUTED) {
    return TAGGER_OK;
  } else if (tagger->state == TAGGER_PARTIALLY_TRAINED) {
    if (errmsg) *errmsg = strdup("Some items need to be cached");
    return TAGGER_PENDING_ITEM_ADDITION;    
  } else {
    if (errmsg) *errmsg = strdup("Unaccounted for tagger state");
    error("Unaccounted for tagger state: %i", tagger->state);
    return UNKNOWN;
  }
}

static int fetch_or_update_tagger(TagRetriever tag_retriever, const char *tag_url, Tagger **tagger, char ** errmsg) {
  int updated = 0;
  
  if (!(*tagger) && (*tagger = fetch_tagger(tag_retriever, tag_url, -1, errmsg))) {
    updated = 1;
  } else if (*tagger && (*tagger)->state != TAGGER_PARTIALLY_TRAINED) {
    /* The tagger is cached, so we need to see if it has been updated, but only if it has no pending items. */
    Tagger *updated_tagger = NULL;
    
    if ((updated_tagger = fetch_tagger(tag_retriever, tag_url, (*tagger)->updated, errmsg))) {
      updated = 1;
      *tagger = updated_tagger;          
    } else {
      debug("Tag %s not modified, using cached version", (*tagger)->training_url);
    }
  }
  
  return updated;
}

int get_tagger_without_fetching(TaggerCache *tagger_cache, const char * tag_training_url, Tagger ** tagger, char ** errmsg) {
  int rc = -1;
  *tagger = NULL;
  
  if (tagger_cache && tag_training_url) {
    int cache_rc = checkout_tagger(tagger_cache, tag_training_url, tagger);
    
    if (cache_rc == TAGGER_CHECKED_OUT) {
      rc = cache_rc;     
      if (errmsg) *errmsg = strdup(CHECKED_OUT_MSG);
    } else {
      prepare_tagger(*tagger, tagger_cache->item_cache);
      rc = determine_return_state(*tagger, errmsg);
      
      if (rc != TAGGER_OK) {
        release_tagger_by_url(tagger_cache, tag_training_url);
      }
    }
  }
  
  return rc;
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
  if (tagger) *tagger = NULL;
  
  if (tagger_cache && tag_training_url) {
    int tagger_is_new = false;
    Tagger *temp_tagger = NULL;
    
    int cache_rc = checkout_tagger(tagger_cache, tag_training_url, &temp_tagger);
    
    if (TAGGER_CHECKED_OUT == cache_rc) {
      if (errmsg) *errmsg = strdup(CHECKED_OUT_MSG);        
      rc = cache_rc;
    } else {
      tagger_is_new = fetch_or_update_tagger(tagger_cache->tag_retriever, tag_training_url, &temp_tagger, errmsg);
            
      if (temp_tagger) {
        prepare_tagger(temp_tagger, tagger_cache->item_cache);
      }
      
      if (tagger_is_new) {        
        cache_tagger(tagger_cache, temp_tagger);
        add_missing_entries_if_needed(tagger_cache->item_cache, temp_tagger);        
      }     
      
      rc = determine_return_state(temp_tagger, errmsg);
            
      if (rc != TAGGER_OK) {
        release_tagger_by_url(tagger_cache, (uint8_t*) tag_training_url);
      } else {
        if (tagger) *tagger = temp_tagger;
      }
    }
  }
  
  return rc;
}

int release_tagger(TaggerCache *tagger_cache, Tagger * tagger) {
  int rc = 1;
  if (tagger_cache && tagger) {
    debug("releasing tagger %s", tagger->training_url);
    rc = release_tagger_by_url(tagger_cache, (uint8_t*) tagger->training_url);
  }
  
  return rc;
}

int is_cached(TaggerCache *cache, const char * tag) {
  int cached = 0;
  
  if (cache && tag) {
    pthread_mutex_lock(&cache->mutex);
    
    if (get_cached_tagger(cache, tag)) {
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

static void mark_as_error(TaggerCache *tagger_cache, const char * tag) {
  pthread_mutex_lock(&tagger_cache->mutex);  
  PWord_t tag_pointer;
  JSLI(tag_pointer, tagger_cache->failed_tags, (uint8_t*) tag);
  pthread_mutex_unlock(&tagger_cache->mutex);
}

struct background_fetch_data {
  TaggerCache *tagger_cache;
  char * tag;
};

static void *background_fetcher(void *memo) {
  struct background_fetch_data *data = (struct background_fetch_data*) memo;
  debug("background fetcher started for %s", data->tag);
  
  Tagger *tagger = NULL;
  if (TAGGER_OK == get_tagger(data->tagger_cache, data->tag, &tagger, NULL)) {
    release_tagger(data->tagger_cache, tagger);
  } else {
    mark_as_error(data->tagger_cache, data->tag);
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
