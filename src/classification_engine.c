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
#include "item_cache.h"
#include "tag.h"
#include "job_queue.h"
#include "misc.h"
#include "logging.h"

#ifdef HAVE_DATABASE_ACCESS
#include <mysql.h>
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
  
  /* Pointer to the ItemCache.
   *  
   */
  ItemCache *item_cache;
    
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

static void ce_record_classification_job_timings(ClassificationEngine *ce, const ClassificationJob *job);
static void *classification_worker_func(void *engine_vp);
static void *insertion_worker_func(void *engine_vp);
//static void *flusher_func(void *engine_vp);
static void cjob_process(ClassificationJob *job, ItemCache *is);
static void cjob_free_taggings(ClassificationJob *job);
static void item_cache_updated_hook(ItemCache * item_cache, void * memo);

/********************************************************************************
 * Classification Job functions
 ********************************************************************************/
#define SET_JOB_ERROR(job, err, msg, ...) \
  job->state = CJOB_STATE_ERROR; \
  job->error = err;              \
  error(msg, __VA_ARGS__);
  
static float tdiff(struct timeval from, struct timeval to) {
  double from_d = from.tv_sec + (from.tv_usec / 1000000.0);
  double to_d = to.tv_sec + (to.tv_usec / 1000000.0);
  return to_d - from_d;  
}

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

static ClassificationJob * create_classification_job(const char * tag_url) {
  ClassificationJob *job = malloc(sizeof(ClassificationJob));
  if (job) {
    job->tag_url          = strdup(tag_url);
    job->progress         = 0;
    job->id           = generate_job_id();
    job->state            = CJOB_STATE_WAITING;
    job->error            = CJOB_ERROR_NO_ERROR;
    job->item_scope       = ITEM_SCOPE_ALL;
    job->tags_classified  = 0;
    job->items_classified = 0;
    job->auto_cleanup     = false;
    NOW(job->created_at);
  }
  
  return job;
}

void free_classification_job(ClassificationJob * job) {
  if (job) {
    free((char *) job->id);
    free((char *) job->tag_url);
    free(job);
  }
}

static const char * state_msgs[] = {
    "Waiting",
    "Training",
    "Calculating",
    "Classifying",
    "Inserting",
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

/* Creates but doesn't start a classification engine.
 * 
 * This verifies that the classifiation engine has a valid item source,
 * if the item source is invalid it returns NULL.
 */
ClassificationEngine * create_classification_engine(ItemCache *item_cache, const Config *config) {
  ClassificationEngine *engine = calloc(1, sizeof(ClassificationEngine));
  
  if (engine) {
    engine->config = config;
    engine->item_cache = item_cache;
    item_cache_set_update_callback(item_cache, item_cache_updated_hook, engine);
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
        fprintf(engine->performance_log, "# Classifier started at %s", time_s);
        fflush(engine->performance_log);
      }
    }
    
    INIT_MUTEX(engine->classification_suspension_mutex);
    INIT_MUTEX(engine->suspension_notification_mutex);
    INIT_MUTEX(engine->classification_jobs_mutex);
    INIT_MUTEX(engine->perf_log_mutex);
    INIT_COND(engine->classification_suspension_cond);
    INIT_COND(engine->suspension_notification_cond);    
        
    engine->classification_job_queue = new_queue();
    engine->tagging_store_queue = new_queue();
  }
  
exit:
  return engine;
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
    
    free_queue(engine->classification_job_queue);
    free_queue(engine->tagging_store_queue);
    free(engine);
  }
}

/************************************************************************************
 * Functions for adding, fetching and removing classification jobs.
 */

