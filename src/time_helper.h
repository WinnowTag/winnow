/* Copyright (c) 2007 The Kaphan Foundation
 *
 * Possession of a copy of this file grants no permission or license
 * to use, modify, or create derivate works.
 *
 * Please contact info@peerworks.org for further information.
 */
#ifndef TIME_HELPER_H_
#define TIME_HELPER_H_

#include <config.h>
#include <time.h>

#if HAVE_MYSQL_H
#include <mysql.h>

void     convert_mysql_time  (const MYSQL_TIME *my_time, time_t *time);

#endif

#endif /*TIME_HELPER_H_*/
