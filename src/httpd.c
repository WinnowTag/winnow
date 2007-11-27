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
#include <errno.h>
#include "httpd.h"
#include "cls_config.h"
#include "logging.h"

#define CONTENT_TYPE "application/xml"
#define NOT_FOUND "<?xml version='1.0' ?>\n<errors><error>Resource not found.</error></errors>"
#define METHOD_NOT_ALLOWED "<?xml version='1.0' ?>\n<errors><error>Method is not allowed for that resource.</errors></errors>"
#define BAD_XML "<?xml version='1.0' ?>\n<error><error>Badly formatted XML.</error></errors>"
#define MISSING_TAG_ID "<?xml version='1.0' ?>\n<errors><error>Missing tag or user id in job description</error></errors>"

#ifdef HAVE_LIBMICROHTTPD
#include <microhttpd.h>
#include <libxml/tree.h>
#include <libxml/xpath.h>

#define LOG_RESPONSE(url, code) \
  debug("%s resulted in %i", url, code);

#define SEND_204(returncode) \
   LOG_RESPONSE(url, MHD_HTTP_NO_CONTENT); \
   struct MHD_Response *response = MHD_create_response_from_data(0, "", MHD_NO, MHD_NO); \
   MHD_add_response_header(response, MHD_HTTP_HEADER_CONTENT_TYPE, CONTENT_TYPE);        \
   returncode = MHD_queue_response(connection, MHD_HTTP_NO_CONTENT, response);           \
   MHD_destroy_response(response);

#define SEND_404(returncode) \
    LOG_RESPONSE(url, MHD_HTTP_NOT_FOUND); \
   struct MHD_Response *response = MHD_create_response_from_data(strlen(NOT_FOUND), NOT_FOUND, MHD_NO, MHD_NO); \
   MHD_add_response_header(response, MHD_HTTP_HEADER_CONTENT_TYPE, CONTENT_TYPE);                               \
   returncode = MHD_queue_response(connection, MHD_HTTP_NOT_FOUND, response);                                   \
   MHD_destroy_response(response);

#define SEND_405(returncode, allowed) \
   LOG_RESPONSE(url, MHD_HTTP_METHOD_NOT_ALLOWED); \
   struct MHD_Response *response = MHD_create_response_from_data(strlen(METHOD_NOT_ALLOWED), METHOD_NOT_ALLOWED, MHD_NO, MHD_NO); \
   MHD_add_response_header(response, MHD_HTTP_HEADER_CONTENT_TYPE, CONTENT_TYPE);                   \
   MHD_add_response_header(response, MHD_HTTP_HEADER_ALLOW, allowed);                               \
   returncode = MHD_queue_response(connection, MHD_HTTP_METHOD_NOT_ALLOWED, response);              \
   MHD_destroy_response(response);

#define SEND_415(returncode) \
   LOG_RESPONSE(url, MHD_HTTP_UNSUPPORTED_MEDIA_TYPE); \
   struct MHD_Response *response = MHD_create_response_from_data(strlen(BAD_XML), BAD_XML, MHD_NO, MHD_NO); \
   MHD_add_response_header(response, MHD_HTTP_HEADER_CONTENT_TYPE, CONTENT_TYPE);                   \
   returncode = MHD_queue_response(connection, MHD_HTTP_UNSUPPORTED_MEDIA_TYPE, response);          \
   MHD_destroy_response(response);

#define SEND_MISSING_TAG_ID(returncode) \
   debug("Missing tag_id sending %i", MHD_HTTP_UNPROCESSABLE_ENTITY); \
   struct MHD_Response *response = MHD_create_response_from_data(strlen(MISSING_TAG_ID), MISSING_TAG_ID, MHD_NO, MHD_NO); \
   MHD_add_response_header(response, MHD_HTTP_HEADER_CONTENT_TYPE, CONTENT_TYPE);                   \
   returncode = MHD_queue_response(connection, MHD_HTTP_UNPROCESSABLE_ENTITY, response);            \
   MHD_destroy_response(response);

#define SEND_XML_DATA(returncode, httpcode, data, location) \
  debug("Sending XML: %s", data); \
  struct MHD_Response *response = MHD_create_response_from_data(strlen(data), data, MHD_YES, MHD_YES); \
  MHD_add_response_header(response, MHD_HTTP_HEADER_CONTENT_TYPE, CONTENT_TYPE);                     \
  if (location) MHD_add_response_header(response, MHD_HTTP_HEADER_LOCATION, location);               \
  returncode = MHD_queue_response(connection, httpcode, response);                                   \
  MHD_destroy_response(response);

struct HTTPD {
  struct MHD_Daemon *mhd;
  Config *config;
  ClassificationEngine *ce;
};

