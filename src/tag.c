// Copyright (c) 2007 The Kaphan Foundation
//
// Possession of a copy of this file grants no permission or license
// to use, modify, or create derivate works.
//
// Please contact info@peerworks.org for further information.

#include "tag.h"
#include "misc.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "logging.h"
#include "time_helper.h"

#define DEFAULT_BIAS 1.0

/************************************************************************
 *  TagList functions
 ************************************************************************/

TagList * create_tag_list(void) {
  TagList *list = malloc(sizeof(TagList));
  if (list) {
    list->size = 0;
    list->tag_list_allocation = 5
    ;
    list->tags = calloc(list->tag_list_allocation, sizeof(Tag*));
    if (!list->tags) {
      free(list);
      list = NULL;
      fatal("Malloc error in tag list");
    }
  }
  
  return list;
}

void taglist_add_tag(TagList *taglist, Tag *tag) {
  if (taglist && tag) {
    /* Do we need to grow the taglist? */
    if (taglist->size == taglist->tag_list_allocation) {
      Tag **new_list = realloc(taglist->tags, taglist->tag_list_allocation * 2 * sizeof(Tag*));
      if (NULL == new_list) {
        fatal("Out of memory allocating tag list");
      } else {
        taglist->tags = new_list;
        taglist->tag_list_allocation *= 2;
      }
    }
        
    taglist->tags[taglist->size] = tag;
    taglist->size++;
  }
}

/** Adds a tag to a tag list.
 *
 *  If the tag is already in the list, that tag is returned.
 *
 *  This is a private function that should only be called by tag list creators.
 */
Tag *taglist_find_or_add_tag(TagList *taglist, const char * user, const char * tag_name) {
  int i;
  Tag *tag = NULL;
  
  for (i = 0; i < taglist->size; i++) {
    Tag *temp_tag = taglist->tags[i];
    if ((0 == strcmp(temp_tag->tag_name, tag_name)) && (0 == strcmp(temp_tag->user, user))) {
      tag = temp_tag;
      break;
    }
  }
  
  if (NULL == tag) {
    tag = create_tag(user, tag_name, -1, -1);
    if (NULL == tag) {
      fatal("Out of memory allocation a tag");
    } else {
      taglist_add_tag(taglist, tag);    
    }
  }
  
  return tag;
}

void free_taglist(TagList *taglist) { 
  if (taglist) {
    int i;
    
    for (i = 0; i < taglist->size; i++) {
      free_tag(taglist->tags[i]);
    }
    
    free(taglist->tags);
    free(taglist);
  }  
}  

/*****************************************************************************
 * Tag functions
 *****************************************************************************/
 
Tag * create_tag(const char * user, const char * tag_name, int user_id, int tag_id) {
  Tag *tag = malloc(sizeof(Tag));
  if (NULL != tag) {
    tag->user_id = user_id;
    tag->tag_id  = tag_id;
    tag->bias = DEFAULT_BIAS;
    tag->last_classified_at = 0;
    tag->updated_at = 0;
    tag->positive_examples = NULL;
    tag->negative_examples = NULL;
    
    if (NULL != user) {
      int length = strlen(user) + 1;
      tag->user = malloc(sizeof(char) * length);
      if (NULL == tag->user) goto create_tag_malloc_error;
      strncpy(tag->user, user, length);      
    } else {
      tag->user = NULL;
    }
    
    if (NULL != tag_name) {
      int length = strlen(tag_name) + 1;
      tag->tag_name = malloc(sizeof(char) * length);
      if (NULL == tag->tag_name) goto create_tag_malloc_error;
      strncpy(tag->tag_name, tag_name, length);      
    } else {
      tag->tag_name = NULL;
    }
  }
  
  return tag;
  
  create_tag_malloc_error:
    error("Malloc error creating Tag");
    free_tag(tag);
    return NULL;
}

void free_tag(Tag *tag) {
  if (NULL != tag) {
    Word_t judy_bytes_p, judy_bytes_n;
    if (tag->user)     free(tag->user);
    if (tag->tag_name) free(tag->tag_name);
    if (tag->positive_examples) J1FA(judy_bytes_p, tag->positive_examples);
    if (tag->negative_examples) J1FA(judy_bytes_n, tag->negative_examples);
    free(tag);
  }
}

const char * tag_user(const Tag *tag) {
  return tag->user;
}

const char * tag_tag_name(const Tag * tag) {
  return tag->tag_name;
}

int tag_user_id(const Tag * tag) {
  return tag->user_id;
}

int tag_tag_id(const Tag * tag) {
  return tag->tag_id;
}

float tag_bias(const Tag * tag) {
  return tag->bias;
}

time_t tag_last_classified_at(const Tag *tag) {
  return tag->last_classified_at;
}

time_t tag_updated_at(const Tag *tag) {
  return tag->updated_at;
}

int tag_positive_examples_size(const Tag * tag) {
  Word_t count;
  J1C(count, tag->positive_examples, 0, -1);
  return count;
}

int tag_negative_examples_size(const Tag * tag) {
  Word_t count;
  J1C(count, tag->negative_examples, 0, -1);
  return count;
}


static int * fill_example_array(Pvoid_t tag_examples, int size) {
  int * examples = malloc(size * sizeof(int));
  
  if (NULL != examples) {
    int i = 0;
    Word_t example = 0;
    Word_t index_found;
    
    J1F(index_found, tag_examples, example);
    while(index_found && i < size) {
      examples[i++] = example;
      J1N(index_found, tag_examples, example);
    }
  } else {
    error("Error malloc'ing example array");
  }
  
  return examples;
}

/** Returns an array of item ids for positive examples of the tag.
 *
 *  The array must be freed by the caller.
 */
int * tag_positive_examples(const Tag * tag) {
  int * examples = NULL;
  int size = tag_positive_examples_size(tag);
  
  if (0 < size) {
    examples = fill_example_array(tag->positive_examples, size);
  }
  
  return examples;
}

/** Returns an array of item ids for negative examples of the tag.
 *
 *  The array must be freed by the caller.
 */
int * tag_negative_examples(const Tag * tag) {
  int * examples = NULL;
  int size = tag_negative_examples_size(tag);
  
  if (0 < size) {
    examples = fill_example_array(tag->negative_examples, size);
  }
  
  return examples;
}

int tag_add_example(Tag *tag, int example, float strength) {
  int failure = false;
  
  if (strength == 1.0) {
    Word_t j_return;
    J1S(j_return, tag->positive_examples, example);
  } else if (strength == 0.0) {
    Word_t j_return;
    J1S(j_return, tag->negative_examples, example);
  } else {
    error("Unknown strength for %i: %f", example, strength);
    failure = true;
  }
  
  return failure;
}
