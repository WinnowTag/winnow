// Copyright (c) 2007 The Kaphan Foundation
//
// Possession of a copy of this file grants no permission or license
// to use, modify, or create derivate works.
//
// Please contact info@peerworks.org for further information.

#include "item.h"

#include <stdio.h>
#if HAVE_SYS_PARAM_H
#include <sys/param.h>
#endif
#if HAVE_ERRNO_H
#include <errno.h>
#endif
#if HAVE_STDARG_H
#include <stdarg.h>
#endif
#if HAVE_STRING_H
#include <string.h>
#endif

#ifndef MAXPATHLEN
#define MAXPATHLEN 1024
#endif

/* Some prototypes */
void error(const char * format, ...);
int item_set_path(Item item, const char * itempath);

static const int ERR = 1;

Item create_item(int id) {
  Item item;
  
  item = malloc(sizeof(Item));
  if (NULL != item) {      
    int error = 0;
    int i;
    item->id = id;
    item->total_tokens = 0;
    item->path = NULL;
    item->tokens = NULL;
  }
  
  return item;
}

Item create_item_with_tokens(int id, int tokens[][2], int num_tokens) {
  Item item;
  
  item = create_item(id);
  if (NULL != item) {  
    if (load_tokens_from_array(item, tokens, num_tokens)) {
      free_item(item);
      item = NULL;
    }
  }
  
  return item;
}

Item create_item_from_file(char * corpus, int item_id) {
  Item item;  
  char itempath[MAXPATHLEN];
  int itempath_length;
  
  if (build_item_path(corpus, item_id, itempath, 24)) {    
    return NULL;
  }
  
  item = create_item(item_id);
  if (NULL != item) {    
    if (item_set_path(item, itempath)) {
      free_item(item);
      item = NULL;
    }
    
    if (read_token_file(item, itempath)) {
      free_item(item);
      item = NULL;
    }
  } 
  
  return item;
}

int item_get_id(Item item) {
  return item->id;
}

int item_get_num_tokens(Item item) {
  Word_t count;
  JLC(count, item->tokens, 0, -1);
  return (int) count;
}

int item_get_total_tokens(Item item) {
  return item->total_tokens;
}

int item_get_token(Item item, int token_id, Token_p token) {
  Word_t * frequency;
  JLG(frequency, item->tokens, token_id);
  
  if (NULL == frequency) {
    token->id = 0;
    token->frequency = 0;
  } else {
    token->id = token_id;
    token->frequency = *frequency;
  }
  
  return 1;
}

char * item_get_path(Item item) {
  return item->path;
}

int item_set_path(Item item, const char * itempath) {  
  int return_code = 0;
  int itempath_length;
  
  itempath_length = sizeof(char) * strlen(itempath) + 1;
  item->path = malloc(itempath_length);
  
  if (NULL == item->path) {
    error("Couldn't malloc item->path");
    return_code = ERR;
  } else if (strlcpy(item->path, itempath, itempath_length) >= itempath_length) {
    error("item->path was too short (%d) for '%s'", itempath_length, itempath);
    return_code = ERR;
  }
  
  return return_code;
}

void free_item(Item item) {
  free(item->path);
  return free(item);
}

int build_item_path(char * corpus, int item_id, char * buffer, int length) {
  int return_code = 0;
    
  if ((strlcpy(buffer, corpus, length) >= length)
      || (strlcat(buffer, "/", length) >= length)) {
    return_code = ERR;
  } else {
    int path_length;
    int copied_chars;
    
    path_length = strlen(buffer);  
    copied_chars = snprintf(&buffer[path_length], length - path_length, "%d.tokens", item_id);
    
    if (copied_chars >= length - path_length) {
      return_code = ERR;
    }
  }

  if (ERR == return_code) {    
    error("item path too long: '%s'", buffer);
  }
  
  return return_code;
}

int read_token_file(Item item, const char * itempath) {
  int return_code = 0;
  FILE *token_file;  

  if (NULL == (token_file = fopen(itempath, "r"))) {
    error("Could not open file '%s': %s", itempath, strerror(errno));
    return_code = ERR;
  } else {    
    // First there should be an 'A' character to indicate it is an atomized file
    if ('A' != getc(token_file)) {
      error("Token file is not an atomized file");
      return_code = ERR;
    } else {    
      int count = getw(token_file);
      if (count == EOF) {
        error("Token file corrupt");
        return_code = ERR;
      } else {
        int token[2];
        
        while (1) {
          int items_read;
          items_read = fread(token, 4, 2, token_file);
          if (2 == items_read) {
            item->total_tokens += token[1];
            if (JudyInsert(item, token[0], token[1])) {
              break;
            }
          } else {
            if (1 == items_read) {
              error("odd number of token - frequency pairs, skipping last token");
            }
            break;
          } 
        }
      }
    }
    
    if (fclose(token_file)) {
      error("Error closing token file: '%s'", strerror(errno));
    }
  }
    
  return return_code;
}

int load_tokens_from_array(Item item, int tokens[][2], int num_tokens) {
  int i;
  int return_code = 0;
  for (i = 0; i < num_tokens; i++) {
    int token_id = tokens[i][0];
    int token_frequency = tokens[i][1];      
    item->total_tokens += token_frequency;
    
    if (JudyInsert(item, token_id, token_frequency)) {
      return_code = 1;
      break;
    }      
  }
  
  return return_code;
}

int JudyInsert(Item item, int id, int token_frequency) {
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

void error(const char *fmt, ...) {
	va_list argp;
	fprintf(stderr, "error: ");
	va_start(argp, fmt);
	vfprintf(stderr, fmt, argp);
	va_end(argp);
	fprintf(stderr, "\n");
}