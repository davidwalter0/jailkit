
#include "config.h"

#ifndef HAVE_STRNDUP
char *strndup(const char *s, size_t n);
#endif
#ifndef HAVE_STRNLEN
size_t strnlen(const char *s, size_t n);
#endif

#ifndef HAVE_WORDEXP
#ifndef HAVE_MEMPCPY
void *mempcpy(void *dest, const void *src, size_t n);
#endif
#ifndef HAVE_STPCPY
char *stpcpy(char *dest, const char *src);
#endif
#endif /* HAVE_WORDEXP */

char *return_malloced_getwd(void);
