/*
 * Copyright (C) Olivier Sessink 2003
 */
/* #define DEBUG */

#define _GNU_SOURCE

#include <pthread.h>
#include <sys/types.h> /* socket() */
#include <sys/socket.h> /* socket() */
#include <sys/times.h> /* times() */
#include <unistd.h> /* sysconf(), getopt() */
#include <getopt.h>
#include <time.h> /* nanosleep() */
#include <stdlib.h> /* malloc() */
#include <string.h> /* strcpy() */
#include <fcntl.h> /* fcntl() */
#include <stdio.h> /* DEBUG_MSG() */
#include <errno.h> /* errno */
#include <sys/un.h> /* struct sockaddr_un */
#include <sys/time.h> /* gettimeofday() */
#include <syslog.h> /* syslog() */
#include <signal.h> /* signal() */
#include <pwd.h> /* getpwnam() */
#include <sys/stat.h> /* chmod() */

#define PROGRAMNAME "jk_socketd"
#define CONFIGFILE "/etc/jailkit/jk_socketd.ini"

#define MAX_SOCKETS 32
#define CHECKTIME 100000 /* 0.1 seconds */
#define FULLSECOND 1000000
#define MILLISECOND 1000
#define MICROSECOND 1

#include "jk_lib.h"
#include "iniparser.h"

typedef struct {
	pthread_t thread;
	char *outpath;
	char *inpath;
	int normrate;
	int peekrate;
	int roundtime;

	int outsocket;
	int insocket;

	int lastwaspeek; /* the previous round was a peekround */
	struct timeval lasttime; /* last time the socket was checked */
	struct timeval lastreset; /* last time that lastsize was set to zero */
	int lastsize; /* bytes since lastreset */
} Tsocketlink;

/* the only global variable */
int do_clean_exit = 0;

static void close_socketlink(Tsocketlink *sl) {
	close(sl->insocket);
	shutdown(sl->insocket,2);
	free(sl->inpath);
	free(sl);
}

static void clean_exit(int numsockets, Tsocketlink **sl) {
	int i;
	for (i=0;i<numsockets;i++) {
		close_socketlink(sl[i]);
	}
}

static Tsocketlink *new_socketlink(int outsocket, char *inpath, int normrate, int peekrate, int roundtime, int nodetach) {
	Tsocketlink *sl;
/*	int flags;*/
	int ret;
	struct sockaddr_un serv_addr;

	sl = malloc(sizeof(Tsocketlink));
	sl->outsocket = outsocket;
	sl->inpath = strdup(inpath);
	sl->normrate = normrate;
	sl->peekrate = peekrate;
	sl->roundtime = roundtime;

	sl->insocket = socket(PF_UNIX, SOCK_DGRAM, 0);
/*	DEBUG_MSG("new_socketlink, insocketserver %s at %d\n", sl->inpath, sl->insocket);*/

	strncpy(serv_addr.sun_path, sl->inpath, sizeof(serv_addr.sun_path));
	serv_addr.sun_family = PF_UNIX;
	unlink(sl->inpath);
	ret = bind(sl->insocket, (struct sockaddr *)&serv_addr, sizeof(serv_addr));
	if (ret != 0 || chmod(sl->inpath, 0666)) {
		DEBUG_MSG("bind returned erno %d: %s\n",errno, strerror(errno));
		syslog(LOG_CRIT, "while opening %s: %s", sl->inpath, strerror(errno));
		if (nodetach) printf("while opening %s: %s\n",sl->inpath,strerror(errno));
		close_socketlink(sl);
		return NULL;
	}
	
/*	flags = fcntl(sl->insocket, F_GETFL, 0);
	fcntl(sl->insocket, F_SETFL, O_NONBLOCK|flags);*/
	
	gettimeofday(&sl->lastreset,NULL);
	return sl;
}

static void sleepround(long microseconds, int debug) {
	if (microseconds > 0) {
		struct timespec sleeptime;
		sleeptime.tv_sec = microseconds / FULLSECOND;
		sleeptime.tv_nsec = (microseconds % FULLSECOND)*1000;
		
		DEBUG_MSG("sleepround, sleeping %d milliseconds\n", (int)(microseconds / MILLISECOND));
		nanosleep(&sleeptime, NULL);
		/*pthread_delay_np(&sleeptime);*/
	}
}

/* return time difference in micro-seconds */
long timediff(struct timeval start, struct timeval end) {
	return (long) (end.tv_sec - start.tv_sec) * FULLSECOND + (end.tv_usec - start.tv_usec);
}

#define BUFSIZE 512

