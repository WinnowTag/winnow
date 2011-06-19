// General info: http://doc.winnowtag.org/open-source
// Source code repository: http://github.com/winnowtag
// Questions and feedback: contact@winnowtag.org
//
// Copyright (c) 2007-2011 The Kaphan Foundation
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in
// all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
// THE SOFTWARE.

// contact@winnowtag.org

#include <stdlib.h>
#include <math.h>
#include <check.h>
#include "fixtures.h"
#include "../src/tokenizer.h"
#include "assertions.h"

static void assertFeaturesEqual(Pvoid_t features, char * tokens[], int * frequencies, int size) {
	int i,j;

	for (i = 0; i < size; i++) {
		Word_t *PValue;
		JSLG(PValue, features, tokens[i]);
		assert_not_null_msg(PValue, tokens[i]);
		assert_equal(frequencies[i], *PValue);
	}

	Word_t rc;
	JSLFA(rc, features);
}

static void assertFeatures(const char *html, char * tokens[], int * frequencies, int size) {
	Pvoid_t features = html_tokenize(html);
	assertFeaturesEqual(features, tokens, frequencies, size);
}

static void assertEntryFeatures(const char * atom, char * tokens[], int * frequencies, int size) {
	Pvoid_t features = atom_tokenize(atom);
	assertFeaturesEqual(features, tokens, frequencies, size);
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


START_TEST(should_split_html_from_content) {
	char *atom = "<?xml version=\"1.0\" encoding=\"UTF-8\"?>"
				"<entry xmlns=\"http://www.w3.org/2005/Atom\">"
				  "<content type=\"html\">top &lt;p&gt;text &lt;span&gt;html&lt;/span&gt; content&lt;/p&gt;</content>"
				"</entry>";
	char *tokens[] = {"t:html", "t:top", "t:text", "t:content"};
	int freq[] = {1,1,1,1};
	assertEntryFeatures(atom, tokens, freq, 4);
} END_TEST

START_TEST(should_fold_case_with_atom_properties) {
	char *atom = "<?xml version=\"1.0\" encoding=\"UTF-8\"?>"
				"<entry xmlns=\"http://www.w3.org/2005/Atom\">"
				"<title>Title</title>"
				"<author>"
					"<name>Sean Geoghegan</name>"
				"</author>"
				"<link rel=\"alternate\" href=\"http://example.org/article.html\"/>"
				"<content type=\"html\">&lt;p&gt;Text &lt;span&gt;HTML&lt;/span&gt; content&lt;/p&gt;</content>"
				"</entry>";
	char *tokens[] = {"t:html", "t:title", "t:text", "t:Sean Geoghegan", "t:content", "URLSeg:example.org", "URLSeg:/article.html"};
	int freq[] = {1,1,1,1,1,1,1};
	assertEntryFeatures(atom, tokens, freq, 7);
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
  tcase_add_test(tcase, should_split_html_from_content);
  tcase_add_test(tcase, should_fold_case_with_atom_properties);
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
