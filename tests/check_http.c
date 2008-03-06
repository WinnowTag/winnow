/* Copyright (c) 2007 The Kaphan Foundation
 *
 * Possession of a copy of this file grants no permission or license
 * to use, modify, or create derivate works.
 *
 * Please contact info@peerworks.org for further information.
 */

#include <stdlib.h>
#include <check.h>
#include <config.h>
#include <stdio.h>
#include <string.h>
#include <sqlite3.h>
#include "../src/cls_config.h"
#include "../src/classification_engine.h"
#include "../src/httpd.h"
#include "assertions.h"
#include "../src/logging.h"
#include "fixtures.h"
#include "../src/item_cache.h"

#define PORT 8008

ItemCache *item_cache;
Config *config;
ClassificationEngine *ce;
Httpd *httpd;
static FILE *test_data;
static FILE *devnull;
static FILE *data;
static FILE *headers;

static void read_file(const char * file_name, char * data) {
  FILE *file;

  if (NULL != (file = fopen(file_name, "r"))) {
    fseek(file, 0, SEEK_END);
    int size = ftell(file);
    fseek(file, 0, SEEK_SET);
    fread(data, sizeof(char), size, file);
    data[size] = 0;
    fclose(file);
  }  
}

static void setup_httpd() {
  test_data = fopen("http_test_data.log", "a");
  headers = fopen("/tmp/headers.txt", "w");
  data    = fopen("/tmp/test_data.xml", "w");
  
  setup_fixture_path();
  system("cp fixtures/valid.db fixtures/valid-copy.db");
  item_cache_create(&item_cache, "fixtures/valid-copy.db");
  config = load_config("fixtures/real-db.conf");
  ce = create_classification_engine(item_cache, config);  
  httpd = httpd_start(config, ce, item_cache);
  devnull = fopen("/dev/null", "w");
}

static void teardown_httpd() {
  teardown_fixture_path();
  httpd_stop(httpd);
  ce_stop(ce);
  free_classification_engine(ce);
  free_config(config);
  fclose(test_data);
  fclose(devnull);
}

#ifdef HAVE_LIBCURL
#include <curl/curl.h>
#include <libxml/tree.h>
#include <libxml/parser.h>
#include <libxml/xpath.h>
    
void assert_file_contains_line(char * filename, char * line) {
  int found = 0;
  char buf[1024];
  FILE *file = fopen(filename, "r");
  
  if (NULL == file) {
    fail("Could not open file");
  }
  
  while (NULL != fgets(buf, 1024, file)) {
    if (buf == strstr(buf, line)) {
      found = 1;
    }
  }
  
  fclose(file);
  
  if (!found) {
    fail("%s not found in %s", line, filename);
  }
}

START_TEST(test_http_initialization) {
  assert_get("http://localhost:8008/classifier", 200, devnull);
} END_TEST

START_TEST(test_missing_job_returns_404) {
  assert_get("http://localhost:8008/classifier/jobs/missing", 404, devnull);
} END_TEST

START_TEST(test_missing_job_id_returns_405) {
  /* 405 is Method NOT allowed.
   * We get this because you can only POST to classifier/jobs/
   */
  assert_get("http://localhost:8008/classifier/jobs/", 405, test_data);
} END_TEST

START_TEST(test_post_to_create_job_with_tag_id_missing_returns_422) {
  char *post_data = "<?xml version='1.0'?>\n<job><tag-id></tag-id></job>\n";
  assert_post("http://localhost:8008/classifier/jobs", post_data, 422, devnull, devnull);
} END_TEST

/* Missing XML returns Unsupported media type (415) */
START_TEST(test_post_to_create_job_with_invalid_xml_returns_400) {
  char *post_data = "xxx";
  assert_post("http://localhost:8008/classifier/jobs", post_data, 400, devnull, devnull);
} END_TEST

START_TEST(test_post_to_create_job_without_xml_returns_415) {
  char *post_data = "";
  assert_post("http://localhost:8008/classifier/jobs", post_data, 415, devnull, devnull);
} END_TEST

