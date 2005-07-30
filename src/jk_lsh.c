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
 
/*
 * Limited shell, will only execute files that are configured in /etc/jailkit/jk_lsh.ini
 */
#include "config.h"

#include <string.h>
#include <ctype.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <syslog.h>
#include <unistd.h>
#include <pwd.h>
#include <grp.h>
#include <stdlib.h>
#include <errno.h>

#ifdef HAVE_WORDEXP_H
#include <wordexp.h>
#else
#include "wordexp.h"
/* needed to link wordexp.o with the executable */
int libc_argc;
char **libc_argv;

#endif

#define PROGRAMNAME "jk_lsh"
#define CONFIGFILE "/etc/jailkit/jk_lsh.ini"

/* #define DEBUG */

#include "jk_lib.h"
#include "iniparser.h"

/* doesn't compile on FreeBSD without this */
extern char **environ;

static int executable_is_allowed(Tiniparser *parser, const char *section, const char *executable) {
	int klen;
	char buffer[1024];
	klen = iniparser_get_string(parser, section, "executables", buffer, 1024);
	if (klen) {
		char **arr, **tmp;
		arr = tmp = explode_string(buffer, ',');
		while (tmp && *tmp) {
			DEBUG_MSG("comparing '%s' and '%s'\n",*tmp,executable);
			if (strcmp(*tmp,executable)==0) {
				free_array(arr);
				return 1;
			}
			tmp++;
		}
		free_array(arr);
		return 0;
	} else {
		syslog(LOG_ERR, "section %s does not have a key executables", section);
		exit(5);
	}
}

static int file_exists(const char *path) {
	struct stat sb;
	if (stat(path, &sb) == -1 && errno == ENOENT) {
		return 0;
	}
	return 1;
}

static char *expand_executable_w_path(const char *executable, char **allowed_paths) {
	DEBUG_LOG("expand_executable_w_path, executable=%s",executable);
	if (file_exists(executable)) {
		return strdup(executable);
	}
	if (!allowed_paths) {
		return NULL;
	} else {
		char **path = allowed_paths;
		int elen = strlen(executable);
		while (*path) {
			char *newpath;
			int tlen = strlen(*path);
			newpath = malloc((elen+tlen+2)*sizeof(char));
			memset(newpath, 0, (elen+tlen+2)*sizeof(char));
			newpath = strncpy(newpath, *path, tlen);
			if (*(*path+tlen-1) != '/') {
				newpath = strcat(newpath, "/");
				DEBUG_LOG("newpath=%s",newpath);
			}
			newpath = strncat(newpath, executable, elen);
			if (file_exists(newpath)) {
				DEBUG_MSG("file %s exists\n",newpath);
				return newpath;
			}
			free(newpath);
			path++;
		}
	}
	syslog(LOG_DEBUG,"the requested executable %s is not found\n",executable);
	return NULL;
}
/* returns a NULL terminated array of strings */
char **expand_newargv(char *string) {
	wordexp_t p;
	wordexp(string, &p, 0);
	return p.we_wordv;
}

