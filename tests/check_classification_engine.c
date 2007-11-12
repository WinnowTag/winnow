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

#define TAG_ID 39
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
  assert_equal(1, ce_num_jobs_in_system(ce));
  assert_equal(1, ce_num_waiting_jobs(ce));
} END_TEST

START_TEST(retrieve_job_via_id) {
  ClassificationJob *job = ce_add_classification_job(ce, TAG_ID);
  assert_not_null(job);
  ClassificationJob *fetched_job = ce_fetch_classification_job(ce, cjob_id(job));
  assert_equal(job, fetched_job);
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
//  // END_TESTS

  suite_add_tcase(s, tc_initialization_case);
  suite_add_tcase(s, tc_jt_case);
  return s;
}
