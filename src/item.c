// Copyright (c) 2007 The Kaphan Foundation
//
// Possession of a copy of this file grants no permission or license
// to use, modify, or create derivate works.
//
// Please contact info@peerworks.org for further information.

#include "item.h"
#include "misc.h"
#include "logging.h"

#include <stdio.h>

#if HAVE_STRING_H
#include <string.h>
#endif

static int load_tokens_from_array(Item *item, int tokens[][2], int num_tokens);

/******************************************************************************
 * Item creation functions 
 ******************************************************************************/
 
/** Create a empty item.
 *
 */
Item * create_item(int id) {
  Item *item;
  
  item = malloc(sizeof(Item));
  if (NULL != item) {
    item->id = id;
    item->total_tokens = 0;
    item->time = 0;
    item->tokens = NULL;
  } else {
    fatal("Malloc Error allocating item %d", id);
  }
  
  return item;
}

/** Create an item from an array of tokens.
 *
 *  @param id The id of the item.
 *  @param tokens A 2D array of tokens where each row is {token_id, frequency}.
 *  @param num_tokens The length of the token array.
 *  @returns A new item initialized with the tokens.
 */
Item * create_item_with_tokens(int id, int tokens[][2], int num_tokens) {
  Item *item = create_item(id);
  if (NULL != item) {  
    if (load_tokens_from_array(item, tokens, num_tokens)) {
      free_item(item);
      item = NULL;
    }
  }
  
  return item;
}

/***************************************************************************
 * Item functions 
 ***************************************************************************/

int item_get_id(const Item * item) {
  return item->id;
}

time_t item_get_time(const Item * item) {
  return item->time;
}

int item_get_num_tokens(const Item * item) {
  Word_t count;
  JLC(count, item->tokens, 0, -1);
  return (int) count;
}

int item_get_total_tokens(const Item * item) {
  return item->total_tokens;
}

int item_get_token(const Item * item, int token_id, Token_p token) {
  Word_t * frequency;
  JLG(frequency, item->tokens, token_id);
  
  token->id = token_id;
  if (NULL == frequency) {
    token->frequency = 0;
  } else {
    token->frequency = *frequency;
  }
  
  return 0;
}

int item_next_token(const Item * item, Token_p token) {
  int success = true;  
  PWord_t frequency = NULL;
  Word_t  token_id = token->id;
  
  if (NULL != item) {
    if (0 == token_id) {    
      JLF(frequency, item->tokens, token_id);
    } else {
      JLN(frequency, item->tokens, token_id);
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

void free_item(Item *item) {
  if (NULL != item) {
    int freed_bytes;
    JLFA(freed_bytes, item->tokens);
    free(item);    
  }
}

int load_tokens_from_array(Item *item, int tokens[][2], int num_tokens) {
  int i;
  int return_code = 0;
  for (i = 0; i < num_tokens; i++) {
    int token_id = tokens[i][0];
    int token_frequency = tokens[i][1];      
    item->total_tokens += token_frequency;
    
    if (item_add_token(item, token_id, token_frequency)) {
      return_code = 1;
      break;
    }      
  }
  
  return return_code;
}

int item_add_token(Item *item, int id, int token_frequency) {
  int return_code = 0;  
  Word_t token_id;
  Word_t * token_frequency_p;
  
  token_id = (Word_t) id;
  
  JLI(token_frequency_p, item->tokens, token_id);
  if (PJERR == token_frequency_p) {
    error("Could not malloc memory for token array");
    return_code = ERR;
  } else {
    *token_frequency_p = token_frequency;
  }
  
  return return_code;
}
