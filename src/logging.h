// Copyright (c) 2007 The Kaphan Foundation
//
// Possession of a copy of this file grants no permission or license
// to use, modify, or create derivate works.
//
// Please contact info@peerworks.org for further information.

#ifndef _LOGGING_H_
#define _LOGGING_H_

#include <config.h>

void fatal (const char *fmt, ...);
void error (const char *fmt, ...);
void info  (const char *fmt, ...);
void debug (const char *fmt, ...);
void trace (const char *fmt, ...);

#endif