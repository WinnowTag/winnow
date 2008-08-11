/* Copyright (c) 2007 The Kaphan Foundation
 *
 * Possession of a copy of this file grants no permission or license
 * to use, modify, or create derivate works.
 *
 * Please contact info@peerworks.org for further information.
 */

#include "hmac_sign.h"


#include "tagger.h"

#include <config.h>
#include <string.h>
#include <libxml/tree.h>
#include <libxml/xpath.h>
#include <libxml/xpathInternals.h>
#include <libxml/uri.h>
#include <curl/curl.h>
#include "xml.h"
#include "uri.h"
#include "logging.h"
#include "hmac_sign.h"


/************************************************************************************************
 *  Functions for building taggers from Atom documents.
*************************************************************************************************/

#define OK 0
#define FAIL 1

static int load_negative_examples(Tagger * tagger, xmlXPathContextPtr ctx) {
  int rc = OK;
  
  /* Get the negative taggings */
  xmlXPathObjectPtr xp = xmlXPathEvalExpression(BAD_CAST "/atom:feed/atom:entry/atom:link[@rel = "
                                                         "'http://peerworks.org/classifier/negative-example']"
                                                         "/../atom:id/text()", ctx);
  
  if (!xmlXPathNodeSetIsEmpty(xp->nodesetval)) {
    int i;
    tagger->negative_example_count = xp->nodesetval->nodeNr;
    tagger->negative_examples = calloc(tagger->negative_example_count, sizeof(char*));
    
    
    if (NULL == tagger->negative_examples) {
      rc = FAIL;
    } else {
      for (i = 0; i < tagger->negative_example_count; i++) {
        tagger->negative_examples[i] = strdup((char*) xp->nodesetval->nodeTab[i]->content);
      }
    }
  }
  
  xmlXPathFreeObject(xp);  
  
  return rc;  
}

/** Loads the positive example ids from the document. */
static int load_positive_examples(Tagger *tagger, xmlXPathContextPtr ctx) {
  int rc = OK;
  
  xmlXPathObjectPtr xp = xmlXPathEvalExpression(BAD_CAST "/atom:feed/atom:entry/atom:category/../atom:id/text()", ctx);
  if (!xmlXPathNodeSetIsEmpty(xp->nodesetval)) {
    int i;
    tagger->positive_example_count = xp->nodesetval->nodeNr;
    tagger->positive_examples = calloc(tagger->positive_example_count, sizeof(char*));
    
    if (NULL == tagger->positive_examples) {
      rc = FAIL;
    } else {
      for (i = 0; i < tagger->positive_example_count; i++) {
        tagger->positive_examples[i] = strdup((char*) xp->nodesetval->nodeTab[i]->content);
      }
    }    
  }
  xmlXPathFreeObject(xp);
  
  return rc;
}

/** Builds a Tagger from an atom document.
 *
 * This will parse the atom document given by the 'atom' parameter and
 * return a Tagger that contains the definition for the tag described
 * in the document.
 *
 * TODO Document the atom format somewhere.
 * TODO Handle bad XML.
 */
Tagger * build_tagger(const char * atom) {
  Tagger * tagger = calloc(1, sizeof(struct TAGGER));
  
  if (tagger) {
    xmlDocPtr doc = xmlParseDoc(BAD_CAST atom);
    
    if (doc) {
      xmlXPathContextPtr ctx = xmlXPathNewContext(doc);
      xmlXPathRegisterNs(ctx, BAD_CAST "atom", BAD_CAST ATOM);
      xmlXPathRegisterNs(ctx, BAD_CAST "classifier", BAD_CAST CLASSIFIER);
      
      tagger->tag_id = get_element_value(ctx, "/atom:feed/atom:id/text()");
      tagger->training_url = get_attribute_value(ctx, "/atom:feed/atom:link[@rel = 'self']", "href");
      tagger->classifier_taggings_url = get_attribute_value(ctx, "/atom:feed/atom:link[@rel = 'http://peerworks.org/classifier/edit']", "href");
      tagger->term = get_attribute_value(ctx, "/atom:feed/atom:category", "term");
      tagger->scheme = get_attribute_value(ctx, "/atom:feed/atom:category", "scheme");
      tagger->updated = get_element_value_time(ctx, "/atom:feed/atom:updated/text()");
      tagger->last_classified = get_element_value_time(ctx, "/atom:feed/classifier:classified/text()");
      tagger->bias = get_element_value_double(ctx, "/atom:feed/classifier:bias/text()");
      
      // TODO Validate the above!
      
      if (FAIL == load_positive_examples(tagger, ctx)) {
        // TODO unwind and return NULL
      }  else if (FAIL == load_negative_examples(tagger, ctx)) {
        // TODO unwind and return NULL
      }
            
      xmlXPathFreeContext(ctx);
      xmlFreeDoc(doc);
      
      tagger->state = TAGGER_LOADED;
      tagger->atom = strdup(atom);
    } else {
      debug("Got bad xml back from tag url: %s", atom);
      free(tagger);
      tagger = NULL;
    }    
  }
  
  return tagger;
}

