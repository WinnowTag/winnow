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

#include <pthread.h>
#include <stdlib.h>
#include <time.h>
#include <sys/time.h>
#include <errno.h>
#include "job_queue.h"
#include "logging.h"

#define WAIT_TIME_SECONDS 1

typedef struct NODE Node;
struct NODE {
  void *job;
  Node *next;
};

struct QUEUE {
  pthread_mutex_t lock;
  pthread_mutex_t wait_condition_mutex;
  pthread_cond_t  wait_condition;
  Node *front;
  Node *rear;
};

/** Creates a new empty Queue */
Queue * new_queue() {
  Queue *q = malloc(sizeof(struct QUEUE));
  if (NULL != q) {
    q->front = NULL;
    q->rear = NULL;
    if (pthread_mutex_init(&(q->lock), NULL)) {
      free(q);
      error("Error initializing mutex");
      exit(1);
    }
    
    if (pthread_mutex_init(&(q->wait_condition_mutex), NULL)) {
      free(q);
      error("Error initializing wait condition mutex");
      exit(1);
    }
    
    if (pthread_cond_init(&(q->wait_condition), NULL)) {
      free(q);
      error("Error initializing wait condition");
      exit(1);
    }
  }
  return q;
}

void free_queue(Queue * queue) {
  if (queue) {
    Node *node = queue->front;
    while (node) {
      Node *next = node->next;
      free(node);
      node = next;
    }
    
    free(queue);
  }
}

/** Dequeues a Job from the queue.
 *
 *  Returns NULL immediately if the queue is empty.
 */
void * q_dequeue(Queue * q) {
  void *return_job = NULL;
  Node *dequeued = NULL;
  
  pthread_mutex_lock(&(q->lock));
  dequeued = q->front;
  if (NULL != dequeued) {
    q->front = q->front->next;
  }
  pthread_mutex_unlock(&(q->lock));
  
  if (NULL != dequeued) {
    return_job = dequeued->job;
    free(dequeued);
  }
  
  return return_job;
}

/** Dequeue a Job from the queue.
 *
 *  Waits until a Job has been added to the queue if
 *  the queue is empty, times out after seconds.
 *
 *  This was helped by http://www.yolinux.com/TUTORIALS/LinuxTutorialPosixThreads.html#BASICS
 */
void * q_dequeue_or_wait(Queue * q, int seconds) {
  void *job = NULL;  
  struct timeval tv;
  struct timespec ts;
  gettimeofday(&tv,NULL);
  
  /* Convert from timeval to timespec */
  ts.tv_sec  = tv.tv_sec;
  ts.tv_nsec = tv.tv_usec * 1000;
  ts.tv_sec += seconds;
      
  /* The algorith here is first check if there is a job in the queue.
   *  - If there is no job wait until a job is added.
   *   - When a job is added the thread is woken then we try and
   *     take the job off the queue.
   *  - Keep doing this until we get a job.
   */
  while (NULL == job) {
    trace("getting cond lock: %i", pthread_self());
    pthread_mutex_lock(&(q->wait_condition_mutex));
    if (NULL == q->front) {
      trace("pthread_cond_timedwait: %i", pthread_self());
      int rc = pthread_cond_timedwait(&(q->wait_condition), &(q->wait_condition_mutex), &ts);
      trace("queue wait return: %i", pthread_self());
      if (ETIMEDOUT == rc) {        
        trace("queue wait time out: %i", pthread_self());
        pthread_mutex_unlock(&(q->wait_condition_mutex));
        break;
      }
    }
    pthread_mutex_unlock(&(q->wait_condition_mutex));
    
    job = q_dequeue(q);
  }
  //debug("returning %x as job", job);
  return job;
}

/** Enqueues a Job on the Queue.
 */
void q_enqueue(Queue * q, void * job) {
  Node *new_node = malloc(sizeof(struct NODE));
  if (NULL == new_node) {
    error("Malloc error in enqueue");
    exit(1);
  }
  
  new_node->job = job;
  new_node->next = NULL;
  
  pthread_mutex_lock(&(q->lock));
  if (NULL == q->front) {
    q->front = new_node;
    q->rear = new_node;
  } else {
    q->rear->next = new_node;
    q->rear = new_node;
  }
  pthread_mutex_unlock(&(q->lock));
  
  pthread_mutex_lock(&(q->wait_condition_mutex));
  pthread_cond_signal(&(q->wait_condition));
  pthread_mutex_unlock(&(q->wait_condition_mutex));
}

/** Checks if the Queue is empty.
 *
 *  Returns 0 if the queue is not empty, 1 if it is.
 */
int q_empty(const Queue * queue) {
  return NULL == queue->front;
}

int q_size(const Queue * queue) {
  int size = 0;
  Node *n = queue->front;
  while (n) {
    size++;
    n = n->next;
  }
  return size;
}
