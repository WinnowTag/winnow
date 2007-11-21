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
#include "classifier.h"
#include "item_source.h"
#include "tagging.h"
#include "tag.h"
#include "job_queue.h"
#include "misc.h"
#include "random_background.h"
#include "logging.h"

#ifdef HAVE_DATABASE_ACCESS
#include <mysql.h>
#include "db_item_source.h"
#define DB_SYSTEM_INIT  mysql_library_init(-1, NULL, NULL)
#define DB_THREAD_INIT  mysql_thread_init()
#define DB_THREAD_END   mysql_thread_end()
#define DB_SYSTEM_END   mysql_library_end()
#else
#define DB_SYSTEM_INIT
#define DB_THREAD_INIT
#define DB_THREAD_END
#define DB_SYSTEM_END
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
  
  /* Shared RandomBackground
   */
  Pool *random_background;
  
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
  
  int is_inserting;
  
  /* Thread ids for classification workers */
  pthread_t *classification_worker_threads;
  
  /* Thread id for tagging store worker */
  pthread_t insertion_worker_thread;
};

struct CLASSIFICATION_JOB {
  const char * job_id;  
  int tag_id;
  int user_id;
  float progress;
  ClassificationJobType type;
  ClassificationJobState state;
  ClassificationJobError error;
};

typedef struct TAGGING_INSERTION_JOB {
  const char * job_id;
  Tagging *tagging;
} TaggingInsertionJob;

static ClassificationJob * create_tag_classification_job(int tag_id);
static ClassificationJob * create_user_classification_job(int tag_id);
static void free_classification_job(ClassificationJob *job);
static void *classification_worker_func(void *engine_vp);
static void *insertion_worker_func(void *engine_vp);
static TaggingInsertionJob * create_tagging_insertion_job(Tagging *tagging);
static void free_tagging_insertion_job(TaggingInsertionJob *job);
static void cjob_process(ClassificationJob *job, const ItemSource *is, TagDB *tagdb, const Pool *background, Queue *insertion_queue);

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
    engine->is_inserting = false;
    engine->classification_job_queue = NULL;
    
    /* Attempt to get a valid item store. */
#ifdef HAVE_DATABASE_ACCESS
    DBConfig item_store_config;
    if (cfg_item_db_config(config, &item_store_config)) {
      ItemSource *is = create_db_item_source(&item_store_config);
      if (NULL == is) goto item_source_error; 
      engine->item_source = create_caching_item_source(is);        
      if (NULL == engine->item_source) goto item_source_error;
      engine->random_background = create_random_background_from_db(is, &item_store_config);
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
    
    free_pool(engine->random_background);
    free_queue(engine->classification_job_queue);
    free_queue(engine->tagging_store_queue);
    free(engine);
  }
}

/* Adds a classification job to the engine.
 * 
 */
