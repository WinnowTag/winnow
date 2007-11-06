AC_INIT([classifier], [1.0], [info@peerworks.org])
AM_INIT_AUTOMAKE([-Wall -Werror foreign])
AC_PROG_CC
AC_PROG_LIBTOOL

AC_CHECK_LIB([m], [fabs])
AC_CHECK_LIB(Judy, JudyLIns, [], [AC_MSG_ERROR(Judy is missing)])
AC_CHECK_LIB(pthread, pthread_create, [], [AC_MSG_ERROR(pthread is missing)])
AC_CHECK_LIB(config, config_init, [], [AC_MSG_ERROR(libconfig is missing you might need to install it from http://www.hyperrealm.com/libconfig/)])

# Unit testing library
PKG_CHECK_MODULES([CHECK], [check >= 0.9.4])
PKG_CHECK_MODULES([LIBCONFIG], [check >= 0.9.4])

AC_HEADER_STDC
AC_CHECK_HEADERS([Judy.h], [], [AC_MSG_ERROR(Judy.h is missing)])
AC_CHECK_HEADERS([libconfig.h], [], [AC_MSG_ERROR(libconfig.h is missing)])
AC_CHECK_HEADERS([sys/param.h errno.h stdarg.h stdbool.h])

AC_FUNC_MALLOC

# Output Files
AC_CONFIG_HEADERS([config.h])
AC_CONFIG_FILES([Makefile src/Makefile tests/Makefile])
AC_OUTPUT
