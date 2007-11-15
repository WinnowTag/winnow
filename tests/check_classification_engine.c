/* Copyright (c) 2007 The Kaphan Foundation
 *
 * Possession of a copy of this file grants no permission or license
 * to use, modify, or create derivate works.
 *
 * Please contact info@peerworks.org for further information.
 */

#include <check.h>
#include "assertions.h"
#include "../src/cls_config.h"
#include "../src/classification_engine.h"
#include "tagging_store_fixtures.h"
#include "../src/logging.h"

#define TAG_ID 48
#define BOGUS_TAG_ID 11111

/************************************************************************
 * End to End tests
 ************************************************************************/
START_TEST(inserts_taggings) {
  assert_tagging_count_is(0);
  Config *config = load_config("fixtures/real-db.conf");
  ClassificationEngine *ce = create_classification_engine(config);
  ce_start(ce);
  ClassificationJob *job = ce_add_classification_job(ce, TAG_ID);
  mark_point();
  ce_stop(ce);
  mark_point();
  assert_tagging_count_is(11);
  assert_equal_f(100.0, cjob_progress(job));
  assert_equal(CJOB_STATE_COMPLETE, cjob_state(job));
  free_classification_engine(ce);
  free_config(config);
} END_TEST

START_TEST(cancelled_job_doesnt_insert_taggings_if_cancelled_before_processed) {
  Config *config = load_config("fixtures/real-db.conf");
  ClassificationEngine *ce = create_classification_engine(config);
  ClassificationJob *job = ce_add_classification_job(ce, TAG_ID);
  info("cancelled job: %s", cjob_id(job));
  cjob_cancel(job);
  ce_start(ce);
  ce_stop(ce);
  mark_point();
  //assert_tagging_count_is(0);
  free_classification_engine(ce);
  free_config(config);
}
END_TEST

START_TEST(can_send_bogus_tag_id_without_taking_down_the_server) {
  Config *config = load_config("fixtures/real-db.conf");
  ClassificationEngine *ce = create_classification_engine(config);
  ClassificationJob *job = ce_add_classification_job(ce, BOGUS_TAG_ID);
  
  ce_start(ce);
  ce_stop(ce);
  assert_tagging_count_is(0);
  assert_equal(CJOB_STATE_ERROR, cjob_state(job));
  assert_equal(CJOB_ERROR_NO_SUCH_TAG, cjob_error(job));
  
  free_classification_engine(ce);
  free_config(config);
}
END_TEST


/************************************************************************
 * Job Tracking tests
 ************************************************************************/
Config *ce_config;
ClassificationEngine *ce;

static void setup_engine() {
  ce_config = load_config("fixtures/real-db.conf");
  ce = create_classification_engine(ce_config);
}

static void teardown_engine() {
  free_classification_engine(ce);
  free_config(ce_config);
}

START_TEST(add_job_to_queue) {
  ClassificationJob *job = ce_add_classification_job(ce, TAG_ID);
  assert_not_null(job);
  assert_equal(TAG_ID, cjob_tag_id(job));
  assert_equal(0.0, cjob_progress(job));
  assert_not_null(cjob_id(job));
  assert_equal(CJOB_STATE_WAITING, cjob_state(job));
  assert_equal(1, ce_num_jobs_in_system(ce));
  assert_equal(1, ce_num_waiting_jobs(ce));
} END_TEST

START_TEST(retrieve_job_via_id) {
  ClassificationJob *job = ce_add_classification_job(ce, TAG_ID);
  assert_not_null(job);
  ClassificationJob *fetched_job = ce_fetch_classification_job(ce, cjob_id(job));
  assert_equal(job, fetched_job);
} END_TEST

START_TEST(cancelling_a_job_removes_it_from_the_system_once_a_worker_gets_to_it) {
  ClassificationJob *job = ce_add_classification_job(ce, TAG_ID);
  assert_equal(1, ce_num_waiting_jobs(ce));
  cjob_cancel(job);
  assert_equal(1, ce_num_waiting_jobs(ce));
  ce_start(ce);
  ce_stop(ce);
  assert_equal(0, ce_num_waiting_jobs(ce));
  assert_equal(0, ce_num_jobs_in_system(ce));
} END_TEST

START_TEST(cancelling_a_job_sets_its_state_to_cancelled) {
  ClassificationJob *job = ce_add_classification_job(ce, TAG_ID);
  cjob_cancel(job);
  assert_equal(CJOB_STATE_CANCELLED, cjob_state(job));
} END_TEST

/************************************************************************
 * Initialization tests.
 ************************************************************************/

START_TEST(test_engine_initialization) {
  Config *config = load_config("fixtures/real-db.conf");
  assert_not_null(config);
  ClassificationEngine *engine = create_classification_engine(config);
  assert_not_null(engine);
  assert_false(ce_is_running(engine));
  free_classification_engine(engine);
  free_config(config);
} END_TEST

START_TEST(test_engine_starting_and_stopping) {
  Config *config = load_config("fixtures/real-db.conf");
  assert_not_null(config);
  ClassificationEngine *engine = create_classification_engine(config);
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

START_TEST(test_engine_initialization_with_non_existant_db) {
  Config *config = load_config("fixtures/db.conf");
  assert_not_null(config);
  ClassificationEngine *engine = create_classification_engine(config);
  assert_null(engine);
  free_config(config);
} END_TEST

START_TEST(test_engine_initialization_without_corpus_defined) {
  Config *config = load_config("fixtures/no-db.conf");
  assert_not_null(config);
  ClassificationEngine *engine = create_classification_engine(config);
  assert_null(engine);
  free_config(config);
} END_TEST

START_TEST(end_to_end_test) {
  
}
END_TEST

Suite * classification_engine_suite(void) {
  Suite *s = suite_create("classification_engine");
  TCase *tc_initialization_case = tcase_create("initialization");

  // START_TESTS
  tcase_add_test(tc_initialization_case, test_engine_initialization); 
  tcase_add_test(tc_initialization_case, test_engine_initialization_without_corpus_defined);
  tcase_add_test(tc_initialization_case, test_engine_initialization_with_non_existant_db);
  tcase_add_test(tc_initialization_case, test_engine_starting_and_stopping);
  // END_TESTS
  
  TCase *tc_jt_case = tcase_create("job tracking");
  tcase_add_checked_fixture(tc_jt_case, setup_engine, teardown_engine);
  // START_TESTS
  tcase_add_test(tc_jt_case, add_job_to_queue);
  tcase_add_test(tc_jt_case, retrieve_job_via_id);
  tcase_add_test(tc_jt_case, cancelling_a_job_sets_its_state_to_cancelled);
  tcase_add_test(tc_jt_case, cancelling_a_job_removes_it_from_the_system_once_a_worker_gets_to_it);
  // END_TESTS

  TCase *tc_end_to_end = tcase_create("end to end");
  tcase_add_checked_fixture(tc_end_to_end, setup_tagging_store, teardown_tagging_store);
  tcase_add_test(tc_end_to_end, inserts_taggings);
  tcase_add_test(tc_end_to_end, cancelled_job_doesnt_insert_taggings_if_cancelled_before_processed);
  tcase_add_test(tc_end_to_end, can_send_bogus_tag_id_without_taking_down_the_server);
  
  suite_add_tcase(s, tc_initialization_case);
  suite_add_tcase(s, tc_jt_case);
  suite_add_tcase(s, tc_end_to_end);
  return s;
}