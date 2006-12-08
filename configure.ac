# Jailkit
# configure.ac - the autoconf file
#
#Copyright (C) 2003, 2004, 2005, 2006, Olivier Sessink
#All rights reserved.
#
#Redistribution and use in source and binary forms, with or without
#modification, are permitted provided that the following conditions 
#are met:
#  * Redistributions of source code must retain the above copyright 
#    notice, this list of conditions and the following disclaimer.
#  * Redistributions in binary form must reproduce the above 
#    copyright notice, this list of conditions and the following 
#    disclaimer in the documentation and/or other materials provided 
#    with the distribution.
#  * The names of its contributors may not be used to endorse or 
#    promote products derived from this software without specific 
#    prior written permission.
#
#THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS 
#"AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT 
#LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS 
#FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE 
#COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, 
#INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, 
#BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; 
#LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER 
#CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT 
#LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN 
#ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE 
#POSSIBILITY OF SUCH DAMAGE.
#
#

AC_INIT
AC_PREFIX_DEFAULT(/usr)
if test "$prefix" = "NONE"; then
	sysconfdir="/etc"
	echo "config files will be in /etc/jailkit/"
fi
AC_CONFIG_SRCDIR(src/jk_socketd.c)
AC_CONFIG_HEADER(src/config.h)

PACKAGE=jailkit
VERSION=2.2
AC_SUBST(PACKAGE)
AC_SUBST(VERSION)
AC_DEFINE_UNQUOTED(PACKAGE, "$PACKAGE")
AC_DEFINE_UNQUOTED(VERSION, "$VERSION")
AC_DEFINE_UNQUOTED(INIPREFIX, "${sysconfdir}/etc/")

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

AC_CHECK_HEADERS(errno.h stdio.h ctype.h getopt.h math.h time.h sys/socket.h pthread.h pwd.h wordexp.h liberty.h)

AC_CHECK_FUNC(strndup,
	AC_DEFINE(HAVE_STRNDUP, 1),
	AC_CHECK_LIB(strndup, iberty,
		LIBS="$LIBS -liberty"
		AC_DEFINE(HAVE_STRNDUP, 1)
	)
)

AC_CHECK_FUNC(strnlen,
	AC_DEFINE(HAVE_STRNLEN, 1)
)

AC_CHECK_FUNC(wordexp,
	AC_DEFINE(HAVE_WORDEXP, 1)
)

AC_CHECK_FUNC(mempcpy,
	AC_DEFINE(HAVE_MEMPCPY, 1),
	AC_CHECK_LIB(mempcpy, iberty,
		LIBS="$LIBS -liberty"
		AC_DEFINE(HAVE_MEMPCPY, 1)
	)
)

AC_CHECK_FUNC(stpcpy,
	AC_DEFINE(HAVE_STPCPY, 1),
	AC_CHECK_LIB(stpcpy, iberty,
		LIBS="$LIBS -liberty"
		AC_DEFINE(HAVE_STPCPY, 1)
	)
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

# FreeBSD has a gnugetopt library for this
# Solaris might have libiberty
AC_CHECK_FUNCS([getopt_long],,
	AC_CHECK_LIB(gnugetopt,getopt_long,
		AC_DEFINE(HAVE_GETOPT_LONG)
		LIBS="$LIBS -lgnugetopt",
		AC_CHECK_LIB(iberty,getopt_long,
			AC_DEFINE(HAVE_GETOPT_LONG)
  			LIBS="$LIBS -liberty",
  			AC_MSG_ERROR(getopt_long not found)
  		)
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
