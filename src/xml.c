/* Copyright (c) 2007 The Kaphan Foundation
 *
 * Possession of a copy of this file grants no permission or license
 * to use, modify, or create derivate works.
 *
 * Please contact info@peerworks.org for further information.
 */

#include <libxml/tree.h>
#include <libxml/xpath.h>
#include <libxml/xpathInternals.h>
#include <libxml/uri.h>

char * get_element_value(xmlXPathContextPtr context, const char * path) {
  char *value = NULL;
  
  xmlXPathObjectPtr xp = xmlXPathEvalExpression(BAD_CAST path, context);
  if (!xmlXPathNodeSetIsEmpty(xp->nodesetval)) {
    value = strdup((char*) xp->nodesetval->nodeTab[0]->content);
  }
  
  xmlXPathFreeObject(xp);
  return value;
}

char * get_attribute_value(xmlXPathContextPtr context, const char * path, const char * attr) {
  char *value = NULL;
  
  xmlXPathObjectPtr xp = xmlXPathEvalExpression(BAD_CAST path, context);
  if (!xmlXPathNodeSetIsEmpty(xp->nodesetval)) {
    value = (char*) xmlGetProp(xp->nodesetval->nodeTab[0], BAD_CAST "href");;
  }
  
  xmlXPathFreeObject(xp);
  return value;
}

xmlNodePtr add_element(xmlNodePtr parent, const char * name, const char * type, const char * fmt, ...) {
  char buffer[32];
  
  va_list argp;
  va_start(argp, fmt);
  vsnprintf(buffer, 16, fmt, argp);
  va_end(argp);
  
  xmlNodePtr node = xmlNewChild(parent, NULL, BAD_CAST name, BAD_CAST buffer);
  xmlNewProp(node, BAD_CAST "type", BAD_CAST type);
  
  return node;
}

int url_fragment_as_int(const char *uri) {
  int id = 0;
  
  if (uri) {
    xmlURIPtr id_uri = xmlParseURI(uri);

    if (id_uri && id_uri->fragment) {
      id = strtol(id_uri->fragment, NULL, 10);
    }

    xmlFreeURI(id_uri);
  }
  
  return id;
}
