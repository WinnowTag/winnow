// Copyright (c) 2007 The Kaphan Foundation
//
// Possession of a copy of this file grants no permission or license
// to use, modify, or create derivate works.
//
// Please contact info@peerworks.org for further information.

#include <stdlib.h>
#include <check.h>
#include "assertions.h"
#include "../src/tag.h"

START_TEST (loads_right_number_of_tags) {
  TagList tags = load_tags_from_file("fixtures", "user");
  assert_not_null(tags);
  assert_equal(2, taglist_size(tags));
  free_taglist(tags);
} END_TEST

START_TEST (all_tags_have_user_name_set) {
  printf("all_tags_start\n");
  int i;
  Tag tag;
  TagList tags = load_tags_from_file("fixtures", "user");
  assert_not_null(tags);
  assert_equal(2, taglist_size(tags));
  
  for (i = 1; i <= 2; i++) {
    tag = taglist_get_tag(tags, i);
    assert_not_null(tag);
    assert_equal_s("user", tag_get_user(tag));
  }
  
  free_taglist(tags);
} END_TEST

START_TEST (tags_have_tag_set) {
  printf("tags_have_tag_set\n");
  Tag tag;
  TagList tags = load_tags_from_file("fixtures", "user");
  
  tag = taglist_get_tag(tags, 1);
  assert_not_null(tag);
  assert_equal_s("tag1", tag_get_tag_name(tag));

  tag = taglist_get_tag(tags, 2);
  assert_not_null(tag);
  assert_equal_s("tag with space", tag_get_tag_name(tag));
  
  free_taglist(tags);
} END_TEST

Suite *
tag_suite(void) {
  Suite *s = suite_create("Tag");  
  TCase *tc_tag = tcase_create("Tag");

// START_TESTS
  tcase_add_test(tc_tag, loads_right_number_of_tags);
  tcase_add_test(tc_tag, all_tags_have_user_name_set);
  tcase_add_test(tc_tag, tags_have_tag_set);
// END_TESTS

  suite_add_tcase(s, tc_tag);
  return s;
}
