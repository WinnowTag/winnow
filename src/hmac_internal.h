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

#ifndef _HMAC_INTERNAL_H
#define	_HMAC_INTERNAL_H

#ifdef	__cplusplus
extern "C" {
#endif

#include <curl/curl.h>
  
#define GET "GET"
#define PUT "PUT"
#define POST "POST"
#define DELETE "DELETE"

extern char * canonical_string(const char * method, const char * path, const struct curl_slist *headers);
extern char * build_signature(const char * method, const char * path, const struct curl_slist *headers, const char * secret);
extern char * get_header(const struct curl_slist *header, const char * prefix, int prefix_size);

#ifdef	__cplusplus
}
#endif

#endif	/* _HMAC_INTERNAL_H */