START_TEST(test_post_with_valid_tag_id_queues_job) {
  
  assert_equal(0, ce_num_jobs_in_system(ce));
  char *post_data = "<?xml version='1.0'?>\n<job><tag-id>48</tag-id></job>";
  assert_post("http://localhost:8008/classifier/jobs", post_data, 201, data, headers);
  fclose(headers);
  fclose(data);
  mark_point();
  
  assert_equal(1, ce_num_jobs_in_system(ce));
  xmlDocPtr doc = xmlReadFile("/tmp/test_data.xml", NULL, 0);
  fail_unless(doc != NULL, "Failed to parse xml");
  assert_xpath("/job/id/text()", doc);
  mark_point();
  
  xmlXPathContextPtr context = xmlXPathNewContext(doc);
  xmlXPathObjectPtr result = xmlXPathEvalExpression(BAD_CAST "/job/id/text()", context);
  xmlNodeSetPtr nodeset = result->nodesetval;
  mark_point();
  char *id = (char *) nodeset->nodeTab[0]->content;
  assert_not_null(id);
  
  ClassificationJob *job = ce_fetch_classification_job(ce, id);
  assert_not_null(job);
  assert_equal(48, cjob_tag_id(job));
  
  char line[1024];
  sprintf(line, "Location: /classifier/jobs/%s", id);
  assert_file_contains_line("/tmp/headers.txt", line);

  xmlXPathFreeObject(result);  
  xmlXPathFreeContext(context);
  xmlFree(doc);
} END_TEST

START_TEST(test_post_with_user_id_queues_job) {
  char *post_data = "<?xml version='1.0'?>\n<job><user-id>2</user-id></job>";
  assert_post("http://localhost:8008/classifier/jobs", post_data, 201, data, devnull);
  fclose(data);
  mark_point();
  
  assert_equal(1, ce_num_jobs_in_system(ce));
  xmlDocPtr doc = xmlReadFile("/tmp/test_data.xml", NULL, 0);
  fail_unless(doc != NULL, "Failed to parse XML");
  assert_xpath("/job/id/text()", doc);
  assert_xpath("/job/user-id[text() = '2']", doc);
  
  xmlFree(doc);
} END_TEST


START_TEST(delete_without_job_id_is_405) {
  /* This is 405 since it goes to the start job handler */
  assert_delete("http://localhost:8008/classifier/jobs/", 405, devnull);
} END_TEST

START_TEST(delete_with_missing_job_id_is_404) {
  assert_delete("http://localhost:8008/classifier/jobs/missing", 404, devnull);
} END_TEST


START_TEST(deleting_job_sets_it_cancelled) {
  ClassificationJob *job = ce_add_classification_job_for_tag(ce, 48);
  char url[256];
  snprintf(url, 256, "http://localhost:8008/classifier/jobs/%s", cjob_id(job));
  assert_delete(url, 200, devnull);
  assert_equal(CJOB_STATE_CANCELLED, cjob_state(job));
} END_TEST

START_TEST (deleting_a_completed_job_removes_it_from_the_engine) {
  ClassificationJob *job = ce_add_classification_job_for_tag(ce, 48);
  char id[64];
  strncpy(id, cjob_id(job), 64);
  char url[256];
  snprintf(url, 256, "http://localhost:8008/classifier/jobs/%s", cjob_id(job));
  ce_start(ce);
  ce_stop(ce);
  
  assert_not_null(ce_fetch_classification_job(ce, id));
  assert_get(url, 200, devnull);
  assert_delete(url, 200, devnull);
  assert_get(url, 404, devnull);
  assert_null(ce_fetch_classification_job(ce, id));
} END_TEST

