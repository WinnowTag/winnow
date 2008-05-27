/* Copyright (c) 2007 The Kaphan Foundation
 *
 * Possession of a copy of this file grants no permission or license
 * to use, modify, or create derivate works.
 *
 * Please contact info@peerworks.org for further information.
 */

#include <check.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "../src/tagger.h"
#include "assertions.h"
#include "fixtures.h"

static char * document;

static void read_document(void) {
  setup_fixture_path();
  FILE *file;

  if (NULL != (file = fopen("fixtures/complete_tag.atom", "r"))) {
    fseek(file, 0, SEEK_END);
    int size = ftell(file);
    document = calloc(size, sizeof(char));
    fseek(file, 0, SEEK_SET);
    fread(document, sizeof(char), size, file);
    document[size] = 0;
    fclose(file);
  }
}

static void free_document(void) {
  teardown_fixture_path();
  if (document) {
    free(document);
  }
}

START_TEST (test_load_tagger_from_tag_definition_document_sets_last_classified) {
  Tagger *tagger = build_tagger(document);
  assert_equal(1208222183, tagger->last_classified);
} END_TEST

START_TEST (test_load_tagger_from_tag_definition_document_sets_updated) {
  Tagger *tagger = build_tagger(document);
  assert_equal(1206840258, tagger->updated);  
} END_TEST

START_TEST (test_load_tagger_from_tag_definition_document_sets_bias) {
  Tagger *tagger = build_tagger(document);
  assert_equal_f(1.2, tagger->bias);
} END_TEST

START_TEST (test_load_tagger_from_tag_definition_document_sets_training_url) {
  Tagger *tagger = build_tagger(document);
  assert_not_null(tagger->training_url);
  assert_equal_s("http://trunk.mindloom.org:80/seangeo/tags/a-religion/training.atom", tagger->training_url);
} END_TEST

START_TEST (test_load_tagger_from_tag_definition_document_sets_classifier_taggings_url) {
  Tagger *tagger = build_tagger(document);
  assert_not_null(tagger->classifier_taggings_url);
  assert_equal_s("http://localhost:8888/results", tagger->classifier_taggings_url);
} END_TEST
        
START_TEST (test_load_tagging_from_tag_definition_document_set_tagger_term) {
  Tagger *tagger = build_tagger(document);
  assert_not_null(tagger->term);
  assert_equal_s("a-religion", tagger->term);
} END_TEST
        
START_TEST (test_load_tagging_from_tag_definition_document_set_tagger_scheme) {
  Tagger *tagger = build_tagger(document);
  assert_not_null(tagger->scheme);
  assert_equal_s("http://trunk.mindloom.org:80/seangeo/tags/", tagger->scheme);
} END_TEST

START_TEST (test_load_tagger_from_tag_definition_document_sets_tag_id) {
  Tagger *tagger = build_tagger(document);
  assert_not_null(tagger->tag_id);
  assert_equal_s("http://trunk.mindloom.org:80/seangeo/tags/a-religion", tagger->tag_id);
} END_TEST

START_TEST (test_loads_right_number_of_positive_examples) {
  Tagger *tagger = build_tagger(document);
  assert_equal(4, tagger->positive_example_count);
} END_TEST

START_TEST (test_loads_right_positive_examples) {
  Tagger *tagger = build_tagger(document);
  assert_not_null(tagger->positive_examples);
  assert_equal_s("urn:peerworks.org:entry#753459", tagger->positive_examples[0]);
  assert_equal_s("urn:peerworks.org:entry#886294", tagger->positive_examples[1]);
  assert_equal_s("urn:peerworks.org:entry#888769", tagger->positive_examples[2]);
  assert_equal_s("urn:peerworks.org:entry#884409", tagger->positive_examples[3]);
} END_TEST

START_TEST (test_loads_right_number_of_negative_examples) {
  Tagger *tagger = build_tagger(document);  
  assert_equal(1, tagger->negative_example_count);
} END_TEST

START_TEST (test_loads_right_negative_examples) {
  Tagger *tagger = build_tagger(document);
  assert_not_null(tagger->negative_examples);
  assert_equal_s("urn:peerworks.org:entry#880389", tagger->negative_examples[0]);
} END_TEST

Suite *
tag_loading_suite(void) {
  Suite *s = suite_create("Tag_loading");  
  TCase *tc_complete_tag = tcase_create("complete_tag.atom");

  tcase_add_checked_fixture(tc_complete_tag, read_document, free_document);
  tcase_add_test(tc_complete_tag, test_load_tagger_from_tag_definition_document_sets_last_classified);
  tcase_add_test(tc_complete_tag, test_load_tagger_from_tag_definition_document_sets_updated);
  tcase_add_test(tc_complete_tag, test_load_tagger_from_tag_definition_document_sets_bias);
  tcase_add_test(tc_complete_tag, test_load_tagger_from_tag_definition_document_sets_training_url);
  tcase_add_test(tc_complete_tag, test_load_tagger_from_tag_definition_document_sets_classifier_taggings_url);
  tcase_add_test(tc_complete_tag, test_load_tagger_from_tag_definition_document_sets_tag_id);
  tcase_add_test(tc_complete_tag, test_loads_right_number_of_positive_examples);
  tcase_add_test(tc_complete_tag, test_loads_right_number_of_negative_examples);
  tcase_add_test(tc_complete_tag, test_loads_right_positive_examples);
  tcase_add_test(tc_complete_tag, test_loads_right_negative_examples);
  tcase_add_test(tc_complete_tag, test_load_tagging_from_tag_definition_document_set_tagger_term);
  tcase_add_test(tc_complete_tag, test_load_tagging_from_tag_definition_document_set_tagger_scheme);

  suite_add_tcase(s, tc_complete_tag);
  return s;
}

int main(void) {
  initialize_logging("test.log");
  int number_failed;
  
  SRunner *sr = srunner_create(tag_loading_suite());
  srunner_run_all(sr, CK_NORMAL);
  number_failed = srunner_ntests_failed(sr);
  srunner_free(sr);
  close_log();
  return (number_failed == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}