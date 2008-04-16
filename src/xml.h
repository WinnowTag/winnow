/* Copyright (c) 2007 The Kaphan Foundation
 *
 * Possession of a copy of this file grants no permission or license
 * to use, modify, or create derivate works.
 *
 * Please contact info@peerworks.org for further information.
 */


#ifndef _XML_H_
#define _XML_H_
#include <time.h>

char * get_element_value(xmlXPathContextPtr context, const char * path);
char * get_attribute_value(xmlXPathContextPtr context, const char * path, const char * attr);
time_t get_element_value_time(xmlXPathContextPtr context, const char * path);
double get_element_value_double(xmlXPathContextPtr context, const char * path);

xmlNodePtr add_element(xmlNodePtr parent, const char * name, const char * type, const char * fmt, ...);
// TODO remove uri_fragment_as_int?
int url_fragment_as_int(const char *uri);
#endif /* _XML_H_ */
