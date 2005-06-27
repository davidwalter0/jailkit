# Jailkit
# configure.ac - the autoconf file
# Copyright (C) 2003-2004 Olivier Sessink
#

AC_INIT
AC_PREFIX_DEFAULT(/usr)
AC_CONFIG_SRCDIR(src/jk_socketd.c)
AC_CONFIG_HEADER(src/config.h)

PACKAGE=jailkit
VERSION=post1.3
AC_SUBST(PACKAGE)
AC_SUBST(VERSION)
AC_DEFINE_UNQUOTED(PACKAGE, "$PACKAGE")
AC_DEFINE_UNQUOTED(VERSION, "$VERSION")

AC_GNU_SOURCE

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

AC_CHECK_HEADERS(errno.h stdio.h ctype.h getopt.h math.h time.h sys/socket.h pthread.h pwd.h wordexp.h)

AC_CHECK_FUNC(strndup,
	AC_DEFINE(HAVE_STRNDUP, 1)
	)

AC_CHECK_FUNC(strnlen,
	AC_DEFINE(HAVE_STRNLEN, 1)
	)

AC_CHECK_FUNC(wordexp,
	AC_DEFINE(HAVE_WORDEXP, 1)
	)

AC_CHECK_FUNC(mempcpy,
	AC_DEFINE(HAVE_MEMPCPY, 1)
	)

AC_CHECK_FUNC(stpcpy,
	AC_DEFINE(HAVE_STPCPY, 1)
	)

AC_CHECK_FUNC(clearenv,
	AC_DEFINE(HAVE_CLEARENV, 1)
	)

AC_CHECK_FUNC(get_current_dir_name,
	AC_DEFINE(HAVE_GETCURRENTDIRNAME, 1)
	)

AC_CHECK_LIB(pthread, pthread_create,
	PTHREAD_LIBS=-lpthread,
	AC_CHECK_LIB(c_r, pthread_create,
		PTHREAD_LIBS=-lc_r,
		AC_MSG_ERROR(libpthread not found)
		)
	)

AC_PATH_PROG(PYTHONPATH, python, no)
if test "x$PYTHONPATH" == "xno" ; then
	AC_MSG_ERROR(python not found please install python)
fi
AC_SUBST(PYTHONPATH)
echo -n "checking for up to date python... "
if python -c 'i=1;i+=1' >/dev/null 2>&1 ; then
	echo "ok"
else
	echo "failed"
	AC_MSG_ERROR(your python version is too old, please install python 2 or newer)
fi

BINARIES="jk_socketd jk_lsh jk_chrootsh jk_chrootlaunch"

AC_PATH_PROG(PROCMAILPATH, procmail, no)
HAVEPROCMAIL="0"
if test "x$PROCMAILPATH" != "xno" ; then
	AC_DEFINE_UNQUOTED(PROCMAILPATH, "$PROCMAILPATH")
	BINARIES="${BINARIES} jk_procmailwrapper"
	HAVEPROCMAIL="1"
fi
AC_SUBST(BINARIES)
AC_SUBST(HAVEPROCMAIL)
AC_SUBST(LIBS)
AC_SUBST(LDFLAGS)
AC_SUBST(PTHREAD_LIBS)

AC_CONFIG_FILES(Makefile src/Makefile py/Makefile man/Makefile)
AC_OUTPUT
