/*
 * dhcpcd - DHCP client daemon
 * Copyright (c) 2006-2015 Roy Marples <roy@marples.name>
 * All rights reserved

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

#include <sys/time.h>

#include <errno.h>
#include <limits.h>
#include <poll.h>
#include <signal.h>
#include <stdlib.h>
#include <syslog.h>

#define IN_ELOOP

#include "config.h"
#include "eloop.h"

#ifndef TIMEVAL_TO_TIMESPEC
#define	TIMEVAL_TO_TIMESPEC(tv, ts) do {				\
	(ts)->tv_sec = (tv)->tv_sec;					\
	(ts)->tv_nsec = (tv)->tv_usec * 1000;				\
} while (0 /* CONSTCOND */)
#endif

/* Handy function to get the time.
 * We only care about time advancements, not the actual time itself
 * Which is why we use CLOCK_MONOTONIC, but it is not available on all
 * platforms.
 */
#define NO_MONOTONIC "host does not support a monotonic clock - timing can skew"
static int
get_monotonic(struct timeval *tp)
{
#if defined(_POSIX_MONOTONIC_CLOCK) && defined(CLOCK_MONOTONIC)
	struct timespec ts;

	if (clock_gettime(CLOCK_MONOTONIC, &ts) == 0) {
		tp->tv_sec = ts.tv_sec;
		tp->tv_usec = (suseconds_t)(ts.tv_nsec / 1000);
		return 0;
	}
#elif defined(__APPLE__)
#define NSEC_PER_SEC 1000000000
	/* We can use mach kernel functions here.
	 * This is crap though - why can't they implement clock_gettime?*/
	static struct mach_timebase_info info = { 0, 0 };
	static double factor = 0.0;
	uint64_t nano;
	long rem;

	if (!posix_clock_set) {
		if (mach_timebase_info(&info) == KERN_SUCCESS) {
			factor = (double)info.numer / (double)info.denom;
			clock_monotonic = posix_clock_set = 1;
		}
	}
	if (clock_monotonic) {
		nano = mach_absolute_time();
		if ((info.denom != 1 || info.numer != 1) && factor != 0.0)
			nano *= factor;
		tp->tv_sec = nano / NSEC_PER_SEC;
		rem = nano % NSEC_PER_SEC;
		if (rem < 0) {
			tp->tv_sec--;
			rem += NSEC_PER_SEC;
		}
		tp->tv_usec = rem / 1000;
		return 0;
	}
#endif

#if 0
	/* Something above failed, so fall back to gettimeofday */
	if (!posix_clock_set) {
		syslog(LOG_WARNING, NO_MONOTONIC);
		posix_clock_set = 1;
	}
#endif
	return gettimeofday(tp, NULL);
}

static void
eloop_event_setup_fds(ELOOP_CTX *ctx)
{
	struct eloop_event *e;
	size_t i;

	i = 0;
	TAILQ_FOREACH(e, &ctx->events, next) {
		ctx->fds[i].fd = e->fd;
		ctx->fds[i].events = 0;
		if (e->read_cb)
			ctx->fds[i].events |= POLLIN;
		if (e->write_cb)
			ctx->fds[i].events |= POLLOUT;
		ctx->fds[i].revents = 0;
		e->pollfd = &ctx->fds[i];
		i++;
	}
}

