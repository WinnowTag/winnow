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
#include <arpa/inet.h>
#include <time.h>
#include <regex.h>
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

#define HTTP_NOT_FOUND(response)               \
  response->code = 404;                  \
  response->content = NOT_FOUND;         \
  response->content_type = CONTENT_TYPE;

typedef enum HTTP_METHOD {
  GET,
  POST,
  PUT,
  DELETE
} HTTPMethod;

struct HTTPD {
  struct MHD_Daemon *mhd;
  Config *config;
  ClassificationEngine *ce;
  regex_t start_job_regex;
  regex_t job_status_regex;
  regex_t about_regex;
};

typedef struct posted_data {
  int size;
  char *buffer;
} PostedData;

typedef struct HTTP_REQUEST {
  HTTPMethod method;
  char *path;
  PostedData *data;
  struct MHD_Connection *connection;
  ClassificationEngine *ce;
  struct timeval start_time;
} HTTPRequest;

typedef struct HTTP_RESPONSE {
  int code;
  char *content_type;
  char *location;
  char *content;
  int free_content;
} HTTPResponse;

static float tdiff(struct timeval from, struct timeval to) {
  double from_d = from.tv_sec + (from.tv_usec / 1000000.0);
  double to_d = to.tv_sec + (to.tv_usec / 1000000.0);
  return to_d - from_d;  
}

/*************************************************
 * XML related functions
 *************************************************/

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
static HTTPMethod get_method(const char *method) {
  if (!strcmp(MHD_HTTP_METHOD_POST, method)) {
    return POST;
  } else if (!strcmp(MHD_HTTP_METHOD_PUT, method)) {
    return PUT;
  } else if (!strcmp(MHD_HTTP_METHOD_DELETE, method)) {
    return DELETE;
  } else {
    return GET;
  }
}


static int start_job(const HTTPRequest * request, HTTPResponse * response) {
  int ret = 0;
  
  if (request->method != POST) {
    response->code = MHD_HTTP_METHOD_NOT_ALLOWED;
    response->content = METHOD_NOT_ALLOWED;
    response->content_type = CONTENT_TYPE;    
  } else if (!request->data || request->data->size <= 0) {
    response->code = MHD_HTTP_UNSUPPORTED_MEDIA_TYPE;
    response->content = BAD_XML;
    response->content_type = CONTENT_TYPE;
  } else {
    xmlDocPtr doc = xmlParseDoc(BAD_CAST request->data->buffer);
          
    if (doc == NULL) {
      response->code = MHD_HTTP_UNSUPPORTED_MEDIA_TYPE;
      response->content = BAD_XML;
      response->content_type = CONTENT_TYPE;
    } else {
      ClassificationJob *job = NULL;
      xmlXPathContextPtr context = xmlXPathNewContext(doc);    
      xmlXPathObjectPtr result = xmlXPathEvalExpression(BAD_CAST "/job/tag-id/text()", context);
      
      if (!xmlXPathNodeSetIsEmpty(result->nodesetval)) {
        int tag_id = (int) strtol((char *) result->nodesetval->nodeTab[0]->content, NULL, 10);
        info("Starting classification job for tag %i", tag_id);
        job = ce_add_classification_job_for_tag(request->ce, tag_id);
      } else {
        xmlXPathFreeObject(result);
        result = xmlXPathEvalExpression(BAD_CAST "/job/user-id/text()", context);
        
        if (!xmlXPathNodeSetIsEmpty(result->nodesetval)) {
          int user_id = (int) strtol((char *) result->nodesetval->nodeTab[0]->content, NULL, 10);
          info("Starting classification job for user %i", user_id);
          job = ce_add_classification_job_for_user(request->ce, user_id);
        } else {
          response->code = MHD_HTTP_UNPROCESSABLE_ENTITY;
          response->content = MISSING_TAG_ID;
          response->content_type = CONTENT_TYPE;
        }     
      }
              
      xmlXPathFreeObject(result);
      xmlXPathFreeContext(context);
      xmlFreeDoc(doc);
      
      if (job) {
        response->code = MHD_HTTP_CREATED;
        response->content_type = CONTENT_TYPE;
        response->content = (char*) xml_for_job(job);
        response->free_content = MHD_YES;
        response->location = url_for_job(job);
      }
    }
  }
  
  return ret;
}

static int job_handler(const HTTPRequest * request, HTTPResponse * response) {
  int ret;
  const char *job_id = extract_job_id(request->path);
  ClassificationJob *job = NULL;
  
  if (job_id == NULL) {
    HTTP_NOT_FOUND(response)
  } else if (NULL == (job = ce_fetch_classification_job(request->ce, job_id))) {
    HTTP_NOT_FOUND(response);
  } else if (CJOB_STATE_CANCELLED == cjob_state(job)) {
    // Cancelled jobs are considered deleted to the outside world.
    HTTP_NOT_FOUND(response);
  } else if (GET == request->method) {
    xmlChar *xml = xml_for_job(job);
    response->code = MHD_HTTP_OK;
    response->content_type = CONTENT_TYPE;
    response->content = (char*) xml;
    response->free_content = MHD_YES;    
  } else if (DELETE == request->method) {
    if (CJOB_STATE_COMPLETE == cjob_state(job)) {
      ce_remove_classification_job(request->ce, job);
      free_classification_job(job);
    } else {
      cjob_cancel(job);
    }
    response->code = MHD_HTTP_OK; // Should really by 204, but need Rails 2.0.2 first
    response->content = "";
    response->content_type = NULL;
  } else {
    response->code = MHD_HTTP_METHOD_NOT_ALLOWED;
    response->content = METHOD_NOT_ALLOWED;
    response->content_type = CONTENT_TYPE;
  }
  
  return ret;
}

