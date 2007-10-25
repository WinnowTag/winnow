AC_INIT([classifier], [1.0], [info@peerworks.org])
AM_INIT_AUTOMAKE([-Wall -Werror foreign])
AC_PROG_CC
AC_PROG_LIBTOOL

# Unit testing library
PKG_CHECK_MODULES([CHECK], [check >= 0.9.4])

# Output Files
AC_CONFIG_HEADERS([config.h])
AC_CONFIG_FILES([Makefile src/Makefile tests/Makefile])
AC_OUTPUT
