/* 
 * File:   http_responses.h
 * Author: seangeo
 *
 * Created on August 7, 2008, 2:12 PM
 */

#ifndef _HTTP_RESPONSES_H
#define	_HTTP_RESPONSES_H

#ifdef	__cplusplus
extern "C" {
#endif
#include "microhttpd/src/include/microhttpd.h"

#define HTTP_NOT_FOUND(response)         \
  response->code = 404;                  \
  response->content = NOT_FOUND;         \
  response->content_type = CONTENT_TYPE;

#define HTTP_BAD_XML(response) \
  response->code = MHD_HTTP_BAD_REQUEST; \
  response->content = BAD_XML; \
  response->content_type = CONTENT_TYPE;

#define HTTP_BAD_FEED(response) \
  response->code = MHD_HTTP_UNPROCESSABLE_ENTITY; \
  response->content = "Bad Feed"; \
  response->content_type = "text/plain";

#define HTTP_BAD_ENTRY(response) \
  response->code = MHD_HTTP_UNPROCESSABLE_ENTITY; \
  response->content = "<error>Bad Entry</error>"; \
  response->content_type = CONTENT_TYPE;

#define HTTP_ITEM_CACHE_ERROR(response, item_cache) \
  response->code = MHD_HTTP_INTERNAL_SERVER_ERROR; \
  response->content = (char*) item_cache_errmsg(item_cache); \
  response->content_type = "text/plain";

#define HTTP_ISE(response) \
  response->code = MHD_HTTP_INTERNAL_SERVER_ERROR; \
  response->content = "Internal Server Error"; \
  response->content_type = "text/plain";

#define HTTP_UNAUTHORIZED(response) \
  response->code = MHD_HTTP_UNAUTHORIZED; \
  response->content = "Unauthorized"; \
  response->content_type = "text/plain";
  
#ifdef	__cplusplus
}
#endif

#endif	/* _HTTP_RESPONSES_H */

