#include <string.h>
#include <stdio.h>
#include <pwd.h>
#include <sys/types.h>
#include <unistd.h>

#define NORMAL_PROCMAIL "/usr/bin/procmail"

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
		clean_exit(argv[0],111);
	} else {
		execve(NORMAL_PROCMAIL, argv, envp);
	}
	exit(0);
}
