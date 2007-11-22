// Copyright (c) 2007 The Kaphan Foundation
//
// Possession of a copy of this file grants no permission or license
// to use, modify, or create derivate works.
//
// Please contact info@peerworks.org for further information.

#ifndef TAG_H
#define TAG_H 1

#include <Judy.h>
#include "cls_config.h"


typedef struct TAG {
  char *user;
  char *tag_name;
  int user_id;
  int tag_id;
  float bias;
  Pvoid_t positive_examples;
  Pvoid_t negative_examples;
} Tag;

typedef struct TAGLIST {
  int size;
  int tag_list_allocation;
  Tag **tags;
} TagList;

typedef struct TAG_DB TagDB;

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

// TagDB functions
extern TagDB   * create_tag_db          (DBConfig *db_config);
extern TagList * tag_db_load_tags_to_classify_for_user(TagDB *tag_db, int user_id);
extern Tag     * tag_db_load_tag_by_id  (TagDB *tag_db, int id);
extern void      free_tag_db            (TagDB *tag_db);

#endif
