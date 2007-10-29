// Copyright (c) 2007 The Kaphan Foundation
//
// Possession of a copy of this file grants no permission or license
// to use, modify, or create derivate works.
//
// Please contact info@peerworks.org for further information.

#include <stdlib.h>
#include <math.h>
#include <check.h>
#include "../src/pool.h"
#include "assertions.h"


START_TEST (create_pool) {
  
} END_TEST


  
Suite *
classifier_suite(void) {
  Suite *s = suite_create("Pool");
  
  TCase *tc_pool = tcase_create("Pool");
  
  suite_add_tcase(s, tc_pool);  
  
  return s;
}

int main(void) {
  int number_failed;
  Suite *s = classifier_suite();
  SRunner *sr = srunner_create(s);
  srunner_run_all(sr, CK_NORMAL);
  number_failed = srunner_ntests_failed(sr);
  srunner_free(sr);
  return (number_failed == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}