static int handle_request(const Httpd * httpd, const HTTPRequest * request, HTTPResponse * response) {
    
  if (0 == regexec(&httpd->start_job_regex, request->path, 0, NULL, 0)) {
    start_job(request, response);
  } else if (0 == regexec(&httpd->job_status_regex, request->path, 0, NULL, 0)) {
    job_handler(request, response);
  } else if (0 == regexec(&httpd->about_regex, request->path, 0, NULL, 0)) {
    response->code = MHD_HTTP_OK;
    response->content_type = CONTENT_TYPE;
    response->content = (char*) xml_for_about(httpd->ce);
    response->free_content = MHD_YES;
  } else {
    HTTP_NOT_FOUND(response);
  }
  
  return 1;
}

/* This is the base response handler. It just returns a 404. 
 * 
 * TODO Regex matching of URLs would be much more robust
 */
static int process_request(void * httpd_vp, struct MHD_Connection * connection,
                           const char * raw_url, const char * method, 
                           const char * version, const char * upload_data,
                           unsigned int * upload_data_size, void **memo) {
  int ret = MHD_YES;
  Httpd * httpd = (Httpd*) httpd_vp;
  
  HTTPRequest *request = (HTTPRequest*) *memo;
  if (NULL == request) {
    request = calloc(1, sizeof(HTTPRequest));
    request->method = get_method(method);
    request->connection = connection;
    request->ce = httpd->ce;    
    request->path = strdup(raw_url);    
    gettimeofday(&request->start_time, NULL);
    
    /* strip any .xml extensions */
    char *ext;
    if ((ext = strstr(request->path, ".xml"))) ext[0] = '\0';
    
    if (*upload_data_size > 0) {
      request->data = calloc(1, sizeof(PostedData));
    }
    
    *memo = request;
  }
  
  if (*upload_data_size > 0 && request->data->size == 0) {
    request->data->buffer = strdup(upload_data);
    request->data->size = *upload_data_size;    
    *upload_data_size = 0;
    ret = MHD_YES;      
  } else if (*upload_data_size > 0 && request->data->size > 0) {    
    error("TODO Need to handle incremental processing of POST'ed data");      
  } else if (*upload_data_size == 0) {
    HTTPResponse response;
    memset(&response, 0, sizeof(HTTPResponse));
    response.free_content = false;
    handle_request(httpd, request, &response);
        
    struct MHD_Response *mhd_response = MHD_create_response_from_data(strlen(response.content), response.content, response.free_content, MHD_NO);
    MHD_add_response_header(mhd_response, MHD_HTTP_HEADER_CONTENT_TYPE, response.content_type);
    MHD_add_response_header(mhd_response, MHD_HTTP_HEADER_LOCATION, response.location);    
    ret = MHD_queue_response(connection, response.code, mhd_response);
    MHD_destroy_response(mhd_response);
    
    if (response.free_content) {
      if (response.location) free(response.location);
    }
    
    struct timeval end_time;
    gettimeofday(&end_time, NULL);    
    info("%s %s %i %.7fs", method, raw_url, response.code, tdiff(request->start_time, end_time));
    
    free(request->path);
    if (request->data) free(request->data);
    free(request);
  }
  
  return ret;
}

static int access_policy(void *ip_vp, const struct sockaddr * addr, socklen_t addrlen) {
  char *allowed_ip = ip_vp;
  int allow = MHD_NO;
  
  if (allowed_ip) {
    if (addr->sa_family == AF_INET) {
      char ip[16];      
      inet_ntop(AF_INET, &(((struct sockaddr_in *)addr)->sin_addr), ip, sizeof (ip));
      
      if (0 == strncmp(allowed_ip, ip, sizeof(ip))) {
        allow = MHD_YES;
      } else {
        info("Rejected connection from %s because it didn't match allowed ip %s", ip, allowed_ip);
      }
    }   
  } else {
    allow = MHD_YES;
  }
  
  return allow;
}

Httpd * httpd_start(Config *config, ClassificationEngine *ce) {
  Httpd *httpd = malloc(sizeof(Httpd));
  if (httpd) {
    int regex_error;
    httpd->config = config;
    httpd->ce = ce;
    
    if ((regex_error = regcomp(&httpd->start_job_regex, "^/classifier/jobs/?$", REG_EXTENDED | REG_NOSUB)) != 0) {
      char buffer[1024];
      regerror(regex_error, &httpd->start_job_regex, buffer, sizeof(buffer));
      fatal("Error compiling REGEX: %s", buffer);
    }
    
    if ((regex_error = regcomp(&httpd->job_status_regex, "^/classifier/jobs/.*", REG_EXTENDED | REG_NOSUB))) {
      char buffer[1024];
      regerror(regex_error, &httpd->job_status_regex, buffer, sizeof(buffer));
      fatal("Error compiling REGEX: %s", buffer);
    }
    
    if ((regex_error = regcomp(&httpd->about_regex, "^/classifier$", REG_EXTENDED | REG_NOSUB))) {
      char buffer[1024];
      regerror(regex_error, &httpd->about_regex, buffer, sizeof(buffer));
      fatal("Error compiling REGEX: %s", buffer);
    }
    
    HttpdConfig httpd_config;
    cfg_httpd_config(config, &httpd_config);
    
    httpd->mhd = MHD_start_daemon(MHD_USE_SELECT_INTERNALLY | MHD_USE_DEBUG,
                                  httpd_config.port,
                                  access_policy,
                                  (void*) httpd_config.allowed_ip,
                                  process_request,
                                  httpd,
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

