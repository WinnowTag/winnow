/* Copyright (c) 2007 The Kaphan Foundation
 *
 * Possession of a copy of this file grants no permission or license
 * to use, modify, or create derivate works.
 *
 * Please contact info@peerworks.org for further information.
 */

#include <pthread.h>
#include <stdlib.h>
#include <time.h>
#include <sys/time.h>
#include <Judy.h>
#include <string.h>
#include "classification_engine.h"
#include "uuid.h"
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

#define INIT_MUTEX(mutex) \
  mutex = calloc(1, sizeof(pthread_mutex_t)); \
  if (!mutex) MALLOC_ERR();              \
  if (pthread_mutex_init(mutex, NULL)) MALLOC_ERR();

#define INIT_COND(cond) \
  cond = calloc(1, sizeof(pthread_cond_t)); \
  if (!cond) MALLOC_ERR();             \
  if (pthread_cond_init(cond, NULL)) MALLOC_ERR();

/* Size of a UUID string (36) + 1 for \0 */ 
#define JOB_ID_SIZE UUID_SIZE + 1
#define NOW(tv) gettimeofday(&(tv), NULL);

struct CLASSIFICATION_ENGINE {
  /* Pointer to classification engine configuration */
  const Config *config;
  
  EngineConfig engine_config;
    
  /* Performance log file */
  FILE *performance_log;
  
  pthread_mutex_t *perf_log_mutex;
  
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
  pthread_mutex_t *classification_jobs_mutex;
  
  /* Flag for whether the engine is running */
  int is_running;
  
  /* Flag for whether insertion is running */
  int is_inserting;
  
  /* Flag for whether classification is suspended */
  int is_classification_suspended;
   
  /* pthread mutex for suspension */
  pthread_mutex_t *classification_suspension_mutex;
  
  /* pthread condition for suspension */
  pthread_cond_t *classification_suspension_cond;
  
  pthread_mutex_t *suspension_notification_mutex;
  pthread_cond_t *suspension_notification_cond;
  int num_threads_suspended;
    
  /* Thread ids for classification workers */
  pthread_t *classification_worker_threads;
  
  /* Thread id for tagging store worker */
  pthread_t insertion_worker_thread;
  
  /* ItemSource flushing thread */
  pthread_t flusher;
};

typedef enum ITEM_SCOPE {
  ITEM_SCOPE_ALL,
  ITEM_SCOPE_NEW
} ItemScope;

struct CLASSIFICATION_JOB {
  const char * job_id;  
  int tag_id;
  int user_id;
  float progress;
  ClassificationJobType type;
  ClassificationJobState state;
  ClassificationJobError error;
  int auto_cleanup;
  ItemScope item_scope;
  int tags_classified;
  int items_classified;
  /* Timestamps for process timing */
  struct timeval created_at;
  struct timeval started_at;
  struct timeval trained_at;
  struct timeval computed_at;
  struct timeval completed_at;
};

typedef struct TAGGING_INSERTION_JOB {
  const char * job_id;
  Tagging *tagging;
  struct timeval created_at;
  struct timeval started_at;
  struct timeval completed_at;
} TaggingInsertionJob;

static void ce_record_classification_job_timings(ClassificationEngine *ce, const ClassificationJob *job);
static void ce_record_insertion_job_timings(ClassificationEngine *ce, const TaggingInsertionJob *job);
static ClassificationJob * create_tag_classification_job(int tag_id);
static ClassificationJob * create_user_classification_job(int tag_id);
static void *classification_worker_func(void *engine_vp);
static void *insertion_worker_func(void *engine_vp);
static void *flusher_func(void *engine_vp);
static TaggingInsertionJob * create_tagging_insertion_job(Tagging *tagging);
static void free_tagging_insertion_job(TaggingInsertionJob *job);
static void cjob_process(ClassificationJob *job, const ItemSource *is, TagDB *tagdb, const Pool *background, Queue *insertion_queue);
static float tdiff(struct timeval from, struct timeval to);

/* Creates but doesn't start a classification engine.
 * 
 * This verifies that the classifiation engine has a valid item source,
 * if the item source is invalid it returns NULL.
 */
