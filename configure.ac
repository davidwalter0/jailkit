# Jailkit
# configure.ac - the autoconf file
# Copyright (C) 2003-2004 Olivier Sessink
#

AC_INIT
AC_PREFIX_DEFAULT(/usr)
AC_CONFIG_SRCDIR(src/jk_socketd.c)
AC_CONFIG_HEADER(src/config.h)

PACKAGE=jailkit
VERSION=0.8
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
echo -n "checking for up to date python... "
if python -c 'i=1;i+=1' >/dev/null 2>&1 ; then
	echo "ok"
else
	echo "failed"
	AC_MSG_ERROR(your python version is too old, please install python 2 or newer)
fi

BINARIES="jk_socketd jk_lsh jk_chrootsh jk_chrootlaunch"

AC_PATH_PROG(PROCMAILPATH, procmail, no, /usr/bin)
HAVEPROCMAIL="0"
if test "x$PROCMAILPATH" != "xno" ; then
	AC_DEFINE_UNQUOTED(PROCMAILPATH, "$PROCMAILPATH")
	BINARIES="${BINARIES} jk_procmailwrapper"
	HAVEPROCMAIL="1"
fi
AC_SUBST(BINARIES)
AC_SUBST(HAVEPROCMAIL)

AC_CONFIG_FILES(Makefile src/Makefile py/Makefile man/Makefile)
AC_OUTPUT
