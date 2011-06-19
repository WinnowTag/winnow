// General info: http://doc.winnowtag.org/open-source
// Source code repository: http://github.com/winnowtag
// Questions and feedback: contact@winnowtag.org
//
// Copyright (c) 2007-2011 The Kaphan Foundation
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in
// all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
// THE SOFTWARE.

// contact@winnowtag.org

#include <stdlib.h>
#include <string.h>
#include <libxml/tree.h>
#include <libxml/xpath.h>
#include <libxml/xpathInternals.h>
#include <libxml/uri.h>
#include "tagger.h"
#include "logging.h"
#include "xml.h"

int parse_tag_index(const char * document, Array * a, time_t * updated) {
  int rc = TAG_INDEX_OK;
  
  if (document && a) {
    xmlDocPtr doc = xmlReadMemory(document, strlen(document), "", NULL, XML_PARSE_COMPACT);
    if (doc) {
      xmlXPathContextPtr ctx = xmlXPathNewContext(doc);
      xmlXPathRegisterNs(ctx, BAD_CAST "atom", BAD_CAST ATOM);
      xmlXPathRegisterNs(ctx, BAD_CAST "classifier", BAD_CAST CLASSIFIER);
      
      if (updated) {
        *updated = get_element_value_time(ctx, "/atom:feed/atom:updated/text()");
      }
      
      xmlXPathObjectPtr xp = xmlXPathEvalExpression(BAD_CAST "/atom:feed/atom:entry/atom:link[@rel = 'http://peerworks.org/classifier/training']", ctx);
      
      if (!xmlXPathNodeSetIsEmpty(xp->nodesetval)) {
        int i;    
        for (i = 0; i < xp->nodesetval->nodeNr; i++) {
          xmlNodePtr node = xp->nodesetval->nodeTab[i];
          char *url = xmlGetProp(node, BAD_CAST "href");
          arr_add(a, url);
        }
        
        xmlXPathFreeObject(xp);
        xmlXPathFreeContext(ctx);
        xmlFreeDoc(doc);        
      } else {
        info("No tags in tag index");
      }      
    } else {
      error("Could not parse tag index: %s", document);
      rc = TAG_INDEX_FAIL;
    }
  }
  
  return rc;
}