static void train_pool(Pool * pool, ItemCache * item_cache, char ** examples, 
                       int size, char ** missing_examples, int * missing_size) {
  int i;
  *missing_size = 0;
  
  for (i = 0; i < size; i++) {
    int free_when_done = 0;
    Item * item = item_cache_fetch_item(item_cache, (unsigned char*) examples[i], &free_when_done);
    if (item) {
      pool_add_item(pool, item);
      if (free_when_done) free_item(item);
    } else {
      missing_examples[(*missing_size)++] = examples[i];
    }
  }
}

static int train(Tagger * tagger, ItemCache * item_cache) {
  tagger->state = TAGGER_TRAINED;    
  tagger->positive_pool = new_pool();
  tagger->negative_pool = new_pool();
  // Allocate enough space to store all the negative examples in the missing_positive_examples array
  tagger->missing_negative_examples = calloc(tagger->negative_example_count, sizeof(char*));
  // Allocate enough space to store all the positive examples in the missing_positive_examples array
  tagger->missing_positive_examples = calloc(tagger->positive_example_count, sizeof(char*));

  train_pool(tagger->positive_pool, item_cache, 
             tagger->positive_examples, 
             tagger->positive_example_count, 
             tagger->missing_positive_examples, 
            &tagger->missing_positive_example_count);
  train_pool(tagger->negative_pool, item_cache, 
             tagger->negative_examples, 
             tagger->negative_example_count,
             tagger->missing_negative_examples, 
            &tagger->missing_negative_example_count);
           
  if (tagger->missing_positive_example_count > 0 || tagger->missing_negative_example_count > 0) {
    tagger->state = TAGGER_PARTIALLY_TRAINED;
  } else {
    free(tagger->missing_positive_examples);
    free(tagger->missing_negative_examples);
    tagger->missing_positive_examples = NULL;
    tagger->missing_negative_examples = NULL;
  }
  
  return tagger->state;
}

static int partially_train(Tagger * tagger, ItemCache * item_cache) {
  char ** missing_positive_examples = calloc(tagger->positive_example_count, sizeof(char*));
  char ** missing_negative_examples = calloc(tagger->negative_example_count, sizeof(char*));
  int missing_positive_example_count = 0;
  int missing_negative_example_count = 0;
  
  train_pool(tagger->positive_pool, item_cache,
             tagger->missing_positive_examples,
             tagger->missing_positive_example_count,
             missing_positive_examples,
             &missing_positive_example_count);
  train_pool(tagger->negative_pool, item_cache,
            tagger->missing_negative_examples,
            tagger->missing_negative_example_count,
            missing_negative_examples,
            &missing_negative_example_count);
            
  if (missing_positive_example_count == 0 && missing_negative_example_count == 0) {
    tagger->state = TAGGER_TRAINED;
    tagger->missing_positive_example_count = tagger->missing_negative_example_count = 0;
    free(tagger->missing_positive_examples);
    free(tagger->missing_negative_examples);
    tagger->missing_positive_examples = tagger->missing_negative_examples = NULL;
    free(missing_positive_examples);
    free(missing_negative_examples);
  } else {
    tagger->state = TAGGER_PARTIALLY_TRAINED;    
    tagger->missing_positive_example_count = missing_positive_example_count;
    tagger->missing_negative_example_count = missing_negative_example_count;
    free(tagger->missing_positive_examples);
    free(tagger->missing_negative_examples);
    
    if (tagger->missing_positive_example_count > 0) {
      tagger->missing_positive_examples = missing_positive_examples;
    } else {
      tagger->missing_positive_examples = NULL;
      free(missing_positive_examples);
    }
    
    if (tagger->missing_negative_example_count > 0) {
      tagger->missing_negative_examples = missing_negative_examples;      
    } else {
      tagger->missing_negative_examples = NULL;
      free(missing_negative_examples);
    }
  }
  
  return tagger->state;
}

