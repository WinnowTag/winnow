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

#include "logging.h"

#include <stdlib.h>
#include <stdio.h>
#include <time.h>
#if HAVE_STDARG_H
#include <stdarg.h>
#include <stdio.h>
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
  vfprintf(stderr, fmt, argp);
  fprintf(stderr, "\n");
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
