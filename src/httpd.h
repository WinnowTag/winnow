/* Copyright (c) 2007 The Kaphan Foundation
 *
 * Possession of a copy of this file grants no permission or license
 * to use, modify, or create derivate works.
 *
 * Please contact info@peerworks.org for further information.
 */

#ifndef HTTPD_H_
#define HTTPD_H_

#include "classification_engine.h"

typedef struct HTTPD Httpd;

typedef struct HTTP_CONFIG {
  int port;
  const char *allowed_ip;
} HttpConfig;

extern Httpd * httpd_start(HttpConfig *config, ClassificationEngine *ce, ItemCache *item_cache);
extern void    httpd_stop (Httpd *httpd);
#endif /*HTTPD_H_*/
