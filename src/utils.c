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

