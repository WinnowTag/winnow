// Copyright (c) 2007 The Kaphan Foundation
//
// Possession of a copy of this file grants no permission or license
// to use, modify, or create derivate works.
//
// Please contact info@peerworks.org for further information.

#include "logging.h"

#include <stdlib.h>
#include <stdio.h>
#include <time.h>
#if HAVE_STDARG_H
#include <stdarg.h>
#endif

static FILE *log_file = NULL;

static inline void ensure_logfile(void) {
  if (!log_file) {
    log_file = stderr;
  }
}

void initialize_logging(const char *filename) {
  log_file = fopen(filename, "a");
}

void close_log() {
  if (log_file != stderr) {
    fclose(log_file);
  }
}

static void _log(const char * prefix, const char * file, int line, const char * fmt, va_list argp) {
  time_t now;
  struct tm now_tm;
  char time_s[26];
  
  now = time(NULL); 
  localtime_r(&now, &now_tm);
  strftime(time_s, sizeof(time_s), "%Y-%m-%dT%H:%M:%S", &now_tm);
  
  ensure_logfile();
	fprintf(log_file, "%s [%s] (%s:%i): ", prefix, time_s, file, line);
	vfprintf(log_file, fmt, argp);
	fprintf(log_file, "\n");
  fflush(log_file);
}

void _fatal(const char *file, int line, const char *fmt, ...) {
  va_list argp;
  va_start(argp, fmt);
  _log("FATAL", file, line, fmt, argp);
  va_end(argp);
  exit(1);
}

void _error(const char *file, int line, const char *fmt, ...) {
  va_list argp;
  va_start(argp, fmt);
  _log("ERROR", file, line, fmt, argp);
  va_end(argp);
}

void _info(const char *file, int line, const char *fmt, ...) {
  va_list argp;
  va_start(argp, fmt);
  _log("INFO ", file, line, fmt, argp);
  va_end(argp);
}

void _debug(const char *file, int line, const char *fmt, ...) {
  va_list argp;
  va_start(argp, fmt);
  _log("DEBUG", file, line, fmt, argp);
  va_end(argp);
}

void _trace(const char *file, int line, const char *fmt, ...) {
  va_list argp;
  va_start(argp, fmt);
  _log("TRACE", file, line, fmt, argp);
  va_end(argp);
}
