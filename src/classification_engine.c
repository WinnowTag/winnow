/* Copyright (c) 2007 The Kaphan Foundation
 *
 * Possession of a copy of this file grants no permission or license
 * to use, modify, or create derivate works.
 *
 * Please contact info@peerworks.org for further information.
 */

#include <pthread.h>
#include <stdlib.h>
#include <uuid/uuid.h>
#include <time.h>
#include <Judy.h>
#include <string.h>
#include "classification_engine.h"
#include "item_source.h"
#include "tagging.h"
#include "tag.h"
#include "job_queue.h"
#include "misc.h"

#ifdef HAVE_DATABASE_ACCESS
#include "db_item_source.h"
#endif

/* Size of a UUID string (36) + 1 for \0 */ 
#define JOB_ID_SIZE 37

struct CLASSIFICATION_ENGINE {
  /* Pointer to classification engine configuration */
  const Config *config;
  
  /* Pointer to item source for classification.
   * 
   * The ItemSource can be shared by all threads.
   * It is initialized on engine creation and
   * flushed periodically.  When it is flushed
   * all classification threads are stopped, then
   * the item source is flushed and reloaded.
   */
  ItemSource *item_source;
  
  /* Pointer to tagging store.
   * This is where taggings produced by the classifiers
   * will be stored.
   * This should only be accessed by the thread which does the
   * storing of taggings.
   */
  TaggingStore *tagging_store;
  
  /* The Queue on which classification jobs are stored.
   *
   * This is shared between each classification worker.
   * It handles it's own synchronization.
   */   
  Queue *classification_job_queue;
  
  /* Queue for tagging store jobs.
   * 
   * This is only used by the tagging store insertion worker.
   */ 
  Queue *tagging_store_queue;
  
  /* Store of all the current classification jobs in the system, keyed by job id.
   */
  Pvoid_t classification_jobs;
  
  /* Flag for whether the engine is running */
  int is_running;  
  
  /* Thread ids for classification workers */
  pthread_t **classification_worker_threads;
};

struct CLASSIFICATION_JOB {
  const char * job_id;
  int tag_id;
  float progress;
};

static ClassificationJob * create_classification_job(int tag_id);
static void free_classification_job(ClassificationJob *job);

/* Creates but doesn't start a classification engine.
 * 
 * This verifies that the classifiation engine has a valid item source,
 * if the item source is invalid it returns NULL.
 */
ClassificationEngine * create_classification_engine(const Config *config) {
  ClassificationEngine *engine = calloc(1, sizeof(ClassificationEngine));
  
  if (engine) {
    engine->config = config;
    engine->is_running = false;
    engine->classification_job_queue = NULL;
    
    /* Attempt to get a valid item store. */
#ifdef HAVE_DATABASE_ACCESS
    DBConfig item_store_config;
    if (cfg_item_db_config(config, &item_store_config)) {
      ItemSource *is = create_db_item_source(&item_store_config);
      if (NULL == is) goto item_source_error; 
      engine->item_source = create_caching_item_source(is);        
      if (NULL == engine->item_source) goto item_source_error;
    }
#endif
    
    if (NULL == engine->item_source) {      
      goto item_source_error;
    }
    
    engine->classification_job_queue = new_queue();
    engine->tagging_store_queue = new_queue();
  }
  
exit:
  return engine;
  
item_source_error:
  free_classification_engine(engine);
  engine = NULL;
  error("No item source defined or supported");
  goto exit;
}

/* Frees the classification engine.
 * 
 */
void free_classification_engine(ClassificationEngine *engine) {
  if (engine) {
    ce_kill(engine);
    
    if (engine->item_source) {
      free_item_source(engine->item_source);
      engine->item_source = NULL;
    }
    
    // Free all the jobs in the system
    PWord_t job_pointer;
    uint8_t job_id[JOB_ID_SIZE];
    strcpy((char *)job_id, "");
    JSLF(job_pointer, engine->classification_jobs, job_id);
    while (job_pointer) {
      free_classification_job((ClassificationJob*) (*job_pointer));
      JSLN(job_pointer, engine->classification_jobs, job_id);
    }
    Word_t bytes;
    JSLFA(bytes, engine->classification_jobs);
    
    free_queue(engine->classification_job_queue);
    free_queue(engine->tagging_store_queue);
    free(engine);
  }
}

/* Adds a classification job to the engine.
 * 
 */
ClassificationJob * ce_add_classification_job(ClassificationEngine * engine, int tag_id) {
  ClassificationJob *job = NULL;
  if (engine) {
    PWord_t job_pointer;
    job = create_classification_job(tag_id);
    JSLI(job_pointer, engine->classification_jobs, (uint8_t*) cjob_id(job));
    if (NULL == job_pointer) {
      free_classification_job(job);
      fatal("Error malloc'ing Judy array entry for classification job");
    } else {
      *job_pointer = (Word_t) job;
      q_enqueue(engine->classification_job_queue, job);    
    }    
  }
  return job;
}

ClassificationJob * ce_fetch_classification_job(const ClassificationEngine * engine, const unsigned char * job_id) {
  ClassificationJob *job = NULL;
  
  if (engine) {
    PWord_t job_pointer;
    JSLG(job_pointer, engine->classification_jobs, job_id)
    if (NULL != job_pointer) {
      job = (ClassificationJob*)(*job_pointer);
    }
  }
  
  return job;
}

/* Returns the number of jobs in the system.
 * 
 * This includes the number of jobs in the classification queue
 * and the number of jobs in the jobs in progress and the number
 * of completed jobs.
 */
int ce_num_jobs_in_system(const ClassificationEngine * engine) {
  int jobs = 0;
  jobs += ce_num_waiting_jobs(engine);
  return jobs;
}

/* Returns the number of jobs in the queue.
 */
int ce_num_waiting_jobs(const ClassificationEngine * engine) {
  int jobs_in_queue = 0;
  
  if (engine) {
    jobs_in_queue = q_size(engine->classification_job_queue);
  }
  
  return jobs_in_queue;
}

int ce_is_running(const ClassificationEngine * engine) {
  return (engine && engine->is_running);
}

int ce_start(ClassificationEngine * engine) {
  int success = true;
  if (engine) {
    engine->is_running = true;
  }
  
  return success;
}

int ce_stop(ClassificationEngine * engine) {
  int success = true;
  if (engine) {
    engine->is_running = false;
  }
  return success;
}

int ce_kill(ClassificationEngine * engine) {
  return false;
}

/********************************************************************************
 * Classification Job functions
 ********************************************************************************/
static const char * generate_job_id(ClassificationJob *job) {
  char *job_id = calloc(JOB_ID_SIZE, sizeof(char));
  if (NULL == job_id) {
    fatal("Error malloc'ing job id");
  } else {
    uuid_t uuid;
    uuid_generate(uuid);
    uuid_unparse(uuid, job_id);
  }
  
  return job_id;
}

ClassificationJob * create_classification_job(int tag_id) {
  ClassificationJob *job = malloc(sizeof(ClassificationJob));
  if (job) {
    job->tag_id = tag_id;
    job->progress = 0;
    job->job_id = generate_job_id(job);
  }
  
  return job;
}

void free_classification_job(ClassificationJob * job) {
  if (job) {
    if (job->job_id) {
      free((char *)job->job_id);
    }
    free(job);
  }
}

const char * cjob_id(const ClassificationJob * job) {
  return job->job_id;
}

int cjob_tag_id(const ClassificationJob * job) {
  return job->tag_id;
}

float cjob_progress(const ClassificationJob * job) {
  return job->progress;
}
