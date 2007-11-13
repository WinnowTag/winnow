// Copyright (c) 2007 The Kaphan Foundation
//
// Possession of a copy of this file grants no permission or license
// to use, modify, or create derivate works.
//
// Please contact info@peerworks.org for further information.

#ifndef _CLUE_H_
#define _CLUE_H_

typedef struct CLUE {
  int token_id;
  double probability;
  double strength;
} Clue;

Clue * new_clue  (int token_id, double probability);
void         free_clue (Clue *clue);

#define clue_token_id(clue)        clue->token_id
#define clue_probability(clue)     clue->probability
#define clue_strength(clue)        clue->strength

#endif /* _CLUE_H_ */

