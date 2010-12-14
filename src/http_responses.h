/* Copyright (c) 2007-2010 The Kaphan Foundation
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */

/*
 * contact@winnowtag.org
 */

#ifndef _HTTP_RESPONSES_H
#define	_HTTP_RESPONSES_H

#ifdef	__cplusplus
extern "C" {
#endif

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

