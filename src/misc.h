// Copyright (c) 2007 The Kaphan Foundation
//
// Possession of a copy of this file grants no permission or license
// to use, modify, or create derivate works.
//
// Please contact info@peerworks.org for further information.

#ifndef MISC
#define MISC_H 1

#include <config.h>

#if HAVE_STDBOOL_H
#include <stdbool.h>
#else
#define true 1
#define false 0
#endif

#if HAVE_SYS_PARAM_H
#include <sys/param.h>
#endif

#ifndef MAXPATHLEN
#define MAXPATHLEN 1024
#endif

#if HAVE_ERRNO_H
#include <errno.h>
#endif

#ifndef ERR
#define ERR 1
#endif
#endif
