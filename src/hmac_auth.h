/* 
 * File:   hmac_auth.h
 * Author: seangeo
 *
 * Created on August 6, 2008, 2:56 PM
 */

#ifndef _HMAC_AUTH_H
#define	_HMAC_AUTH_H

#ifdef	__cplusplus
extern "C" {
#endif

#include <curl/curl.h>

extern int hmac_auth(const char * method, const char * path, struct curl_slist * headers, const char * access_id, const char * secret);

#ifdef	__cplusplus
}
#endif

#endif	/* _HMAC_AUTH_H */

