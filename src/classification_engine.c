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
#include <unistd.h>
#include "classification_engine.h"
#include "uuid.h"
#include "classifier.h"
#include "item_cache.h"
#include "tagger.h"
#include "job_queue.h"
#include "misc.h"
#include "logging.h"
#include "array.h"
#include "xml_error_functions.h"

#include <libxml/xmlerror.h>

#define CLASSIFIER_REQUEUE 4
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
  ClassificationEngineOptions *options;

  /* Performance log file */
  FILE *performance_log;

  pthread_mutex_t *perf_log_mutex;

  /* Pointer to the ItemCache.
   *
   */
  ItemCache *item_cache;

  /* Pointer to the TaggerCache. */
  TaggerCache *tagger_cache;

  /* The Queue on which classification jobs are stored.
   *
   * This is shared between each classification worker.
   * It handles it's own synchronization.
   */
  Queue *classification_job_queue;

  /* Store of all the current classification jobs in the system, keyed by job id.
   */
  Pvoid_t classification_jobs;
  pthread_mutex_t *classification_jobs_mutex;

  /* Flag for whether the engine is running */
  int is_running;

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

  /* ItemSource flushing thread */
  pthread_t flusher;
};

static void ce_record_classification_job_timings(ClassificationEngine *ce, const ClassificationJob *job);
static void *classification_worker_func(void *engine_vp);
static void *purge_old_jobs_thread(void *);
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
    job->errmsg           = NULL;
    job->item_scope       = ITEM_SCOPE_ALL;
    job->items_classified = 0;
    job->auto_cleanup     = false;
    job->first_time_tried = -1;
    NOW(job->created_at);
  }

  return job;
}

void free_classification_job(ClassificationJob * job) {
  if (job) {
    if (job->errmsg) free(job->errmsg);
    free((char *) job->id);
    free((char *) job->tag_url);
    free(job);
  }
}

static const char * state_msgs[] = {
    "Waiting",
    "Training",
    "Classifying",
    "Inserting",
    "Complete",
    "Cancelled",
    "Error"
};

static const char * error_msgs[] = {
    "No error",
    "Tag could not be retrieved",
    "No tags to classify for user",
    "Bad job type",
    "The job timed out waiting for some resources",
    "The tag is already being processed",
    "Unknown error"
};

const char * cjob_state_msg(const ClassificationJob * job) {
  debug("state = %i", job->state);
  return state_msgs[job->state];
}

const char * cjob_error_msg(const ClassificationJob *job, char * buffer, size_t size) {
  if (job->errmsg) {
    snprintf(buffer, size, "%s: %s", error_msgs[job->error], job->errmsg);
  } else {
    snprintf(buffer, size, error_msgs[job->error]);
  }

  return buffer;
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
    duration = tdiff(job->created_at, now);
  }

  return duration;
}

struct JobStuff {
  ClassificationJob *job;
  Tagger *tagger;
  Array *taggings;
  double threshold;
  Credentials * credentials;
};

static int classify_item_cb(const Item *item, void *memo) {
  struct JobStuff *stuff = (struct JobStuff*) memo;
  int rc = CLASSIFIER_OK;

  if (stuff->job->item_scope == ITEM_SCOPE_NEW && item_get_time(item) < stuff->tagger->last_classified) {
    rc = CLASSIFIER_FAIL;
  } else {
    stuff->job->items_classified++;
    double probability;
    if (TAGGER_OK == classify_item(stuff->tagger, item, &probability)) {
      if (probability >= stuff->threshold) {
        arr_add(stuff->taggings, create_tagging(item_get_id(item), probability));
      }
    } else {
      error("Error classifying item");
      rc = CLASSIFIER_FAIL;
    }
  }

  stuff->job->progress += stuff->job->progress_increment;

  return rc;
}

static int handle_not_found(ClassificationJob *job) {
	if (job->errmsg) {
		error("TAG_NOT_FOUND: %s, %s", job->tag_url, job->errmsg);
	} else {
		error("TAG_NOT_FOUND: %s", job->tag_url);
	}
	job->state = CJOB_STATE_ERROR;
	job->error = CJOB_ERROR_NO_SUCH_TAG;
  NOW(job->completed_at);
	return CLASSIFIER_FAIL;
}

static int handle_checked_out(ClassificationJob *job) {
	job->state = CJOB_STATE_ERROR;
	job->error = CJOB_ERROR_CHECKED_OUT;
  NOW(job->completed_at);
	return CLASSIFIER_FAIL;
}

