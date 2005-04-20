/*
 * the jailkit chroot() shell
 * this program does a safe chroot() and then executes the shell
 * that the user has within that new root (according to newroot/etc/passwd)
 *
 * I tried to merge some of the ideas from chrsh by Aaron D. Gifford, 
 * start-stop-daemon from Marek Michalkiewicz and suexec by the Apache 
 * group in this shell
 *
 * Copyright (C) Olivier Sessink 2003-2004
 *
 */

#include "config.h"

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <pwd.h>
#include <grp.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/errno.h>
#include <syslog.h>

/* #define DEBUG */

#ifdef DEBUG
#define DEBUG_MSG printf
#else
#define DEBUG_MSG(args...)
 /**/
#endif

#define PROGRAMNAME "jk_chrootsh"
#define CONFIGFILE "/etc/jailkit/jk_chrootsh.ini"

#include "jk_lib.h"
#include "utils.h"
#include "iniparser.h"

/* doesn't compile on FreeBSD without this */
extern char **environ;

/*
typedef struct {
	char *key;
	char *value;
} Tsavedenv;

static Tsavedenv *savedenv_new(const char *key) {
	Tsavedenv *savedenv;
	char *val = getenv(key);
	if (!val) return NULL;
	savedenv = malloc(sizeof(Tsavedenv));
	savedenv->key = strdup(key);
	savedenv->value = strdup(val);
	return savedenv;
}

static void savedenv_restore(Tsavedenv *savedenv) {
	if (savedenv) {
		setenv(savedenv->key, savedenv->value, 1);
		DEBUG_MSG("restored %s=%s\n",savedenv->key, savedenv->value);
	}
}

static void savedenv_free(Tsavedenv *savedenv) {
	if (savedenv) {
		free(savedenv->key);
		free(savedenv->value);
		free(savedenv);
	}
}
*/

static int in_array(char **haystack, char * needle) {
	if (haystack && needle) {
		char **tmp = haystack;
		while (*tmp) {
			if (strcmp(*tmp, needle)==0) return 1;
			tmp++;
		}
	}
	return 0;
}

static void unset_environ_except(char **except) {
	char **tmp = environ;
	while (*tmp) {
		char* pos = strchr(*tmp, '=');
		if (pos != NULL) {
			char *key = strndup(*tmp, pos-*tmp);
			if (in_array(except, key)) {
				DEBUG_MSG("%s is in except\n",key);
			} else {
				DEBUG_MSG("%s is NOT in except\n",key);
				unsetenv(key);
			}
			free(key);
		} else {
			DEBUG_MSG("problem with %s\n",*tmp);
		}
		tmp++;
	}
}


