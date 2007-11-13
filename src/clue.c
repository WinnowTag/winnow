// Copyright (c) 2007 The Kaphan Foundation
//
// Possession of a copy of this file grants no permission or license
// to use, modify, or create derivate works.
//
// Please contact info@peerworks.org for further information.

#include <stdlib.h>
#include <math.h>
#include "clue.h"
#include "logging.h"

Clue * new_clue(int token_id, double probability) {
  Clue *clue = malloc(sizeof(struct CLUE));
  if (NULL != clue) {
    clue->token_id = token_id;
    clue->probability = probability;
    clue->strength = fabs(0.5 - probability);
  }
  return clue;
}

void free_clue(Clue *clue) {
  free(clue);
}