// Expected xml should look like this:
//
//  <job>
//    <id>ID</id>
//    <progress type="float">0.0</progress>
//    <status>Status</status>
//  </job>
//
START_TEST(test_job_status) {
  char url[256];
  ClassificationJob *job = ce_add_classification_job_for_tag(ce, 39);
  sprintf(url, "http://localhost:8008/classifier/jobs/%s", cjob_id(job));
  assert_get(url, 200, data);
  fclose(data);
  
  char idpath[1024];
  sprintf(idpath, "/job/id[text() = '%s']", cjob_id(job));
                
  xmlDocPtr doc = xmlReadFile("/tmp/test_data.xml", NULL, 0);
  if (doc == NULL) fail("Failed to parse xml");
  
  assert_xpath(idpath, doc);
  assert_xpath("/job/progress[text() = '0.0']", doc);
  assert_xpath("/job/status[text() = 'Waiting']", doc);
  
  xmlFree(doc);
} END_TEST

START_TEST(test_job_status_with_xml_suffix) {
  char url[256];
  ClassificationJob *job = ce_add_classification_job_for_tag(ce, 39);
  sprintf(url, "http://localhost:8008/classifier/jobs/%s.xml", cjob_id(job));
  assert_get(url, 200, data);
  fclose(data);
  
  char idpath[1024];
  sprintf(idpath, "/job/id[text() = '%s']", cjob_id(job));
                
  xmlDocPtr doc = xmlReadFile("/tmp/test_data.xml", NULL, 0);
  if (doc == NULL) fail("Failed to parse xml");
  
  assert_xpath(idpath, doc);
  assert_xpath("/job/progress[text() = '0.0']", doc);
  assert_xpath("/job/status[text() = 'Waiting']", doc);
  
  xmlFree(doc);
} END_TEST

START_TEST(test_completed_job_status) {
  char url[256];
  ClassificationJob *job = ce_add_classification_job_for_tag(ce, 39);
  ce_start(ce);
  ce_stop(ce);
  sprintf(url, "http://localhost:8008/classifier/jobs/%s", cjob_id(job));
  assert_get(url, 200, data);
  fclose(data);
  
  char idpath[1024];
  sprintf(idpath, "/job/id[text() = '%s']", cjob_id(job));
                
  xmlDocPtr doc = xmlReadFile("/tmp/test_data.xml", NULL, 0);
  if (doc == NULL) fail("Failed to parse xml");
  
  assert_xpath(idpath, doc);
  assert_xpath("/job/progress[text() = '100.0']", doc);
  assert_xpath("/job/status[text() = 'Complete']", doc);
  assert_xpath("/job/duration/text()", doc);
  assert_xpath("/job/duration[@type = 'float']", doc);
  
  xmlFree(doc);
} END_TEST

START_TEST(cancelled_job_returns_404) {
  char url[256];
  ClassificationJob *job = ce_add_classification_job_for_tag(ce, 48);
  cjob_cancel(job);
  sprintf(url, "http://localhost:8008/classifier/jobs/%s", cjob_id(job));
  assert_get(url, 404, devnull);
} END_TEST

START_TEST(test_error_job_status) {
  char url[256];
  ClassificationJob *job = ce_add_classification_job_for_tag(ce, 1000);
  ce_start(ce);
  ce_stop(ce);
  
  sprintf(url, "http://localhost:8008/classifier/jobs/%s", cjob_id(job));
  assert_get(url, 200, data);
  fclose(data);
  
  char idpath[1024];
  sprintf(idpath, "/job/id[text() = '%s']", cjob_id(job));
                
  xmlDocPtr doc = xmlReadFile("/tmp/test_data.xml", NULL, 0);
  if (doc == NULL) fail("Failed to parse xml");
  
  assert_xpath(idpath, doc);
  assert_xpath("/job/status[text() = 'Error']", doc);
  assert_xpath("/job/error-message/text()", doc);
  
  xmlFree(doc);
} END_TEST

/** Item Cache Tests **/
START_TEST (test_adding_a_feed_with_no_content_returns_400) {
  char *url = "http://localhost:8008/feeds";
  char *post_data = "";
  assert_post(url, post_data, 400, data, devnull);
} END_TEST

