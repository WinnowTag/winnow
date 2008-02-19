AC_INIT([classifier], [1.0], [info@peerworks.org])
AM_INIT_AUTOMAKE([-Wall -Werror foreign])
AC_CONFIG_LIBOBJ_DIR(src)

### Check for programs.
AC_PROG_CC
AC_PROG_LIBTOOL

### Check for mysql_config
AC_ARG_WITH(mysql_config,
        AC_HELP_STRING(
                [--with-mysql_config=PROGRAM],
                [location of the mysql_config program]),
        [mysql_config_prog=$withval])
AC_PATH_PROGS(MYSQL_CONFIG, [mysql_config mysql_config5], $mysql_config_prog)

if test "x$MYSQL_CONFIG" = "x"; then
    AC_MSG_ERROR([Could not find mysql_config. Please verify it is installed or pass it in using --with-mysql_config])
fi

### Check for xml2-config
AC_ARG_WITH(xml2_config,
        AC_HELP_STRING(
                [--with-xml2-config=PROGRAM],
                [location of the xml2-config program]),
        [xml2_config_prog=$withval])        
AC_PATH_PROG(XML2_CONFIG, [xml2-config], $xml2_config_prog)

if test "x$XML2_CONFIG" = "x"; then
  AC_MSG_ERROR([Could not find xml2-config. Please verify libxml2 is installed or pass in path to xml2-config using --with-xml2-config])
fi

### Check for pkg-config
AC_CHECK_PROGS(PKG_CONFIG, [pkg-config])

### Check for debug build
AC_ARG_ENABLE([debug],
     [  --enable-debug    Turn on debugging],
     [case "${enableval}" in
       yes) debug=true ;;
       no)  debug=false ;;
       *) AC_MSG_ERROR([bad value ${enableval} for --enable-debug]) ;;
     esac],[debug=false])
AM_CONDITIONAL([DEBUG], [test x$debug = xtrue])

### Check Functions
AC_CHECK_LIB([uuid], [uuid_generate])
AC_REPLACE_FUNCS(uuid_generate)
AC_REPLACE_FUNCS(uuid_unparse)

### Check libraries

AC_CHECK_LIB([m], [fabs])
AC_CHECK_LIB(Judy, JudyLIns, [], [AC_MSG_ERROR(Judy is missing)])
AC_CHECK_LIB(pthread, pthread_create, [], [AC_MSG_ERROR(pthread is missing)])
AC_CHECK_LIB(config, config_init, [], [AC_MSG_ERROR(libconfig is missing you might need to install it from http://www.hyperrealm.com/libconfig/)])
AC_CHECK_LIB(microhttpd, MHD_start_daemon, [], [AC_MSG_WARN(libmicrohttpd is missing. REST support will be disabled.)])

### Setup libxml2 libs
XML2_LIBS=$($XML2_CONFIG --libs)
LIBS="$XML2_LIBS $LIBS"
AC_CHECK_LIB(xml2, xmlReadFile, [], [AC_MSG_ERROR(libxml2 is missing xmlReadFile)])

### Setup MySQL libraries
MYSQL_LIBS=$($MYSQL_CONFIG --libs_r)
LIBS="$LIBS $MYSQL_LIBS"
AC_CHECK_LIB(mysqlclient_r, mysql_real_connect, 
  [AC_DEFINE(HAVE_DATABASE_ACCESS,,[We have DB libraries])], 
  [AC_MSG_ERROR(Could not find mysqlclient with mysql_real_connect)])

AC_CHECK_LIB(sqlite3, [sqlite3_open_v2], [], [AC_MSG_ERROR(Could not find sqlite3 with sqlite3_open_v2)])

# Unit testing library
if test "x$PKG_CONFIG" != "x"; then
PKG_CHECK_MODULES([CHECK], [check >= 0.9.4])
fi

# libcurl (required for testing)
AC_CHECK_LIB(curl, curl_easy_init, [], [])

### Check for headers
AC_HEADER_STDC
AC_CHECK_HEADERS([Judy.h], [], [AC_MSG_ERROR(Judy.h is missing)])
AC_CHECK_HEADERS([libconfig.h], [], [AC_MSG_ERROR(libconfig.h is missing)])
AC_CHECK_HEADERS([sys/param.h errno.h stdarg.h stdbool.h uuid/uuid.h])

###### Setup libxml2 headers
XML2_INCLUDE=$($XML2_CONFIG --cflags)
CPPFLAGS="$CPPFLAGS $XML2_INCLUDE"
AC_CHECK_HEADERS([libxml/parser.h], [], [AC_MSG_ERROR(libxml/parser.h is missing)])

###### MySQL
MYSQL_INCLUDE=$($MYSQL_CONFIG --cflags)
CPPFLAGS="$CPPFLAGS $MYSQL_INCLUDE"
AC_CHECK_HEADERS([mysql.h], [], [AC_MSG_ERROR(mysql.h is missing)])

### Check for types
AC_CHECK_TYPES(uuid_t, [], [],
[AC_INCLUDES_DEFAULT
#ifdef HAVE_UUID_UUID_H
#include <uuid/uuid.h>
#endif
])

# Output Files
AC_CONFIG_HEADERS([config.h])
AC_CONFIG_FILES([Makefile src/Makefile tests/Makefile])
AC_OUTPUT