/** Trains the tagger using it's examples.
 *
 *  This will build the positive and negative pools for the the tagger.
 *  If this is successful the state of the tagger will be set to 
 *  TAGGER_TRAINED and postive_pool and negative_pool members will
 *  be trained up with the items in the positive and negative examples.
 *
 *  This function uses the item_cache to get the items.  If an example
 *  is not in the item cache it's id will be recorded in either the
 *  missing_positive_examples or missing_negative_examples array. 
 *  It will train as many items that it can and only store missing ids
 *  in the missing id arrays.  If this happens the tagger state will be
 *  set to TAGGER_PARTIALLY_TRAINED and this will be returned.
 *
 *  If a Tagger in a PARTIAL_TRAINED state is passed in, this function
 *  attempt to fetch the missing items from the item cache and add them
 *  to the pools.  If all the missing items are found the Tagger will
 *  change to the TAGGER_TRAINED state, if some missing items are still
 *  missing, the tagger will stay in the TAGGER_PARTIALLY_TRAINED state,
 *  the misisng example counts and arrays will be updated to reflect the
 *  items that are still missing and the items that were found will be
 *  added to the pools.
 *
 *  @params tagger The tagger to train.
 *  @params item_cache The item cache to get the items from.
 *  @return The new state of the tagger.
 */
TaggerState train_tagger(Tagger * tagger, ItemCache * item_cache) {
  TaggerState state = UNKNOWN;
  
  if (tagger && item_cache) {
    switch (tagger->state) {
      case TAGGER_LOADED:
        state = train(tagger, item_cache);
        break;
      case TAGGER_PARTIALLY_TRAINED:
        state = partially_train(tagger, item_cache);
        break;
      default:
        error("Tried to train an already trained tag.  This is probably programmer error.");
        state = TAGGER_SEQUENCE_ERROR;
      break;
    }
  }
  
  return state;
}

/** Precomputes the tagger's clues.
 *
 *  This function expects a tagger in the TRAINED state.
 *
 *  This will create and fill the clues list with probabilities for all
 *  tokens in all the pools in the tagger.  It uses the tagger's probability
 *  function to generate the probability for each token.
 *
 *  Once complete the tagger will be in the PRECOMPUTED state, the positive
 *  and negative pools will have been free'd and set to NULL and the tagger
 *  can be used to classify items.
 */
TaggerState precompute_tagger(Tagger * tagger, const Pool * random_background) {
  TaggerState state = TAGGER_SEQUENCE_ERROR;
  
  if (tagger && tagger->state == TAGGER_TRAINED && tagger->probability_function != NULL) {
    Token working_token;
    state = tagger->state = TAGGER_PRECOMPUTED;
    tagger->clues = new_clue_list();
        
    for (working_token.id = 0; pool_next_token(random_background, &working_token); ) {
      double probability = tagger->probability_function(tagger->positive_pool, tagger->negative_pool, 
                                                        random_background, working_token.id, tagger->bias);
      add_clue(tagger->clues, working_token.id, probability);
    }
    
    for (working_token.id = 0; pool_next_token(tagger->positive_pool, &working_token); ) {
      if (NULL == get_clue(tagger->clues, working_token.id)) {
        double probability = tagger->probability_function(tagger->positive_pool, tagger->negative_pool, 
                                                          random_background, working_token.id, tagger->bias);
        add_clue(tagger->clues, working_token.id, probability);        
      }
    }
    
    for (working_token.id = 0; pool_next_token(tagger->negative_pool, &working_token); ) {
      if (NULL == get_clue(tagger->clues, working_token.id)) {
        double probability = tagger->probability_function(tagger->positive_pool, tagger->negative_pool, 
                                                          random_background, working_token.id, tagger->bias);
        add_clue(tagger->clues, working_token.id, probability);
      }
    }
    
    free_pool(tagger->positive_pool);
    free_pool(tagger->negative_pool);
    tagger->positive_pool = NULL;
    tagger->negative_pool = NULL;
  }
  
  return state;
}