int main (int argc, char **argv) {
	int i;
	struct passwd *pw=NULL;
	struct group *gr=NULL;
	char *jaildir=NULL, *newhome=NULL;
	Tiniparser *parser=NULL;

	DEBUG_MSG(PROGRAMNAME", started\n");
	/* open the log facility */
	openlog(PROGRAMNAME, LOG_PID, LOG_AUTH);
	
	/* check if it us that the user wants */
	{
		char *tmp = strrchr(argv[0], '/');
		if (!tmp) {
			tmp = argv[0];
		} else {
			tmp++;
		}
		if (strcmp(tmp, PROGRAMNAME) && (tmp[0] != '-' || strcmp(&tmp[1], PROGRAMNAME))) {
			DEBUG_MSG("wrong name, tmp=%s, &tmp[1]=%s\n", tmp, &tmp[1]);
			syslog(LOG_ERR, "abort, "PROGRAMNAME" is called as %s", argv[0]);
			exit(1);
		}
	}
	DEBUG_MSG("close filedescriptors\n");
	/* open file descriptors can be used to break out of a chroot, so we close all of them, except for stdin,stdout and stderr */
	for (i=getdtablesize();i>3;i--) {
		while (close(i) != 0 && errno == EINTR);
	}

	/* now test if we are setuid root (the effective user id must be 0, and the real user id > 0 */
	if (geteuid() != 0) {
		syslog(LOG_ERR, "abort, effective user ID is not 0, possibly "PROGRAMNAME" is not setuid root");
		exit(11);
	}
	if (getuid() == 0) {
		syslog(LOG_ERR, "abort, "PROGRAMNAME" is run by root, which does not make sense");
		exit(12);
	}

	DEBUG_MSG("get user info\n");
	pw = getpwuid(getuid());
	if (!pw) {
		syslog(LOG_ERR, "abort, failed to get user information for user ID %d, check /etc/passwd", getuid());
		exit(13);
	}
	gr = getgrgid(getgid());
	if (!gr) {
		syslog(LOG_ERR, "abort, failed to get group information for group ID %d, check /etc/group", getgid());
		exit(13);
	}

	/* now we clear the environment, except for values allowed in /etc/jailkit/jk_chrootsh.ini */
	parser = new_iniparser(CONFIGFILE);
	if (parser) {
		char *groupsec, *section=NULL, buffer[1024];
		groupsec = strcat(strcpy(malloc0(strlen(gr->gr_name)+7), "group "), gr->gr_name);
		if (iniparser_has_section(parser, pw->pw_name)) {
			section = strdup(pw->pw_name);
		} else if (iniparser_has_section(parser, groupsec)) {
			section = groupsec;
		}
		if (section) {
			if (iniparser_get_string(parser, section, "env", buffer, 1024) > 0) {
				char **envs;
/*				int num,i;
				Tsavedenv **envstore;
				/ * there is a 'env' section for this user / this group * / */
				envs = explode_string(buffer, ',');
/*				num = count_array(envs);
				envstore = malloc0(num * sizeof(Tsavedenv *));
				for (i=0;i<num;i++) {
					envstore[i] = savedenv_new(envs[i]);
				}
				/ * clear the environment * /
				clearenv();
				for (i=0;i<num;i++) {
					savedenv_restore(envstore[i]);
					savedenv_free(envstore[i]);
				}
				free(envstore);
				*/
				unset_environ_except(envs);
			}
			free(section);
		}
	}
	if (pw->pw_gid != getgid()) {
		syslog(LOG_ERR, "abort, the group ID from /etc/passwd (%d) does not match the group ID we run with (%d)", pw->pw_gid, getgid());
		exit(15);
	}
	if (!pw->pw_dir || strlen(pw->pw_dir) ==0 || strstr(pw->pw_dir, "/./") == NULL) {
		syslog(LOG_ERR, "abort, the homedir in /etc/passwd does not contain the jail <jail>/./<home>");
		exit(17);
	}
	DEBUG_MSG("get jaildir\n");
	if (!getjaildir(pw->pw_dir, &jaildir, &newhome)) {
		syslog(LOG_ERR, "abort, failed to read the jail and the home from %s",pw->pw_dir);
		exit(17);
	}
	DEBUG_MSG("get chdir()\n");
	if (chdir(jaildir) != 0) {
		syslog(LOG_ERR, "abort, chdir(%s) failed, check the permissions for %s",jaildir,jaildir);
		exit(19);
	} else {
		char test[255];
		/* test if it really succeeded */
		getcwd(test, 255);
		if (strcmp(jaildir, test) != 0) {
			syslog(LOG_ERR, "abort, the current dir does not equal %s after chdir(%s)",jaildir,jaildir);
			exit(21);
		}
	}		
	
	/* here do test the ownership of the jail and the homedir and such
	the function testsafepath doe exit itself on any failure */
	DEBUG_MSG("test paths\n");
	testsafepath(jaildir,0,0);
	testsafepath(pw->pw_dir, getuid(), getgid());

	/* do a final log message */
	syslog(LOG_INFO, "now entering jail %s for user %s (%d)", jaildir, pw->pw_name, getuid());
	
	DEBUG_MSG("chroot()\n");
	/* do the chroot() call */
	if (chroot(jaildir)) {
		syslog(LOG_ERR, "abort, chroot(%s) failed, check the permissions for %s", jaildir, jaildir);
		exit(33);
	}
	
	/* drop all privileges, it seems that we first have to setgid(), 
		then we have to call initgroups(), 
		then we call setuid() */
	if (setgid(getgid())) {
		syslog(LOG_ERR, "abort, failed to set effective group ID %d", getgid());
		exit(34);
	}
	if (initgroups(pw->pw_name, getgid())) {
		syslog(LOG_ERR, "abort, failed to init groups for user %s (%d), check %s/etc/group", pw->pw_name,getuid(),jaildir);
		exit(35);
	}
	if (setuid(getuid())) {
		syslog(LOG_ERR, "abort, failed to set effective user ID %d", getuid());
		exit(36);
	}
	
	/* test for user and group info, is it the same? checks username, groupname and home */
	{
		char *oldpw_name,*oldgr_name;
		oldpw_name = strdup(pw->pw_name);
		oldgr_name = strdup(gr->gr_name);
		
		pw = getpwuid(getuid());
		gr = getgrgid(getgid());
		if (!pw) {
			syslog(LOG_ERR, "abort, failed to get user information in the jail for user ID %d, check %s/etc/passwd",getuid(),jaildir);
			exit(35);
		}
		if (!gr) {
			syslog(LOG_ERR, "abort, failed to get group information in the jail for group ID %d, check %s/etc/group",getgid(),jaildir);
			exit(35);
		}
		if (strcmp(pw->pw_name, oldpw_name)!=0) {
			syslog(LOG_ERR, "abort, username %s differs from jail username %s for user ID %d, check /etc/passwd and %s/etc/passwd", oldpw_name, pw->pw_name, getuid(), jaildir);
			exit(37);
		}
		if (strcmp(gr->gr_name, oldgr_name)!=0) {
			syslog(LOG_ERR, "abort, groupname %s differs from jail groupname %s for group ID %d, check /etc/passwd and %s/etc/passwd", oldgr_name, gr->gr_name, getgid(), jaildir);
			exit(37);
		}
		if (strcmp(pw->pw_dir, newhome)!=0) {
			syslog(LOG_ERR, "abort, home directory %s differs from jail home directory %s for user %s (%d), check /etc/passwd and %s/etc/passwd", newhome, pw->pw_dir, pw->pw_name, getuid(), jaildir);
			exit(39);
		}
		free(oldpw_name);
		free(oldgr_name);
	}
	
	/* test the shell in the jail, it is not allowed to be setuid() root */
	testsafepath(pw->pw_shell,0,0);
	
	/* prepare the new environment */
	setenv("HOME",newhome,1);
	setenv("USER",pw->pw_name,1);
	if (chdir(newhome) != 0) {
		syslog(LOG_ERR, "abort, chdir(%s) failed inside the jail %s, check the permissions for %s/%s",newhome,jaildir,jaildir,newhome);
		exit(41);
	}

	/* cleanup before execution */
	free(newhome);
	

	/* now execute the jailed shell */
	/*execl(pw->pw_shell, pw->pw_shell, NULL);*/
	{
		char **newargv;
		int i;
		newargv = malloc0((argc+1)*sizeof(char *));
		newargv[0] = pw->pw_shell;
		for (i=1;i<argc;i++) {
			newargv[i] = argv[i];
		}
		execv(pw->pw_shell, newargv);
	}
	DEBUG_MSG(strerror(errno));
	syslog(LOG_ERR, "ERROR: failed to execute shell %s for user %s (%d), check the permissions and libraries of %s/%s",pw->pw_shell,pw->pw_name,getuid(),jaildir,pw->pw_shell);

	free(jaildir);
	exit(111);
}