ClassificationEngine * create_classification_engine(const Config *config) {
  ClassificationEngine *engine = calloc(1, sizeof(ClassificationEngine));
  
  if (engine) {
    engine->config = config;
    cfg_engine_config(config, &(engine->engine_config));
    engine->is_running = false;
    engine->is_inserting = false;
    engine->is_classification_suspended = false;
    engine->classification_job_queue = NULL;
    engine->num_threads_suspended = 0;
            
    if (engine->engine_config.performance_log) {
      const char *performance_log = engine->engine_config.performance_log;
      engine->performance_log = fopen(performance_log, "a");
      if (NULL == engine->performance_log) {
        fprintf(stderr, "Error opening %s: %s", performance_log, strerror(errno));
        error("Error opening %s: %s", performance_log, strerror(errno));
      } else {
        char time_s[27];
        time_t now = time(0);
        ctime_r(&now, time_s);
        fprintf(engine->performance_log, "# Classifier started at %s\n", time_s);
      }
    }
    
    INIT_MUTEX(engine->classification_suspension_mutex);
    INIT_MUTEX(engine->suspension_notification_mutex);
    INIT_MUTEX(engine->classification_jobs_mutex);
    INIT_MUTEX(engine->perf_log_mutex);
    INIT_COND(engine->classification_suspension_cond);
    INIT_COND(engine->suspension_notification_cond);    
    
    /* Attempt to get a valid item store. */
#ifdef HAVE_DATABASE_ACCESS
    DBConfig item_store_config;
    if (cfg_item_db_config(config, &item_store_config)) {
      ItemSource *is = create_db_item_source(&item_store_config);
      if (NULL == is) goto item_source_error; 
      engine->item_source = create_caching_item_source(is);        
      if (NULL == engine->item_source) goto item_source_error;
      engine->random_background = create_random_background_from_db(engine->item_source, &item_store_config);
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
malloc_error:
  fatal("Malloc error in classification engine initialization");
  engine = NULL;
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
    
    if (engine->performance_log) {
      fclose(engine->performance_log);
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
    
    pthread_cond_destroy(engine->classification_suspension_cond);
    pthread_cond_destroy(engine->suspension_notification_cond);
    pthread_mutex_destroy(engine->classification_suspension_mutex);
    pthread_mutex_destroy(engine->suspension_notification_mutex);
    pthread_mutex_destroy(engine->perf_log_mutex);
    
    free_pool(engine->random_background);
    free_queue(engine->classification_job_queue);
    free_queue(engine->tagging_store_queue);
    free(engine);
  }
}

/************************************************************************************
 * Functions for adding, fetching and removing classification jobs.
 */

static int add_classification_job(ClassificationEngine *engine, ClassificationJob *job) {
  int failure = true;
  PWord_t job_pointer;
  
  pthread_mutex_lock(engine->classification_jobs_mutex);
  JSLI(job_pointer, engine->classification_jobs, (uint8_t*) cjob_id(job));
  if (NULL != job_pointer) {
    *job_pointer = (Word_t) job;
    failure = false;
  }
  pthread_mutex_unlock(engine->classification_jobs_mutex);
  
  if (failure) {
    fatal("Error malloc'ing Judy array entry for classification job");
  } 
  
  q_enqueue(engine->classification_job_queue, job);
  
  return failure;
}

/* Adds a classification job to the engine.
 * 
 */
ClassificationJob * ce_add_classification_job_for_tag(ClassificationEngine * engine, int tag_id) {
  ClassificationJob *job = NULL;
  if (engine) {
    job = create_tag_classification_job(tag_id);
    if (add_classification_job(engine, job)) {
      free_classification_job(job);
      job = NULL;
    }
  }
  return job;
}

ClassificationJob * ce_add_classify_new_items_job_for_tag(ClassificationEngine * engine, int tag_id) {
  ClassificationJob *job = NULL;
  if (engine) {
    job = create_tag_classification_job(tag_id);
    job->item_scope = ITEM_SCOPE_NEW;
    job->auto_cleanup = true;
    
    if (add_classification_job(engine, job)) {
      free_classification_job(job);
      job = NULL;
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
    job = create_user_classification_job(user_id);
    if (add_classification_job(engine, job)) {
      free_classification_job(job);
      job = NULL;
    }
  }
  return job;
}

ClassificationJob * ce_fetch_classification_job(const ClassificationEngine * engine, const char * job_id) {
  ClassificationJob *job = NULL;
  
  if (engine) {
    pthread_mutex_lock(engine->classification_jobs_mutex);
    PWord_t job_pointer;
    JSLG(job_pointer, engine->classification_jobs, (uint8_t*) job_id)
    if (NULL != job_pointer) {
      job = (ClassificationJob*)(*job_pointer);
    }
    pthread_mutex_unlock(engine->classification_jobs_mutex);
  }
  
  return job;
}

int ce_remove_classification_job(ClassificationEngine * engine, const ClassificationJob *job) {
  int removed = false;
  if (engine && job && cjob_state(job) == CJOB_STATE_COMPLETE) {
    pthread_mutex_lock(engine->classification_jobs_mutex);
    JSLD(removed, engine->classification_jobs, (uint8_t*) job->job_id);
    pthread_mutex_unlock(engine->classification_jobs_mutex);
  }
  return removed;
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
    
    engine->classification_worker_threads = calloc(engine->engine_config.num_workers, sizeof(pthread_t));
    for (i = 0; i < engine->engine_config.num_workers; i++) {
      if (pthread_create(&(engine->classification_worker_threads[i]), NULL, classification_worker_func, engine)) {
        fatal("Error creating thread %i for classification", i + 1);
        exit(1);
      }
    }
    
    if (pthread_create(&(engine->insertion_worker_thread), NULL, insertion_worker_func, engine)) {
      fatal("Error creating thread for insertion");
    }
    
    if (pthread_create(&(engine->flusher), NULL, flusher_func, engine)) {
      fatal("Error creating thread for item source flushing");
    }    
  }
  
  return success;
}

/* Gracefully stops the classification engine by allowing all current jobs to complete.
 */
int ce_stop(ClassificationEngine * engine) {
  int success = true;
  if (engine && engine->is_running) {
    info("stopping engine");
    engine->is_running = false;
    pthread_mutex_lock(engine->classification_suspension_mutex);
    pthread_cond_broadcast(engine->classification_suspension_cond);
    pthread_mutex_unlock(engine->classification_suspension_mutex);
        
    int i;
    for (i = 0; i < engine->engine_config.num_workers; i++) {
      debug("joining thread %i", engine->classification_worker_threads[i]);
      pthread_join(engine->classification_worker_threads[i], NULL);
    }
    debug("Returned from cw join");
    engine->is_inserting = false;
    debug("is_inserting set to false");
     
    pthread_join(engine->insertion_worker_thread, NULL);
    info("finishing with %i insertion jobs on queue", q_size(engine->tagging_store_queue));
  }
  
  return success;
}

/* This will block until the engine has been shutdown.
 */
void ce_run(ClassificationEngine *engine) {
  if (engine) {
    if (!engine->is_running) {
      ce_start(engine);
    }
    
    info("Classification Engine started, now blocking.");
    int i;
    for (i = 0; i < engine->engine_config.num_workers; i++) {
      pthread_join(engine->classification_worker_threads[i], NULL);
    }
    
    pthread_join(engine->insertion_worker_thread, NULL);
  }
}

/** Suspends all classification.
 * 
 *  This function blocks until all worker threads have been suspeneded.
 */
int ce_suspend(ClassificationEngine * engine) {
  if (engine) {
    pthread_mutex_lock(engine->classification_suspension_mutex);
    engine->is_classification_suspended = true;
    pthread_mutex_unlock(engine->classification_suspension_mutex);
    
    /* Wait until all threads have registered as suspended before returning
     * 
     * We only do this if the system is running
     *
     * TODO Add a timeout?
     */
    if (engine->is_running) {
      pthread_mutex_lock(engine->suspension_notification_mutex);
      if (engine->num_threads_suspended < engine->engine_config.num_workers) {
        pthread_cond_wait(engine->suspension_notification_cond, engine->suspension_notification_mutex);
      }
      pthread_mutex_unlock(engine->suspension_notification_mutex);
    }
    
    info("Engine suspended");
  }
  return true;
}

int ce_resume(ClassificationEngine * engine) {
  if (engine) {
    pthread_mutex_lock(engine->classification_suspension_mutex);
    engine->is_classification_suspended = false;
    pthread_cond_broadcast(engine->classification_suspension_cond);
    pthread_mutex_unlock(engine->classification_suspension_mutex);
  }
  return true;
}

/** This should eventually shut things down fast, but for now it just calls
 *  ce_stop.
 */
int ce_kill(ClassificationEngine * engine) {
  return ce_stop(engine);;
}

static void ce_record_classification_job_timings(ClassificationEngine *ce, const ClassificationJob *job) {
  if (ce && job && job->state == CJOB_STATE_COMPLETE) {
    float wait_time = tdiff(job->created_at, job->started_at);
    float train_time = tdiff(job->started_at, job->trained_at);
    float calc_time = tdiff(job->trained_at, job->computed_at);
    float clas_time = tdiff(job->computed_at, job->completed_at);

    if (ce->performance_log) {
      pthread_mutex_lock(ce->perf_log_mutex);    
      fprintf(ce->performance_log, "%i,%i,%.5f,%.5f,%.5f,%.5f\n", 
                job->tags_classified, job->items_classified,
                wait_time, train_time, calc_time, clas_time);
      fflush(ce->performance_log);
      pthread_mutex_lock(ce->perf_log_mutex);
    }    
  }
}

/********************************************************************************
 * Worker thread functions.
 ********************************************************************************/

#define NOTIFY_SUSPENSION(ce) \
  debug("Classification is suspended in %i", pthread_self());         \
  pthread_mutex_lock(ce->suspension_notification_mutex);              \
  ce->num_threads_suspended++;                                        \
  if (ce->num_threads_suspended >= ce->engine_config.num_workers) {   \
    pthread_cond_signal(ce->suspension_notification_cond);            \
  }                                                                   \
  pthread_mutex_unlock(ce->suspension_notification_mutex);

#define NOTIFY_RESUMPTION(ce) \
  pthread_mutex_lock(ce->suspension_notification_mutex);    \
  ce->num_threads_suspended--;                              \
  pthread_mutex_unlock(ce->suspension_notification_mutex);  \
  debug("Classification resumed in %i", pthread_self());

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
  
  /* Create thread specific resources */
  DBConfig tag_db_config;
  cfg_tag_db_config(ce->config, &tag_db_config);
  TagDB *tag_db = create_tag_db(&tag_db_config);
  
  while (!q_empty(job_queue) || ce->is_running) {
    /* Check if the engine is suspended.
     * 
     * If it is we wait on the classification_suspension_cond
     * until the thread gets woken up again.
     * 
     * We can also be woken up to stop, in which case the classification
     * will still be suspended and we quit straight away.
     */
    pthread_mutex_lock(ce->classification_suspension_mutex);
      if (ce->is_classification_suspended) {
        NOTIFY_SUSPENSION(ce);
        pthread_cond_wait(ce->classification_suspension_cond, ce->classification_suspension_mutex);
        NOTIFY_RESUMPTION(ce);
        
        // Been woken up to exit - not to continue
        if (ce->is_classification_suspended) {
          pthread_mutex_unlock(ce->classification_suspension_mutex);
          info("classification worker resumed to exit");
          break;
        }
      }
    pthread_mutex_unlock(ce->classification_suspension_mutex);
     
    trace("About to wait on queue, thread %i", pthread_self());
    ClassificationJob *job = (ClassificationJob*) q_dequeue_or_wait(job_queue);
    trace("Returned from queue, thread %i", pthread_self());
    
    if (job) {
      debug("%i got job off queue: %s", pthread_self(), cjob_id(job));
      
      /* Only proceed if the job is not cancelled */
      if (CJOB_STATE_CANCELLED == cjob_state(job)) {
        /* If it is cancelled we remove and free the job ourselves */
        job->state = CJOB_STATE_COMPLETE;
        ce_remove_classification_job(ce, job);
        free_classification_job(job);
      } else {
        /* Get the reference to the item source here so we get it fresh for each job. This means that if
         * the item source is flushed we get a new copy on the next job.
         */
        cjob_process(job, ce->item_source, tag_db, ce->random_background, ce->tagging_store_queue);
        ce_record_classification_job_timings(ce, job);
        
        if (job->auto_cleanup) {
          ce_remove_classification_job(ce, job);
          free_classification_job(job);
        }
      }      
    }    
  }

  info("classification_worker %i ending", pthread_self());
  free_tag_db(tag_db);
  
  DB_THREAD_END;
  return EXIT_SUCCESS;
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
      NOW(job->started_at);
      tagging_store_store(store, job->tagging);
      NOW(job->completed_at);
      free_tagging_insertion_job(job);
    }
  }
  
  info("insertion worker function ending");
  free_tagging_store(store);
  DB_THREAD_END;
  return EXIT_SUCCESS;
}

