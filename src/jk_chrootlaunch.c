/*
 * the jailkit chroot() launcher
 * this program does a chroot(), changes uid and gid and then executes the daemon
 *
 * I tried to merge some of the ideas from chrsh by Aaron D. Gifford, 
 * start-stop-daemon from Marek Michalkiewicz and suexec by the Apache 
 * group in this utility
 *
 * Copyright (C) Olivier Sessink 2003
 */

#define _GNU_SOURCE

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/errno.h>
#include <syslog.h>
#include <getopt.h>
#include <grp.h>
#include <pwd.h>

#include "jk_lib.h"

#define PROGRAMNAME "jk_chrootlaunch"

static int parse_uid(char *tmpstr) {
	struct passwd *pw=NULL;
	if (!tmpstr) return -1;
	if (tmpstr && tmpstr[0] >= '0' && tmpstr[0] <= '9') {
		int tmp = strtol(tmpstr, NULL, 10);
		if (tmp >= 0) {
			pw = getpwuid(tmp);
			if (!pw) {
				syslog(LOG_ERR, "abort, user '%s' does not exist (interpreted as uid %d)",tmpstr,tmp);
				exit(1);
			}
		} else {
			syslog(LOG_ERR, "abort, user '%s' is a negative uid (interpreted as %d)",tmpstr,tmp);
			exit(1);
		}
	} else {
		pw = getpwnam(tmpstr);
		if (!pw) {
			syslog(LOG_ERR, "abort, user %s does not exist",tmpstr);
			exit(1);
		}
	}
	return pw->pw_uid;
}

static int parse_gid(char *tmpstr) {
	struct group *gr=NULL;
	if (!tmpstr) return -1;
	if (tmpstr && tmpstr[0] >= '0' && tmpstr[0] <= '9') {
		int tmp = strtol(tmpstr, NULL, 10);
		if (tmp >= 0) {
			gr = getgrgid(tmp);
			if (!gr) {
				syslog(LOG_ERR, "abort, group '%s' does not exist (interpreted as gid %d)",tmpstr,tmp);
				exit(1);
			}
		} else {
			syslog(LOG_ERR, "abort, group '%s' is a negative gid (interpreted as %d)",tmpstr,tmp);
			exit(1);
		}
	} else {
		gr = getgrnam(tmpstr);
		if (!gr) {
			syslog(LOG_ERR, "abort, group %s does not exist",tmpstr);
			exit(1);
		}
	}
	return gr->gr_gid;
}

static char *ending_slash(const char *src) {
	int len;
	if (!src) return NULL;
	len = strlen(src);
	if (src[len-1] == '/') {
		return strdup(src);
	} else {
		return strcat(strcat(malloc0((len+1)*sizeof(char)), src), "/");
	}
}

/* tests the jail and executable, if they exists etc. 
returns a newly allocated executable relative to the chroot, 
so it can be used during exec() */
static char *test_jail_and_exec(char *jail, char *exec) {
	struct stat sbuf;
	char *tmpstr, *retval;
	if (!jail) {
		syslog(LOG_ERR,"abort, a jaildir must be specified on the commandline");
		exit(21);
	}
	if (!exec) {
		syslog(LOG_ERR,"abort, an executable must be specified on the commandline");
		exit(23);
	}
	if (jail[0] != '/') {
		syslog(LOG_ERR,"abort, jail '%s' not accepted, the jail must be an absolute path", jail);
		exit(27);
	}
	/* test the jail existance */
	if (lstat(jail, &sbuf) == 0) {
		if (S_ISLNK(sbuf.st_mode)) {
			syslog(LOG_ERR, "abort, jail %s is a symlink", jail);
			exit(27);
		}
		if (!(sbuf.st_mode & S_IFDIR)) {
			syslog(LOG_ERR, "abort, jail %s is not a directory", jail);
			exit(27);
		}
	} else {
		syslog(LOG_ERR, "jail %s does not exist",jail);
		exit(25);
	}
	/* test the executable, first we test if the executable was specified relative in the jail or absolute */
	if (strncmp(jail,exec,strlen(jail))==0) {
		/* the exec contains the path of the jail, so it was absolute */
		tmpstr = strdup(exec);
	} else {
		/* the executable was specified as relative path to the jail, combine them together */
		tmpstr = malloc0((strlen(exec)+strlen(jail))*sizeof(char));
		tmpstr = strcat(strcat(tmpstr, jail), exec);
	}
	if (lstat(tmpstr, &sbuf) == 0) {
		if (S_ISLNK(sbuf.st_mode)) {
			syslog(LOG_ERR, "abort, executable %s is a symlink", tmpstr);
			exit(29);
		}
		if (S_ISREG(sbuf.st_mode) && (sbuf.st_mode & (S_ISUID | S_ISGID))) {
			syslog(LOG_ERR, "abort, executable %s is setuid/setgid file", tmpstr);
			exit(29);
		}
		if (sbuf.st_mode & (S_IWGRP | S_IWOTH)) {
			syslog(LOG_ERR, "abort, executable %s is writable for group or others", tmpstr);
			exit(29);
		}
		if (sbuf.st_uid != 0 || sbuf.st_gid != 0) {
			syslog(LOG_ERR, "abort, executable %s is not owned root:root",tmpstr);
			exit(29);
		}
	} else {
		syslog(LOG_ERR, "executable %s does not exist",tmpstr);
		exit(29);
	}
	retval = strdup(&tmpstr[strlen(jail)]);
	free(tmpstr);
	return retval;
}