int classify_item(const Tagger *tagger, const Item *item, double * probability) {
  int rc = TAGGER_SEQUENCE_ERROR;
  
  if (tagger && tagger->state == TAGGER_PRECOMPUTED && tagger->classification_function != NULL && probability != NULL) {
    rc = TAGGER_OK;
    *probability = tagger->classification_function(tagger->clues, item);
  }
  
  return rc;
}

Clue ** get_clues(const Tagger *tagger, const Item *item, int *num) {
  Clue **clues = NULL;
  
  if (tagger && item && tagger->state == TAGGER_PRECOMPUTED && tagger->get_clues_function) {
    clues = tagger->get_clues_function(tagger->clues, item, num);
  }
  
  return clues;
}

struct output {
  int pos;
  int size;
  char *data;
};

static size_t curl_read_function(void *ptr, size_t size, size_t nmemb, void *stream) {
  struct output *out = (struct output*) stream;
  unsigned int write = size * nmemb;
  
  if (write > out->size - out->pos) {
    write = out->size - out->pos;
  }
  
  debug("read pos %i of %i, will write %i", out->pos, out->size, write);
  
  if (write > 0) {
    memcpy(ptr, &out->data[out->pos], write);
    out->pos += write;
  }
  
  return write;
}

static int xml_for_tagger(const Tagger *tagger, const Array *list, struct output * out) {
  xmlDocPtr doc = xmlNewDoc(BAD_CAST "1.0");
  xmlNodePtr feed = xmlNewNode(NULL, BAD_CAST "feed");
  xmlNsPtr classifier_ns = xmlNewNs(feed, BAD_CAST CLASSIFIER, BAD_CAST "classifier");
  xmlNewProp(feed, BAD_CAST "xmlns", BAD_CAST ATOM);
  xmlDocSetRootElement(doc, feed);
  
  if (tagger->tag_id) {
    xmlNewChild(feed, NULL, BAD_CAST "id", BAD_CAST tagger->tag_id);    
  }
  
  char timebuf[24];
  struct tm tm_time;
  memset(&tm_time, 0, sizeof(tm_time));
  gmtime_r(&tagger->last_classified, &tm_time);
  strftime(timebuf, sizeof(timebuf), "%Y-%m-%dT%H:%M:%SZ", &tm_time);
  xmlNewChild(feed, classifier_ns, BAD_CAST "classified", BAD_CAST timebuf);
  
  int i;
  for (i = 0; i < list->size; i++) {
    Tagging *tagging = (Tagging*) list->elements[i];
    xmlNodePtr entry = xmlNewChild(feed, NULL, BAD_CAST "entry", NULL);
    xmlNewChild(entry, NULL, BAD_CAST "id", BAD_CAST tagging->item_id);
    xmlNodePtr category = xmlNewChild(entry, NULL, BAD_CAST "category", NULL);
    xmlNewProp(category, BAD_CAST "term", BAD_CAST tagger->term);
    xmlNewProp(category, BAD_CAST "scheme", BAD_CAST tagger->scheme);
    char buffer[24];
    snprintf(buffer, 24, "%.6f", tagging->strength);
    xmlNewNsProp(category, classifier_ns, BAD_CAST "strength", BAD_CAST buffer);
  }
  
  xmlDocDumpFormatMemory(doc, (xmlChar **) &out->data, &out->size, 1);
  xmlFreeDoc(doc);
  
  return 0;
}

#define PUT 1
#define POST 2

