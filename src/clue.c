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

ClueList * new_clue_list(void) {
  ClueList *clues = calloc(1, sizeof(struct CLUE_LIST));
  if (NULL == clues) {
    fatal("Could not allocate clue list");
  } else {
    clues->size = 0;
    clues->list = NULL;
  }
  
  return clues;
}

Clue * add_clue(ClueList * clues, int token_id, double probability) {
  Clue * clue = NULL;
  
  if (clues) {
    // Check if it exists
    clue = get_clue(clues, token_id);
    if (clue == NULL) {
      clue = new_clue(token_id, probability);
      if (NULL != clue) {
        PWord_t clue_p;
        JLI(clue_p, clues->list, token_id);
        if (NULL == clue_p) {
          fatal("Could not allocate spot in clue list");
        } else {
          *clue_p = (Word_t) clue;
          clues->size++;    
        }
      } else {
        fatal("Could not allocate clue");
      }    
    }
  }
  
  return clue;
}

Clue * get_clue(const ClueList * clues, int token_id) {
  Clue * clue = NULL;
  
  if (clues) {
    PWord_t clue_pointer;
    JLG(clue_pointer, clues->list, token_id);
    if (NULL != clue_pointer) {
      clue = (Clue*)(*clue_pointer);
    }    
  }
  
  return clue;
}

void free_clue_list(ClueList * clues) {
  if (clues) {
    int size;
    int bytes;
    PWord_t clue_pointer;
    Word_t index = 0;
    
    JLF(clue_pointer, clues->list, index);
    while (clue_pointer) {
      free((Clue*)(*clue_pointer));
      JLN(clue_pointer, clues->list, index);
    }
    
    JLC(size, clues->list, 0, -1);
    JLFA(bytes, clues->list);
    free(clues);
    info("Freed %i bytes from clue list of %i clues", bytes + (size * sizeof(struct CLUE)) + sizeof(ClueList), size);
  }  
}
