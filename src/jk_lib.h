

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
 

#define malloc0(size) memset(malloc(size),0,size)


char *strip_string(char * string);
int count_char(const char *string, char lookfor);
char **explode_string(const char *string, char delimiter);
void free_array(char **arr);
