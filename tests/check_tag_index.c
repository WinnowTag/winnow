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
#include "../src/array.h"
#include "assertions.h"

static char * document;

static void read_document(void) {
  FILE *file;

  if (NULL != (file = fopen("fixtures/tag_index.atom", "r"))) {
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

START_TEST(test_parsing_tag_index_returns_TAG_INDEX_OK) {
  Array *a = create_array(1);
  int rc = parse_tag_index(document, a, NULL);
  assert_equal(TAG_INDEX_OK, rc);
} END_TEST

START_TEST(test_parsing_tag_index_with_bogus_data_returns_TAG_INDEX_FAIL) {
  Array *a = create_array(1);
  int rc = parse_tag_index("bogus", a, NULL);
  assert_equal(TAG_INDEX_FAIL, rc);
} END_TEST
        
START_TEST(test_parsing_tag_index_fills_array_with_training_links) {
  Array *a = create_array(1);
  parse_tag_index(document, a, NULL);
  assert_equal(2, a->size);
  assert_equal_s("http://localhost:8888/quentin/tags/tag/training.atom", (char *) a->elements[0]);
  assert_equal_s("http://localhost:8888/aaron/tags/other/training.atom", (char *) a->elements[1]);
} END_TEST
        
START_TEST(test_parses_updated_date) {
  Array *a = create_array(1);
  time_t updated = -1;
  parse_tag_index(document, a, &updated);
  assert_equal(1210560134, updated);
} END_TEST
        
Suite *
tag_index_parsing_suite(void) {
  Suite *s = suite_create("Tag Index");  
  TCase *tag_index_parsing = tcase_create("tag_index_parsing");

  tcase_add_checked_fixture(tag_index_parsing, read_document, free_document);
  tcase_add_test(tag_index_parsing, test_parsing_tag_index_returns_TAG_INDEX_OK);
  tcase_add_test(tag_index_parsing, test_parsing_tag_index_with_bogus_data_returns_TAG_INDEX_FAIL);
  tcase_add_test(tag_index_parsing, test_parsing_tag_index_fills_array_with_training_links);
  tcase_add_test(tag_index_parsing, test_parses_updated_date);

  suite_add_tcase(s, tag_index_parsing);
  return s;
}

int main(void) {
  initialize_logging("test.log");
  int number_failed;
  
  SRunner *sr = srunner_create(tag_index_parsing_suite());
  srunner_run_all(sr, CK_NORMAL);
  number_failed = srunner_ntests_failed(sr);
  srunner_free(sr);
  close_log();
  return (number_failed == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}
