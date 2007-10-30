// Copyright (c) 2007 The Kaphan Foundation
//
// Possession of a copy of this file grants no permission or license
// to use, modify, or create derivate works.
//
// Please contact info@peerworks.org for further information.

#include "pool.h"
#include "misc.h"

Pool new_pool(void) {
  Pool pool = malloc(sizeof(Pool));
  if (NULL != pool) {
    pool->total_tokens = 0;
    pool->tokens = NULL;
  }
  return pool;
}

void free_pool(Pool pool) {
  if (NULL != pool) {
    Word_t bytes_freed;
    JLFA(bytes_freed, pool->tokens);
    free(pool);
  }  
}

/** Not Re-entrant */
int pool_add_item(Pool pool, Item item) {
  int success = true;
  Token token;
  token.id = 0;
  
  while (item_next_token(item, &token)) {
    PWord_t pool_frequency;
    JLG(pool_frequency, pool->tokens, token.id);
    
    if (NULL == pool_frequency) {
      JLI(pool_frequency, pool->tokens, token.id);
      if (PJERR == pool_frequency) goto malloc_error;
      *pool_frequency = token.frequency;
    } else {
      *pool_frequency = *pool_frequency + token.frequency;
    }
    
    pool->total_tokens += token.frequency;
  }
  
  return success;
  malloc_error:
    error("Error allocating memory for Judy Array");
    return false;    
}

int pool_num_tokens(Pool pool) {
  int num_tokens = 0;
  if (NULL != pool) {
    JLC(num_tokens, pool->tokens, 0, -1);
  }
  return num_tokens;
}

int pool_total_tokens(Pool pool) {
  int total_tokens = 0;
  if (NULL != pool) {
    total_tokens = pool->total_tokens;
  }
  return total_tokens;
}

int pool_token_frequency(Pool pool, int token_id) {
  int frequency = 0;
  PWord_t frequency_p;
  JLG(frequency_p, pool->tokens, token_id);
  
  if (NULL != frequency_p) {
    frequency = *frequency_p;
  }
  
  return frequency;
}

int pool_next_token(Pool pool, Token_p token) {
  int success = true;
  PWord_t frequency = NULL;
  Word_t token_id = token->id;
  
  if (NULL != pool) {
    if (0 == token_id) {
      JLF(frequency, pool->tokens, token_id);
    } else {
      JLN(frequency, pool->tokens, token_id);
    }
  }
    
  if (NULL == frequency) {
    success = false;
    token->id = 0;
    token->frequency = 0;
  } else {
    token->id = token_id;
    token->frequency = *frequency;
  }
  
  return success;
}