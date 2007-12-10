/* Copyright (c) 2007 The Kaphan Foundation
 *
 * Possession of a copy of this file grants no permission or license
 * to use, modify, or create derivate works.
 *
 * Please contact info@peerworks.org for further information.
 */

#ifndef TAGGING_H_
#define TAGGING_H_

#include "cls_config.h"

typedef struct TAGGING_STORE TaggingStore;

typedef struct TAGGING {
  const char * user;
  const char * tag_name;
  int user_id;
  int tag_id;
  int item_id;
  double strength;
} Tagging;

/** Functions for Tagging stores */
extern TaggingStore * create_db_tagging_store (const DBConfig *config, float insertion_threshold);
extern int            tagging_store_store     (TaggingStore *store, const Tagging *tagging);
extern void           tagging_store_close     (TaggingStore *store);
extern int            tagging_store_is_alive  (TaggingStore *store);
extern void           free_tagging_store      (TaggingStore *store);

/*** Macros for taggings ***/
#define tagging_tag_name(tagging) tagging->tag_name
#define tagging_user(tagging)     tagging->user
#define tagging_strength(tagging) tagging->strength
#define tagging_tag_id(tagging)   tagging->tag_id
#define tagging_user_id(tagging)  tagging->user_id

#endif /*TAGGING_H_*/
