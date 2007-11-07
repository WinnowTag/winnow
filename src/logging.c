// Copyright (c) 2007 The Kaphan Foundation
//
// Possession of a copy of this file grants no permission or license
// to use, modify, or create derivate works.
//
// Please contact info@peerworks.org for further information.

#include "logging.h"

#include <stdio.h>
#if HAVE_STDARG_H
#include <stdarg.h>
#endif

void fatal(const char *fmt, ...) {
  va_list argp;
	fprintf(stderr, "FATAL: ");
	va_start(argp, fmt);
	vfprintf(stderr, fmt, argp);
	va_end(argp);
	fprintf(stderr, "\n");
  exit(1);
}

void error(const char *fmt, ...) {
	va_list argp;
	fprintf(stderr, "ERROR: ");
	va_start(argp, fmt);
	vfprintf(stderr, fmt, argp);
	va_end(argp);
	fprintf(stderr, "\n");
}

void info(const char *fmt, ...) {
	va_list argp;
	fprintf(stderr, "INFO: ");
	va_start(argp, fmt);
	vfprintf(stderr, fmt, argp);
	va_end(argp);
	fprintf(stderr, "\n");
}

void debug(const char *fmt, ...) {
	va_list argp;
	fprintf(stderr, "DEBUG: ");
	va_start(argp, fmt);
	vfprintf(stderr, fmt, argp);
	va_end(argp);
	fprintf(stderr, "\n");
}