static int do_classification(struct JobStuff *job_stuff, ItemCache *item_cache) {
	NOW(job_stuff->job->trained_at);

	job_stuff->job->state = CJOB_STATE_CLASSIFYING;
	job_stuff->job->progress = 20.0;
	job_stuff->job->progress_increment = 60.0 / item_cache_cached_size(item_cache);

	job_stuff->taggings = create_array(1000);
	item_cache_each_item(item_cache, &classify_item_cb, job_stuff);
	NOW(job_stuff->job->classified_at);
	job_stuff->tagger->last_classified = time(NULL);

	/* Save the results */
	job_stuff->job->state = CJOB_STATE_INSERTING;

	if (job_stuff->job->item_scope == ITEM_SCOPE_NEW) {
		update_taggings(job_stuff->tagger, job_stuff->taggings, job_stuff->credentials, &(job_stuff->job->errmsg));
	} else {
		replace_taggings(job_stuff->tagger, job_stuff->taggings, job_stuff->credentials, &(job_stuff->job->errmsg));
	}

	free_array(job_stuff->taggings);

	NOW(job_stuff->job->completed_at);
	job_stuff->job->progress = 100.0;
	job_stuff->job->state = CJOB_STATE_COMPLETE;

	return CLASSIFIER_OK;
}

static int run_classifcation_job(ClassificationJob * job, ItemCache * item_cache, TaggerCache * tagger_cache, ClassificationEngineOptions * opts) {
  int rc = CLASSIFIER_OK;
  struct JobStuff job_stuff;
  job_stuff.job = job;
  job_stuff.threshold = opts->positive_threshold;
  job_stuff.credentials = opts->credentials;
  job_stuff.taggings = NULL;

  /* If the job is cancelled bail out before doing anything */
  if (job->state == CJOB_STATE_CANCELLED) return CLASSIFIER_OK;

  NOW(job->started_at);
  job->state = CJOB_STATE_TRAINING;

  /* Clear the error message if it exists */
  if (job->errmsg) {
    char *errmsg = job->errmsg;
    job->errmsg = NULL;
    free(errmsg);
  }

  /* Try and get the tagger from the tagger_cache */
  job_stuff.tagger = NULL;
  int cache_rc = get_tagger(tagger_cache, job->tag_url, &(job_stuff.tagger), &job->errmsg);
  debug("return from get_tagger with %i", cache_rc);

  switch (cache_rc) {
    case TAGGER_OK:
      rc = do_classification(&job_stuff, item_cache);
      release_tagger(tagger_cache, job_stuff.tagger);
      break;
    case TAG_NOT_FOUND:
      rc = handle_not_found(job);
      break;
    case TAGGER_CHECKED_OUT:
      rc = handle_checked_out(job);
      break;
    default:
    	rc = CLASSIFIER_FAIL;
    	fatal("Got unknown value from get_tagger: %i", cache_rc);
    	break;
  }

  return rc;
}

/* Creates but doesn't start a classification engine.
 *
 * This verifies that the classifiation engine has a valid item source,
 * if the item source is invalid it returns NULL.
 */
