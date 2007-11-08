/* Copyright (c) 2007 The Kaphan Foundation
 *
 * Possession of a copy of this file grants no permission or license
 * to use, modify, or create derivate works.
 *
 * Please contact info@peerworks.org for further information.
 */

#include <stdlib.h>
#include <libconfig.h>
#include "cls_config.h"
#include "misc.h"

#define ITEM_DB "db.item"
#define TAG_DB  "db.tag"
#define TAGGING_STORE_DB "db.tagging_store"

struct CONFIG {
  config_t *config;
};

static int load_db_config(const Config *config, DBConfig *db_config, const char * path);

/** Load the configuration from a file.
 *
 *  Returns NULL if there is an error loading the configuration file.
 */
Config * load_config(const char * config_file) {
  Config *config = malloc(sizeof(Config));
  if (NULL == config) goto malloc_err;
  
  config->config = malloc(sizeof(config_t));
  if (NULL == config->config) goto malloc_err;
  
  config_init(config->config);
  if (CONFIG_FALSE == config_read_file(config->config, config_file)) {
    fatal("Config loading failure: %s on line %i", config_error_text(config->config), 
                                                   config_error_line(config->config));
    free_config(config);
    config = NULL;
  }
  
  return config;
malloc_err:
  free_config(config);
  fatal("Malloc error occurred loading config file: %s", config_file);
}

/** Free the configuration object.
 */
void free_config(Config *config) {
  if (NULL != config) {
    if (NULL != config->config) {
      config_destroy(config->config);
      free(config->config);
    }
    free(config);
  }  
}

/** Gets the item DB configuration.
 *
 *  The passed in db_config will be initialized with the DB settings
 *  defined in the configuration. The char* assigned to the db_config
 *  will be managed by the configuration object and should not be freed
 *  by the caller.
 *
 *  Returns true if the db_config is set, false otherwise.
 */
int cfg_item_db_config(const Config * config, DBConfig *db_config) {
  return load_db_config(config, db_config, ITEM_DB);
}

int cfg_tag_db_config(const Config * config, DBConfig *db_config) {
  return load_db_config(config, db_config, TAG_DB);
}

int cfg_tagging_store_db_config(const Config * config, DBConfig *db_config) {
  return load_db_config(config, db_config, TAGGING_STORE_DB);
}

static int load_db_config(const Config *config, DBConfig *db_config, const char * path) {
  int success = false;
  config_setting_t *db_setting = config_lookup(config->config, path);
  
  if (NULL != db_setting) {
    success = true;
    config_setting_t *setting;
    if (setting = config_setting_get_member(db_setting, "host")) {
      db_config->host = config_setting_get_string(setting);
    }
    if (setting = config_setting_get_member(db_setting, "user")) {
      db_config->user = config_setting_get_string(setting);
    }
    if (setting = config_setting_get_member(db_setting, "password")) {
      db_config->password = config_setting_get_string(setting);
    }
    if (setting = config_setting_get_member(db_setting, "database")) {
      db_config->database = config_setting_get_string(setting);
    }
    if (setting = config_setting_get_member(db_setting, "port")) {
      db_config->port = config_setting_get_int(setting);
    }    
  } else {
    debug("Missing %s", path);
  }
  
  return success;
}