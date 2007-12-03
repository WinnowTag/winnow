// Copyright (c) 2007 The Kaphan Foundation
//
// Possession of a copy of this file grants no permission or license
// to use, modify, or create derivate works.
//
// Please contact info@peerworks.org for further information.

#include "logging.h"

#include <stdlib.h>
#include <stdio.h>
#if HAVE_STDARG_H
#include <stdarg.h>
#endif

#define PREFIX(type) type " (%s:%i): "

static FILE *log_file;

void initialize_logging(const char *filename) {
  log_file = fopen(filename, "a");
}

void close_log() {
  if (log_file != stderr) {
    fclose(log_file);
  }
}

void _fatal(const char *file, int line, const char *fmt, ...) {
  va_list argp;
	fprintf(log_file, PREFIX("FATAL"), file, line);
	va_start(argp, fmt);
	vfprintf(log_file, fmt, argp);
	va_end(argp);
	fprintf(log_file, "\n");
  exit(1);
}

void _error(const char *file, int line, const char *fmt, ...) {
	va_list argp;
	fprintf(log_file, PREFIX("ERROR"), file, line);
	va_start(argp, fmt);
	vfprintf(log_file, fmt, argp);
	va_end(argp);
	fprintf(log_file, "\n");
	fflush(log_file);
}

void _info(const char *file, int line, const char *fmt, ...) {
	va_list argp;
	fprintf(log_file, PREFIX("INFO "), file, line);
	va_start(argp, fmt);
	vfprintf(log_file, fmt, argp);
	va_end(argp);
	fprintf(log_file, "\n");
	fflush(log_file);
}

void _debug(const char *file, int line, const char *fmt, ...) {
	va_list argp;
	fprintf(log_file, PREFIX("DEBUG"), file, line);
	va_start(argp, fmt);
	vfprintf(log_file, fmt, argp);
	va_end(argp);
	fprintf(log_file, "\n");
	fflush(log_file);
}

void _trace(const char *file, int line, const char *fmt, ...) {
  va_list argp;
  fprintf(log_file, PREFIX("TRACE"), file, line);
  va_start(argp, fmt);
  vfprintf(log_file, fmt, argp);
  va_end(argp);
  fprintf(log_file, "\n");
  fflush(log_file);
}
