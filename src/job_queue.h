// Copyright (c) 2007 The Kaphan Foundation
//
// Possession of a copy of this file grants no permission or license
// to use, modify, or create derivate works.
//
// Please contact info@peerworks.org for further information.

#ifndef _QUEUE_H_
#define _QUEUE_H_

typedef struct JOB {
  int id;
} Job;

typedef struct QUEUE Queue;

extern Queue * new_queue          ();
extern Job   * q_dequeue          (Queue * queue);
extern Job   * q_dequeue_or_wait  (Queue * queue);
extern void    q_enqueue          (Queue * queue, Job * job);
extern int     q_empty            (const Queue * queue);

#endif /* _QUEUE_H_ */
