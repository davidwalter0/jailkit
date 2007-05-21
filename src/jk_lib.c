/*
Copyright (c) 2003, 2004, 2005, 2006, Olivier Sessink
All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions 
are met:
  * Redistributions of source code must retain the above copyright 
    notice, this list of conditions and the following disclaimer.
  * Redistributions in binary form must reproduce the above 
    copyright notice, this list of conditions and the following 
    disclaimer in the documentation and/or other materials provided 
    with the distribution.
  * The names of its contributors may not be used to endorse or 
    promote products derived from this software without specific 
    prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS 
"AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT 
LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS 
FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE 
COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, 
INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, 
BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; 
LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER 
CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT 
LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN 
ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE 
POSSIBILITY OF SUCH DAMAGE.
 */

/*#define DEBUG*/
#include "config.h"

#include <string.h>
#include <ctype.h>
#include <stdlib.h>
#include <stdio.h>
#include <syslog.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

#include "jk_lib.h"
#include "utils.h"

/*
 * the path should be owned owner:group
 * if it is a file it should not have any setuid or setgid bits set
 * it should not be writable for group or others
 * it should not be a symlink
 */
int testsafepath(const char *path, int owner, int group) {
	struct stat sbuf;
	DEBUG_MSG("testsafepath %s\n",path);
	if (lstat(path, &sbuf) == 0) {
		int retval=0;
		if (S_ISLNK(sbuf.st_mode)) {
			syslog(LOG_ERR, "abort, path %s is a symlink", path);
			retval |= TESTPATH_NOREGPATH;
		}
		if (sbuf.st_mode & S_ISUID) {
			syslog(LOG_ERR, "abort, path %s is setuid", path);
			retval |= TESTPATH_SETUID;
		}
		if (sbuf.st_mode & S_ISGID) {
			syslog(LOG_ERR, "abort, path %s is setgid", path);
			retval |= TESTPATH_SETGID;
		}
		if (sbuf.st_mode & S_IWGRP) {
			syslog(LOG_ERR, "abort, path %s is setgid", path);
			retval |= TESTPATH_GROUPW;
		}
		if (sbuf.st_mode & S_IWOTH) {
			syslog(LOG_ERR, "abort, path %s is setgid", path);
			retval |= TESTPATH_OTHERW;
		}
		if (sbuf.st_uid != owner){
			retval |= TESTPATH_OWNER;
		}
		if (sbuf.st_gid != group){
			retval |= TESTPATH_GROUP;
		}
		return retval;
	} else {
		return TESTPATH_NOREGPATH;
	}
}

int basicjailissafe(const char *path) {
	if (path && testsafepath(path, 0, 0) ==0) {
		char *tmp;
		int retval = 1;
		tmp = malloc0(strlen(path)+6);
		if (retval == 1 && (testsafepath(strcat(strcpy(tmp,path), "dev/"), 0, 0) &~TESTPATH_NOREGPATH)!=0) retval = 0;
		if (retval == 1 && (testsafepath(strcat(strcpy(tmp,path), "etc/"), 0, 0) &~TESTPATH_NOREGPATH)!=0) retval = 0;
		if (retval == 1 && (testsafepath(strcat(strcpy(tmp,path), "lib/"), 0, 0) &~TESTPATH_NOREGPATH)!=0) retval = 0;
		if (retval == 1 && (testsafepath(strcat(strcpy(tmp,path), "usr/"), 0, 0) &~TESTPATH_NOREGPATH)!=0) retval = 0;
		if (retval == 1 && (testsafepath(strcat(strcpy(tmp,path), "bin/"), 0, 0) &~TESTPATH_NOREGPATH)!=0) retval = 0;
		if (retval == 1 && (testsafepath(strcat(strcpy(tmp,path), "sbin/"), 0, 0)&~TESTPATH_NOREGPATH)!=0) retval = 0;
		free(tmp);
		DEBUG_MSG("basicjailissafe, returning %d\n",retval);
		return retval;
	}
	return 0;
}

/* if it returns 1 it will allocate new memory for jaildir and newhomedir
 * else it will return 0
 */
int getjaildir(const char *oldhomedir, char **jaildir, char **newhomedir) {
	int i=strlen(oldhomedir);
	/* we will not accept /./ as jail, so we continue looking while i > 4 (minimum then is /a/./ )
	 * we start at the end so if there are multiple /path/./path2/./path3 the user will be jailed in the most minimized path 
	 */
	while (i > 4) {
/*		DEBUG_MSG("oldhomedir[%d]=%c\n",i,oldhomedir[i]);*/
		if (oldhomedir[i] == '/') {
			if (oldhomedir[i-1] == '.') {
				if (oldhomedir[i-2] == '/') {
					DEBUG_MSG("&oldhomedir[%d]=%s\n",i,&oldhomedir[i]);
					*jaildir = strndup(oldhomedir, i-2);
					*newhomedir = strdup(&oldhomedir[i]);
					return 1;
				}
			}
		}
		i--;
	}
	return 1;
}

char *strip_string(char * string) {
	int numstartspaces=0, endofcontent=strlen(string)-1;
	while (isspace(string[numstartspaces]) && numstartspaces < endofcontent) numstartspaces++;
	while (isspace(string[endofcontent]) && endofcontent > numstartspaces) endofcontent--;
	if (numstartspaces != 0) memmove(string, &string[numstartspaces], (endofcontent - numstartspaces+1)*sizeof(char));
	string[(endofcontent - numstartspaces+1)] = '\0';
	return string;
}

int count_char(const char *string, char lookfor) {
	int count=0;
	while (*string != '\0') {
		if (*string == lookfor) count++;
		string++;
	}
	DEBUG_LOG("count_char, returning %d\n",count);
	return count;
}

char **explode_string(const char *string, char delimiter) {
	char **arr;
	const char *tmp = string;
	int cur= 0;
	int size = ((count_char(string, delimiter) + 2)*sizeof(char*));
	arr = malloc(size);
	
	DEBUG_LOG("exploding string '%s', arr=%p with size %d, sizeof(char*)=%d\n",string,arr,size,sizeof(char*));

	while (tmp) {
		char *tmp2 = strchr(tmp, delimiter);
		if (tmp2) {
			arr[cur] = strip_string(strndup(tmp, (tmp2-tmp)));
		} else {
			arr[cur] = strip_string(strdup(tmp));
		}
		if (strlen(arr[cur])==0) {
			free(arr[cur]);
		} else {
			DEBUG_LOG("found string '%s' at %p\n",arr[cur], arr[cur]);
			cur++;
		}
		tmp = (tmp2) ? tmp2+1 : NULL;
	}
	arr[cur] = NULL;
	DEBUG_MSG("exploding string, returning %p\n",arr);
	return arr;
}

int count_array(char **arr) {
	char **tmp = arr;
	DEBUG_MSG("count_array, started for %p\n",arr);
	while (*tmp) tmp++;
	return (tmp-arr);
}

void free_array(char **arr) {
	char **tmp = arr;
	if (!arr) return;
	while (*tmp) {
		free(*tmp);
		tmp++;
	}
	free(arr);
}
