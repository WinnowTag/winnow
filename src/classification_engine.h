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

#ifndef CLASSIFICATION_ENGINE_H_
#define CLASSIFICATION_ENGINE_H_

#include "item_cache.h"
#include "tagger.h"
#include "hmac_credentials.h"

typedef struct CLASSIFICATION_ENGINE ClassificationEngine;

typedef struct CLASSIFICATION_ENGINE_OPTIONS {
  int worker_threads;
  double positive_threshold;
  char *performance_log;
  Credentials *credentials;
} ClassificationEngineOptions;

typedef enum CLASSIFICATION_JOB_STATE {
  CJOB_STATE_WAITING,
  CJOB_STATE_TRAINING,
  CJOB_STATE_CLASSIFYING,
  CJOB_STATE_INSERTING,
  CJOB_STATE_COMPLETE,
  CJOB_STATE_CANCELLED,
  CJOB_STATE_ERROR
} ClassificationJobState;

typedef enum CLASSIFICATION_JOB_ERROR {
  CJOB_ERROR_NO_ERROR,
  CJOB_ERROR_NO_SUCH_TAG,
  CJOB_ERROR_NO_TAGS_FOR_USER,
  CJOB_ERROR_BAD_JOB_TYPE,
  CJOB_ERROR_MISSING_ITEM_TIMEOUT,
  CJOB_ERROR_CHECKED_OUT,
  CJOB_ERROR_UNKNOWN_ERROR
} ClassificationJobError;

typedef enum ITEM_SCOPE {
  ITEM_SCOPE_ALL,
  ITEM_SCOPE_NEW
} ItemScope;

typedef struct CLASSIFICATION_JOB {
  const char * id;
  const char * tag_url;
  float progress;
  float progress_increment;
  ClassificationJobState state;
  ClassificationJobError error;
  char *errmsg;
  int auto_cleanup;
  ItemScope item_scope;
  int items_classified;
  /* Timestamps for process timing */
  struct timeval created_at;
  struct timeval started_at;
  struct timeval trained_at;
  struct timeval classified_at;
  struct timeval completed_at;
  time_t first_time_tried;
} ClassificationJob;

extern ClassificationEngine * create_classification_engine(ItemCache *item_cache, TaggerCache *tagger_cache, ClassificationEngineOptions *options);
extern void                   free_classification_engine(ClassificationEngine * engine);
extern int                    ce_is_running(const ClassificationEngine *engine);
extern int                    ce_start(ClassificationEngine *engine);
extern int                    ce_stop(ClassificationEngine *engine);
extern int                    ce_suspend(ClassificationEngine *engine);
extern int                    ce_resume(ClassificationEngine *engine);
extern int                    ce_kill(ClassificationEngine *engine);
extern void                   ce_run(ClassificationEngine *engine);

/* Job tracking and management */
extern int                    ce_num_jobs_in_system(const ClassificationEngine *engine);
extern int                    ce_num_waiting_jobs(const ClassificationEngine *engine);
extern ClassificationJob    * ce_add_classification_job(ClassificationEngine *engine, const char * tag_url);
extern ClassificationJob    * ce_fetch_classification_job(const ClassificationEngine *engine, const char * job_id);
extern int                    ce_remove_classification_job(ClassificationEngine *engine, const ClassificationJob *job, int force);
extern float                  cjob_duration(const ClassificationJob *job);
extern const char *           cjob_error_msg(const ClassificationJob *job, char * buffer, size_t size);
extern const char *           cjob_state_msg(const ClassificationJob * job);
extern void                   cjob_cancel(ClassificationJob *job);

extern void                   free_classification_job(ClassificationJob * job);
#endif /*CLASSIFICATION_ENGINE_H_*/
