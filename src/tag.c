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

static int build_tagging_path(const char *, const char *, char *, int);
static int read_tagging_file(TagList taglist, const char *, const char *);
static Tag add_tag(TagList, const char *, const char *);
static const int * fill_example_array(Pvoid_t tag_examples, int size);
static int tag_add_example(Tag tag, int example, float strength);


TagList load_tags_from_file(const char * corpus, const char * user) {
  TagList taglist;
  char tagging_path[MAXPATHLEN];
  
  if (build_tagging_path(corpus, user, tagging_path, MAXPATHLEN)) {
    return NULL;
  }
  
  taglist = malloc(sizeof(struct TAGLIST));
  if (NULL != taglist) {
    taglist->tags = NULL;
    
    if (read_tagging_file(taglist, user, tagging_path)) {
      free_taglist(taglist);
      taglist = NULL;
    }    
  }
  
  return taglist;
}

Tag taglist_get_tag(TagList taglist, int index) {
  Tag tag = NULL;
  PWord_t tag_pointer;
  Word_t J_index;
  
  JLBC(tag_pointer, taglist->tags, index, J_index);
  if (NULL != tag_pointer) {
    tag = (Tag)(*tag_pointer);
  }
  
  return tag;
}

int taglist_size(TagList taglist) {
  Word_t count;
  JLC(count, taglist->tags, 0, -1);
  return count;
}

void free_taglist(TagList taglist) {
  Word_t bytes_freed;
  PWord_t tag_pointer;
  Word_t index;
  
  JLF(tag_pointer, taglist->tags, index);
  while (NULL != tag_pointer) {
    Tag tag = (Tag)(*tag_pointer);
    free_tag(tag);
    JLN(tag_pointer, taglist->tags, index);
  }
  
  JLFA(bytes_freed, taglist->tags);
  free(taglist);
}  

/*****************************************
 * Tag functions
 */
 
Tag create_tag(const char * user, const char * tag_name) {
  Tag tag = malloc(sizeof(struct TAG));
  if (NULL != tag) {
    tag->positive_examples = NULL;
    tag->negative_examples = NULL;
    
    int length = strlen(user) + 1;
    tag->user = malloc(sizeof(char) * length);
    if (NULL == tag->user) goto create_tag_malloc_error;
    strncpy(tag->user, user, length);
    
    length = strlen(tag_name) + 1;
    tag->tag_name = malloc(sizeof(char) * length);
    if (NULL == tag->tag_name) goto create_tag_malloc_error;
    strncpy(tag->tag_name, tag_name, length);
  }
  
  return tag;
  
  create_tag_malloc_error:
    error("Malloc error creating Tag");
    free_tag(tag);
    return NULL;
}

void free_tag(Tag tag) {
  if (tag) {
    if (tag->user)     free(tag->user);
    if (tag->tag_name) free(tag->tag_name);
    free(tag);
  }
}

char * tag_get_user(Tag tag) {
  return tag->user;
}

char * tag_get_tag_name(Tag tag) {
  return tag->tag_name;
}

int tag_positive_examples_size(Tag tag) {
  Word_t count;
  J1C(count, tag->positive_examples, 0, -1);
  return count;
}

int tag_negative_examples_size(Tag tag) {
  Word_t count;
  J1C(count, tag->negative_examples, 0, -1);
  return count;
}

/** Returns an array of item ids for positive examples of the tag.
 *
 *  The array must be freed by the caller.
 */
const int * tag_positive_examples(Tag tag) {
  const int * examples = NULL;
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
const int * tag_negative_examples(Tag tag) {
  const int * examples = NULL;
  int size = tag_negative_examples_size(tag);
  
  if (0 < size) {
    examples = fill_example_array(tag->negative_examples, size);
  }
  
  return examples;
}

const int * fill_example_array(Pvoid_t tag_examples, int size) {
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

int tag_add_example(Tag tag, int example, float strength) {
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

/*****************************************
 * Private functions
 */
int build_tagging_path(const char * corpus, const char * user, char * buffer, int length) {
  int return_code = 0;

  if ((strlcpy(buffer, corpus, length) >= length)
     || (strlcat(buffer, "/", length) >= length)
     || (strlcat(buffer, user, length) >= length)
     || (strlcat(buffer, "-taggings.csv", length) >= length)) {
    error("tagging path too long: %s", buffer);
    return_code = ERR;
  } 

  return return_code;
}

int read_tagging_file(TagList taglist, const char * user, const char * filename) {
  int failure = false;
  FILE *file;
  
  file = fopen(filename, "r");
  if (NULL == file) {
    error("Could not open file '%s':%s", filename, strerror(errno));
    failure = true;    
  } else {
    int scan_result;
    char tag_name[256];
    int item_id;
    float strength;
     
    while (EOF != fscanf(file, "%255[^,],%d,%f\n", tag_name, &item_id, &strength)) {
      Tag tag = add_tag(taglist, user, tag_name);
      if (NULL == tag) {
        failure = true;
        break;
      } else if (tag_add_example(tag, item_id, strength)) {
        failure = true;
        break;
      }
    }
    
    fclose(file);
  }
  
  return failure;
}

Tag add_tag(TagList taglist, const char * user, const char * tag_name) {
  Tag tag = NULL;
  Word_t index;
  PWord_t tag_pointer;
  
  JLF(tag_pointer, taglist->tags, index);
  while (NULL != tag_pointer) {
    Tag temp_tag = (Tag)*tag_pointer;
    if ((0 == strcmp(temp_tag->tag_name, tag_name)) && (0 == strcmp(temp_tag->user, user))) {
      tag = temp_tag;
      break;
    } 
    
    JLN(tag_pointer, taglist->tags, index);
  }
  
  if (NULL == tag) {
    tag = create_tag(user, tag_name);
    
    if (NULL == tag) {
      error("Out of memory allocation a tag");
    } else {
      Word_t count;
      JLC(count, taglist->tags, 0, -1);
      JLI(tag_pointer, taglist->tags, count + 1);
      if (NULL == tag_pointer) {
        error("Out of memory on Judy allocation");
        free(tag);
        tag = NULL;
      } else {        
        *tag_pointer = (Word_t)tag;
      }
    }
  }
  
  return tag;
}