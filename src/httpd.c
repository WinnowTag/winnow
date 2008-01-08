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
#include "misc.h"
#include "svnversion.h"

#define CONTENT_TYPE "application/xml"
#define NOT_FOUND "<?xml version='1.0' ?>\n<errors><error>Resource not found.</error></errors>\n"
#define METHOD_NOT_ALLOWED "<?xml version='1.0' ?>\n<errors><error>Method is not allowed for that resource.</errors></errors>\n"
#define BAD_XML "<?xml version='1.0' ?>\n<error><error>Badly formatted XML.</error></errors>\n"
#define MISSING_TAG_ID "<?xml version='1.0' ?>\n<errors><error>Missing tag or user id in job description</error></errors>\n"

#ifdef HAVE_LIBMICROHTTPD
#include <microhttpd.h>
#include <libxml/tree.h>
#include <libxml/xpath.h>

#define LOG_RESPONSE(url, code) \
  debug("%s resulted in %i", url, code);

// TODO: Change to 204 when Rails bug #10445 is fixed and released
#define SEND_204(returncode) \
   LOG_RESPONSE(url, MHD_HTTP_NO_CONTENT); \
   struct MHD_Response *response = MHD_create_response_from_data(0, "", MHD_NO, MHD_NO); \
   MHD_add_response_header(response, MHD_HTTP_HEADER_CONTENT_TYPE, CONTENT_TYPE);        \
   returncode = MHD_queue_response(connection, MHD_HTTP_OK, response);           \
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

#define SEND_500(returncode) \
   LOG_RESPONSE(url, MHD_HTTP_INTERNAL_SERVER_ERROR); \
   struct MHD_Response *response = MHD_create_response_from_data(strlen("Error"), "Error", MHD_NO, MHD_NO); \
   returncode = MHD_queue_response(connection, MHD_HTTP_INTERNAL_SERVER_ERROR, response);                                   \
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

static xmlNodePtr add_element(xmlNodePtr parent, const char * name, const char * type, const char * fmt, ...) {
  char buffer[32];
  
  va_list argp;
  va_start(argp, fmt);
  vsnprintf(buffer, 16, fmt, argp);
  va_end(argp);
  
  xmlNodePtr node = xmlNewChild(parent, NULL, BAD_CAST name, BAD_CAST buffer);
  xmlNewProp(node, BAD_CAST "type", BAD_CAST type);
  
  return node;
}

/*  <job>
 *    <id>ID</id>
 *    <progress type="float">0.0</progress>
 *    <tag-id type="integer">N</tag-id>
 *  </job>
 */
