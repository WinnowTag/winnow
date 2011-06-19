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

#include <string.h>
#include <libxml/tree.h>
#include <libxml/xpath.h>
#include <libxml/xpathInternals.h>
#include <libxml/uri.h>
#include "xml.h"
#include "logging.h"

char * get_element_value(xmlXPathContextPtr context, const char * path) {
  char *value = NULL;
  
  xmlXPathObjectPtr xp = xmlXPathEvalExpression(BAD_CAST path, context);
  if (!xmlXPathNodeSetIsEmpty(xp->nodesetval)) {
    if (xp->nodesetval->nodeTab[0]->content) {
      value = strdup((char*) xp->nodesetval->nodeTab[0]->content);
    }
  }
  
  xmlXPathFreeObject(xp);
  return value;
}

time_t get_element_value_time(xmlXPathContextPtr context, const char * path) {
  struct tm _tm;
  time_t _time = time(NULL);
  char * value = get_element_value(context, path);
  
  if (value) {
    if (NULL != strptime(value, "%Y-%m-%dT%H:%M:%S%Z", &_tm)) {
      _time = timegm(&_tm);
    } else if (NULL != strptime(value, "%Y-%m-%dT%H:%M:%S", &_tm)) {
      _time = timegm(&_tm);
    } else {
      error("Couldn't parse datetime: %s", value);
    }
    
    free(value);
  }
  
  return _time;
}

double get_element_value_double(xmlXPathContextPtr context, const char * path) {
  double result = 0;
  char * value = get_element_value(context, path);
  
  if (value) {
    result = strtod(value, NULL);
    free(value);
  }
  
  return result;
}

char * get_attribute_value(xmlXPathContextPtr context, const char * path, const char * attr) {
  char *value = NULL;
  
  xmlXPathObjectPtr xp = xmlXPathEvalExpression(BAD_CAST path, context);
  if (!xmlXPathNodeSetIsEmpty(xp->nodesetval)) {
    value = (char*) xmlGetProp(xp->nodesetval->nodeTab[0], BAD_CAST attr);;
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
