/* Copyright (c) 2007 The Kaphan Foundation
 *
 * Possession of a copy of this file grants no permission or license
 * to use, modify, or create derivate works.
 *
 * Please contact info@peerworks.org for further information.
 */


#ifndef _XML_H_
#define _XML_H_

char * get_element_value(xmlXPathContextPtr context, const char * path);
char * get_attribute_value(xmlXPathContextPtr context, const char * path, const char * attr);
xmlNodePtr add_element(xmlNodePtr parent, const char * name, const char * type, const char * fmt, ...);
int url_fragment_as_int(const char *uri);
#endif /* _XML_H_ */
