/* 
 * File:   hmac_credentials.h
 * Author: seangeo
 *
 * Created on August 7, 2008, 12:34 PM
 */

#ifndef _HMAC_CREDENTIALS_H
#define	_HMAC_CREDENTIALS_H

#ifdef	__cplusplus
extern "C" {
#endif

#define valid_credentials(c) (c && c->access_id && c->secret_key)
  
typedef struct CREDENTIALS {
  const char * access_id;
  const char * secret_key;
} Credentials;


#ifdef	__cplusplus
}
#endif

#endif	/* _HMAC_CREDENTIALS_H */

