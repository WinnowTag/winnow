// Copyright (c) 2007 The Kaphan Foundation
//
// Possession of a copy of this file grants no permission or license
// to use, modify, or create derivate works.
//
// Please contact info@peerworks.org for further information.

#ifndef TAG_H
#define TAG_H 1

#include <Judy.h>

typedef struct TAGLIST {
  Pvoid_t tags;
} *TagList;

typedef struct TAG {
  char *user;
  char *tag_name;
  Pvoid_t positive_examples;
  Pvoid_t negative_examples;
} *Tag;

// Tag list functions
extern TagList  load_tags_from_file   (const char * corpus, const char * user);
extern int      taglist_size          (TagList taglist);
extern Tag      taglist_get_tag       (TagList taglist, int index);
extern void     free_taglist          (TagList taglist);

// Tag functions
extern void         free_tag                    (Tag tag);
extern char *       tag_get_tag_name            (Tag tag);
extern char *       tag_get_user                (Tag tag);
extern const int *  tag_positive_examples       (Tag tag);
extern const int *  tag_negative_examples       (Tag tag);
extern int          tag_negative_examples_size  (Tag tag);
extern int          tag_positive_examples_size  (Tag tag);
#endif