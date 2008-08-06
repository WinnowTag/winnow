/* Copyright (c) 2007 The Kaphan Foundation
 *
 * Possession of a copy of this file grants no permission or license
 * to use, modify, or create derivate works.
 *
 * Please contact info@peerworks.org for further information.
 */

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

