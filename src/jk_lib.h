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


#define TESTPATH_NOREGPATH 1    // (0000 0001)
#define TESTPATH_GROUPW    2    // (0000 0010)
#define TESTPATH_OTHERW    4    // (0000 0100)
#define TESTPATH_SETUID    8    // (0000 1000)
#define TESTPATH_SETGID   16    // (0001 0000)
#define TESTPATH_OWNER    32    // (0010 0000)
#define TESTPATH_GROUP    64    // (0100 0000)

char *ending_slash(const char *src);
int testsafepath(const char *path, int owner, int group);
int basicjailissafe(const char *path);
int dirs_equal(const char *dir1, const char *dir2);
int getjaildir(const char *oldhomedir, char **jaildir, char **newhomedir);
char *strip_string(char * string);
int count_char(const char *string, char lookfor);
char **explode_string(const char *string, char delimiter);
int count_array(char **arr);
void free_array(char **arr);

#endif /* __JK_LIB_H */
