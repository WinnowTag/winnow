/* Copyright (c) 2007 The Kaphan Foundation
 *
 * Possession of a copy of this file grants no permission or license
 * to use, modify, or create derivate works.
 *
 * Please contact info@peerworks.org for further information.
 */

#include <config.h>
#include <stdlib.h>
#include <string.h>
#include "httpd.h"
#include "cls_config.h"
#include "logging.h"

#define NOT_FOUND "<html><head><title>File not found</title></head><body>File not found</body></html>"

#ifdef HAVE_LIBMICROHTTPD
#include <microhttpd.h>

struct HTTPD {
  struct MHD_Daemon *mhd;
  Config *config;
  ClassificationEngine *ce;
};

/* This is the base response handler. It just returns a 404. */
static int process_request(void * ce_vp, struct MHD_Connection * connection,
                           const char * url, const char * method, 
                           const char * version, const char * upload_data,
                           unsigned int * upload_data_size, void **ignored){
  struct MHD_Response *response = MHD_create_response_from_data(strlen(NOT_FOUND), NOT_FOUND, MHD_NO, MHD_NO);
  int ret = MHD_queue_response(connection, MHD_HTTP_NOT_FOUND, response);
  MHD_destroy_response(response);
  return ret;
}

Httpd * httpd_start(Config *config, ClassificationEngine *ce) {
  Httpd *httpd = malloc(sizeof(Httpd));
  if (httpd) {
    httpd->config = config;
    httpd->ce = ce;
    
    HttpdConfig httpd_config;
    cfg_httpd_config(config, &httpd_config);
    
    httpd->mhd = MHD_start_daemon(MHD_USE_SELECT_INTERNALLY,
                                  httpd_config.port,
                                  NULL,
                                  NULL,
                                  process_request,
                                  ce,
                                  MHD_OPTION_END);
  }
  
  return httpd;
}

void httpd_stop(Httpd *httpd) {
  
}

#else
Httpd * httpd_start(Config *config, ClassificationEngine *ce) {
  fatal("Missing libmicrohttpd. Can't start embedded httpd server.");
}

void httpd_stop(Httpd *httpd) {
  
}

#endif

