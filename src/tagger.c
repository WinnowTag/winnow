/* Copyright (c) 2007 The Kaphan Foundation
 *
 * Possession of a copy of this file grants no permission or license
 * to use, modify, or create derivate works.
 *
 * Please contact info@peerworks.org for further information.
 */

#include "tagger.h"

#include <string.h>
#include <libxml/tree.h>
#include <libxml/xpath.h>
#include <libxml/xpathInternals.h>
#include <libxml/uri.h>
#include "xml.h"
#include "uri.h"
#include "logging.h"


/************************************************************************************************
 *  Functions for building taggers from Atom documents.
*************************************************************************************************/

#define ATOM "http://www.w3.org/2005/Atom"
#define CLASSIFIER "http://peerworks.org/classifier"
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
    // Allocate enough space to store all the negative examples in the missing_positive_examples array
    tagger->missing_negative_examples = calloc(tagger->negative_example_count, sizeof(char*));
    
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
    // Allocate enough space to store all the positive examples in the missing_positive_examples array
    tagger->missing_positive_examples = calloc(tagger->positive_example_count, sizeof(char*));
    
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
      tagger->updated = get_element_value_time(ctx, "/atom:feed/atom:updated/text()");
      tagger->last_classified = get_element_value_time(ctx, "/atom:feed/classifier:classified/text()");
      tagger->bias = get_element_value_double(ctx, "/atom:feed/classifier:bias/text()");
      
      if (FAIL == load_positive_examples(tagger, ctx)) {
        // TODO unwind and return NULL
      }  else if (FAIL == load_negative_examples(tagger, ctx)) {
        // TODO unwind and return NULL
      }
            
      xmlXPathFreeContext(ctx);
      xmlFreeDoc(doc);
    }
    
    tagger->state = TAGGER_LOADED;
    tagger->atom = strdup(atom);
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
    }
    
    if (tagger->missing_negative_example_count > 0) {
      tagger->missing_negative_examples = missing_negative_examples;      
    } else {
      tagger->missing_negative_examples = NULL;
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
    xmlDocPtr doc = xmlNewDoc(BAD_CAST "1.0");
    xmlDocSetRootElement(doc, xmlDocCopyNode(xp->nodesetval->nodeTab[0], doc, 1));
    entry = create_entry_from_atom_xml_document(0, doc, NULL);
    xmlFreeDoc(doc);
      
    if (entry) {
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
