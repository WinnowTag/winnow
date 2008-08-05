//
// Possession of a copy of this file grants no permission or license
// to use, modify, or create derivate works.
//
// Please contact info@peerworks.org for further information.

#include "../src/misc.h"
#include <math.h>

#ifndef assert_false
#define assert_false(o) fail_unless(o == false, "expected to be false")
#endif

#ifndef assert_true
#define assert_true(o) fail_unless(o, "expected to be true")
#endif

#ifndef assert_null
#define assert_null(o) fail_unless(o == NULL, "expected object to be null")
#endif

#ifndef assert_not_null
#define assert_not_null(o) fail_unless(o != NULL, "expected object to not be null")
#endif

#ifndef assert_equal
#define assert_equal(expected, actual) fail_unless(expected == actual, "expected %d but got %d", expected, actual)
#endif

#ifndef assert_not_equal
#define assert_not_equal(expected, actual) fail_unless(expected != actual, "expected %d to be something else", expected, actual)
#endif

#ifndef assert_equal_s
#define assert_equal_s(e, a) fail_unless(strcmp(e,a) == 0, "expected %s but got %s", e, a)
#endif

#ifndef assert_equal_f
#define assert_equal_f(expected, actual) fail_unless(fabs(expected - actual) <= 0.000001, "expected %f but got %f", expected, actual)
#endif

#ifndef assert_between_ex
#define assert_between_ex(bottom, top, actual) \
  fail_unless(bottom < actual && actual < top, "expected %f to be between %f and %f", actual, bottom, top)
#endif

#ifndef assert_xpath
#define assert_xpath(xp, doc) {                                                       \
  xmlXPathContextPtr context = xmlXPathNewContext(doc);                               \
  xmlXPathObjectPtr result = xmlXPathEvalExpression((xmlChar *)xp, context);           \
  xmlXPathFreeContext(context);                                                       \
  if (xmlXPathNodeSetIsEmpty(result->nodesetval)) fail("missing %s in document", xp); \
  xmlXPathFreeObject(result);                                                         \
}
#endif

#if HAVE_LIBCURL
#define assert_get(url, response_code, store_output) {\
    int code;                                                             \
    char curlerr[CURL_ERROR_SIZE];                                        \
    char *content_type;                                                   \
    CURL *curl = curl_easy_init();                                        \
    curl_easy_setopt(curl, CURLOPT_URL, url);                             \
    curl_easy_setopt(curl, CURLOPT_HTTP_VERSION, CURL_HTTP_VERSION_1_1);  \
    curl_easy_setopt(curl, CURLOPT_ERRORBUFFER, curlerr);                 \
    curl_easy_setopt(curl, CURLOPT_HEADER, 0);                            \
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, store_output);              \
    if (curl_easy_perform(curl)) fail("HTTP server not accessible: %s", curlerr); \
    if (CURLE_OK != curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &code)) fail("Could not get response code"); \
    if (curl_easy_getinfo(curl, CURLINFO_CONTENT_TYPE, &content_type)) fail("Could not get content type"); \
    assert_equal(response_code, code);                                              \
    assert_not_null(content_type);                                        \
    assert_equal_s("application/xml", content_type);                      \
    curl_easy_cleanup(curl);                                              \
}

#define assert_delete(url, response_code, store_headers) {\
    int code;                                                             \
    char curlerr[CURL_ERROR_SIZE];                                        \
    char *content_type;                                                   \
    CURL *curl = curl_easy_init();                                        \
    curl_easy_setopt(curl, CURLOPT_URL, url);                             \
    curl_easy_setopt(curl, CURLOPT_HTTP_VERSION, CURL_HTTP_VERSION_1_1);  \
    curl_easy_setopt(curl, CURLOPT_ERRORBUFFER, curlerr);                 \
    curl_easy_setopt(curl, CURLOPT_HEADER, 1);                            \
    curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, "DELETE");              \
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, store_headers);             \
    if (curl_easy_perform(curl)) fail("HTTP server not accessible: %s", curlerr); \
    if (CURLE_OK != curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &code)) fail("Could not get response code"); \
    if (curl_easy_getinfo(curl, CURLINFO_CONTENT_TYPE, &content_type)) fail("Could not get content type"); \
    assert_equal(response_code, code);                                    \
    curl_easy_cleanup(curl);                                              \
}

#define assert_post(url, data, response_code, store_output, store_headers) { \
    mark_point();\
    int code;                                                             \
    char curlerr[CURL_ERROR_SIZE];                                        \
    char *content_type;                                                   \
    CURL *curl = curl_easy_init();                                        \
    curl_easy_setopt(curl, CURLOPT_URL, url);                             \
    curl_easy_setopt(curl, CURLOPT_HTTP_VERSION, CURL_HTTP_VERSION_1_1);  \
    curl_easy_setopt(curl, CURLOPT_ERRORBUFFER, curlerr);                 \
    curl_easy_setopt(curl, CURLOPT_POST, 1);                              \
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, data);                    \
    curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, strlen(data));         \
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, store_output);              \
    curl_easy_setopt(curl, CURLOPT_WRITEHEADER, store_headers);           \
    if (curl_easy_perform(curl)) fail("HTTP server not accessible: %s", curlerr); \
    _mark_point("assertions.h", 88);\
    if (CURLE_OK != curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &code)) fail("Could not get response code"); \
    if (curl_easy_getinfo(curl, CURLINFO_CONTENT_TYPE, &content_type)) fail("Could not get content type"); \
    assert_equal(response_code, code);                                              \
    assert_not_null(content_type);                                        \
    assert_equal_s("application/xml", content_type);                      \
    curl_easy_cleanup(curl);                                              \
}

#define assert_put(url, data, size, response_code, store_output, store_headers) { \
    mark_point();\
    int code;                                                             \
    char curlerr[CURL_ERROR_SIZE];                                        \
    char *content_type;                                                   \
    CURL *curl = curl_easy_init();                                        \
    curl_easy_setopt(curl, CURLOPT_URL, url);                             \
    curl_easy_setopt(curl, CURLOPT_HTTP_VERSION, CURL_HTTP_VERSION_1_1);  \
    curl_easy_setopt(curl, CURLOPT_ERRORBUFFER, curlerr);                 \
    curl_easy_setopt(curl, CURLOPT_UPLOAD, 1);                              \
    curl_easy_setopt(curl, CURLOPT_READDATA, data);                    \
    curl_easy_setopt(curl, CURLOPT_INFILESIZE_LARGE, (curl_off_t) size);                    \
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, store_output);              \
    curl_easy_setopt(curl, CURLOPT_WRITEHEADER, store_headers);           \
    if (curl_easy_perform(curl)) fail("HTTP server not accessible: %s", curlerr); \
    _mark_point("assertions.h", 88);\
    if (CURLE_OK != curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &code)) fail("Could not get response code"); \
    if (curl_easy_getinfo(curl, CURLINFO_CONTENT_TYPE, &content_type)) fail("Could not get content type"); \
    assert_equal(response_code, code);                                              \
    assert_not_null(content_type);                                        \
    assert_equal_s("application/xml", content_type);                      \
    curl_easy_cleanup(curl);                                              \
}
#endif

#include <regex.h>

static int assert_match(const char * pattern, const char * s) {
  regex_t regex;

  if (regcomp(&regex, pattern, REG_EXTENDED)) {
    fail("Error compiling regex");
    return -1;
  }

  if (regexec(&regex, s, 0, NULL, 0)) {
    fail("Pattern '%s' did not match string '%s'", pattern, s);
  }

  regfree(&regex);
  
  return 0;
}
