
#include "config.h"

#ifndef HAVE_STRNDUP
char *strndup(const char *s, size_t n);
#endif
#ifndef HAVE_STRNLEN
size_t strnlen(const char *s, size_t n);
#endif

char *return_malloced_getwd(void);
