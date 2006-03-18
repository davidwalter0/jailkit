/*
Copyright (c) 2003, 2004, 2005, Olivier Sessink
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
#include "config.h"
#include <stdio.h>
#include <sys/types.h>
#include <pwd.h>
#include <stdlib.h>
#include <string.h> /* memset() */

#include "utils.h"

static char *field_from_line(const char *line, int field) {
	int pos=0, fcount=0, fstart=0;
	while (1) {
		if (line[pos]==':') {
			if (field == fcount) { /* found the end of the field */
				return strndup(line+fstart,pos-fstart);
			} else {
				fcount++;
				fstart = pos+1;
			}
		} else if (line[pos] == '\0') {
			if (fcount == field) return strndup(line+fstart,pos-fstart);
			return NULL;
		}
		pos ++;
	}
	return NULL; /* should not get to this line */
}

static int int_field_from_line(const char *line, int field) {
	char *tmp;
	int retval;
	tmp = field_from_line(line, field);
	retval = atoi(tmp);
	free(tmp);
	return retval;
}

#define BLOCKSIZE 4096
static char * find_line(const char *filename, const char *fcont, int fnum) {
	FILE *fp;
	char buf[BLOCKSIZE];
	char *prev, *next, *retline=NULL;
	size_t num;
	int restlen=0;
	
/*	printf("searching for %s in field %d\n",fcont,fnum);*/
	fp = fopen(filename,"r");
	prev = buf;
	num = fread(buf, 1, BLOCKSIZE, fp);
	while (num || restlen) {
		next = strchr(prev, '\n');
		if (next || num==0) {
			char *field;
			if (num) *next = '\0';
/*			printf("line: %s\n",prev);*/
			field = field_from_line(prev,fnum);
/*			printf("field: %s\n",field);*/
			if (field && strcmp(field,fcont)==0) {
				/* we found the line */
				retline = strdup(prev);
/*				printf("retline: %s\n",retline);*/
			}
			if (field) free(field);
			if (retline) return retline;
			if (num) *next = '\n';
			prev = next+1;
		} else {
			restlen = BLOCKSIZE-(prev-buf);
			/* no more newlines, move the  */
			memmove(buf, prev, restlen);
			num = fread(buf+restlen, 1, BLOCKSIZE-restlen, fp);
		}
	}
	return NULL;
}

struct passwd *internal_getpwuid(uid_t uid) {
	static struct passwd retpw;
	char find[10], *line;
	
	snprintf(find,10,"%d",(int)uid);
	line = find_line("/etc/passwd", find, 2);
	if (line) {
		retpw.pw_name = field_from_line(line, 0);
		retpw.pw_passwd = NULL; /* not required */
		retpw.pw_uid = uid;
		retpw.pw_gid = int_field_from_line(line, 3);
		retpw.pw_gecos = NULL; /* not required */
		retpw.pw_dir = field_from_line(line, 5);
		retpw.pw_shell = field_from_line(line, 6);
		return &retpw;
	}
	return NULL;
}

