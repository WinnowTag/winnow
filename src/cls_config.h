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

typedef struct ENGINE_CONFIG {
  int num_workers;
  float insertion_threshold;
  const char *performance_log;
} EngineConfig;

typedef struct HTTP_CONFIG {
  int port;
  const char *allowed_ip;
} HttpdConfig;

extern Config   * load_config        (const char * config_file);
extern int        cfg_engine_config  (const Config * config, EngineConfig *econfig);
extern int        cfg_item_db_config (const Config * config, DBConfig *db_config);
extern int        cfg_tag_db_config  (const Config * config, DBConfig *db_config);
extern int        cfg_tagging_store_db_config (const Config * config, DBConfig *db_config);
extern int        cfg_httpd_config   (const Config *config, HttpdConfig *httpd);
extern void       free_config        (Config *config);

#endif /* _CONFIG_H_ */
