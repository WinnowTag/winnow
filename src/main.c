/* Copyright (c) 2007 The Kaphan Foundation
 *
 * Possession of a copy of this file grants no permission or license
 * to use, modify, or create derivate works.
 *
 * Please contact info@peerworks.org for further information.
 */

#include <pthread.h>
#include <config.h>
#include <stdlib.h>
#include <getopt.h>
#include <stdio.h>
#include <signal.h>
#include <errno.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include "logging.h"
#include "classification_engine.h"
#include "httpd.h"
#include "misc.h"
#include "git_revision.h"
#include "feature_extractor.h"
#include "fetch_url.h"
#include "xml_error_functions.h"

#define DEFAULT_LOG_FILE "classifier.log"
#define DEFAULT_PID_FILE "classifier.pid"
#define DEFAULT_DB_FILE "classifier.db"
#define DEFAULT_TOKENIZER_URL "http://localhost"
#define DEFAULT_CACHE_UPDATE_WAIT_TIME 60
#define DEFAULT_LOAD_ITEMS_SINCE 30
#define DEFAULT_MIN_TOKENS 50
#define DEFAULT_MISSING_ITEM_TIMEOUT 60

#define PID_VAL 512
#define DB_VAL  513
#define CREATE_DB_VAL 514
#define CACHE_UPDATE_WAIT_TIME_VAL 515
#define LOAD_ITEMS_SINCE_VAL 516
#define MIN_TOKENS_VAL 517
#define TOKENIZER_URL_VAL 518
#define PERFORMANCE_LOG_FILE_VAL 519
#define TAG_INDEX_VAL 520
#define MISSING_ITEM_TIMEOUT_VAL 521

#define SHORT_OPTS "hvdo:t:n:p:a:c:"
#define USAGE "Usage: classifier [-dvh] [-o LOGFILE] [--db DATABASE_FILE] [--pid PIDFILE] [-t tokenizer_url] [--create-db]\n"

static ItemCacheOptions item_cache_options;
static ItemCache *item_cache;
static Credentials classifier_credentials = {"classifier_id", "classifier_secret"};
static TaggerCacheOptions tagger_cache_options = {"", &classifier_credentials};
static TaggerCache *tagger_cache;
static ClassificationEngineOptions ce_options = {1, 0.0, DEFAULT_MISSING_ITEM_TIMEOUT, NULL, &classifier_credentials};
static ClassificationEngine *engine;
static Httpd *httpd;
static HttpConfig http_config = {8080, NULL};

static void printHelp(void) {
  printf("This is the Peerwork classifier.\n\n");
  printf("Usage: classifier [OPTIONS]\n\n");
  printf(" General Options\n");
  printf("    -d               runs as a daemon\n");
  printf("    -v, --version    prints version information\n");
  printf("    -h, --help       prints this help text\n");
  printf("    -o, --log-file FILE\n");
  printf("                     location of the file to write log messages to\n");
  printf("                     Default: %s\n", DEFAULT_LOG_FILE);
  printf("        --pid FILE   location to write the pid to when running as a daemon\n");
  printf("                     Default: %s\n\n", DEFAULT_PID_FILE);

  printf(" Classification Options:\n");
  printf("    -n, --worker-threads N\n");
  printf("                     number of threads for processing jobs\n");
  printf("                     Default: 1\n");
  printf("    -t, --positive-threshold N\n");
  printf("                     probability threshold for considering a tag to be\n");
  printf("                     applied to and item\n");
  printf("                     Default: 0\n");
  printf("        --performance-log FILE\n");
  printf("                     location of the file in which to write job timings\n\n");
  printf("        --tag-index URL\n");
  printf("                     URL which provides an index of the tags to classify\n\n");
  printf("        --missing-item-timeout N\n");
  printf("                     Number of seconds to wait for missing items to be added\n");
  printf("                     before a job depending on them is canceled and an error\n");
  printf("                     is returned instead\n");
  printf("                     Default: %i seconds\n\n", DEFAULT_MISSING_ITEM_TIMEOUT);

  printf(" Item Cache Options:\n");
  printf("        --db FILE    location of the item cache database file\n");
  printf("        --create     if provide the classifier with create the database at\n");
  printf("                     --db and exit\n");
  printf("        --cache-update-wait-time N\n");
  printf("                     number of seconds to wait after a cache update\n");
  printf("                     before spawning classification jobs\n");
  printf("                     Default: %i seconds\n", DEFAULT_CACHE_UPDATE_WAIT_TIME);
  printf("        --load-items-since N\n");
  printf("                     how many days back to load items from the item cache\n");
  printf("                     Default: %i days\n", DEFAULT_LOAD_ITEMS_SINCE);
  printf("        --tokenizer-url URL\n");
  printf("                     URL of the tokenization service\n");
  printf("        --min-tokens N\n");
  printf("                     the minimum number of tokens an item requires to be\n");
  printf("                     classified\n\n");

  printf(" HTTP Options:\n");
  printf("    -p, --port N     the port to run the HTTP server on\n");
  printf("    -a, --allowed_ip IP_ADDRESS\n");
  printf("                     An IP address to allow to make HTTP requests\n");
  printf("                     Default: any\n\n");
}