ClassificationJob * ce_add_classification_job_for_tag(ClassificationEngine * engine, int tag_id) {
  ClassificationJob *job = NULL;
  if (engine) {
    PWord_t job_pointer;
    job = create_tag_classification_job(tag_id);
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

/* Adds a classification job to the engine.
 * 
 */
ClassificationJob * ce_add_classification_job_for_user(ClassificationEngine * engine, int user_id) {
  ClassificationJob *job = NULL;
  if (engine) {
    PWord_t job_pointer;
    job = create_user_classification_job(user_id);
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

ClassificationJob * ce_fetch_classification_job(const ClassificationEngine * engine, const char * job_id) {
  ClassificationJob *job = NULL;
  
  if (engine) {
    PWord_t job_pointer;
    JSLG(job_pointer, engine->classification_jobs, (uint8_t*) job_id)
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

/* Starts the classification engine.
 * 
 * If this returns true all the classification threads and the insertion thread have
 * been started. The engine will then process jobs as they are added to the engine.
 */
int ce_start(ClassificationEngine * engine) {
  DB_SYSTEM_INIT;
  int success = true;
  if (engine) {
    engine->is_running = true;
    engine->is_inserting = true;

    int i;
    EngineConfig config;
    cfg_engine_config(engine->config, &config);
    
    engine->classification_worker_threads = calloc(config.num_workers, sizeof(pthread_t));
    for (i = 0; i < config.num_workers; i++) {
      if (pthread_create(&(engine->classification_worker_threads[i]), NULL, classification_worker_func, engine)) {
        fatal("Error creating thread %i for classification", i + 1);
        exit(1);
      }
    }
    
    if (pthread_create(&(engine->insertion_worker_thread), NULL, insertion_worker_func, engine)) {
      fatal("Error creating thread for insertion");
    }
    
  }
  
  return success;
}

/* Gracefully stops the classification engine by allowing all current jobs to complete.
 */
int ce_stop(ClassificationEngine * engine) {
  debug("stopping engine");
  int success = true;
  if (engine && engine->is_running) {
    engine->is_running = false;
    
    EngineConfig config;
    cfg_engine_config(engine->config, &config);
    
    int i;
    for (i = 0; i < config.num_workers; i++) {
      info("joining thread %i", engine->classification_worker_threads[i]);
      pthread_join(engine->classification_worker_threads[i], NULL);
    }
    info("Returned from cw join");
    engine->is_inserting = false;
    info("is_inserting set to false");
     
    pthread_join(engine->insertion_worker_thread, NULL);
    info("finishing with %i insertion jobs on queue", q_size(engine->tagging_store_queue));
  }
  
  return success;
}

int ce_kill(ClassificationEngine * engine) {
  return false;
}

/********************************************************************************
 * Worker thread functions.
 ********************************************************************************/

/* This is the function for classificaiton work threads.
 * 
 * Each worker shares the ItemSource, Random Background and Queues of
 * the engine but have their own TagDB instance.
 * 
 * All shared resources should already be created by the engine.
 * 
 */
void *classification_worker_func(void *engine_vp) {
  DB_THREAD_INIT;
  /* Grab references to shared resources */
  ClassificationEngine *ce = (ClassificationEngine*) engine_vp;
  Queue *job_queue         = ce->classification_job_queue;
  Queue *tagging_queue     = ce->tagging_store_queue;
  
  
  /* Create thread specific resources */
  DBConfig tag_db_config;
  cfg_tag_db_config(ce->config, &tag_db_config);
  TagDB *tag_db = create_tag_db(&tag_db_config);
  
  while (!q_empty(job_queue) || ce->is_running) {
    ClassificationJob *job = (ClassificationJob*) q_dequeue_or_wait(job_queue);
    
    if (job) {
      info("Got job off queue: %s", cjob_id(job));

      /* Get the reference to the item source here so we get it fresh for each job. This means that if
       * the item source is flushed we get a new copy on the next job.
       */
      cjob_process(job, ce->item_source, tag_db, ce->random_background, ce->tagging_store_queue);
    }    
  }

  info("classification_worker %i ending", pthread_self());
  free_tag_db(tag_db);
  
  DB_THREAD_END;
}

void *insertion_worker_func(void *engine_vp) {
  info("Starting insertion worker thread id = %i", pthread_self());
  DB_THREAD_INIT;
  ClassificationEngine *ce = (ClassificationEngine*) engine_vp;
  EngineConfig econfig;
  cfg_engine_config(ce->config, &econfig);
  
  DBConfig tagging_store_config;
  cfg_tagging_store_db_config(ce->config, &tagging_store_config);
  TaggingStore *store = create_db_tagging_store(&tagging_store_config, econfig.insertion_threshold);  
    
  while(!q_empty(ce->tagging_store_queue) || ce->is_inserting) {
    TaggingInsertionJob *job = q_dequeue_or_wait(ce->tagging_store_queue);
    
    if (job) {
      info("Got insertion job off queue %s", job->job_id);
      tagging_store_store(store, job->tagging);
      free_tagging_insertion_job(job);
    }
  }
  
  info("insertion worker function ending");
  free_tagging_store(store);
  DB_THREAD_END;
}

/********************************************************************************
 * Classification Job functions
 ********************************************************************************/
#define SET_JOB_ERROR(job, err, msg, ...) \
  job->state = CJOB_STATE_ERROR; \
  job->error = err;              \
  error(msg, __VA_ARGS__);

static const char * generate_job_id() {
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

ClassificationJob * create_tag_classification_job(int tag_id) {
  ClassificationJob *job = malloc(sizeof(ClassificationJob));
  if (job) {
    job->tag_id = tag_id;
    job->user_id = 0;
    job->progress = 0;
    job->job_id = generate_job_id();
    job->state = CJOB_STATE_WAITING;
    job->error = CJOB_ERROR_NO_ERROR;
    job->type = CJOB_TYPE_TAG_JOB;
  }
  
  return job;
}

ClassificationJob * create_user_classification_job(int user_id) {
  ClassificationJob *job = malloc(sizeof(ClassificationJob));
  if (job) {
    job->tag_id = 0;
    job->user_id = user_id;
    job->progress = 0;
    job->job_id = generate_job_id();
    job->state = CJOB_STATE_WAITING;
    job->error = CJOB_ERROR_NO_ERROR;
    job->type = CJOB_TYPE_USER_JOB;
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

int cjob_type(const ClassificationJob * job) {
  return job->type;
}

int cjob_user_id(const ClassificationJob * job) {
  return job->user_id;
}

float cjob_progress(const ClassificationJob * job) {
  return job->progress;
}

ClassificationJobState cjob_state(const ClassificationJob * job) {
  return job->state;
}

static const char * state_msgs[] = {
    "Waiting",
    "Training",
    "Calculating",
    "Classifying",
    "Complete",
    "Cancelled",
    "Error"
};

const char * cjob_state_msg(const ClassificationJob * job) {
  return state_msgs[job->state];
}

ClassificationJobError cjob_error(const ClassificationJob *job) {
  return job->error;
}

void cjob_cancel(ClassificationJob *job) {
  job->state = CJOB_STATE_CANCELLED;
}

/** Do the actual classification.
 * 
 *  TODO Check malloc'ing eventually
 */
static int cjob_classify(ClassificationJob *job, const TagList *taglist, const ItemSource *item_source, 
                          const Pool *random_background, Queue *tagging_queue) {
  
  if (taglist->size < 1) {
    return 1;
  }
  
  int i;
  int failed = false;
  ItemList *items = is_fetch_all_items(item_source);
  int number_of_items = item_list_size(items);
  const float progress_increment = 80.0 / number_of_items;

  TrainedClassifier **tc = calloc(taglist->size, sizeof(TrainedClassifier*));
  if (!tc) goto malloc_error;
  for (i = 0; i < taglist->size; i++) {
    tc[i] = train(taglist->tags[i], item_source);      
  }
  job->progress = 10.0;
      
  job->state = CJOB_STATE_CALCULATING;
  Classifier **classifier = calloc(taglist->size, sizeof(Classifier*));
  if (!classifier) goto malloc_error;
  for (i = 0; i < taglist->size; i++) {
    classifier[i] = precompute(tc[i], random_background);
  }
  job->progress = 20.0;
            
  job->state = CJOB_STATE_CLASSIFYING;  
  for (i = 1; i <= number_of_items; i++) {
    Item *item = item_list_item_at(items, i);
    
    if (NULL == item) {
      error("item at %i was NULL", i);
    } else {
      int j;
      
      for (j = 0; j < taglist->size; j++) {
        Tagging *tagging = classify(classifier[j], item);
        
        if (NULL != tagging) {
          TaggingInsertionJob *tagging_job = create_tagging_insertion_job(tagging);
          q_enqueue(tagging_queue, tagging_job);
        } else {
          error("Got NULL tagging");
        }
      }
    }

    job->progress += progress_increment;
  }
  
exit:

  for (i = 0; i < taglist->size; i++) {
    if (tc && tc[i]) {
      tc_free(tc[i]);      
    }
    
    if (classifier && classifier[i]) {
      free_classifier(classifier[i]);      
    }
  }  
  
  if (classifier) {
    free(classifier);
  }
  
  if (tc) {
    free(tc);
  }  
  
  return failed;
  
malloc_error:
  job->error = CJOB_STATE_ERROR;
  job->error = CJOB_ERROR_UNKNOWN_ERROR;
  fatal("Malloc error in classification processing");
  failed = true;
  goto exit;
}

static void cjob_process(ClassificationJob *job, const ItemSource *item_source, TagDB *tag_db, 
                  const Pool *random_background, Queue *tagging_queue) {
  debug("cjob_process: %s", job->job_id);
  /* If the job is cancelled bail out before doing anything */
  if (job->state == CJOB_STATE_CANCELLED) return;
  
  job->state = CJOB_STATE_TRAINING;  
  TagList *taglist = NULL;
  
  switch (job->type) {
    case CJOB_TYPE_TAG_JOB: {
      Tag *tag = tag_db_load_tag_by_id(tag_db, job->tag_id);
      if (tag) {
        taglist = create_tag_list();
        taglist_add_tag(taglist, tag);
      } else {
        SET_JOB_ERROR(job, CJOB_ERROR_NO_SUCH_TAG, "Tag %i does not exist", job->tag_id);
      }
    }
    break;
    case CJOB_TYPE_USER_JOB:
      taglist = tag_db_load_tags_to_classify_for_user(tag_db, job->user_id);          
      if (!taglist || taglist->size == 0) {
        SET_JOB_ERROR(job, CJOB_ERROR_NO_TAGS_FOR_USER, "No tags for user %i", job->user_id);
      }
    break;
    default:
      SET_JOB_ERROR(job, CJOB_ERROR_BAD_JOB_TYPE, "Unknown CJOB type: %i", job->type);
    break;
  }
    
  if (taglist) {
    job->progress = 5.0;
    if (!cjob_classify(job, taglist, item_source, random_background, tagging_queue)) {
      int i;
      for (i = 0; i < taglist->size; i++) {
        tag_db_update_last_classified_at(tag_db, taglist->tags[i]);        
      }
      
      job->progress = 100.0;
      job->state = CJOB_STATE_COMPLETE;
    }
    free_taglist(taglist);      
  }
}

/********************************************************************************
 * Tagging insertion Job functions
 ********************************************************************************/
TaggingInsertionJob * create_tagging_insertion_job(Tagging *tagging) {
  TaggingInsertionJob *job = malloc(sizeof(TaggingInsertionJob));
  if (job) {
    job->job_id = generate_job_id();
    job->tagging = tagging;
  }
  
  return job;
}

void free_tagging_insertion_job(TaggingInsertionJob *job) {
  if (job) {
    free(job->tagging);
    free(job);
  }
}
