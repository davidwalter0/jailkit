#ifndef __JK_LIB_H
#define __JK_LIB_H

#include "config.h"

#ifdef DEBUG
#define DEBUG_MSG printf
#else
#define DEBUG_MSG(args...)
 /**/
#endif

#ifdef DEBUG
#define DEBUG_LOG(args...) syslog(LOG_DEBUG, args)
#else
#define DEBUG_LOG(args...)
 /**/
#endif

#ifndef HAVE_MALLOC0
#define malloc0(size) memset(malloc(size),0,size)
#define HAVE_MALLOC0
#endif /* HAVE_MALLOC0 */

void testsafepath(const char *path, int owner, int group);
int getjaildir(const char *oldhomedir, char **jaildir, char **newhomedir);
char *strip_string(char * string);
int count_char(const char *string, char lookfor);
char **explode_string(const char *string, char delimiter);
int count_array(char **arr);
void free_array(char **arr);

#endif /* __JK_LIB_H */
