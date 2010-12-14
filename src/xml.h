// Copyright (c) 2007-2010 The Kaphan Foundation
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
