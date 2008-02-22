/* Copyright (c) 2007 The Kaphan Foundation
 *
 * Possession of a copy of this file grants no permission or license
 * to use, modify, or create derivate works.
 *
 * Please contact info@peerworks.org for further information.
 */

#include <check.h>
#include <string.h>
#include "assertions.h"
#include "../src/cls_config.h"
#include "../src/classification_engine.h"
#include "tagging_store_fixtures.h"
#include "../src/logging.h"
#include "../src/item_cache.h"
#include "fixtures.h"

#define TAG_ID 48
#define BOGUS_TAG_ID 11111

/************************************************************************
 * End to End tests
 ************************************************************************/
ItemCache *item_cache;
ClassificationEngine *ce;
Config *ce_config;

static void setup_end_to_end(void) {
  setup_fixture_path();
  setup_tagging_store();
  item_cache_create(&item_cache, "fixtures/valid.db");
  item_cache_load(item_cache);
  ce_config = load_config("conf/test.conf");
  ce = create_classification_engine(item_cache, ce_config);
}

static void teardown_end_to_end(void) {
  setup_fixture_path();
  setup_tagging_store();
  free_classification_engine(ce);
  free_config(ce_config);
  free_item_cache(item_cache);
}

START_TEST(inserts_taggings) {
  assert_tagging_count_is(0);  
  ce_start(ce);
  ClassificationJob *job = ce_add_classification_job_for_tag(ce, TAG_ID);
  mark_point();
  ce_stop(ce);
  mark_point();
  assert_tagging_count_is(item_cache_cached_size(item_cache));
  assert_equal_f(100.0, cjob_progress(job));
  assert_equal(CJOB_STATE_COMPLETE, cjob_state(job));
} END_TEST

START_TEST(delete_existing_taggings_before_it_inserts_new_taggings) {
  if (mysql_query(mysql, "insert into taggings (feed_item_id, tag_id, classifier_tagging, user_id, strength) VALUES (1,48,1,2,0.95)")) {
    fail("Failed to create fixture taggings %s", mysql_error(mysql));
  }
  assert_tagging_count_is(1);
  ce_start(ce);
  ClassificationJob *job = ce_add_classification_job_for_tag(ce, TAG_ID);
  mark_point();
  ce_stop(ce);
  mark_point();
  assert_tagging_count_is(item_cache_cached_size(item_cache));
  assert_equal_f(100.0, cjob_progress(job));
  assert_equal(CJOB_STATE_COMPLETE, cjob_state(job));
} END_TEST

START_TEST(inserts_taggings_for_all_users_tags) {
  assert_tagging_count_is(0);
  ce_start(ce);
  ClassificationJob *job = ce_add_classification_job_for_user(ce, 2);
  mark_point();
  ce_stop(ce);
  mark_point();
  assert_tagging_count_is(6 * item_cache_cached_size(item_cache));
  assert_equal_f(100.0, cjob_progress(job));
  assert_equal(CJOB_STATE_COMPLETE, cjob_state(job));
} END_TEST

START_TEST (test_new_items_job_insert_taggings_for_items_with_time_later_than_last_classified) {
  /* This should not delete existing tags */
  if (mysql_query(mysql, "insert into taggings (feed_item_id, tag_id, classifier_tagging, user_id, strength) VALUES (1,38,1,2,0.95)")) {
      fail("Failed to create fixture taggings %s", mysql_error(mysql));
    }
  assert_tagging_count_is(1);
  ce_start(ce);
  ClassificationJob *job = ce_add_classify_new_items_job_for_tag(ce, 38);
  mark_point();
  ce_stop(ce);
  assert_tagging_count_is(5);
} END_TEST


START_TEST(cancelled_job_doesnt_insert_taggings_if_cancelled_before_processed) {
  ClassificationJob *job = ce_add_classification_job_for_tag(ce, TAG_ID);
  info("cancelled job: %s", cjob_id(job));
  cjob_cancel(job);
  ce_start(ce);
  ce_stop(ce);
  mark_point();
  //assert_tagging_count_is(0);
} END_TEST

START_TEST(can_send_bogus_tag_id_without_taking_down_the_server) {
  ClassificationJob *job = ce_add_classification_job_for_tag(ce, BOGUS_TAG_ID);
  
  ce_start(ce);
  ce_stop(ce);
  assert_tagging_count_is(0);
  assert_equal(CJOB_STATE_ERROR, cjob_state(job));
  assert_equal(CJOB_ERROR_NO_SUCH_TAG, cjob_error(job));
  
} END_TEST

