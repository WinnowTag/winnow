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
#include "fixtures.h"
#include "read_document.h"

static char * document;

static void setup_document(void) {
  setup_fixture_path();
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
  teardown_fixture_path();
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
        
static TaggerCache *tagger_cache;
static TaggerCacheOptions options = {"tag_url", NULL};

static int load_tag_index_document(const char * tag_index_url, time_t last_updated, const Credentials * ignore, char ** tag_document, char ** errmsg) {
  *tag_document = read_document("fixtures/tag_index.atom");
  return TAG_INDEX_OK;
}

static int load_bogus_tag_index(const char * tag_index_url, time_t last_updated, const Credentials * ignore, char ** tag_document, char ** errmsg) {
  *tag_document = malloc(6);
  strcpy(*tag_document, "bogus");
  return TAG_INDEX_OK;
}

static void setup_fetcher(void) {
	setup_fixture_path();
  tagger_cache = create_tagger_cache(NULL, &options);
  tagger_cache->tag_index_retriever = load_tag_index_document;
}

static void teardown_fetcher(void) {
	teardown_fixture_path();
  free_tagger_cache(tagger_cache);
}

START_TEST(test_tag_index_fetching) {
  char *err;
  Array *a;
  int rc = fetch_tags(tagger_cache, &a, &err);
  assert_equal(TAG_INDEX_OK, rc);
  assert_equal(2, a->size);
  assert_equal_s("http://localhost:8888/quentin/tags/tag/training.atom", (char *) a->elements[0]);
  assert_equal_s("http://localhost:8888/aaron/tags/other/training.atom", (char *) a->elements[1]);
} END_TEST

START_TEST(test_fetched_tag_index_without_caching_errors) {
  Array *a;
  char *err;
  int rc = fetch_tags(tagger_cache, &a, &err);
  assert_equal(TAG_INDEX_OK, rc);
  assert_equal(2, a->size);
  assert_equal_s("http://localhost:8888/quentin/tags/tag/training.atom", (char *) a->elements[0]);
  assert_equal_s("http://localhost:8888/aaron/tags/other/training.atom", (char *) a->elements[1]);

  tagger_cache->tag_index_retriever = load_bogus_tag_index;
  rc = fetch_tags(tagger_cache, &a, NULL);
  assert_equal(TAG_INDEX_OK, rc);
  assert_equal(2, a->size);
  assert_equal_s("http://localhost:8888/quentin/tags/tag/training.atom", (char *) a->elements[0]);
  assert_equal_s("http://localhost:8888/aaron/tags/other/training.atom", (char *) a->elements[1]);
} END_TEST

Suite *
tag_index_parsing_suite(void) {
  Suite *s = suite_create("Tag Index");  
  TCase *tag_index_parsing = tcase_create("tag_index_parsing");

  tcase_add_checked_fixture(tag_index_parsing, setup_document, free_document);
  tcase_add_test(tag_index_parsing, test_parsing_tag_index_returns_TAG_INDEX_OK);
  tcase_add_test(tag_index_parsing, test_parsing_tag_index_with_bogus_data_returns_TAG_INDEX_FAIL);
  tcase_add_test(tag_index_parsing, test_parsing_tag_index_fills_array_with_training_links);
  tcase_add_test(tag_index_parsing, test_parses_updated_date);

  TCase *tag_index_fetching = tcase_create("tag_index_fetching");
  tcase_add_checked_fixture(tag_index_fetching, setup_fetcher, teardown_fetcher);
  tcase_add_test(tag_index_fetching, test_tag_index_fetching);
  tcase_add_test(tag_index_fetching, test_fetched_tag_index_without_caching_errors);
  
  suite_add_tcase(s, tag_index_parsing);
  suite_add_tcase(s, tag_index_fetching);
  
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
