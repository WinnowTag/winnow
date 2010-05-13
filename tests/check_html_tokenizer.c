// Copyright (c) 2007 The Kaphan Foundation
//
// Possession of a copy of this file grants no permission or license
// to use, modify, or create derivate works.
//
// Please contact info@peerworks.org for further information.

#include <stdlib.h>
#include <math.h>
#include <check.h>
#include "fixtures.h"
#include "../src/tokenizer.h"
#include "assertions.h"

static void assertFeatures(const char *html, char * tokens[], int * frequencies, int size) {
	int i,j;
	Pvoid_t features;

	features = html_tokenize(html);

	//assert_equal(size, features->size);

	for (i = 0; i < size; i++) {
		Word_t *PValue;
		JSLG(PValue, features, tokens[i]);
		assert_not_null_msg(PValue, tokens[i]);
		assert_equal(frequencies[i], *PValue);
	}

	Word_t rc;
	JSLFA(rc, features);
}

START_TEST(should_split_text_from_html) {
	const char *html = "top <p>text <span>html</span> content</p>";
	char * tokens[] = {"t:top", "t:text", "t:html", "t:content"};
	int freq[] = {1,1,1,1};
	assertFeatures(html, tokens, freq, 4);
} END_TEST

START_TEST(should_extract_urls) {
	char *html = "top <p>text <span>html</span> <a href=\"http://www.test.com/path/page.html\">content</a></p><img src=\"/path/img\"/>";
	char *tokens[] = {"t:html", "t:text", "URLSeg:/path/page.html", "URLSeg:test.com", "t:content", "URLSeg:/path/img", "t:top"};
	int freq[] = {1, 1, 1, 1, 1, 1, 1};
	assertFeatures(html, tokens, freq, 7);
} END_TEST

START_TEST(should_fold_case) {
	char *html = "<p>Text <span>HTML</span> content</p>";
	char *tokens[] = {"t:html", "t:text", "t:content"};
	int freq[] = {1, 1, 1};
	assertFeatures(html, tokens, freq, 3);

} END_TEST

START_TEST(should_strip_out_punctuation) {
	char *html = "<p>text, -other-<span>?html!!</span> content.</p>";
	char *tokens[] = {"t:html", "t:text", "t:content", "t:other"};
	int freq[] = {1, 1, 1, 1};
	assertFeatures(html, tokens, freq, 4);
} END_TEST

START_TEST(should_strip_out_html_entities) {
	char *html = "<p>text&amp; <span>&#8110;html</span> foo-content</p>";
	char *tokens[] = {"t:html", "t:text", "t:foo-content"};
	int freq[] = {1, 1, 1};
	assertFeatures(html, tokens, freq, 3);
} END_TEST

START_TEST(should_remove_single_characters) {
	char *html = "<p>text <span>html</span> content a</p>";
	char *tokens[] = {"t:html", "t:text", "t:content"};
	int freq[] = {1, 1, 1};
	assertFeatures(html, tokens, freq, 3);
} END_TEST

START_TEST(should_aggregate_tokens) {
	char *html = "<p>text text <span>html html html</span> content</p>";
	char *tokens[] = {"t:html", "t:text", "t:content"};
	int freq[] = {3, 2, 1};
	assertFeatures(html, tokens, freq, 3);
} END_TEST

Suite *
pool_suite(void) {
  Suite *s = suite_create("HTML Tokenizer");

  TCase *tcase = tcase_create("HTML Tokenizer");
  tcase_add_test(tcase, should_split_text_from_html);
  tcase_add_test(tcase, should_extract_urls);
  tcase_add_test(tcase, should_fold_case);
  tcase_add_test(tcase, should_strip_out_punctuation);
  tcase_add_test(tcase, should_strip_out_html_entities);
  tcase_add_test(tcase, should_remove_single_characters);
  tcase_add_test(tcase, should_aggregate_tokens);
  suite_add_tcase(s, tcase);

  return s;
}

int main(void) {
  initialize_logging("test.log");
  int number_failed;

  SRunner *sr = srunner_create(pool_suite());
  srunner_run_all(sr, CK_NORMAL);
  number_failed = srunner_ntests_failed(sr);
  srunner_free(sr);
  close_log();
  return (number_failed == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}
