/* Copyright (c) 2007 The Kaphan Foundation
 *
 * Possession of a copy of this file grants no permission or license
 * to use, modify, or create derivate works.
 *
 * Please contact info@peerworks.org for further information.
 */

#ifndef HTTPD_H_
#define HTTPD_H_

#include "cls_config.h"
#include "classification_engine.h"

typedef struct HTTPD Httpd;

extern Httpd * httpd_start(Config *config, ClassificationEngine *ce);
extern void    httpd_stop (Httpd *httpd);
#endif /*HTTPD_H_*/