START_TEST(can_send_bogus_user_id_without_taking_down_the_server) {
  ClassificationJob *job = ce_add_classification_job_for_user(ce, 10);
  
  ce_start(ce);
  ce_stop(ce);
  assert_tagging_count_is(0);
  assert_equal(CJOB_STATE_ERROR, cjob_state(job));
  assert_equal(CJOB_ERROR_NO_TAGS_FOR_USER, cjob_error(job));
  
} END_TEST

/************************************************************************
 * Job Tracking tests
 ************************************************************************/


static void setup_engine() {
  setup_fixture_path();
  item_cache_create(&item_cache, "fixtures/valid.db");
  item_cache_load(item_cache);
  ce_config = load_config("conf/test.conf");
  ce = create_classification_engine(item_cache, ce_config);
}

static void teardown_engine() {
  teardown_fixture_path();
  free_classification_engine(ce);
  free_config(ce_config);
  free_item_cache(item_cache);
}

START_TEST(add_job_to_queue) {
  ClassificationJob *job = ce_add_classification_job_for_tag(ce, TAG_ID);
  assert_not_null(job);
  assert_equal(TAG_ID, cjob_tag_id(job));
  assert_equal(0.0, cjob_progress(job));
  assert_not_null(cjob_id(job));
  assert_equal(CJOB_STATE_WAITING, cjob_state(job));
  assert_equal(1, ce_num_jobs_in_system(ce));
  assert_equal(1, ce_num_waiting_jobs(ce));
} END_TEST

START_TEST(add_user_job_to_queue) {
  ClassificationJob *job = ce_add_classification_job_for_user(ce, 2);
  assert_not_null(job);
  assert_equal(0, cjob_tag_id(job));
  assert_equal(2, cjob_user_id(job));
  assert_not_null(cjob_id(job));
  assert_equal(CJOB_STATE_WAITING, cjob_state(job));
  assert_equal(1, ce_num_jobs_in_system(ce));
  assert_equal(1, ce_num_waiting_jobs(ce));
} END_TEST

START_TEST(retrieve_job_via_id) {
  ClassificationJob *job = ce_add_classification_job_for_tag(ce, TAG_ID);
  assert_not_null(job);
  ClassificationJob *fetched_job = ce_fetch_classification_job(ce, cjob_id(job));
  assert_equal(job, fetched_job);
} END_TEST

START_TEST(cancelling_a_job_removes_it_from_the_system_once_a_worker_gets_to_it) {
  ClassificationJob *job = ce_add_classification_job_for_tag(ce, TAG_ID);
  char job_id[64];
  strncpy(job_id, cjob_id(job), 64);
    
  assert_equal(1, ce_num_waiting_jobs(ce));
  cjob_cancel(job);
  assert_equal(1, ce_num_waiting_jobs(ce));
  ce_start(ce);
  ce_stop(ce);
  assert_equal(0, ce_num_waiting_jobs(ce));
  assert_equal(0, ce_num_jobs_in_system(ce));

  ClassificationJob *j2 = ce_fetch_classification_job(ce, job_id);
  assert_null(j2);
} END_TEST

START_TEST(cancelling_a_job_sets_its_state_to_cancelled) {
  ClassificationJob *job = ce_add_classification_job_for_tag(ce, TAG_ID);
  cjob_cancel(job);
  assert_equal(CJOB_STATE_CANCELLED, cjob_state(job));
} END_TEST

START_TEST(suspended_classification_engine_processes_no_jobs) {
  ce_add_classification_job_for_tag(ce, TAG_ID);
  ce_add_classification_job_for_tag(ce, TAG_ID);
  assert_equal(2, ce_num_waiting_jobs(ce));
  ce_start(ce);
  int suspended = ce_suspend(ce);
  assert_true(suspended);
  assert_equal(2, ce_num_waiting_jobs(ce));
  ce_stop(ce);
  assert_equal(2, ce_num_waiting_jobs(ce));
} END_TEST

START_TEST(resuming_suspended_engine_processes_jobs) {
  ce_add_classification_job_for_tag(ce, TAG_ID);
  ce_add_classification_job_for_tag(ce, TAG_ID);
  assert_equal(2, ce_num_waiting_jobs(ce));
  int suspended = ce_suspend(ce);
  assert_true(suspended);
  ce_start(ce);
  ce_resume(ce);
  ce_stop(ce);
  assert_equal(0, ce_num_waiting_jobs(ce));
} END_TEST

START_TEST (remove_classification_job_removes_the_job_from_the_engines_job_index_if_job_is_complete) {
  ClassificationJob *job = ce_add_classification_job_for_tag(ce, TAG_ID);
  
  ce_start(ce);
  ce_stop(ce);
  assert_equal(CJOB_STATE_COMPLETE, cjob_state(job));
  int res = ce_remove_classification_job(ce, job);
  assert_true(res);
  ClassificationJob *j2 = ce_fetch_classification_job(ce, cjob_id(job));
  assert_null(j2);
} END_TEST

