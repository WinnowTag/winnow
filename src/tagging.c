/* Copyright (c) 2007 The Kaphan Foundation
 *
 * Possession of a copy of this file grants no permission or license
 * to use, modify, or create derivate works.
 *
 * Please contact info@peerworks.org for further information.
 */

#include "tagger.h"
#include "logging.h"

Tagging * create_tagging(const char * item_id, double strength) {
  Tagging *tagging = malloc(sizeof (struct TAGGING));

  if (tagging) {
    tagging->item_id = item_id;
    tagging->strength = strength;
  } else {
    fatal("Could not malloc Tagging");
  }

  return tagging;
}
