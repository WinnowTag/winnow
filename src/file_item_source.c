/* Copyright (c) 2007 The Kaphan Foundation
 *
 * Possession of a copy of this file grants no permission or license
 * to use, modify, or create derivate works.
 *
 * Please contact info@peerworks.org for further information.
 */

#include <string.h>
#include "file_item_source.h"
#include "misc.h"
#include "logging.h"

static int read_token_file(Item *item, const char * itempath);
static int build_item_path(const char * corpus, int item, char * buffer, int size);

/** Create a File based ItemSource.
 *
 *  @param corpus The Corpus directory.
 */
ItemSource * create_file_item_source(const char * corpus) {
  ItemSource *is = malloc(sizeof(ItemSource));
  if (NULL != is) {
    is->fetch_func = create_item_from_file;
    is->state = malloc(strlen(corpus) + 1);
    if (NULL == is->state) {
      fatal("Error malloc-ing corpus directory");
    } else {
      memcpy(is->state, corpus, strlen(corpus) + 1);
    }    
  }
  
  return is;
}

/** Create an item from a token file.
 *
 *  This is used by a file based ItemSource. Loads an Item from a
 *  file in the directory. This file should be in the "standard"
 *  tokenized file format.
 *
 *  @param state The directory fo the corpus.
 *  @param item_id The id of the item.
 *  @return The item from the file.
 */
Item * create_item_from_file(const void * state, const int item_id) {
  char * corpus = (char * ) state;
  Item *item;  
  char itempath[MAXPATHLEN];
  int itempath_length;
  
  if (build_item_path(corpus, item_id, itempath, 24)) {    
    return NULL;
  }
  
  item = create_item(item_id);
  if (NULL != item) {
    if (read_token_file(item, itempath)) {
      free_item(item);
      item = NULL;
    }
  } 
  
  return item;
}

/************************************************************************************
 *  "Private" functions for file-based item source.
 ************************************************************************************/

int build_item_path(const char * corpus, int item_id, char * buffer, int length) {
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

int read_token_file(Item *item, const char * itempath) {
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
            if (item_add_token(item, token[0], token[1])) {
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
