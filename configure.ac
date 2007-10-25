AC_INIT([classifier], [1.0], [info@peerworks.org])
AM_INIT_AUTOMAKE([-Wall -Werror foreign])
AC_PROG_CC
AC_PROG_LIBTOOL

AC_CHECK_LIB([m], [fabs])
AC_CHECK_LIB(Judy, JudyLIns, [], [AC_MSG_ERROR(Judy is missing)])

# Unit testing library
PKG_CHECK_MODULES([CHECK], [check >= 0.9.4])

AC_HEADER_STDC
AC_CHECK_HEADERS([Judy.h], [], [AC_MSG_ERROR(Judy.his missing)])

# Output Files
AC_CONFIG_HEADERS([config.h])
AC_CONFIG_FILES([Makefile src/Makefile tests/Makefile])
AC_OUTPUT