ClassificationEngine * create_classification_engine(ItemCache *item_cache, TaggerCache * tagger_cache, ClassificationEngineOptions *options) {
  ClassificationEngine *engine = calloc(1, sizeof(ClassificationEngine));

  if (engine) {
    engine->options = options;
    engine->item_cache = item_cache;
    engine->tagger_cache = tagger_cache;
    item_cache_set_update_callback(item_cache, item_cache_updated_hook, engine);
    engine->is_running = false;
    engine->is_classification_suspended = false;
    engine->classification_job_queue = NULL;
    engine->num_threads_suspended = 0;

    if (engine->options->performance_log) {
      const char *performance_log = engine->options->performance_log;
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

int ce_remove_classification_job(ClassificationEngine * engine, const ClassificationJob *job, int force) {
  int removed = false;
  if (engine && job && (job->state == CJOB_STATE_COMPLETE || job->state == CJOB_STATE_ERROR || force)) {
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
  int success = true;
  if (engine) {
    engine->is_running = true;

    int i;

    engine->classification_worker_threads = calloc(engine->options->worker_threads, sizeof(pthread_t));
    for (i = 0; i < engine->options->worker_threads; i++) {
      if (pthread_create(&(engine->classification_worker_threads[i]), NULL, classification_worker_func, engine)) {
        fatal("Error creating thread %i for classification", i + 1);
        exit(1);
      }
    }
    
    if (pthread_create(&(engine->flusher), NULL, purge_old_jobs_thread, engine)) {
      fatal("Error created purge thread");
      exit(1);
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
    for (i = 0; i < engine->options->worker_threads; i++) {
      debug("joining thread %i", engine->classification_worker_threads[i]);
      pthread_join(engine->classification_worker_threads[i], NULL);
    }
    debug("Returned from cw join");
    
    pthread_cancel(engine->flusher);
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
    for (i = 0; i < engine->options->worker_threads; i++) {
      pthread_join(engine->classification_worker_threads[i], NULL);
    }
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
      if (engine->num_threads_suspended < engine->options->worker_threads) {
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
    float clas_time = tdiff(job->trained_at, job->classified_at);
    float insert_time = tdiff(job->classified_at, job->completed_at);

    if (ce->performance_log) {
      pthread_mutex_lock(ce->perf_log_mutex);
      fprintf(ce->performance_log, "%i,%.5f,%.5f,%.5f,%.5f\n",
                job->items_classified,
                wait_time, train_time, clas_time,
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
  if (ce->num_threads_suspended >= ce->options->worker_threads) {   \
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
    ce_remove_classification_job(ce, job, true);       \
    free_classification_job(job);                \
    continue;                                    \
  }

static int wait_if_suspended(ClassificationEngine * ce) {
  int exit = false;
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
        exit = true;
      }
    }
  pthread_mutex_unlock(ce->classification_suspension_mutex);
  return exit;
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
  SET_XML_ERROR_HANDLERS;

  /* Grab references to shared resources */
  ClassificationEngine *ce = (ClassificationEngine*) engine_vp;
  Queue *job_queue         = ce->classification_job_queue;

  while (!q_empty(job_queue) || ce->is_running) {
    if (wait_if_suspended(ce)) break;
    trace("About to wait on queue, thread %i", pthread_self());
    ClassificationJob *job = (ClassificationJob*) q_dequeue_or_wait(job_queue, 1);
    trace("Returned from queue, thread %i", pthread_self());

    if (job) {
      debug("%i got job off queue: %s", pthread_self(), job->id);

      /* Only proceed if the job is not cancelled */
      NEXT_IF_CANCELLED(ce, job);

      int rc = run_classifcation_job(job, ce->item_cache, ce->tagger_cache, ce->options);

      if (rc == CLASSIFIER_REQUEUE) {
        debug("Requeuing job");
        q_enqueue(job_queue, job);
      } else {
        ce_record_classification_job_timings(ce, job);
        if (job->auto_cleanup) {
          ce_remove_classification_job(ce, job, true);
          free_classification_job(job);
        }
      }
    }
  }

  info("classification_worker %i ending", pthread_self());

  return EXIT_SUCCESS;
}

static void purge_old_jobs(ClassificationEngine *ce) {
  pthread_mutex_lock(ce->classification_jobs_mutex);
  time_t purge_time = time(NULL) - (60 * 60);
  PWord_t job_pointer = NULL;
  uint8_t index[JOB_ID_SIZE + 1];
  strcpy((char*) index, "");
  
  JSLF(job_pointer, ce->classification_jobs, index);
  while(job_pointer != NULL) {
    ClassificationJob *job = (ClassificationJob*) (*job_pointer);
    if (job->state == CJOB_STATE_COMPLETE || job->state == CJOB_STATE_ERROR) {
      if (job->completed_at.tv_sec < purge_time) {
        debug("Purging %s", index);
        int removed = false;
        JSLD(removed, ce->classification_jobs, (uint8_t*) job->id);
        free_classification_job(job);
      }
    }
    
    JSLN(job_pointer, ce->classification_jobs, index);
  }
  
  pthread_mutex_unlock(ce->classification_jobs_mutex);
}

void * purge_old_jobs_thread(void *engine_vp) {
  ClassificationEngine *ce = (ClassificationEngine*) engine_vp;
  
  while (ce->is_running) {
    sleep(60 * 60); // Runs every hour
    info("Purging old classification jobs");
    purge_old_jobs(ce);    
  }
  
  return EXIT_SUCCESS;
}

static void create_classify_new_item_jobs_for_all_tags(ClassificationEngine *ce) {
  if (ce) {
    Array *tag_urls;
    char *errmsg = NULL;
    int rc = fetch_tags(ce->tagger_cache, &tag_urls, &errmsg);

    if (rc == TAG_INDEX_OK) {
      int i;

      for (i = 0; i < tag_urls->size; i++) {
       ce_add_classify_new_items_job_for_tag(ce, (char *) tag_urls->elements[i]);
      }

      info("Created %i classify new items jobs", tag_urls->size);
    } else {
      error("Could not fetch tag urls: %s", errmsg);
      free(errmsg);
    }
  }
}

void item_cache_updated_hook(ItemCache * item_cache, void *memo) {
  ClassificationEngine *ce = memo;
  if (ce) {
    create_classify_new_item_jobs_for_all_tags(ce);
  }
}