int main (int argc, char **argv) {
	Tiniparser *parser;
	const char *section;
	unsigned int section_pos, umaskval;
	struct passwd *pw;
	char *new, buffer[1024];
	char **paths = NULL;
	char ** newargv;
	struct group *gr;
	char *groupsec;

	
	DEBUG_MSG(PROGRAMNAME", started\n");
#ifndef HAVE_WORDEXP_H
	libc_argc = argc;
	libc_argv = argv;
#endif
	/* open the log facility */
	openlog(PROGRAMNAME, LOG_PID, LOG_AUTH);
	syslog(LOG_INFO, PROGRAMNAME", started");

	DEBUG_MSG(PROGRAMNAME" log started\n");

	gr = getgrgid(getgid());
	pw = getpwuid(getuid());
	if (!pw || !gr) {
		syslog(LOG_ERR, "uid %d or gid %d does not have a name", getuid(), getgid());
		DEBUG_MSG(PROGRAMNAME" cannot get user or group info for %d:%d\n", getuid(),getgid());
		exit(2);
	}
	
	if (argc != 3 || strcmp(argv[1],"-c")!=0) {
		DEBUG_MSG("WARNING: user %s (%d) tried to get an interactive shell session, which is never allowed by jk_lsh\n", pw->pw_name, getuid());
		syslog(LOG_ERR, "WARNING: user %s (%d) tried to get an interactive shell session, which is never allowed by jk_lsh", pw->pw_name, getuid());
		exit(7);
	}
	
	/* start the config parser */
	parser = new_iniparser(CONFIGFILE);
	if (!parser) {
		syslog(LOG_ERR, "configfile "CONFIGFILE" is not available");
		DEBUG_MSG(PROGRAMNAME" configfile missing\n");
		exit(1);
	}
	/* check if this user has a section */

	groupsec = strcat(strcpy(malloc0(strlen(gr->gr_name)+6), "group "), gr->gr_name);
	if (iniparser_has_section(parser, pw->pw_name)) {
		section = pw->pw_name;
	} else if (iniparser_has_section(parser, groupsec)) {
		section = groupsec;
	} else {
		syslog(LOG_ERR, "both the user %s and the group %s have no section in the configfile "CONFIGFILE, pw->pw_name, gr->gr_name);
		exit(3);
	}
	section_pos = iniparser_get_position(parser) - strlen(section) - 2;
	section_pos = section_pos >= 0 ? section_pos : 0;
	DEBUG_MSG("using section %s\n",section);
	
	DEBUG_MSG("setting umask\n");
	umaskval = iniparser_get_octalint_at_position(parser, section, "umask", section_pos);
	if (umaskval != -1) {
		mode_t oldumask;
		oldumask = umask(umaskval);
/*		syslog(LOG_DEBUG, "changing umask from 0%o to 0%o", oldumask, umaskval);*/
	}
	
	DEBUG_MSG("exploding string '%s'\n",argv[2]);
	if (iniparser_get_int_at_position(parser, section, "allow_word_expansion", section_pos)) {
		newargv = expand_newargv(argv[2]);
	} else {
		newargv = explode_string(argv[2], ' ');
	}
	if (iniparser_get_string_at_position(parser, section, "paths", section_pos, buffer, 1024) > 0) {
		DEBUG_LOG("paths, buffer=%s\n",buffer);
		paths = explode_string(buffer, ',');
	} else {
		DEBUG_LOG("no key paths found\n");
	}
	DEBUG_LOG("paths=%p, newargv[0]=%s",paths,newargv[0]);
	new = expand_executable_w_path(newargv[0], paths);
	free_array(paths);
	if (new) {
		free(newargv[0]);
		newargv[0] = new;
	}
	if (executable_is_allowed(parser, section, newargv[0])) {
		int retval;
		DEBUG_MSG("executing command '%s' for user %s (%d)\n", newargv[0],pw->pw_name, getuid());
		syslog(LOG_INFO, "executing command '%s' for user %s (%d)", newargv[0],pw->pw_name, getuid());
		retval = execve(newargv[0],newargv,environ);
		DEBUG_MSG("errno=%d, error=%s\n",errno,strerror(errno));
		DEBUG_MSG("execve() command '%s' returned %d\n", newargv[0], retval);
		syslog(LOG_ERR, "WARNING: running %s failed for user %s (%d): %s", newargv[0],pw->pw_name, getuid(), strerror(retval));
		syslog(LOG_ERR, "WARNING: check the permissions and libraries for %s", newargv[0]);
		return retval;
	} else {
		DEBUG_MSG("WARNING: user %s (%d) tried to run '%s'\n", pw->pw_name, getuid(),newargv[0]);
		syslog(LOG_ERR, "WARNING: user %s (%d) tried to run '%s', which is not allowed according to "CONFIGFILE, pw->pw_name, getuid(),newargv[0]);
		exit(4);
	}

	return 0;
}
