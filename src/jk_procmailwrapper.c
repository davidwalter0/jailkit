/*
 * Copyright (C) Olivier Sessink 2002-2004
 *
 * jk_procmailwrapper
 * this program will simply execute procmail for users that are not in a jail
 * and it will exit() for users that are in a jail (mail will *not* be delivered)
 *
 * this will probably extended in the near future
 */

#include <string.h>
#include <stdio.h>
#include <pwd.h>
#include <sys/types.h>
#include <unistd.h>
#include <stdlib.h>

#include "config.h"

int user_is_chrooted(const char *homedir) {
	char *tmp;
	tmp = strchr(homedir, '.');
	if (tmp != NULL) {
		return 1;
	}
	return 0;
}

void clean_exit(char * name, int error) {
	printf("%s, exiting with error %d\n", name, error);
	exit(error);
}

int main(int argc, char *argv[], char *envp[]) {
	struct passwd *pw;
	uid_t uid;
	uid = getuid();
	if ((pw = getpwuid(uid)) == NULL) {
		clean_exit(argv[0], 131);
	}
	if (user_is_chrooted(pw->pw_dir)) {
		/* would it be nice if we can do a chroot() and then execute procmail 
		within the chroot with the right user/group permissions? we'll work on that
		for the next jailkit release.. */
		clean_exit(argv[0],111);
	} else {
		execve(PROCMAILPATH, argv, envp);
	}
	exit(0);
}