static int save_taggings(const Tagger *tagger, Array *taggings, int method, const Credentials * credentials, char ** errmsg) {
  int rc;
  
  if (tagger && tagger->classifier_taggings_url) {
    info("save_taggings: %s", tagger->classifier_taggings_url);
    char curlerr[CURL_ERROR_SIZE];
    struct curl_slist *http_headers = NULL;
    struct output tagger_xml;
    memset(&tagger_xml, 0, sizeof(tagger_xml));
    xml_for_tagger(tagger, taggings, &tagger_xml);
    debug("XML is %i but lixml says it is %i ", strlen(tagger_xml.data), tagger_xml.size);

    http_headers = curl_slist_append(http_headers, "Content-Type: application/atom+xml");
    http_headers = curl_slist_append(http_headers, "Expect:");
    http_headers = curl_slist_append(http_headers, "Connection: close");
    
    
    if (valid_credentials(credentials)) {
      char *method_s = method == PUT ? "PUT" : method == POST ? "POST" : "";
      xmlURIPtr uri = xmlParseURIRaw(tagger->classifier_taggings_url, 1);
      if (uri) {
        http_headers = hmac_sign(method_s, uri->path, http_headers, credentials);
        xmlFreeURI(uri);
      }
    } else {
      debug("No credentials provided");
    }
    
    CURL *curl = curl_easy_init();
    
    char ua[512];
    snprintf(ua, sizeof(ua), "PeerworksClassifier/%s %s", PACKAGE_VERSION, curl_version());
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, http_headers);    
    curl_easy_setopt(curl, CURLOPT_USERAGENT, ua);
    curl_easy_setopt(curl, CURLOPT_URL, tagger->classifier_taggings_url);
    //curl_easy_setopt(curl, CURLOPT_VERBOSE, 1);
    curl_easy_setopt(curl, CURLOPT_HTTP_VERSION, CURL_HTTP_VERSION_1_1);
    curl_easy_setopt(curl, CURLOPT_ERRORBUFFER, curlerr);
    
    if (method == PUT) {
      curl_easy_setopt(curl, CURLOPT_INFILESIZE, tagger_xml.size);
      curl_easy_setopt(curl, CURLOPT_UPLOAD, 1);
      curl_easy_setopt(curl, CURLOPT_READFUNCTION, &curl_read_function);
      curl_easy_setopt(curl, CURLOPT_READDATA, &tagger_xml);
    } else if (method == POST) {      
      curl_easy_setopt(curl, CURLOPT_POSTFIELDS, tagger_xml.data);
      curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, tagger_xml.size);
    } else {
      fatal("Got something other than PUT or POST for method. This is a bug.");
      rc = FAIL;
    }
        
    if (rc != FAIL && curl_easy_perform(curl)) {
      error("URL %s not accessible: %s", tagger->classifier_taggings_url, curlerr);
      rc = FAIL;
    } else {
      debug("save_taggings success");
      rc = OK;
    }
  
    curl_easy_cleanup(curl);
    curl_slist_free_all(http_headers);
    free(tagger_xml.data);
    
    info("save_taggings complete");
  }
  
  return rc;
}

int replace_taggings(const Tagger * tagger, Array *taggings, const Credentials * credentials, char **errmsg) {
  return save_taggings(tagger, taggings, PUT, credentials, errmsg);
}

int update_taggings(const Tagger * tagger, Array *taggings, const Credentials * credentials, char **errmsg) {
  return save_taggings(tagger, taggings, POST, credentials, errmsg);
}

/* Creates a ItemCacheEntry from an entry in this Tagger's atom document.
 *
 * This is used by get_missing_entries to create ItemCacheEntry objects for
 * each of the items in the tag that were missing from the ItemCache.
 */ 