int
eloop_event_add(ELOOP_CTX *ctx, int fd,
    void (*read_cb)(void *), void *read_cb_arg,
    void (*write_cb)(void *), void *write_cb_arg)
{
	struct eloop_event *e;
	struct pollfd *nfds;

	/* We should only have one callback monitoring the fd */
	TAILQ_FOREACH(e, &ctx->events, next) {
		if (e->fd == fd) {
			if (read_cb) {
				e->read_cb = read_cb;
				e->read_cb_arg = read_cb_arg;
			}
			if (write_cb) {
				e->write_cb = write_cb;
				e->write_cb_arg = write_cb_arg;
			}
			eloop_event_setup_fds(ctx);
			return 0;
		}
	}

	/* Allocate a new event if no free ones already allocated */
	if ((e = TAILQ_FIRST(&ctx->free_events))) {
		TAILQ_REMOVE(&ctx->free_events, e, next);
	} else {
		e = malloc(sizeof(*e));
		if (e == NULL) {
			syslog(LOG_ERR, "%s: %m", __func__);
			return -1;
		}
	}

	/* Ensure we can actually listen to it */
	ctx->events_len++;
	if (ctx->events_len > ctx->fds_len) {
		ctx->fds_len += 5;
		nfds = malloc(sizeof(*ctx->fds) * (ctx->fds_len + 5));
		if (nfds == NULL) {
			syslog(LOG_ERR, "%s: %m", __func__);
			ctx->events_len--;
			TAILQ_INSERT_TAIL(&ctx->free_events, e, next);
			return -1;
		}
		ctx->fds_len += 5;
		free(ctx->fds);
		ctx->fds = nfds;
	}

	/* Now populate the structure and add it to the list */
	e->fd = fd;
	e->read_cb = read_cb;
	e->read_cb_arg = read_cb_arg;
	e->write_cb = write_cb;
	e->write_cb_arg = write_cb_arg;
	/* The order of events should not matter.
	 * However, some PPP servers love to close the link right after
	 * sending their final message. So to ensure dhcpcd processes this
	 * message (which is likely to be that the DHCP addresses are wrong)
	 * we insert new events at the queue head as the link fd will be
	 * the first event added. */
	TAILQ_INSERT_HEAD(&ctx->events, e, next);
	eloop_event_setup_fds(ctx);
	return 0;
}

void
eloop_event_delete(ELOOP_CTX *ctx, int fd, int write_only)
{
	struct eloop_event *e;

	TAILQ_FOREACH(e, &ctx->events, next) {
		if (e->fd == fd) {
			if (write_only) {
				e->write_cb = NULL;
				e->write_cb_arg = NULL;
			} else {
				TAILQ_REMOVE(&ctx->events, e, next);
				TAILQ_INSERT_TAIL(&ctx->free_events, e, next);
				ctx->events_len--;
			}
			eloop_event_setup_fds(ctx);
			break;
		}
	}
}

int
eloop_q_timeout_add_tv(ELOOP_CTX *ctx, int queue,
    const struct timeval *when, void (*callback)(void *), void *arg)
{
	struct timeval now;
	struct timeval w;
	struct eloop_timeout *t, *tt = NULL;

	get_monotonic(&now);
	timeradd(&now, when, &w);
	/* Check for time_t overflow. */
	if (timercmp(&w, &now, <)) {
		errno = ERANGE;
		return -1;
	}

	/* Remove existing timeout if present */
	TAILQ_FOREACH(t, &ctx->timeouts, next) {
		if (t->callback == callback && t->arg == arg) {
			TAILQ_REMOVE(&ctx->timeouts, t, next);
			break;
		}
	}

	if (t == NULL) {
		/* No existing, so allocate or grab one from the free pool */
		if ((t = TAILQ_FIRST(&ctx->free_timeouts))) {
			TAILQ_REMOVE(&ctx->free_timeouts, t, next);
		} else {
			t = malloc(sizeof(*t));
			if (t == NULL) {
				syslog(LOG_ERR, "%s: %m", __func__);
				return -1;
			}
		}
	}

	t->when.tv_sec = w.tv_sec;
	t->when.tv_usec = w.tv_usec;
	t->callback = callback;
	t->arg = arg;
	t->queue = queue;

	/* The timeout list should be in chronological order,
	 * soonest first. */
	TAILQ_FOREACH(tt, &ctx->timeouts, next) {
		if (timercmp(&t->when, &tt->when, <)) {
			TAILQ_INSERT_BEFORE(tt, t, next);
			return 0;
		}
	}
	TAILQ_INSERT_TAIL(&ctx->timeouts, t, next);
	return 0;
}

int
eloop_q_timeout_add_sec(ELOOP_CTX *ctx, int queue, time_t when,
    void (*callback)(void *), void *arg)
{
	struct timeval tv;

	tv.tv_sec = when;
	tv.tv_usec = 0;
	return eloop_q_timeout_add_tv(ctx, queue, &tv, callback, arg);
}

int
eloop_timeout_add_now(ELOOP_CTX *ctx,
    void (*callback)(void *), void *arg)
{

	if (ctx->timeout0 != NULL) {
		syslog(LOG_WARNING, "%s: timeout0 already set", __func__);
		return eloop_q_timeout_add_sec(ctx, 0, 0, callback, arg);
	}

	ctx->timeout0 = callback;
	ctx->timeout0_arg = arg;
	return 0;
}