START_TEST (test_adding_a_feed_returns_201) {
  char *url = "http://localhost:8008/feeds";
  char *post_data = "<?xml version=\"1.0\" ?>\n<entry><title>Feed 1337</title><id>urn:peerworks.org:feeds#1337</id></entry>\n";
  assert_post(url, post_data, 201, data, devnull);
} END_TEST

START_TEST (test_adding_a_feed_adds_it_to_the_database) {
  char *url = "http://localhost:8008/feeds";
  char *post_data = "<?xml version=\"1.0\" ?>\n<entry><title>Feed 1337</title><id>urn:peerworks.org:feeds#1337</id></entry>\n";
  assert_post(url, post_data, 201, data, devnull);
  
  sqlite3 *db;
  sqlite3_stmt *stmt;
  sqlite3_open_v2("fixtures/valid-copy.db", &db, SQLITE_OPEN_READONLY, NULL);
  sqlite3_prepare_v2(db, "select * from feeds where id = 1337", -1, &stmt, NULL);
  int rc = sqlite3_step(stmt);
  assert_equal(SQLITE_ROW, rc);
  assert_equal_s("Feed 1337", sqlite3_column_text(stmt, 1));
  sqlite3_close(db);
} END_TEST


START_TEST (test_removing_a_feed_returns_204) {
  char *url = "http://localhost:8008/feeds/141";
  assert_delete(url, 204, devnull);
} END_TEST

//START_TEST (test_removing_a_feed_that_doesnt_exist_returns_404) {
//  char *url = "http://localhost:8008/feeds/1337";
//  assert_delete(url, 404, devnull);
//} END_TEST

START_TEST (test_removing_a_feed_removes_it_from_the_database) {
  char *url = "http://localhost:8008/feeds/141";
  assert_delete(url, 204, devnull);
  
  sqlite3 *db;
  sqlite3_stmt *stmt;
  sqlite3_open_v2("fixtures/valid-copy.db", &db, SQLITE_OPEN_READONLY, NULL);
  sqlite3_prepare_v2(db, "select * from feeds where id = 1337", -1, &stmt, NULL);
  int rc = sqlite3_step(stmt);
  assert_equal(SQLITE_DONE, rc);
  sqlite3_close(db);
} END_TEST

START_TEST (test_adding_an_entry_using_get_returns_405) {
  char *url = "http://localhost:8008/feeds/1/feed_items";
  assert_get(url, 405, devnull);
} END_TEST

START_TEST (test_adding_an_empty_entry_returns_400) {
  char *url = "http://localhost:8008/feeds/141/feed_items";
  assert_post(url, "", 400, data, devnull);
} END_TEST

START_TEST (test_adding_an_entry_to_nonexistant_feed_returns_422) {
  char *url = "http://localhost:8008/feeds/1/feed_items";
  char xml[2048];
  read_file("fixtures/entry.atom", xml);
  assert_post(url, xml, 422, data, devnull);
} END_TEST

START_TEST (test_adding_an_entry_returns_201) {
  char *url = "http://localhost:8008/feeds/141/feed_items";
  char xml[2048];
  read_file("fixtures/entry.atom", xml);
  assert_post(url, xml, 201, data, devnull);
} END_TEST

