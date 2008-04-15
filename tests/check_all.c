// Copyright (c) 2007 The Kaphan Foundation
//
// Possession of a copy of this file grants no permission or license
// to use, modify, or create derivate works.
//
// Please contact info@peerworks.org for further information.

#include <stdlib.h>
#include <check.h>
#include "../src/logging.h"

Suite * pool_suite (void);
Suite * classifier_suite (void);
Suite * clue_suite (void);
Suite * queue_suite (void);
Suite * config_suite (void);
Suite * item_cache_suite(void);

int main(void) {
  initialize_logging("test.log");
  int number_failed;
  
  SRunner *sr = srunner_create(pool_suite());
  srunner_add_suite(sr, classifier_suite());
  srunner_add_suite(sr, clue_suite());
  srunner_add_suite(sr, queue_suite());
  srunner_add_suite(sr, config_suite());
  srunner_add_suite(sr, item_cache_suite());
  
  srunner_run_all(sr, CK_NORMAL);
  number_failed = srunner_ntests_failed(sr);
  srunner_free(sr);
  close_log();
  return (number_failed == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}
