// Copyright (c) 2007 The Kaphan Foundation
//
// Possession of a copy of this file grants no permission or license
// to use, modify, or create derivate works.
//
// Please contact info@peerworks.org for further information.

#ifndef _CLUE_H_
#define _CLUE_H_

#include <Judy.h>

typedef struct CLUE {
  int token_id;
  double probability;
  double strength;
} Clue;

typedef struct CLUE_LIST {
  int size;
  Pvoid_t list;
} ClueList;

Clue * new_clue  (int token_id, double probability);
void         free_clue (Clue *clue);

ClueList * new_clue_list();
Clue *     add_clue(ClueList * clues, int token_id, double probability);
Clue *     get_clue(ClueList * clues, int token_id);
void free_clue_list(ClueList * clues);

#define clue_token_id(clue)        clue->token_id
#define clue_probability(clue)     clue->probability
#define clue_strength(clue)        clue->strength

#endif /* _CLUE_H_ */

