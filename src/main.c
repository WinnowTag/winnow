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
#include "cls_config.h"
#include "classification_engine.h"
#include "httpd.h"
#include "misc.h"
#include "git_revision.h"
#include "feature_extractor.h"

#define DEFAULT_CONFIG_FILE "config/classifier.conf"
#define DEFAULT_LOG_FILE "log/classifier.log"
#define DEFAULT_PID_FILE "log/classifier.pid"
#define DEFAULT_DB_FILE "log/classifier.db"
#define DEFAULT_TOKENIZER_URL "http://localhost"

#define PID_VAL 512
#define DB_VAL  513
#define CREATE_DB_VAL 514
#define SHORT_OPTS "hvdc:l:t:"
#define USAGE "Usage: classifier [-dvh] [-c CONFIGFILE] [-l LOGFILE] [--db DATABASE_FILE] [--pid PIDFILE] [-t tokenizer_url] [--create-db]\n"

static Config *config;
static ItemCache *item_cache;
static ClassificationEngine *engine;
static Httpd *httpd;

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
    if (config) {
      free_config(config);
    }
    
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
  if (CLASSIFIER_OK != item_cache_create(&item_cache, db_file)) {
    fprintf(stderr, "Error opening classifier database file at %s: %s\n", db_file, item_cache_errmsg(item_cache));
    free_item_cache(item_cache);
    return EXIT_FAILURE;
  } else {
    item_cache_load(item_cache);
    item_cache_set_feature_extractor(item_cache, tokenize_entry, tokenizer_url);
    item_cache_start_feature_extractor(item_cache);
    item_cache_start_cache_updater(item_cache);
    item_cache_start_purger(item_cache, 60 * 60 * 24);
    engine = create_classification_engine(item_cache, config);
    httpd = httpd_start(config, engine, item_cache);  
    ce_run(engine);
    return EXIT_SUCCESS;
  }
}

int main(int argc, char **argv) {
  int create_database = false;
  int daemonize = false;
  char *tokenizer_url = DEFAULT_TOKENIZER_URL;
  char *config_file = DEFAULT_CONFIG_FILE;
  char *log_file = DEFAULT_LOG_FILE;
  char *pid_file = DEFAULT_PID_FILE;
  char *db_file = DEFAULT_DB_FILE;
  char real_config_file[MAXPATHLEN];
  char real_log_file[MAXPATHLEN];
  char real_db_file[MAXPATHLEN];
  
  int longindex;
  int opt;
  static struct option long_options[] = {
      {"version", no_argument, 0, 'v'},
      {"help", no_argument, 0, 'h'},
      {"config-file", required_argument, 0, 'c'},
      {"log-file", required_argument, 0, 'l'},
      {"tokenizer-url", required_argument, 0, 't'},
      {"pid", required_argument, 0, PID_VAL},
      {"db", required_argument, 0, DB_VAL},      
      {"create-db", no_argument, 0, CREATE_DB_VAL},
      {0, 0, 0, 0}
  };
  
  while (-1 != (opt = getopt_long(argc, argv, SHORT_OPTS, long_options, &longindex))) {
    switch (opt) {
      case 'v':
        printf("%s, build: %s\n", PACKAGE_STRING, git_revision);
        exit(EXIT_SUCCESS);
      break;
      case 'c':
        config_file = optarg;
      break;
      case 'l':
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
      case 't':
        tokenizer_url = optarg;
      break;
      case 'd':
        daemonize = true;
      break;
      case 'h':
        // TODO Add help
        printf(USAGE);
        exit(0);
      default:
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
    
    if (NULL == realpath(config_file, real_config_file)) {
      fprintf(stderr, "Could not find %s: %s\n", real_config_file, strerror(errno));
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
    
    /* Load config before we daemonize to allow us to pick
     * up and relative paths defined in the config file.
     */
    config = load_config(real_config_file);
      
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
