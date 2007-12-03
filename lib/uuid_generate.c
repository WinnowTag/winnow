/* Copyright (c) 2007 The Kaphan Foundation
 *
 * Possession of a copy of this file grants no permission or license
 * to use, modify, or create derivate works.
 *
 * Please contact info@peerworks.org for further information.
 */

#include "../src/uuid.h"
#include <config.h>
#include <time.h>
#include <unistd.h>
#include <sys/types.h>

#ifdef HAVE_UUID_T
#error "Don't know how to replace uuid_generate when uuid_t is defined"
#endif

/* This is a very dumb replacement of the uuid library */
void uuid_generate(uuid_t uuid) {
  time_t now = time(0);
  pid_t pid = getpid();
  int random = rand();
  
  short rand_high = (short) random >> 16;
  short rand_low  = (short) random;
  
  snprintf(uuid, UUID_SIZE, "%8X-%4X-%4X-%4X-%12X", random, rand_high, rand_low, (short) pid, now);
}
