// Copyright (c) 2007 The Kaphan Foundation
//
// Possession of a copy of this file grants no permission or license
// to use, modify, or create derivate works.
//
// Please contact info@peerworks.org for further information.

#ifndef TAG_H
#define TAG_H 1

#include <Judy.h>
#include <time.h>
#include "cls_config.h"


typedef struct TAG {
  char *user;
  char *tag_name;
  int user_id;
  int tag_id;
  float bias;
  time_t last_classified_at;
  time_t updated_at;
  Pvoid_t positive_examples;
  Pvoid_t negative_examples;
} Tag;

typedef struct TAGLIST {
  int size;
  int tag_list_allocation;
  Tag **tags;
} TagList;

// Tag list functions
extern TagList    *  create_tag_list(void);
extern void          taglist_add_tag(TagList *taglist, Tag *tag);
extern TagList    *  load_tags_from_file   (const char * corpus, const char * user);
extern void          free_taglist          (TagList *taglist);

// Tag functions
extern Tag *        create_tag                  (const char *user, const char *tag_name, int user_id, int tag_id);
extern void         free_tag                    (Tag *tag);
extern const char * tag_tag_name                (const Tag *tag);
extern int          tag_tag_id                  (const Tag *tag);
extern const char * tag_user                    (const Tag *tag);
extern int          tag_user_id                 (const Tag *tag);
extern float        tag_bias                    (const Tag *tag);
extern int *        tag_positive_examples       (const Tag *tag);
extern int *        tag_negative_examples       (const Tag *tag);
extern int          tag_negative_examples_size  (const Tag *tag);
extern int          tag_positive_examples_size  (const Tag *tag);
extern time_t       tag_last_classified_at      (const Tag *tag);
extern time_t       tag_updated_at      (const Tag *tag);

#endif
