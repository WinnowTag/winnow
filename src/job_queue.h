// Copyright (c) 2007 The Kaphan Foundation
//
// Possession of a copy of this file grants no permission or license
// to use, modify, or create derivate works.
//
// Please contact info@peerworks.org for further information.

#ifndef _QUEUE_H_
#define _QUEUE_H_

typedef struct QUEUE Queue;

extern Queue * new_queue          ();
extern void    free_queue         (Queue * queue);
extern void  * q_dequeue          (Queue * queue);
extern void  * q_dequeue_or_wait  (Queue * queue);
extern void    q_enqueue          (Queue * queue, void *job);
extern int     q_empty            (const Queue * queue);
extern int     q_size             (const Queue * queue);

#endif /* _QUEUE_H_ */
