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

static int build_tagging_path(const char *, const char *, char *, int);
static int read_tagging_file(TagList* taglist, const char *, const char *);
static Tag * add_tag(TagList*, const char *, const char *);
static int * fill_example_array(Pvoid_t tag_examples, int size);
static int tag_add_example(Tag *tag, int example, float strength);
static TagList * create_tag_list(void);

/** Load tags from a file
 *
 *  Format: tag,item_id,strength
 */
TagList * load_tags_from_file(const char * corpus, const char * user) {
  TagList *taglist;
  char tagging_path[MAXPATHLEN];
  
  if (build_tagging_path(corpus, user, tagging_path, MAXPATHLEN)) {
    return NULL;
  }
  
  taglist = create_tag_list();
  if (NULL != taglist) {    
    if (read_tagging_file(taglist, user, tagging_path)) {
      free_taglist(taglist);
      taglist = NULL;
    }    
  }
  
  return taglist;
}

/************************************************************************
 *  TagList functions
 ************************************************************************/

TagList * create_tag_list(void) {
  TagList *list = malloc(sizeof(TagList));
  if (list) {
    list->size = 0;
    list->tag_list_allocation = 7;
    list->tags = calloc(list->tag_list_allocation, sizeof(TagList*));
    if (!list->tags) {
      free(list);
      list = NULL;
      fatal("Malloc error in tag list");
    }
  }
  
  return list;
}

/** Adds a tag to a tag list.
 *
 *  If the tag is already in the list, that tag is returned.
 *
 *  This is a private function that should only be called by tag list creators.
 */
