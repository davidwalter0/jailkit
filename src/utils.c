#include "config.h"

#include <string.h>
#include <ctype.h>
#include <stdlib.h>
#include <stdio.h>
#include <syslog.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

#include "utils.h"

#ifndef HAVE_CLEARENV
/* doesn't compile on FreeBSD without this */
extern char **environ;
#endif

#ifndef HAVE_STRNDUP
char *strndup(const char *s, size_t n) {
	char *ret;
	n = strnlen(s, n);
	ret = malloc(n+1);
	if (!ret) return NULL;
	memcpy(ret, s, n);
	ret[n] = 0;
	return ret;
}
#endif

#ifndef HAVE_STRNLEN
size_t strnlen(const char *s, size_t n) {
	int i;
	for (i=0; s[i] && i<n; i++)
		/* noop */ ;
	return i;
}
#endif

#ifndef HAVE_WORDEXP
#ifndef HAVE_MEMPCPY
void *mempcpy(void *dest, const void *src, size_t n) {
	memcpy(dest,src,n);
	return dest+n;
}
#endif

#ifndef HAVE_STPCPY
char *stpcpy(char *dest, const char *src) {
	strcpy(dest, src);
	return dest + strlen(src);
}
#endif
#endif /* HAVE_WORDEXP */

#ifndef HAVE_CLEARENV
/* from Linux Programmer's Manual man clearenv() 
	Used  in  security-conscious  applications.  If  it  is unavailable the
	assignment
		environ = NULL;
	will probably do. */
int clearenv(void) {
	environ = NULL;
}
#endif /* HAVE_CLEARENV */

#ifndef HAVE_GETCURRENTDIRNAME
char *get_current_dir_name(void) {
	char *string;
	string = malloc0(512);
	return getcwd(string, 512);
}
#endif /* HAVE_GETCURRENTDIRNAME */
