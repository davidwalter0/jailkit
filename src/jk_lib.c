/*
 * Copyright (C) Olivier Sessink 2003
 */

/* #define DEBUG */

#define _GNU_SOURCE
#include <string.h>
#include <ctype.h>
#include <stdlib.h>
#include <stdio.h>
#include <syslog.h>

#include "jk_lib.h"

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
	return arr;
}

int count_array(char **arr) {
	int len=0;
	char **tmp = arr;
	while (*tmp) len++;
	return len;
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