volatile sig_atomic_t termination_in_progress = 0;

void termination_handler(int sig) {
  if (termination_in_progress) {
    raise(sig);
  } else {
    termination_in_progress = 1;
    fprintf(stderr, "Terminating classifier...\n");

    if (httpd) {
      fprintf(stderr, "\tShutting down httpd.\n");
      httpd_stop(httpd);
    }

    if (engine) {
      fprintf(stderr, "\tShutting down engine.\n");
      ce_stop(engine);
      free_classification_engine(engine);
    }

    if (item_cache) {
      fprintf(stderr, "\tClosing database.\n");
      free_item_cache(item_cache);
    }

    fprintf(stderr, "Complete.\n");
    exit(sig);
  }
}

static void _daemonize(const char * pid_file) {
  int pid, sid;
  pid = fork();

  if (pid < 0) {
    exit(EXIT_FAILURE);
  } else if (pid > 0) {
    if (pid_file) {
      FILE *pidout = fopen(pid_file, "w");
      if (pidout) {
        fprintf(pidout, "%i", pid);
        fclose(pidout);
      } else {
        fprintf(stderr, "Could not open pid file %s: %s", pid_file, strerror(errno));
      }
    }
    // exit the foreground process
    exit(EXIT_SUCCESS);
  }

  umask(0);

  sid = setsid();
  if (sid < 0) {
    fprintf(stderr, "setsid failed: %s", strerror(errno));
    exit(EXIT_FAILURE);
  }

  if (chdir("/") < 0) {
    fprintf(stderr, "chdir(\"/\") failed: %s", strerror(errno));
    exit(EXIT_FAILURE);
  }
}

static int start_classifier(const char * db_file, const char * tokenizer_url) {
  SET_XML_ERROR_HANDLERS;

  if (CLASSIFIER_OK != item_cache_create(&item_cache, db_file, &item_cache_options)) {
    fprintf(stderr, "Error opening classifier database file at %s: %s\n", db_file, item_cache_errmsg(item_cache));
    free_item_cache(item_cache);
    return EXIT_FAILURE;
  } else {
    item_cache_load(item_cache);
    item_cache_set_feature_extractor(item_cache, tokenize_entry, tokenizer_url);
    item_cache_start_feature_extractor(item_cache);
    item_cache_start_cache_updater(item_cache);
    item_cache_start_purger(item_cache, 60 * 60 * 24);

    tagger_cache = create_tagger_cache(item_cache, &tagger_cache_options);
    tagger_cache->tag_retriever = &fetch_url;
    tagger_cache->tag_index_retriever = &fetch_url;

    info("Fetching tag index for the first time...");
    Array *tags = NULL;
    char *errmsg = NULL;
    int rc = fetch_tags(tagger_cache, &tags, &errmsg);
    if (TAG_INDEX_OK == rc) {
      info("Fetched %i tags from %s", tags->size, tagger_cache_options.tag_index_url);
    } else {
      error("Error fetching tag index: %s", errmsg);
      free(errmsg);
    }

    engine = create_classification_engine(item_cache, tagger_cache, &ce_options);
    httpd = httpd_start(&http_config, engine, item_cache, tagger_cache);
    ce_run(engine);
    return EXIT_SUCCESS;
  }
}

