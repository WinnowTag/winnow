/* Copyright (c) 2007 The Kaphan Foundation
 *
 * Possession of a copy of this file grants no permission or license
 * to use, modify, or create derivate works.
 *
 * Please contact info@peerworks.org for further information.
 */

#ifndef CLASSIFICATION_ENGINE_H_
#define CLASSIFICATION_ENGINE_H_

#include "cls_config.h"

typedef struct CLASSIFICATION_ENGINE ClassificationEngine;
typedef struct CLASSIFICATION_JOB ClassificationJob;

extern ClassificationEngine * create_classification_engine(const Config * config);
extern void                   free_classification_engine(ClassificationEngine * engine);
extern int                    ce_is_running(const ClassificationEngine *engine);
extern int                    ce_start(ClassificationEngine *engine);
extern int                    ce_stop(ClassificationEngine *engine);
extern int                    ce_kill(ClassificationEngine *engine);

/* Job tracking and management */
extern int                    ce_num_jobs_in_system(const ClassificationEngine *engine);
extern int                    ce_num_waiting_jobs(const ClassificationEngine *engine);
extern ClassificationJob    * ce_add_classification_job(ClassificationEngine *engine, int tag_id);
extern ClassificationJob    * ce_fetch_classification_job(const ClassificationEngine *engine, const unsigned char * job_id);

/** Functions for Classification Jobs */
extern const char *  cjob_id(const ClassificationJob * job);
extern int                    cjob_tag_id(const ClassificationJob * job);
extern float                  cjob_progress(const ClassificationJob * job);
#endif /*CLASSIFICATION_ENGINE_H_*/