static ItemCacheEntry * create_entry(xmlXPathContextPtr ctx, char * entry_id) {
  ItemCacheEntry * entry = NULL;
  char xpath[265]; // TODO is this big enough for the xpath?
  snprintf(xpath, 256, "/atom:feed/atom:entry/atom:id[text() = '%s']/..", entry_id);
    
  xmlXPathObjectPtr xp = xmlXPathEvalExpression(BAD_CAST xpath, ctx);

  if (!xmlXPathNodeSetIsEmpty(xp->nodesetval)) {
    debug("Creating entry for missing item %s", entry_id);
    
    /* Create a temporary XML document and copy the entry node into it so
     * we can use the create_entry_from_atom_xml_document function which
     * expects an entry element as the root node of the document.
     */
    xmlChar *atom;
    int size;
    xmlDocPtr doc = xmlNewDoc(BAD_CAST "1.0");
    xmlDocSetRootElement(doc, xmlDocCopyNode(xp->nodesetval->nodeTab[0], doc, 1));
    xmlDocDumpFormatMemory(doc, &atom, &size, 1);
    entry = create_entry_from_atom_xml_document(0, doc, atom);
    xmlFreeDoc(doc);
    xmlFree(atom);
    
    if (!entry) {
      error("Couldn't create entry for %s", entry_id);
    }
  } else {
    fatal("missing item %s in atom document - this should not happen", entry_id);
  }
    
  xmlXPathFreeObject(xp);

  return entry;
}

/** Gets ItemCacheEntry objects for all the items that are missing
 *  from the item cache.
 *
 *  @param tagger The tagger to get the missing items from. This should
 *         be a tagger that is partially trained. If the tagger is not
 *         partially trained or had no missing items, this will be a no-op. 
 *  @param entries A array of ItemCacheEntry* that will be filled with
 *         the ItemCacheEntries for the missing items.  This should
 *         be big enough to hold all the missing items in the tagger, i.e.
 *         it should be tagger->missing_positive_example_count + 
 *         tagger->missing_negative_example_count in size.
 */
int get_missing_entries(Tagger * tagger, ItemCacheEntry ** entries) {
  int rc = OK;
  
  if (tagger && tagger->state == TAGGER_PARTIALLY_TRAINED) {
    int i;
    
     xmlDocPtr doc = xmlParseDoc(BAD_CAST tagger->atom);
     xmlXPathContextPtr ctx = xmlXPathNewContext(doc);
     xmlXPathRegisterNs(ctx, BAD_CAST "atom", BAD_CAST ATOM);
    
    for (i = 0; i < tagger->missing_positive_example_count; i++) {
      entries[i] = create_entry(ctx, tagger->missing_positive_examples[i]);
    }
    
    for (i = 0; i < tagger->missing_negative_example_count; i++) {
      entries[i + tagger->missing_positive_example_count] = create_entry(ctx, tagger->missing_negative_examples[i]);
    }
    
    xmlXPathFreeContext(ctx);
    xmlFreeDoc(doc);
  } else {
    rc = FAIL;
  }
  
  return rc;
}

TaggerState prepare_tagger(Tagger *tagger, ItemCache *item_cache) {
  if (tagger && item_cache) {
    if (tagger->state != TAGGER_PRECOMPUTED) {      
      if (TAGGER_TRAINED == tagger->state || TAGGER_TRAINED == train_tagger(tagger, item_cache)) {        
        precompute_tagger(tagger, item_cache_random_background(item_cache));
      }
    }
    
    return tagger->state;
  }
  
  return TAGGER_SEQUENCE_ERROR;
}

void free_tagger(Tagger * tagger) {
  if (tagger) {
    if (tagger->tag_id)                  free(tagger->tag_id);
    if (tagger->training_url)            free(tagger->training_url);
    if (tagger->classifier_taggings_url) free(tagger->classifier_taggings_url);
    if (tagger->term)                    free(tagger->term);
    if (tagger->scheme)                  free(tagger->scheme);
     
    if (tagger->positive_examples) {
      int i;
      for (i = 0; i < tagger->positive_example_count; i++) {
        free(tagger->positive_examples[i]);
      }
      
      free(tagger->positive_examples);
    }
    
    if (tagger->negative_examples) {
      int i;
      for (i = 0; i < tagger->negative_example_count; i++) {
        free(tagger->negative_examples[i]);
      }
      
      free(tagger->negative_examples);
    }
    
    if (tagger->missing_positive_examples) free(tagger->missing_positive_examples);
    if (tagger->missing_negative_examples) free(tagger->missing_negative_examples);
    
    if (tagger->positive_pool) free_pool(tagger->positive_pool);
    if (tagger->negative_pool) free_pool(tagger->negative_pool);
    
    if (tagger->clues) free_clue_list(tagger->clues);
    if (tagger->atom) free(tagger->atom);
    
    free(tagger);
  }
}
