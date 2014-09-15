/*
 * dhcpcd-online
 * Copyright 2014 Roy Marples <roy@marples.name>
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <errno.h>
#include <limits.h>
#include <poll.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <syslog.h>
#include <time.h>
#include <unistd.h>

#include "dhcpcd.h"

#if __GNUC__ > 2 || defined(__INTEL_COMPILER)
# ifndef __dead
#  define __dead __attribute__((__noreturn__))
# endif
# ifndef __unused
#  define __unused   __attribute__((__unused__))
# endif
#else
# ifndef __dead
#  define __dead
# endif
# ifndef __unused
#  define __unused
# endif
#endif

#ifndef timespeccmp
#define	timespeccmp(tsp, usp, cmp)					\
	(((tsp)->tv_sec == (usp)->tv_sec) ?				\
	    ((tsp)->tv_nsec cmp (usp)->tv_nsec) :			\
	    ((tsp)->tv_sec cmp (usp)->tv_sec))
#endif
#ifndef timespecsub
#define	timespecsub(tsp, usp, vsp)					\
	do {								\
		(vsp)->tv_sec = (tsp)->tv_sec - (usp)->tv_sec;		\
		(vsp)->tv_nsec = (tsp)->tv_nsec - (usp)->tv_nsec;	\
		if ((vsp)->tv_nsec < 0) {				\
			(vsp)->tv_sec--;				\
			(vsp)->tv_nsec += 1000000000L;			\
		}							\
	} while (/* CONSTCOND */ 0)
#endif

static void __dead
do_exit(DHCPCD_CONNECTION *con, int code)
{

	/* Unregister the status callback so that close doesn't spam. */
	dhcpcd_set_status_callback(con, NULL, NULL);

	dhcpcd_close(con);
	dhcpcd_free(con);
	exit(code);
}

static void
do_status_cb(DHCPCD_CONNECTION *con, const char *status, void *arg)
{
	struct pollfd *pfd;

	syslog(LOG_INFO, "%s", status);
	if (strcmp(status, "connected") == 0)
		do_exit(con, EXIT_SUCCESS);
	if (strcmp(status, "down") == 0) {
		pfd = arg;
		pfd->fd = -1;
	}
}

int
main(int argc, char **argv)
{
	DHCPCD_CONNECTION *con;
	bool xflag;
	struct timespec now, end, t;
	struct pollfd pfd;
	int timeout, n, lerrno;
	long lnum;
	char *lend;

	/* Defaults */
	timeout = 30;

	xflag = false;

	openlog("dhcpcd-online", LOG_PERROR, 0);
	setlogmask(LOG_UPTO(LOG_INFO));

	while ((n = getopt(argc, argv, "qt:x")) != -1) {
		switch (n) {
		case 'q':
			closelog();
			openlog("dhcpcd-online", 0, 0);
			break;
		case 't':
			lnum = strtol(optarg, &lend, 0);
			if (lend == NULL || *lend != '\0' ||
			    lnum < 0 || lnum > INT_MAX)
			{
				syslog(LOG_ERR, "-t %s: invalid timeout",
				    optarg);
				exit(EXIT_FAILURE);
			}
			timeout = (int)lnum;
			break;
		case 'x':
			xflag = true;
			break;
		case '?':
			fprintf(stderr, "usage: dhcpcd-online "
			    "[-q] [-t timeout]\n");
			exit(EXIT_FAILURE);
		}
	}

	if ((con = dhcpcd_new()) == NULL) {
		syslog(LOG_ERR, "dhcpcd_new: %m");
		return EXIT_FAILURE;
	}

	dhcpcd_set_status_callback(con, do_status_cb, &pfd);

	if ((pfd.fd = dhcpcd_open(con, false)) == -1) {
		lerrno = errno;
		syslog(LOG_WARNING, "dhcpcd_open: %m");
		if (xflag)
			do_exit(con, EXIT_FAILURE);
	} else
		lerrno = 0;
	pfd.events = POLLIN;
	pfd.revents = 0;

	/* Work out our timeout time */
	if (clock_gettime(CLOCK_MONOTONIC, &end) == -1) {
		syslog(LOG_ERR, "clock_gettime: %m");
		do_exit(con, EXIT_FAILURE);
	}
	end.tv_sec += timeout;

	for (;;) {
		if (clock_gettime(CLOCK_MONOTONIC, &now) == -1) {
			syslog(LOG_ERR, "clock_gettime: %m");
			do_exit(con, EXIT_FAILURE);
		}
		if (timespeccmp(&now, &end, >)) {
			syslog(LOG_ERR, "timed out");
			do_exit(con, EXIT_FAILURE);
		}
		if (pfd.fd == -1) {
			n = poll(NULL, 0, DHCPCD_RETRYOPEN);
		} else {
			/* poll(2) should really take a timespec */
			timespecsub(&end, &now, &t);
			if (t.tv_sec > INT_MAX / 1000 ||
			    (t.tv_sec == INT_MAX / 1000 &&
			    (t.tv_nsec + 999999) / 1000000 > INT_MAX % 1000000))
				timeout = INT_MAX;
			else
				timeout = (int)(t.tv_sec * 1000 +
				    (t.tv_nsec + 999999) / 1000000);
			n = poll(&pfd, 1, timeout);
		}
		if (n == -1) {
			syslog(LOG_ERR, "poll: %m");
			do_exit(con, EXIT_FAILURE);
		}
		if (pfd.fd == -1) {
			if ((pfd.fd = dhcpcd_open(con, false)) == -1) {
				if (lerrno != errno) {
					lerrno = errno;
					syslog(LOG_WARNING, "dhcpcd_open: %m");
				}
			}
		} else {
			if (n > 0 && pfd.revents)
				dhcpcd_dispatch(con);
		}
	}

	/* Impossible to reach here */
	do_exit(con, EXIT_FAILURE);
}
