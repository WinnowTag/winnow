/* Copyright (c) 2007 The Kaphan Foundation
 *
 * Possession of a copy of this file grants no permission or license
 * to use, modify, or create derivate works.
 *
 * Please contact info@peerworks.org for further information.
 */

#include <config.h>
#include "../src/uuid.h"

#ifdef HAVE_UUID_T
#error "Don't know how to replace uuid_generate when uuid_t is defined"
#endif

void uuid_unparse(uuid_t uuid, char *out) {
  strncpy(out, uuid, UUID_SIZE);
}
