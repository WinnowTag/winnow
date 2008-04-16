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

static char * document;

static void read_document(void) {
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
  assert_equal_s("http://trunk.mindloom.org:80/seangeo/tags/a-religion/classifier_taggings.atom", tagger->classifier_taggings_url);
} END_TEST

START_TEST (test_load_tagger_from_tag_definition_document_sets_tag_id) {
  Tagger *tagger = build_tagger(document);
  assert_not_null(tagger->tag_id);
  assert_equal_s("http://trunk.mindloom.org:80/seangeo/tags/a-religion", tagger->tag_id);
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