static xmlChar * xml_for_job(const ClassificationJob *job) {
  xmlChar *buffer = NULL;
  int buffersize;
  xmlDocPtr doc = xmlNewDoc(BAD_CAST "1.0");
  xmlNodePtr root = xmlNewNode(NULL, BAD_CAST "job");
  xmlDocSetRootElement(doc, root);
  
  xmlNewChild(root, NULL, BAD_CAST "id", BAD_CAST cjob_id(job));
  
  switch (cjob_type(job)) {
    case CJOB_TYPE_TAG_JOB:
      add_element(root, "tag-id", "integer", "%d", cjob_tag_id(job));    
      break;
    case CJOB_TYPE_USER_JOB:
      add_element(root, "user-id", "integer", "%d", cjob_user_id(job));
      break;
    default:
      break;
  }
  
  if (CJOB_STATE_ERROR == cjob_state(job)) {
    xmlNewChild(root, NULL, BAD_CAST "error-message", BAD_CAST cjob_error_msg(job));
  }
  
  add_element(root, "duration", "float", "%.2f", cjob_duration(job));
  add_element(root, "progress", "float", "%.1f", cjob_progress(job));  
  xmlNewChild(root, NULL, BAD_CAST "status", BAD_CAST cjob_state_msg(job));
  
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

static xmlChar * xml_for_about(const ClassificationEngine *ce) {
  xmlChar *buffer = NULL;
  int buffersize;
  
  xmlDocPtr doc = xmlNewDoc(BAD_CAST "1.0");
  xmlNodePtr root = xmlNewNode(NULL, BAD_CAST "classifier");
  xmlDocSetRootElement(doc, root);
  
  add_element(root, "version", "string", "%s", PACKAGE_VERSION);
  add_element(root, "svnversion", "string", "%s", svn_version);
  
  xmlDocDumpFormatMemory(doc, &buffer, &buffersize, 1);
  xmlFreeDoc(doc);
  
  return buffer;
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
    if (CJOB_STATE_COMPLETE == cjob_state(job)) {
      ce_remove_classification_job(ce, job);
      free_classification_job(job);
    } else {
      cjob_cancel(job);
    }
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
  const char *clen = MHD_lookup_connection_value(connection, MHD_HEADER_KIND, MHD_HTTP_HEADER_CONTENT_LENGTH);
  
  if (strcmp(MHD_HTTP_METHOD_POST, method)) {
    SEND_405(ret, "POST");
  } else if (clen && !strcmp(clen, "0")) {
    SEND_415(ret);
  } else {  
    debug("posted %s", upload_data);
    // First call, but data is empty
    int first_call = false;
    
    struct posted_data *pd = *memo;
    if (!(pd)) {
      first_call = true;
      pd = malloc(sizeof(struct posted_data));
      pd->buffer = NULL;
      pd->size = 0;
      *memo = pd;
    }
      
    if (*upload_data_size > 0 && pd->size == 0) {
      pd->buffer = calloc(*upload_data_size, sizeof(char));
      pd->size = *upload_data_size;
      strncpy(pd->buffer, upload_data, pd->size);
      *upload_data_size = 0;
      ret = MHD_YES;      
    } else if (*upload_data_size > 0 && pd->size > 0) {
      // TODO Need to handle incremental processing of POST'ed data
      error("Need to handle incremental processing of POST'ed data");      
    } else if (*upload_data_size == 0 && pd->size > 0) {
      ret = start_job(ce_vp, connection, url, pd->buffer);
      free(pd->buffer);
      free(pd);
    } else {
      ret = MHD_YES;
    }
  }
  
  return ret;
}

/* This is the base response handler. It just returns a 404. 
 * 
 * TODO Regex matching of URLs would be much more robust
 */
static int process_request(void * ce_vp, struct MHD_Connection * connection,
                           const char * raw_url, const char * method, 
                           const char * version, const char * upload_data,
                           unsigned int * upload_data_size, void **memo) {
  int ret = 1;
  
  char url[1024];
  if (sizeof(url) < snprintf(url, sizeof(url), "%s", raw_url)) {
    error("URL '%s' is too long", raw_url);
    return MHD_NO;
  }
 
  /* strip any .xml extensions */
  char *ext;
  if ((ext = strstr(url, ".xml"))) {
    ext[0] = '\0';
  }  
 
  if (0 == strcmp(url, "/classifier/jobs/") || 
      0 == strcmp(url, "/classifier/jobs")) {
    info("%s %s size(%i) start_job\n", method, url, *upload_data_size);
    ret = start_job_handle(ce_vp, connection, url, method, version, upload_data, upload_data_size, memo);
  } else if (url == strstr(url, "/classifier/jobs/")) {
    info("%s %s size(%i) job", method, url, *upload_data_size);
    ret = job_handler(ce_vp, connection, url, method, version);
  } else if (!strcmp(url, "/classifier")) {
    xmlChar *xml = xml_for_about((ClassificationEngine*) ce_vp);
    SEND_XML_DATA(ret, MHD_HTTP_OK, (char *) xml, NULL);
    xmlFree(xml);
  } else {
    info("%s %s size(%i) 404", method, url, *upload_data_size);
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
    
    httpd->mhd = MHD_start_daemon(MHD_USE_SELECT_INTERNALLY | MHD_USE_DEBUG,
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

