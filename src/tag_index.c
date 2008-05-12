/* Copyright (c) 2007 The Kaphan Foundation
 *
 * Possession of a copy of this file grants no permission or license
 * to use, modify, or create derivate works.
 *
 * Please contact info@peerworks.org for further information.
 */

#include <stdlib.h>
#include <string.h>
#include <libxml/tree.h>
#include <libxml/xpath.h>
#include <libxml/xpathInternals.h>
#include <libxml/uri.h>
#include "tagger.h"
#include "logging.h"
#include "xml.h"

int parse_tag_index(const char * document, Array * a) {
  int rc = TAG_INDEX_OK;
  
  if (document && a) {
    xmlDocPtr doc = xmlReadMemory(document, strlen(document), "", NULL, XML_PARSE_COMPACT);
    if (doc) {
      xmlXPathContextPtr ctx = xmlXPathNewContext(doc);
      xmlXPathRegisterNs(ctx, BAD_CAST "atom", BAD_CAST ATOM);
      xmlXPathRegisterNs(ctx, BAD_CAST "classifier", BAD_CAST CLASSIFIER);
      
      xmlXPathObjectPtr xp = xmlXPathEvalExpression(BAD_CAST "/atom:feed/atom:entry/atom:link[@rel = 'http://peerworks.org/classifier/training']", ctx);
      
      if (!xmlXPathNodeSetIsEmpty(xp->nodesetval)) {
        int i;    
        for (i = 0; i < xp->nodesetval->nodeNr; i++) {
          xmlNodePtr node = xp->nodesetval->nodeTab[i];
          char *url = strdup(xmlGetProp(node, BAD_CAST "href"));
          arr_add(a, url);
        }
      } else {
        info("No tags in tag index");
      }   
    } else {
      error("Could not parse tag index");
      rc = TAG_INDEX_FAIL;
    }
  }
  
  return rc;
}
