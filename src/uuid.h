/* Copyright (c) 2007 The Kaphan Foundation
 *
 * Possession of a copy of this file grants no permission or license
 * to use, modify, or create derivate works.
 *
 * Please contact info@peerworks.org for further information.
 */
#ifndef UUID_H_
#define UUID_H_

#include <config.h>
#define UUID_SIZE 36

#ifdef HAVE_UUID_UUID_H
#include <uuid/uuid.h>
#endif

#if !HAVE_UUID_T
typedef char uuid_t[UUID_SIZE];
#endif

#if !HAVE_UUID_GENERATE
extern void uuid_generate(uuid_t out);
#endif

#if !HAVE_UUID_UNPARSE
extern void uuid_unparse(uuid_t uuid, char *out);
#endif

#endif /*UUID_H_*/