static void create_classify_new_item_jobs_for_all_tags(ClassificationEngine *ce) {
  if (ce) {
    DBConfig dbConfig;
    cfg_tag_db_config(ce->config, &dbConfig);
    TagDB *tag_db = create_tag_db(&dbConfig);
    if (tag_db) {
      int i;
      int size;
      int *ids = tag_db_get_all_tag_ids(tag_db, &size);
      
      for (i = 0; i < size; i++) {
        ce_add_classify_new_items_job_for_tag(ce, ids[i]);
      }
      
      free(ids);
      info("Created %i classify new items jobs", size);
    } else {
      error("Could not create tag_db in create_classify_new_item_jobs_for_all_tags");
    }
    
    free_tag_db(tag_db);
  }
}

void *flusher_func(void *engine_vp) {
  DB_THREAD_INIT;
  ClassificationEngine *engine = (ClassificationEngine*) engine_vp;
  
  if (engine) {
    pthread_mutex_t flush_wait_mutex;
    pthread_cond_t flush_wait_cond;
    
    if (pthread_mutex_init(&flush_wait_mutex, NULL) || pthread_cond_init(&flush_wait_cond, NULL)) {
      fatal("Could not create mutex or cond for flusher thread");
    } else {
      while (engine->is_running) {        
        // This will run at 0200 everyday
        char timebuffer[26];
        time_t now = time(0);
        struct tm now_tm;
        localtime_r(&now, &now_tm);
        
        // If we are past 0200 move to the next day
        if (now_tm.tm_hour >= 3) {
          now_tm.tm_mday++;
        }
        
        now_tm.tm_hour = 3;
        now_tm.tm_min = 0;
        now_tm.tm_sec = 0;
        struct timespec wake_at;
        wake_at.tv_sec = mktime(&now_tm);
        wake_at.tv_nsec = 0;
        
        pthread_mutex_lock(&flush_wait_mutex);
        if (ETIMEDOUT == pthread_cond_timedwait(&flush_wait_cond, &flush_wait_mutex, &wake_at)) {
          time_t woke_at = time(0);
          ctime_r(&woke_at, timebuffer);
          info("Flusher thread woke up at %s", timebuffer);
          
          ce_suspend(engine);
          is_flush(engine->item_source);
          create_classify_new_item_jobs_for_all_tags(engine);
          ce_resume(engine);
          
          time_t done_at = time(0);
          ctime_r(&done_at, timebuffer);
          info("Flushing complete and classification resumed at %s", timebuffer);
        } else {
          // woke up for some other reasons
        }
        pthread_mutex_unlock(&flush_wait_mutex);
      }
    }    
  }
  
  DB_THREAD_END;
  return EXIT_SUCCESS;
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
    job->item_scope = ITEM_SCOPE_ALL;
    NOW(job->created_at);
    job->tags_classified = 0;
    job->items_classified = 0;
    job->auto_cleanup = false;
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
    job->item_scope = ITEM_SCOPE_ALL;
    NOW(job->created_at);
    job->tags_classified = 0;
    job->items_classified = 0;
    job->auto_cleanup = false;
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

static const char * error_msgs[] = {
    "No error",
    "Tag does not exist",
    "No tags to classify for user",
    "Bad job type",
    "Unknown error"
};

const char * cjob_state_msg(const ClassificationJob * job) {
  return state_msgs[job->state];
}

ClassificationJobError cjob_error(const ClassificationJob *job) {
  return job->error;
}

const char * cjob_error_msg(const ClassificationJob *job) {
  return error_msgs[job->error];
}

void cjob_cancel(ClassificationJob *job) {
  job->state = CJOB_STATE_CANCELLED;
}

float cjob_duration(const ClassificationJob *job) {
  float duration;
  
  if (CJOB_STATE_COMPLETE == job->state) {
    duration =  tdiff(job->created_at, job->completed_at);
  } else {
    struct timeval now;
    NOW(now);
    duration = tdiff(job->started_at, now);
  }
  
  return duration;
}

/** Do the actual classification.
 * 
 */
static int cjob_classify(ClassificationJob *job, const TagList *taglist, const ItemSource *item_source, 
                          const Pool *random_background, Queue *tagging_queue) {
  
  if (taglist->size < 1) {
    return 1;
  }
  
  int i;
  int failed = false;
  ItemList *items = is_fetch_all_items(item_source);
  if (!items) MALLOC_ERR();
  int number_of_items = item_list_size(items);
  const float progress_increment = 80.0 / number_of_items;

  /* Start by training up all the classifiers for the tags */
  TrainedClassifier **tc = calloc(taglist->size, sizeof(TrainedClassifier*));
  if (!tc) MALLOC_ERR();
  for (i = 0; i < taglist->size; i++) {
    tc[i] = train(taglist->tags[i], item_source);
    if (!tc[i]) MALLOC_ERR();
  }
  job->progress = 10.0;
  NOW(job->trained_at);
      
  /* Now precompute all the classifiers */
  job->state = CJOB_STATE_CALCULATING;
  Classifier **classifier = calloc(taglist->size, sizeof(Classifier*));
  if (!classifier) MALLOC_ERR();
  for (i = 0; i < taglist->size; i++) {
    classifier[i] = precompute(tc[i], random_background);
    if (!classifier[i]) MALLOC_ERR();
    
    /*  Once we have the precomputed classifier
     *  we can discard the trained classifier.
     */
    tc_free(tc[i]);
    tc[i] = NULL;
  }
  job->progress = 20.0;
  NOW(job->computed_at);
            
  /* Now do the actual classification of each item for each classifier */
  job->state = CJOB_STATE_CLASSIFYING;  
  for (i = 0; i < number_of_items; i++) {    
    Item *item = item_list_item_at(items, i);
    
    if (NULL == item) {
      error("item at %i was NULL", i);    
    } else {
      int complete = false; 
      int j;
      
      for (j = 0; j < taglist->size; j++) {
        if (job->item_scope == ITEM_SCOPE_NEW && 
            item_get_time(item) < tag_last_classified_at(taglist->tags[j])) {
          // Only classifying new items and we have reached the point where
          // the tag has already been applied to subsequent items, so if
          // this is the only tag we bail out completely, if there are other
          // tags just try the next one.
          if (taglist->size > 1) {
            continue;
          } else {
            complete = true;
            break;
          }
        }
         
        job->items_classified++;
        Tagging *tagging = classify(classifier[j], item);
        if (!tagging) MALLOC_ERR();
        TaggingInsertionJob *tagging_job = create_tagging_insertion_job(tagging);
        if (!tagging_job) MALLOC_ERR();
        q_enqueue(tagging_queue, tagging_job);        
      }
      
      if (complete) {
        break;
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
  fatal("Malloc error in classification processing, probably out of memory??");
  failed = true;
  goto exit;
}

static void cjob_process(ClassificationJob *job, const ItemSource *item_source, TagDB *tag_db, 
                  const Pool *random_background, Queue *tagging_queue) {
  /* If the job is cancelled bail out before doing anything */
  if (job->state == CJOB_STATE_CANCELLED) return;
  
  NOW(job->started_at);
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
    job->tags_classified = taglist->size;
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
  
  NOW(job->completed_at);
}

/********************************************************************************
 * Tagging insertion Job functions
 ********************************************************************************/
TaggingInsertionJob * create_tagging_insertion_job(Tagging *tagging) {
  TaggingInsertionJob *job = malloc(sizeof(TaggingInsertionJob));
  if (job) {
    job->job_id = generate_job_id();
    NOW(job->created_at);
    job->tagging = tagging;
  }
  
  return job;
}

void free_tagging_insertion_job(TaggingInsertionJob *job) {
  if (job) {
    free((char *) job->job_id);
    free(job->tagging);
    free(job);
  }
}

/**********************************************************************************
 * Helpers
 */

static float tdiff(struct timeval from, struct timeval to) {
  double from_d = from.tv_sec + (from.tv_usec / 1000000.0);
  double to_d = to.tv_sec + (to.tv_usec / 1000000.0);
  return to_d - from_d;  
}