static int _add_classification_job(ClassificationEngine *engine, ClassificationJob *job) {
  int failure = true;
  PWord_t job_pointer;
  
  pthread_mutex_lock(engine->classification_jobs_mutex);
  JSLI(job_pointer, engine->classification_jobs, (uint8_t*) job->id);
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
ClassificationJob * ce_add_classification_job(ClassificationEngine * engine, const char * tag_url) {
  ClassificationJob *job = NULL;
  
  if (engine) {
    job = create_classification_job(tag_url);
    if (_add_classification_job(engine, job)) {
      free_classification_job(job);
      job = NULL;
    }
  }
  
  return job;
}

ClassificationJob * ce_add_classify_new_items_job_for_tag(ClassificationEngine * engine, const char * tag_url) {
  ClassificationJob *job = NULL;
  if (engine) {
    job = create_classification_job(tag_url);
    job->item_scope = ITEM_SCOPE_NEW;
    job->auto_cleanup = true;
    
    if (_add_classification_job(engine, job)) {
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
  if (engine && job && job->state == CJOB_STATE_COMPLETE) {
    pthread_mutex_lock(engine->classification_jobs_mutex);
    JSLD(removed, engine->classification_jobs, (uint8_t*) job->id);
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
    
    
    // if (pthread_create(&(engine->insertion_worker_thread), NULL, insertion_worker_func, engine)) {
    //      fatal("Error creating thread for insertion");
    //    }
    
//    if (pthread_create(&(engine->flusher), NULL, flusher_func, engine)) {
//      fatal("Error creating thread for item source flushing");
//    }    
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
    float clas_time = tdiff(job->computed_at, job->classified_at);
    float insert_time = tdiff(job->classified_at, job->completed_at);

    if (ce->performance_log) {
      pthread_mutex_lock(ce->perf_log_mutex);    
      fprintf(ce->performance_log, "%i,%i,%.5f,%.5f,%.5f,%.5f,%.5f\n", 
                job->tags_classified, job->items_classified,
                wait_time, train_time, calc_time, clas_time, 
                insert_time);
      fflush(ce->performance_log);
      pthread_mutex_unlock(ce->perf_log_mutex);
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

#define NEXT_IF_CANCELLED(ce, job) \
  if (CJOB_STATE_CANCELLED == job->state) { \
    /* If it is cancelled we remove              \
     * and free the job ourselves */             \
    job->state = CJOB_STATE_COMPLETE;            \
    ce_remove_classification_job(ce, job);       \
    free_classification_job(job);                \
    continue;                                    \
  }

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
  // TODO TagDB *tag_db = create_tag_db(&tag_db_config);
  
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
    ClassificationJob *job = (ClassificationJob*) q_dequeue_or_wait(job_queue, 1);
    trace("Returned from queue, thread %i", pthread_self());
    
    if (job) {
      debug("%i got job off queue: %s", pthread_self(), job->id);
      
      /* Only proceed if the job is not cancelled */
      NEXT_IF_CANCELLED(ce, job);
      /* Get the reference to the item source here so we get it fresh for each job. This means that if
       * the item source is flushed we get a new copy on the next job.
       */
      // TODO cjob_process(job, ce->item_cache, tag_db);
      
      if (CJOB_STATE_ERROR != job->state) {
        q_enqueue(ce->tagging_store_queue, job);
      }     
    }    
  }

  info("classification_worker %i ending", pthread_self());
  // TODO free_tag_db(tag_db);
  
  DB_THREAD_END;
  return EXIT_SUCCESS;
}

// void *insertion_worker_func(void *engine_vp) {
//   DB_THREAD_INIT;
//   info("Starting insertion worker thread id = %i", pthread_self());
//   ClassificationEngine *ce = (ClassificationEngine*) engine_vp;
//   EngineConfig econfig;
//   cfg_engine_config(ce->config, &econfig);
//   
//   DBConfig tagging_store_config;
//   cfg_tagging_store_db_config(ce->config, &tagging_store_config);
//   // TODO TaggingStore *store = create_db_tagging_store(&tagging_store_config, econfig.insertion_threshold);
//       
//   while(!q_empty(ce->tagging_store_queue) || ce->is_inserting) {
//     ClassificationJob *job = q_dequeue_or_wait(ce->tagging_store_queue, 1);
//     
//     if (job && job->taggings) {
//       debug("%i got job off insertion queue: %s", pthread_self(), cjob_id(job));
//       /* Only proceed if the job is not cancelled */
//       NEXT_IF_CANCELLED(ce, job);
//       
//       if (job->item_scope == ITEM_SCOPE_ALL) {
//         // TODO tagging_store_replace_taggings(store, job->taglist, job->taggings, job->num_taggings, &(job->progress));
//       } else {
//         // TODO tagging_store_replace_taggings(store, NULL, job->taggings, job->num_taggings, &(job->progress));
//       }
//       
//       job->state = CJOB_STATE_COMPLETE;
//       job->progress = 100;
//       NOW(job->completed_at);       
//       ce_record_classification_job_timings(ce, job);
//             
//       if (job->auto_cleanup) {
//         ce_remove_classification_job(ce, job);
//         free_classification_job(job);
//       } else {
//         // clean up taggings, since they are no longer needed      
//         cjob_free_taggings(job);
//       }
//     }
//   }
//   
//   info("insertion worker function ending");
//   // TODO free_tagging_store(store);
//   DB_THREAD_END;
//   return EXIT_SUCCESS;
// }

// TODO static void create_classify_new_item_jobs_for_all_tags(ClassificationEngine *ce) {
//  if (ce) {
//    DBConfig dbConfig;
//    cfg_tag_db_config(ce->config, &dbConfig);
//    TagDB *tag_db = create_tag_db(&dbConfig);
//    if (tag_db) {
//      int i;
//      int size;
//      int *ids = tag_db_get_all_tag_ids(tag_db, &size);
//      
//      for (i = 0; i < size; i++) {
//        ce_add_classify_new_items_job_for_tag(ce, ids[i]);
//      }
//      
//      free(ids);
//      info("Created %i classify new items jobs", size);
//    } else {
//      error("Could not create tag_db in create_classify_new_item_jobs_for_all_tags");
//    }
//    
//    free_tag_db(tag_db);
//  }
// }

void item_cache_updated_hook(ItemCache * item_cache, void *memo) {
  ClassificationEngine *ce = memo;
  if (ce) {
    // TODO create_classify_new_item_jobs_for_all_tags(ce);
  }
}

//void *flusher_func(void *engine_vp) {
//  ClassificationEngine *engine = (ClassificationEngine*) engine_vp;
//  
//  if (engine) {
//    pthread_mutex_t flush_wait_mutex;
//    pthread_cond_t flush_wait_cond;
//    
//    if (pthread_mutex_init(&flush_wait_mutex, NULL) || pthread_cond_init(&flush_wait_cond, NULL)) {
//      fatal("Could not create mutex or cond for flusher thread");
//    } else {
//      while (engine->is_running) {        
//        // This will run at 0200 everyday
//        char timebuffer[26];
//        time_t now = time(0);
//        struct tm now_tm;
//        localtime_r(&now, &now_tm);
//        
//        // If we are past 0200 move to the next day
//        if (now_tm.tm_hour >= 3) {
//          now_tm.tm_mday++;
//        }
//        
//        now_tm.tm_hour = 4;
//        now_tm.tm_min = 30;
//        now_tm.tm_sec = 0;
//        struct timespec wake_at;
//        wake_at.tv_sec = mktime(&now_tm);
//        wake_at.tv_nsec = 0;
//        
//        pthread_mutex_lock(&flush_wait_mutex);
//        if (ETIMEDOUT == pthread_cond_timedwait(&flush_wait_cond, &flush_wait_mutex, &wake_at)) {
//          time_t woke_at = time(0);
//          ctime_r(&woke_at, timebuffer);
//          info("Flusher thread woke up at %s", timebuffer);
//          
//          ce_suspend(engine);
//          is_flush(engine->item_source);
//          create_classify_new_item_jobs_for_all_tags(engine);
//          ce_resume(engine);
//          
//          time_t done_at = time(0);
//          ctime_r(&done_at, timebuffer);
//          info("Flushing complete and classification resumed at %s", timebuffer);
//        } else {
//          // woke up for some other reasons
//        }
//        pthread_mutex_unlock(&flush_wait_mutex);
//      }
//    }    
//  }
//  
//  return EXIT_SUCCESS;
//}



// static int classify_item(const Item *item, void *memo) {
//   ClassificationJob *job = (ClassificationJob *) memo;
//   int rc = CLASSIFIER_OK;
//   int i;
//         
//   for (i = 0; i < job->taglist->size; i++) {
//     if (job->item_scope == ITEM_SCOPE_NEW && 
//         item_get_time(item) < tag_last_classified_at(job->taglist->tags[i])) {
//       /* Only classifying new items and we have reached the point where
//        * the tag has already been applied to subsequent items, so if
//        * this is the only tag we bail out completely, if there are other
//        * tags just try the next one.
//        */
//       if (job->taglist->size > 1) {
//         continue;
//       } else {
//         debug("item time %i less than last classified time %i", item_get_time(item), tag_last_classified_at(job->taglist->tags[i]));
//         rc = CLASSIFIER_FAIL;
//         break;
//       }
//     }
//      
//     job->items_classified++;
//     debug("classifying %d", item_get_id(item));
//     // TODO Tagging *tagging = classify(job->classifiers[i], item);
//     //     if (!tagging) MALLOC_ERR();
//     //     
//     //     job->taggings[job->num_taggings++] = tagging;        
//   }
// 
//   job->progress += job->progress_increment;
//   
//   return rc;
// malloc_error:
//   job->error = CJOB_STATE_ERROR;
//   job->error = CJOB_ERROR_UNKNOWN_ERROR;
//   fatal("Malloc error in classification processing, probably out of memory??");
//   return CLASSIFIER_FAIL;
// }

/** Do the actual classification.
 * 
 */
static int cjob_classify(ClassificationJob *job, ItemCache *item_cache) {
  
  // if (job->taglist->size < 1) {
  //   return 1;
  // }
  // 
  // int i;
  int failed = false;
  
  // TODO Re-write cjob_classify to work with new Tagger
  // int number_of_items = item_cache_cached_size(item_cache);
  //   job->progress_increment = 60.0 / number_of_items;
  // 
  //   // allocate enough space for a tagging for each item in each tag
  //   job->taggings = calloc(job->taglist->size * number_of_items, sizeof(Tagging*));
  //   if (!job->taggings) MALLOC_ERR();
  //   // allocate enough space for pointers to all the classifiers
  //   job->classifiers = calloc(job->taglist->size, sizeof(Classifier*));
  //   if (!job->classifiers) MALLOC_ERR();
  //   
  //   /* Start by training up all the classifiers for the tags */
  //   job->state = CJOB_STATE_TRAINING;
  //   TrainedClassifier **tc = calloc(job->taglist->size, sizeof(TrainedClassifier*));
  //   if (!tc) MALLOC_ERR();
  //   
  //   for (i = 0; i < job->taglist->size; i++) {
  //     tc[i] = train(job->taglist->tags[i], item_cache);
  //     if (!tc[i]) MALLOC_ERR();
  //   }
  //   job->progress = 10.0;
  //   NOW(job->trained_at);
  //       
  //   /* Now precompute all the classifiers */
  //   job->state = CJOB_STATE_CALCULATING;
  //   
  //   for (i = 0; i < job->taglist->size; i++) {
  //     job->classifiers[i] = precompute(tc[i], item_cache_random_background(item_cache));
  //     if (!job->classifiers[i]) MALLOC_ERR();
  //     
  //     /*  Once we have the precomputed classifier
  //      *  we can discard the trained classifier.
  //      */
  //     tc_free(tc[i]);
  //     tc[i] = NULL;
  //   }
  //   job->progress = 20.0;
  //   NOW(job->computed_at);
  //   
  //   /* Clean up trained classifier array */
  //   free(tc);
  //   tc = NULL;
  //             
  //   /* Now do the actual classification of each item for each classifier */
  //   job->state = CJOB_STATE_CLASSIFYING;  
  //   item_cache_each_item(item_cache, classify_item, job);
  //   
  // exit:
  //   for (i = 0; i < job->taglist->size; i++) {
  //     if (tc && tc[i]) {
  //       tc_free(tc[i]);      
  //     }
  //     
  //     if (job->classifiers && job->classifiers[i]) {
  //       free_classifier(job->classifiers[i]);      
  //     }
  //   }  
  //   
  //   if (job->classifiers) {
  //     free(job->classifiers);
  //   }
  //   
  //   if (tc) {
  //     free(tc);
  //   }  
  
  return failed;
  
// malloc_error:
//   job->error = CJOB_STATE_ERROR;
//   job->error = CJOB_ERROR_UNKNOWN_ERROR;
//   fatal("Malloc error in classification processing, probably out of memory??");
//   failed = true;
//   goto exit;
}

static void cjob_process(ClassificationJob *job, ItemCache *item_cache) {
  // /* If the job is cancelled bail out before doing anything */
  //   if (job->state == CJOB_STATE_CANCELLED) return;
  //   
  //   NOW(job->started_at);
  //   
  //   switch (job->type) {
  //     case CJOB_TYPE_TAG_JOB: {
  //       // TODO Tag *tag = tag_db_load_tag_by_id(tag_db, job->tag_id);
  //       Tag *tag = NULL;
  //       if (tag) {
  //         job->taglist = create_tag_list();
  //         taglist_add_tag(job->taglist, tag);
  //       } else {
  //         SET_JOB_ERROR(job, CJOB_ERROR_NO_SUCH_TAG, "Tag %i does not exist", job->tag_id);
  //       }
  //     }
  //     break;
  //     case CJOB_TYPE_USER_JOB:
  //       // TODO job->taglist = tag_db_load_tags_to_classify_for_user(tag_db, job->user_id);          
  //       if (!job->taglist || job->taglist->size == 0) {
  //         SET_JOB_ERROR(job, CJOB_ERROR_NO_TAGS_FOR_USER, "No tags for user %i", job->user_id);
  //       }
  //     break;
  //     default:
  //       SET_JOB_ERROR(job, CJOB_ERROR_BAD_JOB_TYPE, "Unknown CJOB type: %i", job->type);
  //     break;
  //   }
  //     
  //   if (job->taglist) {
  //     job->tags_classified = job->taglist->size;
  //     job->progress = 5.0;
  //     
  //     if (!cjob_classify(job, item_cache)) {
  //       int i;
  //       for (i = 0; i < job->taglist->size; i++) {
  //         // TODO tag_db_update_last_classified_at(tag_db, job->taglist->tags[i]);        
  //       }
  //       job->progress = 80.0;
  //       job->state = CJOB_STATE_INSERTING;
  //       NOW(job->classified_at);
  //     }    
  //   }  
}

/**********************************************************************************
 * Helpers
 */

