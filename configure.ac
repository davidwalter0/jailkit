# Jailkit
# configure.ac - the autoconf file
# Copyright (C) 2003 Olivier Sessink
#

AC_INIT
AC_PREFIX_DEFAULT(/usr)
AC_CONFIG_SRCDIR(src/jk_socketd.c)
AC_CONFIG_HEADER(src/config.h)

PACKAGE=jailkit
VERSION=pre0.4
AC_SUBST(PACKAGE)
AC_SUBST(VERSION)
AC_DEFINE_UNQUOTED(PACKAGE, "$PACKAGE")
AC_DEFINE_UNQUOTED(VERSION, "$VERSION")

AC_ARG_PROGRAM
AC_PROG_CC
AC_ISC_POSIX
AC_C_INLINE
AC_PROG_INSTALL

dnl ************************
dnl Check for standard headers
dnl ************************

AC_HEADER_STDC

dnl ************************
dnl Checks for header files.
dnl ************************

AC_CHECK_HEADERS(errno.h stdio.h ctype.h getopt.h math.h time.h sys/socket.h pthread.h pwd.h)

AC_CHECK_LIB(pthread, pthread_create,
	PTHREAD_LIBS=-lpthread,
	AC_MSG_ERROR(libpthread not found)
	)

AC_CHECK_PROG(HAVE_PYTHON, python, yes)
if test "x$HAVE_PYTHON" != "xyes" ; then
	AC_MSG_ERROR(python not found please install python)
fi

AC_CONFIG_FILES(Makefile src/Makefile py/Makefile man/Makefile)
AC_OUTPUT