void
eloop_q_timeout_delete(ELOOP_CTX *ctx, int queue,
    void (*callback)(void *), void *arg)
{
	struct eloop_timeout *t, *tt;

	TAILQ_FOREACH_SAFE(t, &ctx->timeouts, next, tt) {
		if ((queue == 0 || t->queue == queue) &&
		    t->arg == arg &&
		    (!callback || t->callback == callback))
		{
			TAILQ_REMOVE(&ctx->timeouts, t, next);
			TAILQ_INSERT_TAIL(&ctx->free_timeouts, t, next);
		}
	}
}

void
eloop_exit(ELOOP_CTX *ctx, int code)
{

	ctx->exitcode = code;
	ctx->exitnow = 1;
}

ELOOP_CTX *
eloop_init(void)
{
	ELOOP_CTX *ctx;

	ctx = calloc(1, sizeof(*ctx));
	if (ctx) {
		TAILQ_INIT(&ctx->events);
		TAILQ_INIT(&ctx->free_events);
		TAILQ_INIT(&ctx->timeouts);
		TAILQ_INIT(&ctx->free_timeouts);
		ctx->exitcode = EXIT_FAILURE;
	}
	return ctx;
}


void eloop_free(ELOOP_CTX *ctx)
{
	struct eloop_event *e;
	struct eloop_timeout *t;

	if (ctx == NULL)
		return;

	while ((e = TAILQ_FIRST(&ctx->events))) {
		TAILQ_REMOVE(&ctx->events, e, next);
		free(e);
	}
	while ((e = TAILQ_FIRST(&ctx->free_events))) {
		TAILQ_REMOVE(&ctx->free_events, e, next);
		free(e);
	}
	while ((t = TAILQ_FIRST(&ctx->timeouts))) {
		TAILQ_REMOVE(&ctx->timeouts, t, next);
		free(t);
	}
	while ((t = TAILQ_FIRST(&ctx->free_timeouts))) {
		TAILQ_REMOVE(&ctx->free_timeouts, t, next);
		free(t);
	}
	free(ctx->fds);
	free(ctx);
}

int
eloop_start(ELOOP_CTX *ctx)
{
	struct timeval now;
	int n;
	struct eloop_event *e;
	struct eloop_timeout *t;
	struct timeval tv;
	struct timespec ts, *tsp;
	void (*t0)(void *);
	int timeout;

	for (;;) {
		if (ctx->exitnow)
			break;

		/* Run all timeouts first */
		if (ctx->timeout0) {
			t0 = ctx->timeout0;
			ctx->timeout0 = NULL;
			t0(ctx->timeout0_arg);
			continue;
		}
		if ((t = TAILQ_FIRST(&ctx->timeouts))) {
			get_monotonic(&now);
			if (timercmp(&now, &t->when, >)) {
				TAILQ_REMOVE(&ctx->timeouts, t, next);
				t->callback(t->arg);
				TAILQ_INSERT_TAIL(&ctx->free_timeouts, t, next);
				continue;
			}
			timersub(&t->when, &now, &tv);
			TIMEVAL_TO_TIMESPEC(&tv, &ts);
			tsp = &ts;
		} else
			/* No timeouts, so wait forever */
			tsp = NULL;

		if (tsp == NULL && ctx->events_len == 0) {
			syslog(LOG_ERR, "nothing to do");
			break;
		}

		if (tsp == NULL)
			timeout = -1;
		else if (tsp->tv_sec > INT_MAX / 1000 ||
		    (tsp->tv_sec == INT_MAX / 1000 &&
		    (tsp->tv_nsec + 999999) / 1000000 > INT_MAX % 1000000))
			timeout = INT_MAX;
		else
			timeout = (int)(tsp->tv_sec * 1000 +
			    (tsp->tv_nsec + 999999) / 1000000);
		n = poll(ctx->fds, ctx->events_len, timeout);
		if (n == -1) {
			if (errno == EINTR)
				continue;
			syslog(LOG_ERR, "poll: %m");
			break;
		}

		/* Process any triggered events. */
		if (n > 0) {
			TAILQ_FOREACH(e, &ctx->events, next) {
				if (e->pollfd->revents & POLLOUT &&
					e->write_cb)
				{
					e->write_cb(e->write_cb_arg);
					/* We need to break here as the
					 * callback could destroy the next
					 * fd to process. */
					break;
				}
				if (e->pollfd->revents) {
					e->read_cb(e->read_cb_arg);
					/* We need to break here as the
					 * callback could destroy the next
					 * fd to process. */
					break;
				}
			}
		}
	}

	return ctx->exitcode;
}
