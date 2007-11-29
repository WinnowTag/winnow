/* Copyright (c) 2007 The Kaphan Foundation
 *
 * Possession of a copy of this file grants no permission or license
 * to use, modify, or create derivate works.
 *
 * Please contact info@peerworks.org for further information.
 */

#include "time_helper.h"

#if HAVE_MYSQL_H

void convert_mysql_time(const MYSQL_TIME *my_time, time_t *time) {
  struct tm my_tm;
  gmtime_r(time, &my_tm);
  my_tm.tm_year = my_time->year - 1900;
  my_tm.tm_mon  = my_time->month - 1;
  my_tm.tm_mday = my_time->day;
  my_tm.tm_hour = my_time->hour;
  my_tm.tm_min  = my_time->minute;
  my_tm.tm_sec  = my_time->second;  
  *time = timegm(&my_tm);  
}

#endif