Tag *add_tag(TagList *taglist, const char * user, const char * tag_name) {
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
    /* Do we need to grow the taglist? */
    if (taglist->size == taglist->tag_list_allocation) {
      Tag **new_list = realloc(taglist->tags, taglist->tag_list_allocation * 2);
      if (NULL == new_list) {
        fatal("Out of memory allocating tag list");
      } else {
        taglist->tags = new_list;
        taglist->tag_list_allocation *= 2;
      }
    }
    
    tag = create_tag(user, tag_name, -1, -1);
    
    if (NULL == tag) {
      fatal("Out of memory allocation a tag");
    } else {
      taglist->tags[taglist->size] = tag;
      taglist->size++;
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

int * fill_example_array(Pvoid_t tag_examples, int size) {
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

/*****************************************************************************
 * Private functions for reading tag definitions from a file.
 *****************************************************************************/

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

int read_tagging_file(TagList *taglist, const char * user, const char * filename) {
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
      Tag *tag = add_tag(taglist, user, tag_name);
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

/***********************************************************************************
 *  MySQL based tag source functions 
 ***********************************************************************************/
#include <mysql.h>
#include <errmsg.h>

#define FIND_TAG_STMT "select user_id, name from tags where id = ?" 
#define LOAD_EXAMPLES "select feed_item_id, strength from taggings where tag_id = ?"
#define UPDATE_LAST_CLASSIFIED_AT "update tags set last_classified_at = UTC_TIMESTAMP() where id = ?"

struct TAG_DB {
  const DBConfig *config;
  MYSQL *mysql;
  MYSQL_STMT *find_tag_stmt;
  MYSQL_STMT *load_examples_stmt;
  MYSQL_STMT *update_last_classified_at_stmt;
};

static int establish_connection(TagDB *tag_db);

/** Creates a TagDB for a given DB Config.
 * 
 */ 
TagDB * create_tag_db(DBConfig * config) {
  info("Creating tag db source (host='%s',user='%s',pass='%s',db='%s')", 
      config->host, config->user, config->password, config->database);
  TagDB *tag_db = malloc(sizeof(TagDB));
  if (NULL != tag_db) {
    tag_db->config = config;
    tag_db->mysql = mysql_init(NULL);
    
    if (NULL == tag_db->mysql) {
      free_tag_db(tag_db);
      tag_db = NULL;
      fatal("Error malloc'ing MYSQL for Tag DB");
    } else if (!establish_connection(tag_db)) {
      free_tag_db(tag_db);
      tag_db = NULL;
      fatal("Error establishing connection on Tag DB creation");
    }    
  } else {
    fatal("Error malloc'ing TagDB");
  }
  
  return tag_db; 
}

void free_tag_db(TagDB *tag_db) {
  if (tag_db) {
    if (tag_db->find_tag_stmt) mysql_stmt_close(tag_db->find_tag_stmt);
    if (tag_db->load_examples_stmt) mysql_stmt_close(tag_db->load_examples_stmt);
    if (tag_db->update_last_classified_at_stmt) mysql_stmt_close(tag_db->update_last_classified_at_stmt);
    if (tag_db->mysql) mysql_close(tag_db->mysql);
    free(tag_db);
  }
}

/** Checks if the Tag DB is alive. If it isn't a reconnect will be attempted.
 * 
 *  @return true if the TagDB is alive.
 */ 
int tag_db_is_alive(TagDB *tag_db) {
  int alive = true; 

  if (NULL != tag_db) {
    if (mysql_ping(tag_db->mysql)) {
      error("MySQL server is down %s. Attempting reconnect.", mysql_error(tag_db->mysql));
      if (!establish_connection(tag_db)) {
        error("Could not reconnect: %s. TagDB will be disabled.", mysql_error(tag_db->mysql));
        alive = false;
      }
    }
  }
  
  return alive;
}

TagList * tag_db_load_tags_to_classify_for_user(TagDB *tag_db, int user_id) {
  return NULL;
}

/** Loads a tag from the database.
 *  
 *  @param tag_db The tag database to load from.
 *  @return The tag or NULL if it could not be found.
 */
Tag * tag_db_load_tag_by_id(TagDB *tag_db, int tag_id) {
  Tag *tag = NULL;
  if (tag_db && tag_db_is_alive(tag_db)) {
    MYSQL_BIND param[1];
    memset(param, 0, sizeof(param));
    param[0].buffer_type = MYSQL_TYPE_LONG;
    param[0].buffer = (char *) &tag_id;
    
    if (mysql_stmt_bind_param(tag_db->find_tag_stmt, param)) goto tag_query_error;
    if (mysql_stmt_execute(tag_db->find_tag_stmt))          goto tag_query_error;
    
    char tag_name[256];
    unsigned long tag_name_length = 0;
    memset(tag_name, 0, sizeof(tag_name));
    unsigned long user_id;
    MYSQL_BIND results[2];
    memset(results, 0, sizeof(results));
    results[0].buffer_type = MYSQL_TYPE_LONG;
    results[0].buffer = (char *) &user_id;
    results[1].buffer_type = MYSQL_TYPE_VAR_STRING;
    results[1].buffer = (char *) tag_name;
    results[1].buffer_length = 256;
    results[1].length = &tag_name_length;
   
    if (mysql_stmt_bind_result(tag_db->find_tag_stmt, results)) goto tag_query_error;
    if (mysql_stmt_store_result(tag_db->find_tag_stmt))         goto tag_query_error;
    int stmt_fetch_result = mysql_stmt_fetch(tag_db->find_tag_stmt);
    
    if (!stmt_fetch_result) {
      tag = create_tag("", tag_name, user_id, tag_id);
      if (tag) {
        if (mysql_stmt_bind_param(tag_db->load_examples_stmt, param)) goto load_example_error;
        if (mysql_stmt_execute(tag_db->load_examples_stmt))           goto load_example_error;
        
        int item_id;
        float strength;
        MYSQL_BIND examples[2];
        memset(examples, 0, sizeof(examples));
        examples[0].buffer_type = MYSQL_TYPE_LONG;
        examples[0].buffer = (char *) &item_id;
        examples[1].buffer_type = MYSQL_TYPE_FLOAT;
        examples[1].buffer = (char *) &strength;
        
        if (mysql_stmt_bind_result(tag_db->load_examples_stmt, examples)) goto load_example_error;
        if (mysql_stmt_store_result(tag_db->load_examples_stmt))          goto load_example_error;
        
        while (!mysql_stmt_fetch(tag_db->load_examples_stmt)) {
          tag_add_example(tag, item_id, strength);
        }
      }
    } else if (MYSQL_NO_DATA == stmt_fetch_result) {
      error("No tag found with id %i", tag_id);
    } else if (MYSQL_DATA_TRUNCATED == stmt_fetch_result) {
      error("data was truncated from %i for tag = %i", tag_name_length, tag_id);
    } else {
      error("tag fetching error %i for %i", stmt_fetch_result, tag_id);
      goto tag_query_error;
    }
  }
  
  exit:
    if (tag_db) {
      mysql_stmt_free_result(tag_db->load_examples_stmt);
      mysql_stmt_free_result(tag_db->find_tag_stmt);
    }
    
    return tag;
  tag_query_error:
    if (mysql_stmt_errno(tag_db->find_tag_stmt) == CR_OUT_OF_MEMORY) {
      fatal("Out of memory error fetching tag: %s", mysql_stmt_error(tag_db->find_tag_stmt));
    } else {
      error("Error fetching tag: %s", mysql_stmt_error(tag_db->find_tag_stmt));
    }
    if (tag) {
      free_tag(tag);
    }
    tag = NULL;
    goto exit;
    
  load_example_error:
    if (mysql_stmt_errno(tag_db->load_examples_stmt) == CR_OUT_OF_MEMORY) {
      fatal("Out of memory error fetching tag: %s", mysql_stmt_error(tag_db->load_examples_stmt));
    } else {
      error("Error loading examples for tag %i: %s", tag_id, mysql_stmt_error(tag_db->load_examples_stmt));
    }
    if (tag) {
      free_tag(tag);
    }
    tag = NULL;
    goto exit;
}

int tag_db_update_last_classified_at(TagDB *tag_db, const Tag *tag) {
  int failed = false;
  if (tag_db && tag_db_is_alive(tag_db)) {
    MYSQL_BIND param[1];
    memset(param, 0, sizeof(param));
    param[0].buffer_type = MYSQL_TYPE_LONG;
    param[0].buffer = (char*) &(tag->tag_id);
    
    if (mysql_stmt_bind_param(tag_db->update_last_classified_at_stmt, param)) {
      error("Error binding parameters: %s", mysql_stmt_error(tag_db->update_last_classified_at_stmt));
      failed = true;
    }
    
    if (mysql_stmt_execute(tag_db->update_last_classified_at_stmt)) {
      error("Error executing last classified update statement: %s", mysql_stmt_error(tag_db->update_last_classified_at_stmt));
      failed = true;
    }
  }
  
  return failed;
}

/** Establishes the MySQL connection and prepares the statements needs to load a tag.
 *
 *  @returns true on successful connection establishment, false otherwise.
 */ 
int establish_connection(TagDB *tag_db) {
  int success = true;
  const DBConfig *config = tag_db->config;
  if (!mysql_real_connect(tag_db->mysql, config->host, config->user, 
                   config->password, config->database, config->port, NULL, 0)) {
    error("Failed to connect to tag database : %s", mysql_error(tag_db->mysql));
    success = false;
  } else {
    tag_db->find_tag_stmt = mysql_stmt_init(tag_db->mysql);
    tag_db->load_examples_stmt = mysql_stmt_init(tag_db->mysql);
    tag_db->update_last_classified_at_stmt = mysql_stmt_init(tag_db->mysql);

    if (NULL == tag_db->find_tag_stmt || NULL == tag_db->load_examples_stmt || NULL == tag_db->update_last_classified_at_stmt) {
      error("Error creating prepared statement: %s", mysql_error(tag_db->mysql));
      success = false;
    } else if (mysql_stmt_prepare(tag_db->find_tag_stmt, FIND_TAG_STMT, strlen(FIND_TAG_STMT))) {
      error("Error preparing hard coded statement: %s : %s", FIND_TAG_STMT, mysql_error(tag_db->mysql));
      success = false;
    } else if (mysql_stmt_prepare(tag_db->load_examples_stmt, LOAD_EXAMPLES, strlen(LOAD_EXAMPLES))) {
      error("Error preparing hard coded statement: %s : %s" LOAD_EXAMPLES, mysql_error(tag_db->mysql));
      success = false;
    } else if (mysql_stmt_prepare(tag_db->update_last_classified_at_stmt, UPDATE_LAST_CLASSIFIED_AT, strlen(UPDATE_LAST_CLASSIFIED_AT))) {
      error("Error preparing hard coded statement: %s : %s", UPDATE_LAST_CLASSIFIED_AT, mysql_error(tag_db->mysql));
      success = false;
    }
  }
  
  return success;
}
