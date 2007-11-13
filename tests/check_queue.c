// Copyright (c) 2007 The Kaphan Foundation
//
// Possession of a copy of this file grants no permission or license
// to use, modify, or create derivate works.
//
// Please contact info@peerworks.org for further information.

#include <check.h>
#include "assertions.h"
#include "../src/job_queue.h"

typedef struct JOB {
  int id;
} Job;

START_TEST (empty) {
  Queue *q = new_queue();
  assert_not_null(q);
  assert_true(q_empty(q));
  assert_null(q_dequeue(q));
  free_queue(q);
} END_TEST

START_TEST (single_item) {
  Job job;
  Queue *q = new_queue();
  assert_not_null(q);
  q_enqueue(q, &job);
  Job *d_job = q_dequeue(q);
  assert_equal(&job, d_job);
  free_queue(q);
} END_TEST

START_TEST (multiple_items) {
  Job job1, job2;
  Queue *q = new_queue();
  assert_not_null(q);
  q_enqueue(q, &job1);
  q_enqueue(q, &job2);
  assert_false(q_empty(q));
  
  Job *d_job1 = q_dequeue(q);
  Job *d_job2 = q_dequeue(q);
  assert_equal(&job1, d_job1);
  assert_equal(&job2, d_job2);
  free_queue(q);
} END_TEST

START_TEST(check_queue_size) {
  Job job1, job2;
  Queue *q = new_queue();
  assert_equal(0, q_size(q));
  q_enqueue(q, &job1);
  assert_equal(1, q_size(q));
  q_enqueue(q, &job2);
  assert_equal(2, q_size(q));
  free_queue(q);
} END_TEST

Job *dequeued_by_thread = NULL;
void dequeue_it(void *qp) {
  Queue *q = (Queue*) qp;
  dequeued_by_thread = q_dequeue_or_wait(q);
}

START_TEST (check_dequeue_or_wait) {
  Job job;
  Queue *q = new_queue();
  pthread_t thread;
  pthread_create(&thread, NULL, dequeue_it, q);
  usleep(10);
  q_enqueue(q, &job);
  pthread_join(thread);
  assert_not_null(dequeued_by_thread);
  assert_equal(&job, dequeued_by_thread);
} END_TEST

START_TEST(check_timeout) {
  Queue *q = new_queue();
  Job *j = q_dequeue_or_wait(q);
  mark_point();
  assert_null(j);
}
END_TEST


Suite *
queue_suite(void) {
  Suite *s = suite_create("Queue");  
  TCase *tc_queue = tcase_create("Queue");

// START_TESTS
  tcase_add_test(tc_queue, empty);
  tcase_add_test(tc_queue, single_item);
  tcase_add_test(tc_queue, multiple_items);
  tcase_add_test(tc_queue, check_dequeue_or_wait);
  tcase_add_test(tc_queue, check_queue_size);
  tcase_add_test(tc_queue, check_timeout);
// END_TESTS

  suite_add_tcase(s, tc_queue);
  return s;
}