START_TEST (remove_classification_job_wont_removes_the_job_from_the_engines_job_index_if_job_is_not_complete) {
  ClassificationJob *job = ce_add_classification_job_for_tag(ce, TAG_ID);
  
  int res = ce_remove_classification_job(ce, job);
  assert_false(res);
  ClassificationJob *j2 = ce_fetch_classification_job(ce, cjob_id(job));
  assert_not_null(j2);
} END_TEST

/************************************************************************
 * Initialization tests.
 ************************************************************************/

START_TEST(test_engine_initialization) {
  ItemCache *item_cache;
  item_cache_create(&item_cache, "fixtures/valid.db");
  item_cache_load(item_cache);
  Config *config = load_config("conf/test.conf");
  assert_not_null(config);
  ClassificationEngine *engine = create_classification_engine(item_cache, config);
  assert_not_null(engine);
  assert_false(ce_is_running(engine));
  free_classification_engine(engine);
  free_config(config);
} END_TEST

START_TEST(test_engine_starting_and_stopping) {
  ItemCache *item_cache;
  item_cache_create(&item_cache, "fixtures/valid.db");
  item_cache_load(item_cache);
  Config *config = load_config("conf/test.conf");
  assert_not_null(config);
  
  ClassificationEngine *engine = create_classification_engine(item_cache, config);
  assert_not_null(engine);
  int start_code = ce_start(engine);
  assert_equal(1, start_code);
  assert_true(ce_is_running(engine));
  assert_equal(0, ce_num_jobs_in_system(engine));
  int stop_code = ce_stop(engine);
  assert_equal(1, stop_code);
  assert_false(ce_is_running(engine));
  free_classification_engine(engine);
  free_config(config);
} END_TEST

Suite * classification_engine_suite(void) {
  Suite *s = suite_create("classification_engine");
  TCase *tc_initialization_case = tcase_create("initialization");

  // START_TESTS
  tcase_add_checked_fixture(tc_initialization_case, setup_fixture_path, teardown_fixture_path);
  tcase_add_test(tc_initialization_case, test_engine_initialization); 
  tcase_add_test(tc_initialization_case, test_engine_starting_and_stopping);
  // END_TESTS
  
  TCase *tc_jt_case = tcase_create("job tracking");
  tcase_add_checked_fixture(tc_jt_case, setup_engine, teardown_engine);
  // START_TESTS
  tcase_add_test(tc_jt_case, add_job_to_queue);
  tcase_add_test(tc_jt_case, retrieve_job_via_id);
  tcase_add_test(tc_jt_case, cancelling_a_job_sets_its_state_to_cancelled);
  tcase_add_test(tc_jt_case, cancelling_a_job_removes_it_from_the_system_once_a_worker_gets_to_it);
  tcase_add_test(tc_jt_case, add_user_job_to_queue);
  tcase_add_test(tc_jt_case, suspended_classification_engine_processes_no_jobs);
  tcase_add_test(tc_jt_case, resuming_suspended_engine_processes_jobs);
  tcase_add_test(tc_jt_case, remove_classification_job_removes_the_job_from_the_engines_job_index_if_job_is_complete);
  tcase_add_test(tc_jt_case, remove_classification_job_wont_removes_the_job_from_the_engines_job_index_if_job_is_not_complete);
  // END_TESTS

  TCase *tc_end_to_end = tcase_create("end to end");
  tcase_add_checked_fixture(tc_end_to_end, setup_end_to_end, teardown_end_to_end);
  tcase_add_test(tc_end_to_end, inserts_taggings);
  tcase_add_test(tc_end_to_end, delete_existing_taggings_before_it_inserts_new_taggings);
  tcase_add_test(tc_end_to_end, inserts_taggings_for_all_users_tags);
  tcase_add_test(tc_end_to_end, cancelled_job_doesnt_insert_taggings_if_cancelled_before_processed);
  tcase_add_test(tc_end_to_end, can_send_bogus_tag_id_without_taking_down_the_server);
  tcase_add_test(tc_end_to_end, can_send_bogus_user_id_without_taking_down_the_server);
  tcase_add_test(tc_end_to_end, test_new_items_job_insert_taggings_for_items_with_time_later_than_last_classified);
  
  suite_add_tcase(s, tc_initialization_case);
  suite_add_tcase(s, tc_jt_case);
  suite_add_tcase(s, tc_end_to_end);
  return s;
}
