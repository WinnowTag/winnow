/* Copyright (c) 2007 The Kaphan Foundation
 *
 * Possession of a copy of this file grants no permission or license
 * to use, modify, or create derivate works.
 *
 * Please contact info@peerworks.org for further information.
 */

#ifndef _CLS_CONFIG_H_
#define _CLS_CONFIG_H_

typedef struct CONFIG Config;
typedef struct DB_CONFIG {
  const char * host;
  const char * user;
  const char * password;
  const char * database;
  int port;
} DBConfig;

extern Config   * load_config        (const char * config_file);
extern int        cfg_item_db_config (const Config * config, DBConfig *db_config);
extern int        cfg_tag_db_config  (const Config * config, DBConfig *db_config);
extern int        cfg_tagging_store_db_config (const Config * config, DBConfig *db_config);
extern void       free_config        (Config *config);

#endif /* _CONFIG_H_ */