struct posted_data {
  int size;
  char *buffer;
};

/*  <classification-job>
 *    <id>ID</id>
 *    <progress type="float">0.0</progress>
 *    <tag-id type="integer">N</tag-id>
 *  </classification-job>
 */
static xmlChar * xml_for_job(const ClassificationJob *job) {
  xmlChar *buffer = NULL;
  int buffersize;
  xmlDocPtr doc = xmlNewDoc(BAD_CAST "1.0");
  xmlNodePtr root = xmlNewNode(NULL, BAD_CAST "job");
  xmlDocSetRootElement(doc, root);
  
  xmlNodePtr id = xmlNewChild(root, NULL, BAD_CAST "id", BAD_CAST cjob_id(job));
  
  if (CJOB_TYPE_TAG_JOB == cjob_type(job)) {
    xmlChar tag_id_buffer[16];
    xmlStrPrintf(tag_id_buffer, 16, BAD_CAST "%d", cjob_tag_id(job));
    xmlNodePtr tag_id = xmlNewChild(root, NULL, BAD_CAST "tag-id", tag_id_buffer);
    xmlAttrPtr tag_id_type = xmlNewProp(tag_id, BAD_CAST "type", BAD_CAST "integer");    
  } else if (CJOB_TYPE_USER_JOB == cjob_type(job)) {
    xmlChar user_id_buffer[16];
    xmlStrPrintf(user_id_buffer, 16, BAD_CAST "%d", cjob_user_id(job));
    xmlNodePtr tag_id = xmlNewChild(root, NULL, BAD_CAST "user-id", user_id_buffer);
    xmlAttrPtr tag_id_type = xmlNewProp(tag_id, BAD_CAST "type", BAD_CAST "integer");
  }
  
  if (CJOB_STATE_ERROR == cjob_state(job)) {
    xmlNodePtr error_msg = xmlNewChild(root, NULL, BAD_CAST "error-message", cjob_error_msg(job));
  }
  
  xmlChar progress_buffer[16];
  xmlStrPrintf(progress_buffer, 16, BAD_CAST "%.1f", cjob_progress(job));  
  xmlNodePtr progress = xmlNewChild(root, NULL, BAD_CAST "progress", progress_buffer);
  xmlAttrPtr progress_type = xmlNewProp(progress, BAD_CAST "type", BAD_CAST "float");
  
  xmlNodePtr status = xmlNewChild(root, NULL, BAD_CAST "status", BAD_CAST cjob_state_msg(job));
  
  xmlDocDumpFormatMemory(doc, &buffer, &buffersize, 1);
  xmlFreeDoc(doc);
  
  return buffer;
}

static char * url_for_job(const ClassificationJob *job) {
  char *url = calloc(128, sizeof(char));
  snprintf(url, 128, "/classifier/jobs/%s", cjob_id(job));
  return url;
}

static const char * extract_job_id(const char * url) {
  char *job_id = NULL;
  char *last_slash = rindex(url, '/');
  char *string_end = rindex(url, '\0');
  
  if (last_slash && (string_end - last_slash) > sizeof(char)) {
    job_id = last_slash + sizeof(char);
  }
  
  return job_id;
}

/********************************************************************************
 * HTTP Interface Handlers
 ********************************************************************************/
static int start_job(ClassificationEngine *ce, struct MHD_Connection *connection, const char * url, const char *xml_input) {
  int ret;
  xmlDocPtr doc = xmlParseDoc(BAD_CAST xml_input);
          
  if (doc == NULL) {
    debug("XML was malformed: %s", xml_input);
    SEND_415(ret);
  } else {
    ClassificationJob *job = NULL;
    xmlXPathContextPtr context = xmlXPathNewContext(doc);    
    xmlXPathObjectPtr result = xmlXPathEvalExpression(BAD_CAST "/job/tag-id/text()", context);
    
    if (!xmlXPathNodeSetIsEmpty(result->nodesetval)) {
      int tag_id = (int) strtol((char *) result->nodesetval->nodeTab[0]->content, NULL, 10);
      info("Starting classification job for tag %i", tag_id);
      job = ce_add_classification_job_for_tag(ce, tag_id);
    } else {
      xmlXPathFreeObject(result);
      result = xmlXPathEvalExpression(BAD_CAST "/job/user-id/text()", context);
      
      if (!xmlXPathNodeSetIsEmpty(result->nodesetval)) {
        int user_id = (int) strtol((char *) result->nodesetval->nodeTab[0]->content, NULL, 10);
        info("Starting classification job for user %i", user_id);
        job = ce_add_classification_job_for_user(ce, user_id);
      } else {
        SEND_MISSING_TAG_ID(ret);        
      }     
    }
            
    xmlXPathFreeObject(result);
    xmlXPathFreeContext(context);
    
    if (job) {
      xmlChar *xml = xml_for_job(job);
      info("XML = %s", (char *) xml);
      char *url = url_for_job(job);
      SEND_XML_DATA(ret, 201, (char *) xml, url);
      xmlFree(xml);
      free(url);      
    }
  }
          
  xmlFreeDoc(doc);
  
  return ret;
}