START_TEST (test_adding_an_entry_saves_it_in_the_database) {
  char *url = "http://localhost:8008/feeds/141/feed_items";
  char xml[2048];
  read_file("fixtures/entry.atom", xml);
  assert_post(url, xml, 201, data, devnull);
  
  sqlite3 *db;
  sqlite3_stmt *stmt;
  sqlite3_open_v2("fixtures/valid-copy.db", &db, SQLITE_OPEN_READONLY, NULL);
  sqlite3_prepare_v2(db, "select * from entries where id = 1", -1, &stmt, NULL);
  int rc = sqlite3_step(stmt);
  assert_equal(SQLITE_ROW, rc);
  assert_equal_s("urn:peerworks.org:entry#1", sqlite3_column_text(stmt, 1));
  assert_equal_s("Entry 1", sqlite3_column_text(stmt, 2));
  assert_equal_s("Sean Geoghegan", sqlite3_column_text(stmt, 3));
  assert_not_null(sqlite3_column_text(stmt, 4));
  assert_equal_s("http://example.org/entry.html", sqlite3_column_text(stmt, 4));
  assert_not_null(sqlite3_column_text(stmt, 5));
  assert_equal_s("http://example.org/entry.atom", sqlite3_column_text(stmt, 5));
  assert_not_null(sqlite3_column_text(stmt, 6));
  assert_equal_s("<p><i>This is an example</i></p>", sqlite3_column_text(stmt, 6));
  assert_equal_f(2453583.02047454, sqlite3_column_double(stmt, 7));
  assert_equal(141, sqlite3_column_int(stmt, 8));
  sqlite3_close(db);
} END_TEST

#endif

Suite * http_suite(void) {
  Suite *s = suite_create("http");
  TCase *tc_case = tcase_create("classifier engine");
  tcase_add_checked_fixture(tc_case, setup_httpd, teardown_httpd);
  TCase *tc_item_cache = tcase_create("item cache");
  tcase_add_checked_fixture(tc_item_cache, setup_httpd, teardown_httpd);
  
#ifdef HAVE_LIBCURL
  tcase_add_test(tc_case, test_http_initialization);
  tcase_add_test(tc_case, test_job_status);
  tcase_add_test(tc_case, test_job_status_with_xml_suffix);
  tcase_add_test(tc_case, test_error_job_status);
  tcase_add_test(tc_case, test_completed_job_status); 
  tcase_add_test(tc_case, test_missing_job_returns_404);
  tcase_add_test(tc_case, test_missing_job_id_returns_405);
  
  tcase_add_test(tc_case, test_post_to_create_job_without_xml_returns_415);
  tcase_add_test(tc_case, test_post_to_create_job_with_invalid_xml_returns_400);
  tcase_add_test(tc_case, test_post_to_create_job_with_tag_id_missing_returns_422);
  tcase_add_test(tc_case, test_post_with_valid_tag_id_queues_job);
  tcase_add_test(tc_case, test_post_with_user_id_queues_job);
  tcase_add_test(tc_case, deleting_job_sets_it_cancelled);
  tcase_add_test(tc_case, delete_without_job_id_is_405);
  tcase_add_test(tc_case, delete_with_missing_job_id_is_404);
  tcase_add_test(tc_case, deleting_a_completed_job_removes_it_from_the_engine);
  
  tcase_add_test(tc_item_cache, test_adding_a_feed_with_no_content_returns_400);
  tcase_add_test(tc_item_cache, test_adding_a_feed_adds_it_to_the_database);
  tcase_add_test(tc_item_cache, test_adding_a_feed_returns_201);
  tcase_add_test(tc_item_cache, test_removing_a_feed_removes_it_from_the_database);
  //tcase_add_test(tc_item_cache, test_removing_a_feed_that_doesnt_exist_returns_404);
  tcase_add_test(tc_item_cache, test_removing_a_feed_returns_204);
  tcase_add_test(tc_item_cache, test_adding_an_entry_using_get_returns_405);
  tcase_add_test(tc_item_cache, test_adding_an_entry_returns_201);
  tcase_add_test(tc_item_cache, test_adding_an_entry_saves_it_in_the_database);
  tcase_add_test(tc_item_cache, test_adding_an_empty_entry_returns_400);
  tcase_add_test(tc_item_cache, test_adding_an_entry_to_nonexistant_feed_returns_422);
  
#endif
  suite_add_tcase(s, tc_case);
  suite_add_tcase(s, tc_item_cache);
  return s;
}

int main(void) {
  initialize_logging("http_test.log");
  int number_failed;
  
  SRunner *sr = srunner_create(http_suite());
  
  srunner_run_all(sr, CK_NORMAL);
  number_failed = srunner_ntests_failed(sr);
  srunner_free(sr);
  close_log();
  return (number_failed == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}