int main(int argc, char **argv) {
  int create_database = false;
  int daemonize = false;
  char *tokenizer_url = DEFAULT_TOKENIZER_URL;
  char *log_file = DEFAULT_LOG_FILE;
  char *pid_file = DEFAULT_PID_FILE;
  char *db_file = DEFAULT_DB_FILE;
  char real_log_file[MAXPATHLEN];
  char real_db_file[MAXPATHLEN];
  item_cache_options.cache_update_wait_time = DEFAULT_CACHE_UPDATE_WAIT_TIME;
  item_cache_options.load_items_since = DEFAULT_LOAD_ITEMS_SINCE;
  item_cache_options.min_tokens = DEFAULT_MIN_TOKENS;

  int longindex;
  int opt;
  static struct option long_options[] = {
      {"version", no_argument, 0, 'v'},
      {"help", no_argument, 0, 'h'},

      {"log-file", required_argument, 0, 'o'},
      {"pid", required_argument, 0, PID_VAL},
      {"db", required_argument, 0, DB_VAL},
      {"create-db", no_argument, 0, CREATE_DB_VAL},

      {"cache-update-wait-time", required_argument, 0, CACHE_UPDATE_WAIT_TIME_VAL},
      {"load-items-since", required_argument, 0, LOAD_ITEMS_SINCE_VAL},
      {"tokenizer-url", required_argument, 0, TOKENIZER_URL_VAL},
      {"min-tokens", required_argument, 0, MIN_TOKENS_VAL},
      {"missing-item-timeout", required_argument, 0, MISSING_ITEM_TIMEOUT_VAL},

      {"worker-threads", required_argument, 0, 'n'},
      {"positive-threshold", required_argument, 0, 't'},
      {"performance-log", required_argument, 0, PERFORMANCE_LOG_FILE_VAL},

      {"port", required_argument, 0, 'p'},
      {"allowed_ip", required_argument, 0, 'a'},
      
      {"credentials", required_argument, 0, 'c'},

      {"tag-index", required_argument, 0, TAG_INDEX_VAL},

      {0, 0, 0, 0}
  };

  while (-1 != (opt = getopt_long(argc, argv, SHORT_OPTS, long_options, &longindex))) {
    switch (opt) {
      case 'o':
        log_file = optarg;
        break;
      case PID_VAL:
        pid_file = optarg;
        break;
      case DB_VAL:
        db_file = optarg;
        break;
      case CREATE_DB_VAL:
        create_database = true;
        break;
      case 'd':
        daemonize = true;
        break;

      /* Item Cache Options */
      case CACHE_UPDATE_WAIT_TIME_VAL:
        item_cache_options.cache_update_wait_time = strtol(optarg, NULL, 10);
        break;
      case LOAD_ITEMS_SINCE_VAL:
        item_cache_options.load_items_since = strtol(optarg, NULL, 10);
        break;
      case MIN_TOKENS_VAL:
        item_cache_options.min_tokens = strtol(optarg, NULL, 10);
        break;
      case TOKENIZER_URL_VAL:
        tokenizer_url = optarg;
        break;

      /* Classification Engine Options */
      case 'n': /* Number of worker threads */
        ce_options.worker_threads = strtol(optarg, NULL, 10);
        break;
      case 't': /* Positive threshold for classification */
        ce_options.positive_threshold = strtod(optarg, NULL);
        break;
      case PERFORMANCE_LOG_FILE_VAL:
        ce_options.performance_log = optarg;
        break;
      case MISSING_ITEM_TIMEOUT_VAL:
        ce_options.missing_item_timeout = strtol(optarg, NULL, 10);
        break;

      /* HTTP options */
      case 'p':
        http_config.port = strtol(optarg, NULL, 10);
        break;
      case 'a':
        http_config.allowed_ip = optarg;
        break;
        
      case 'c':
        break;

      /* Tagger Cache options */
      case TAG_INDEX_VAL:
        tagger_cache_options.tag_index_url = optarg;
        break;

      /* Common Options */
      case 'h':
        // TODO Add help
        printHelp();
        exit(0);
      case 'v':
        printf("%s, build: %s\n", PACKAGE_STRING, git_revision);
        exit(EXIT_SUCCESS);
        break;
      default:
        fprintf(stderr, USAGE);
        exit(EXIT_FAILURE);
      break;
    }
  }

  int rc = EXIT_SUCCESS;

  if (create_database) {
    char error[512];
    if (CLASSIFIER_OK != item_cache_initialize(db_file, error)) {
      fprintf(stderr, "%s\n", error);
      rc = EXIT_FAILURE;
    } else {
      fprintf(stderr, "Database successfully initialized at '%s'\n", db_file);
    }
  } else {
    if (create_file(log_file)) {
      fprintf(stderr, "Could not create %s: %s\n", log_file, strerror(errno));
      exit(EXIT_FAILURE);
    }

    if (NULL == realpath(log_file, real_log_file)) {
      fprintf(stderr, "Could not find %s: %s\n", real_log_file, strerror(errno));
      exit(EXIT_FAILURE);
    }

    if (NULL == realpath(db_file, real_db_file)) {
      fprintf(stderr, "Could not find %s: %s\n", real_db_file, strerror(errno));
      exit(EXIT_FAILURE);
    }

    if (daemonize) {
      _daemonize(pid_file);
    }

    if (signal(SIGINT, termination_handler) == SIG_IGN)  signal(SIGINT, SIG_IGN);
    if (signal(SIGHUP, termination_handler) == SIG_IGN)  signal(SIGHUP, SIG_IGN);
    if (signal(SIGTERM, termination_handler) == SIG_IGN) signal(SIGTERM, SIG_IGN);

    initialize_logging(real_log_file);
    rc = start_classifier(real_db_file, tokenizer_url);
  }

  return rc;
}