static int job_handler(void * ce_vp, struct MHD_Connection * connection,
                        const char * url, const char * method, const char * version) {
  debug("job_status_handler %s", url);
  int ret;
  const char *job_id = extract_job_id(url);
  ClassificationEngine *ce = (ClassificationEngine*) ce_vp;
  ClassificationJob *job = NULL;
  
  if (job_id == NULL) {
    SEND_404(ret);
  } else if (NULL == (job = ce_fetch_classification_job(ce, job_id))) {
    SEND_404(ret);
  } else if (CJOB_STATE_CANCELLED == cjob_state(job)) {
    // Cancelled jobs are considered deleted to the outside world.
    SEND_404(ret);
  } else if (!strcmp("GET", method)) {
    xmlChar *xml = xml_for_job(job);
    SEND_XML_DATA(ret, MHD_HTTP_OK, (char*) xml, NULL);
    xmlFree(xml);
  } else if (!strcmp("DELETE", method)) {
    cjob_cancel(job);
    SEND_204(ret);
  } else {
    SEND_405(ret, "GET, DELETE");
  }
  
  return ret;
}

/* This handles collection of the XML document data for
 * starting a classification job.
 * 
 * It calls start_job when all the data is collected.
 */
static int start_job_handle(void * ce_vp, struct MHD_Connection * connection,
                             const char * url, const char * method, 
                             const char * version, const char * upload_data,
                             unsigned int * upload_data_size, void **memo) {
  int ret = MHD_NO;  
  
  if (strcmp(MHD_HTTP_METHOD_POST, method)) {
    SEND_405(ret, "POST");
  } else {
    if (*upload_data_size == 0) {     
      struct posted_data *pd = *memo;
      if (!pd) {
        SEND_415(ret);
      } else {
        ret = start_job(ce_vp, connection, url, pd->buffer);
        free(pd->buffer);
        free(pd); 
      }
    } else if (!(*memo)) {
      struct posted_data *pd;
      *memo = pd = malloc(sizeof(struct posted_data));      
      pd->buffer = calloc(*upload_data_size, sizeof(char));
      pd->size = *upload_data_size;
      strncpy(pd->buffer, upload_data, pd->size);
      debug("memoize: %s", pd->buffer);
      *upload_data_size = 0;
      ret = MHD_YES;
    } else {
      // TODO Need to handle incremental processing of POST'ed data
      error("Need to handle incremental processing of POST'ed data");
    }    
  }
  
  return ret;
}
/* This is the base response handler. It just returns a 404. */
static int process_request(void * ce_vp, struct MHD_Connection * connection,
                           const char * url, const char * method, 
                           const char * version, const char * upload_data,
                           unsigned int * upload_data_size, void **memo) {
  int ret;
  
  char processesed_url[1024];
  strncpy(processesed_url, url, sizeof(processesed_url));
  
  char *ext;
  if (ext = strcasestr(processesed_url, ".xml")) {
    ext[0] = '\0';
  }
  
  if (0 == strcmp(processesed_url, "/classifier/jobs/") || 
      0 == strcmp(processesed_url, "/classifier/jobs")) {
    info("%s %s size(%i) start_job", method, processesed_url, *upload_data_size);
    ret = start_job_handle(ce_vp, connection, processesed_url, method, version, upload_data, upload_data_size, memo);
  } else if (processesed_url == strstr(processesed_url, "/classifier/jobs/")) {
    info("%s %s size(%i) job", method, processesed_url, *upload_data_size);
    ret = job_handler(ce_vp, connection, processesed_url, method, version);
  } else {
    info("%s %s size(%i) 404", method, processesed_url, *upload_data_size);
    SEND_404(ret);
  }
  
  
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
    if (NULL == httpd->mhd) {
      fatal("Could not start httpd: %s", strerror(errno));
    }
  }
  
  return httpd;
}

void httpd_stop(Httpd *httpd) {
  MHD_stop_daemon(httpd->mhd);
  httpd->mhd = NULL;
}

#else
Httpd * httpd_start(Config *config, ClassificationEngine *ce) {
  fatal("Missing libmicrohttpd. Can't start embedded httpd server.");
}

void httpd_stop(Httpd *httpd) {
  
}

#endif