static void socketlink_handle(Tsocketlink *sl) {
	while (do_clean_exit == 0) {
		char *buf[BUFSIZE];
		int numbytes;
		numbytes = recvfrom(sl->insocket, &buf, BUFSIZE, 0, NULL, 0);
		/* numbytes = read(sl->insocket, &buf, BUFSIZE); */
		if (numbytes < 0) {
			DEBUG_MSG("recvfrom error %d: %s\n", errno, strerror(errno));
		} else if (numbytes > 0) {
			if (send(sl->outsocket, &buf, numbytes, 0) != numbytes) {
				DEBUG_MSG("send error %d: %s\n", errno, strerror(errno));
			}
			/* write(sl->outsocket, &buf, numbytes); */
			gettimeofday(&sl->lasttime,NULL);
			sl->lastsize += numbytes;
			DEBUG_MSG("lastsize=%d\n",sl->lastsize);
			if (sl->lastsize > sl->peekrate) {
				/* size is over the peekrate, mark this round as peek, and sleep the rest of the second */
				DEBUG_MSG("sleep, we're over peekrate!! (size=%d)\n",sl->lastsize);
				syslog(LOG_WARNING, "device %s is over the peek limit (%d bytes/s)", sl->inpath, (sl->peekrate * 1000000 / sl->roundtime));
				sleepround(sl->roundtime - timediff(sl->lastreset, sl->lasttime),1);
				sl->lastsize = 0;
				gettimeofday(&sl->lastreset,NULL);
				sl->lastwaspeek = 1;
				DEBUG_MSG("reset all to zero, peek=1\n");
			} else if (sl->lastsize > sl->normrate) {
				/* size is over the normal size, check if the time is also over the normal time */
				if (timediff(sl->lastreset, sl->lasttime) > sl->roundtime) {
					/* we will reset, the time is over a second */
					DEBUG_MSG("time is over a second (timediff=%ld), reset all to zero, peek=1\n", timediff(sl->lastreset, sl->lasttime));
					sl->lastsize = 0;
					gettimeofday(&sl->lastreset,NULL);
					sl->lastwaspeek = 1;
				} else {
					DEBUG_MSG("timediff = %ld\n",timediff(sl->lastreset, sl->lasttime));
					/* it is under a second, this is a peek, what to do now? */
					if (sl->lastwaspeek) {
						/* lastround was a peek, so this one is not allowed to be a peek, sleeping!! */
						DEBUG_MSG("sleep, previous was a peek and we're over the normal rate (size=%d)!\n", sl->lastsize);
						syslog(LOG_WARNING, "device %s is over the normal limit (%d bytes/s), directly after a peek", sl->inpath, (sl->normrate * 1000000 / sl->roundtime));
						sleepround(sl->roundtime - timediff(sl->lastreset, sl->lasttime),1);
						sl->lastsize = 0;
						gettimeofday(&sl->lastreset,NULL);
						sl->lastwaspeek = 1;
						DEBUG_MSG("reset all to zero, peek=1\n");
					} else {
						/* lastround was not a peek, so this round is allowed to be a peek */
						DEBUG_MSG("detected a new peek (size=%d)!\n", sl->lastsize);
					}
				}
			} else if (timediff(sl->lastreset, sl->lasttime) > sl->roundtime) {
				DEBUG_MSG("time is over a second (timediff=%ld), reset all to zero, peek=0\n", timediff(sl->lastreset, sl->lasttime));
				sl->lastsize = 0;
				gettimeofday(&sl->lastreset,NULL);
				sl->lastwaspeek = 0;
			}
		}
	}
}

static void sigterm_handler(int signal) {
	if (do_clean_exit != 1) {
		syslog(LOG_NOTICE, "got signal %d, exiting", signal);
		do_clean_exit = 1;
		raise(SIGTERM);
	}
}

static void usage() {
	printf(PROGRAMNAME" usage:\n\n");
	printf(" -n|--nodetach                do not detach from the terminal, useful for debugging\n");
	printf(" -p pidfile|--pidfile=pidfile write PID to file pidfile\n");
	printf(" -h|--help                    this help screen\n\n");
}

