// Copyright (c) 2007 The Kaphan Foundation
//
// Possession of a copy of this file grants no permission or license
// to use, modify, or create derivate works.
//
// Please contact info@peerworks.org for further information.

#include <stdlib.h>
#include <check.h>

Suite * pool_suite (void);
Suite * classifier_suite (void);
Suite * item_suite (void);
Suite * tag_suite (void);

int main(void) {
  int number_failed;
  
  SRunner *sr = srunner_create(pool_suite());
  srunner_add_suite(sr, classifier_suite());
  srunner_add_suite(sr, item_suite());
  srunner_add_suite(sr, tag_suite());
  
  srunner_run_all(sr, CK_NORMAL);
  number_failed = srunner_ntests_failed(sr);
  srunner_free(sr);
  return (number_failed == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}