static void print_usage() {
	printf("\nUsage: "PROGRAMNAME" -j jaildir [-u user] [-g group] [-p pidfile] -x executable -- [executable options]\n");
	printf("\t-p|--pidfile pidfile\n");
	printf("\t-j|--jail jaildir\n");
	printf("\t-x|--exec executable\n");
	printf("\t-u|--user username|uid\n");
	printf("\t-g|--group group|gid\n");
	printf("\t-h|--help\n");
	printf(PROGRAMNAME" logs all errors to syslog, for diagnostics check your logfiles\n");
}

int main (int argc, char **argv) {
	char *pidfile=NULL, *jail=NULL, *exec=NULL;
	int uid=-1,gid=-1,i;
	char **newargv;

	openlog(PROGRAMNAME, LOG_PID, LOG_DAEMON);

	/* open file descriptors can be used to break out of a chroot, so we close all of them, except for stdin,stdout and stderr */
	for (i=getdtablesize();i>3;i--) {
		while (close(i) != 0 && errno == EINTR);
	}
	
	{
		int c=0;
		char *tuser=NULL, *tgroup=NULL, *texec=NULL;
		while (c != -1) {
			int option_index = 0;
			static struct option long_options[] = {
				{"pidfile", required_argument, NULL, 'p'},
				{"jail", required_argument, NULL, 'j'},
				{"exec", required_argument, NULL, 'x'},
				{"user", required_argument, NULL, 'u'},
				{"group", required_argument, NULL, 'g'},
				{"help", no_argument, NULL, 'h'},
				{NULL, 0, NULL, 0}
			};
		 	c = getopt_long(argc, argv, "j:p:u:g:x:h",long_options, &option_index);
			switch (c) {
			case 'j':
				jail = ending_slash(optarg);
				break;
			case 'p':
				pidfile = strdup(optarg);
				break;
			case 'u':
				tuser = strdup(optarg);
				break;
			case 'g':
				tgroup = strdup(optarg);
				break;
			case 'x':
				texec = strdup(optarg);
				break;
			case 'h':
				print_usage();
				exit(1);
			}
		}
		uid = parse_uid(tuser);
		gid = parse_gid(tgroup);
		exec = test_jail_and_exec(jail,texec);
		/* construct the new argv from all leftover options */
		newargv = malloc0((2 + argc - optind)*sizeof(char *));
		newargv[0] = exec;
		c = 1;
		while (optind < argc) {
			newargv[c] = strdup(argv[optind]);
			c++;
			optind++;
		}
		free(tuser);
		free(tgroup);
		free(texec);
	}
	
	if (pidfile) {
		FILE *pidfilefd = fopen(pidfile, "w");
		if (pidfilefd) {
			int pid = getpid();
			fscanf(pidfilefd,"%d",&pid);
			fclose(pidfilefd);
		} else {
			syslog(LOG_NOTICE, "failed to write PID into %s", pidfile);
		}
	}
	
	if (chdir(jail)) {
		syslog(LOG_ERR, "abort, could not change directory chdir() to the jail %s", jail);
		exit(33);
	}
	if (chroot(jail)) {
		syslog(LOG_ERR, "abort, could not change root chroot() to the jail %s", jail);
		exit(35);
	}
	if (gid != -1 && setgid(gid)) {
		syslog(LOG_ERR, "abort, could not setgid %d", gid);
		exit(37);
	}
	if (uid != -1 && setuid(uid)) {
		syslog(LOG_ERR, "abort, could not setuid %d", uid);
		exit(39);
	}
	syslog(LOG_NOTICE,"executing %s in jail %s",exec,jail);
	execv(exec, newargv);
	syslog(LOG_ERR, "error: failed to execute %s in jail %s",exec,jail);
	exit(31);
}