int main(int argc, char**argv) {
	Tsocketlink *sl[MAX_SOCKETS];
	
/*	struct timeval startround, endround;*/
	int numsockets = 0;
	int outsocket;
	int i;
	int nodetach = 0;
	char *pidfile = NULL;
	FILE *pidfilefd = NULL;

	signal(SIGINT, sigterm_handler);
	signal(SIGTERM, sigterm_handler);

	{
		int c;
		while (1) {
			int option_index = 0;
			static struct option long_options[] = {
				{"pidfile", required_argument, NULL, 0},
				{"nodetach", no_argument, NULL, 0},
				{"help", no_argument, NULL, 0},
				{NULL, 0, NULL, 0}
			};
		 	c = getopt_long(argc, argv, "p:nh",long_options, &option_index);
			if (c == -1)
				break;
			switch (c) {
			case 0:
				switch (option_index) {
				case 0:
					pidfile = strdup(optarg);
					break;
				case 1:
					nodetach = 1;
					break;
				case 2:
					usage();
					exit(0);
				}
				break;
			case 'p':
				pidfile = strdup(optarg);
				break;
			case 'n':
				nodetach = 1;
				break;
			case 'h':
				usage();
				exit(0);
			}
		}
	}
	openlog(PROGRAMNAME, LOG_PID, LOG_DAEMON);
	
	outsocket = socket(AF_UNIX, SOCK_DGRAM, 0);
	DEBUG_MSG("outsocket at %d\n", outsocket);
	{
		struct sockaddr client_addr;
		strncpy(client_addr.sa_data, "/dev/log", sizeof(client_addr.sa_data));
		client_addr.sa_family = AF_UNIX;
		if (connect(outsocket, &client_addr, sizeof(client_addr)) != 0) {
			/*DEBUG_MSG("connect returned erno %d: %s\n",errno, strerror(errno));*/
			syslog(LOG_CRIT, "while connecting to /dev/log: %s", strerror(errno));
			if (nodetach) printf("while connecting to /dev/log: %s\n",strerror(errno) );
			close(outsocket);
			exit(1);
		} else {
			if (nodetach) printf("opened /dev/log\n");
		}
	}
	
	{
		char buf[1024], *tmp;
		Tiniparser *ip = new_iniparser(CONFIGFILE);
		if (!ip) {
			syslog(LOG_CRIT, "abort, could not parse configfile "CONFIGFILE);
			if (nodetach) printf("abort, could not parse configfile "CONFIGFILE"\n");
			exit(11);
		}
		while (tmp = iniparser_next_section(ip, buf, 1024)) {
			int base=511, peek=2048, interval;
			long prevpos, secpos;
			prevpos = iniparser_get_position(ip);
			secpos = prevpos - strlen(tmp)-4;
			DEBUG_MSG("secpos=%d, prevpos=%d\n",secpos,prevpos);
			base = iniparser_get_int(ip, tmp, "base");
			iniparser_set_position(ip, secpos);
			peek = iniparser_get_int(ip, tmp, "peek");
			iniparser_set_position(ip, secpos);
			interval = iniparser_get_int(ip, tmp, "interval");
			iniparser_set_position(ip, secpos);
			if (10 > base || base >  10000) base = 511;
			if (100 > peek || peek > 100000 || peek < base) peek = 2048;
			if (1 > interval || interval > 60) interval = 10;
			sl[numsockets] = new_socketlink(outsocket, tmp, base, peek, interval*1000000, nodetach);
			if (sl[numsockets]) {
				syslog(LOG_NOTICE, "listening on socket %s with rates [%d:%d]/%d",tmp,base,peek,interval);
				if (nodetach) printf("listening on socket %s with rates [%d:%d]/%d\n",tmp,base,peek,interval);
				numsockets++;
			} else {
				if (nodetach) printf("failed to create socket %s\n",tmp);
			}
			DEBUG_MSG("setting position to %d\n",prevpos);
			iniparser_set_position(ip, prevpos);
		}
	}
	
	if (numsockets == 0) {
		printf("refusing to run without any sockets, aborting...\n");
		syslog(LOG_ERR,"refusing to run without any sockets, aborting...");
		exit(1);
	}

	if (pidfile) pidfilefd = fopen(pidfile, "w");

	{
		struct passwd *pw = getpwnam("nobody");
		if (setgid(pw->pw_gid) != 0 || setuid(pw->pw_uid) != 0) {
			syslog(LOG_ERR, "failed to change to user nobody (uid=%d, gid=%d)", pw->pw_uid, pw->pw_gid);
			if (nodetach) printf("failed to change to user nobody (uid=%d, gid=%d)\n", pw->pw_uid, pw->pw_gid);
		}
	}

	if (!nodetach) {
		/* detach and set the detached process as the new process group leader */
		if (fork() != 0) {
			exit(0);
		}
		setsid();
	}
	if (pidfile) {
		if (pidfilefd) {
			char buf[32];
			int size;
			size = snprintf(buf, 32, "%d", getpid());
			fwrite(buf, size, sizeof(char), pidfilefd);
			fclose(pidfilefd);
		} else {
			syslog(LOG_NOTICE, "failed to write pid to %s", pidfile);
		}
	}	
	
	for (i=0;i<numsockets;i++) {
		pthread_create(&sl[i]->thread, NULL,(void*)&socketlink_handle, (void*) sl[i]);
	}
	pause();
	DEBUG_MSG("before clean_exit\n");
	clean_exit(numsockets,sl);
	DEBUG_MSG("after clean_exit\n");
	if (nodetach) printf("caught signal, exiting\n");
	exit(0);
}
