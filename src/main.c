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
#include "logging.h"
#include "cls_config.h"
#include "classification_engine.h"
#include "httpd.h"

#define DEFAULT_CONFIG_FILE "config.conf"
#define DEFAULT_LOG_FILE "classifier.log"
#define SHORT_OPTS "hvc:l:"

static Config *config;
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
    
    fprintf(stderr, "Complete.\n");
    if (config) {
      free_config(config);
    }
    
    exit(sig);
  }
}

int main(int argc, char **argv) {
  char *config_file = DEFAULT_CONFIG_FILE;
  char *log_file = DEFAULT_LOG_FILE;
  
  int longindex;
  int opt;
  static struct option long_options[] = {
      {"version", no_argument, 0, 'v'},
      {"help", no_argument, 0, 'h'},
      {"config-file", required_argument, 0, 'c'},
      {"log-file", required_argument, 0, 'l'},
      {0, 0, 0, }
  };
  
  while (-1 != (opt = getopt_long(argc, argv, SHORT_OPTS, long_options, &longindex))) {
    switch (opt) {
      case 'v':
        printf("%s\n", PACKAGE_STRING);
        exit(EXIT_SUCCESS);
      break;
      case 'c':
        config_file = optarg;
      break;
      case 'l':
        log_file = optarg;
      break;
      case 'h':
        // TODO Add help
        printf("TODO: add help\n");
        exit(0);
      default:
        exit(EXIT_FAILURE);
      break;
    }
  }
  
  if (signal(SIGINT, termination_handler) == SIG_IGN) 
    signal(SIGINT, SIG_IGN);
  if (signal(SIGHUP, termination_handler) == SIG_IGN) 
    signal(SIGHUP, SIG_IGN);
  if (signal(SIGTERM, termination_handler) == SIG_IGN)
    signal(SIGTERM, SIG_IGN);
    
  initialize_logging(log_file);
  config = load_config(config_file);
  engine = create_classification_engine(config);
  httpd = httpd_start(config, engine);
  
  ce_run(engine);  
  return EXIT_SUCCESS;
}
