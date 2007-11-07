AC_INIT([classifier], [1.0], [info@peerworks.org])
AM_INIT_AUTOMAKE([-Wall -Werror foreign])

### Check for programs.

### Check for mysql_config
AC_ARG_WITH(mysql_config,
        AC_HELP_STRING(
                [--with-mysql_config=PROGRAM],
                [location of the mysql_config program]),
        [mysql_config_prog=$withval])
AC_PATH_PROGS(MYSQL_CONFIG, [mysql_config mysql_config5], $mysql_config_prog)

if test "x$MYSQL_CONFIG" = "x"; then
    AC_MSG_ERROR([Couldn't find mysql_config. Please verify it is installed or pass it in using --with-mysql_config])
fi


AC_PROG_CC
AC_PROG_LIBTOOL

### Check libraries

AC_CHECK_LIB([m], [fabs])
AC_CHECK_LIB(Judy, JudyLIns, [], [AC_MSG_ERROR(Judy is missing)])
AC_CHECK_LIB(pthread, pthread_create, [], [AC_MSG_ERROR(pthread is missing)])
AC_CHECK_LIB(config, config_init, [], [AC_MSG_ERROR(libconfig is missing you might need to install it from http://www.hyperrealm.com/libconfig/)])

MYSQL_LIBS=$($MYSQL_CONFIG --libs)
LIBS="$LIBS $MYSQL_LIBS"
AC_CHECK_LIB(mysqlclient, mysql_real_connect, [], [AC_MSG_ERROR(Could not find mysqlclient with mysql_real_connect)])

# Unit testing library
PKG_CHECK_MODULES([CHECK], [check >= 0.9.4])
PKG_CHECK_MODULES([LIBCONFIG], [check >= 0.9.4])

### Check for headers
AC_HEADER_STDC
AC_CHECK_HEADERS([Judy.h], [], [AC_MSG_ERROR(Judy.h is missing)])
AC_CHECK_HEADERS([libconfig.h], [], [AC_MSG_ERROR(libconfig.h is missing)])
AC_CHECK_HEADERS([sys/param.h errno.h stdarg.h stdbool.h])

###### MySQL
MYSQL_INCLUDE=$($MYSQL_CONFIG --cflags)
CPPFLAGS="$CPPFLAGS $MYSQL_INCLUDE"
AC_CHECK_HEADERS([mysql.h], [], [AC_MSG_ERROR(mysql.h is missing)])


AC_FUNC_MALLOC

# Output Files
AC_CONFIG_HEADERS([config.h])
AC_CONFIG_FILES([Makefile src/Makefile tests/Makefile])
AC_OUTPUT
