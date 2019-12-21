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

#include <sys/ioctl.h>

#include <curses.h>
#include <err.h>
#include <errno.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "dhcpcd-curses.h"

#ifdef HAVE_NC_FREE_AND_EXIT
	void _nc_free_and_exit(void);
#endif

static const int sigs[] = {
	SIGHUP,
	SIGINT,
	SIGPIPE,
	SIGTERM,
	SIGWINCH
};

#ifndef __arraycount
#define __arraycount(__x)       (sizeof(__x) / sizeof(__x[0]))
#endif

static void try_open(void *);

static void
set_status(struct ctx *ctx, const char *status)
{
	int w;
	size_t slen;

	w = getmaxx(ctx->win_status);
	w -= (int)(slen = strlen(status));
	if (ctx->status_len > slen) {
		wmove(ctx->win_status, 0, w - (int)(ctx->status_len - slen));
		wclrtoeol(ctx->win_status);
	}
	mvwprintw(ctx->win_status, 0, w, "%s", status);
	wrefresh(ctx->win_status);
	ctx->status_len = slen;
}

static int
set_summary(struct ctx *ctx, const char *msg)
{
	int r;

	wclear(ctx->win_summary);
	if (msg)
		r = wprintw(ctx->win_summary, "%s", msg);
	else
		r = 0;
	wrefresh(ctx->win_summary);
	return r;
}

__printflike(2, 3) static int
debug(struct ctx *ctx, const char *fmt, ...)
{
	va_list args;
	int r;

	if (ctx->win_debug == NULL)
		return 0;
	waddch(ctx->win_debug, '\n');
	va_start(args, fmt);
	r = vw_printw(ctx->win_debug, fmt, args);
	va_end(args);
	wrefresh(ctx->win_debug);
	return r;
}

__printflike(2, 3) static int
warning(struct ctx *ctx, const char *fmt, ...)
{
	va_list args;
	int r;

	if (ctx->win_debug == NULL)
		return 0;
	waddch(ctx->win_debug, '\n');
	va_start(args, fmt);
	r = vw_printw(ctx->win_debug, fmt, args);
	va_end(args);
	wrefresh(ctx->win_debug);
	return r;
}

__printflike(2, 3) static int
notify(struct ctx *ctx, const char *fmt, ...)
{
	va_list args;
	int r;

	if (ctx->win_debug == NULL)
		return 0;
	waddch(ctx->win_debug, '\n');
	va_start(args, fmt);
	r = vw_printw(ctx->win_debug, fmt, args);
	va_end(args);
	wrefresh(ctx->win_debug);
	return r;
}

static void
update_online(struct ctx *ctx, bool show_if)
{
	bool online, carrier;
	char *msg, *msgs, *nmsg;
	size_t msgs_len, mlen;
	DHCPCD_IF *ifs, *i;

	online = carrier = false;
	msgs = NULL;
	msgs_len = 0;
	ifs = dhcpcd_interfaces(ctx->con);
	for (i = ifs; i; i = i->next) {
		if (i->type == DHT_LINK) {
			if (i->up)
				carrier = true;
		} else {
			if (i->up)
				online = true;
		}
		msg = dhcpcd_if_message(i, NULL);
		if (msg) {
			if (show_if) {
				if (i->up)
					notify(ctx, "%s", msg);
				else
					warning(ctx, "%s", msg);
			}
			if (msgs == NULL) {
				msgs = msg;
				msgs_len = strlen(msgs) + 1;
			} else {
				mlen = strlen(msg) + 1;
				nmsg = realloc(msgs, msgs_len + mlen);
				if (nmsg) {
					msgs = nmsg;
					msgs[msgs_len - 1] = '\n';
					memcpy(msgs + msgs_len, msg, mlen);
					msgs_len += mlen;
				} else
					warn("realloc");
				free(msg);
			}
		} else if (show_if) {
			if (i->up)
				notify(ctx, "%s: %s", i->ifname, i->reason);
			else
				warning(ctx, "%s: %s", i->ifname, i->reason);
		}
	}

	set_summary(ctx, msgs);
	free(msgs);
}

static void
dispatch(void *arg)
{
	struct ctx *ctx = arg;

	dhcpcd_dispatch(ctx->con);
}

static void
try_open(void *arg)
{
	struct ctx *ctx = arg;
	static int last_error;
	int fd;

	fd = dhcpcd_open(ctx->con, true);
	if (fd == -1) {
		if (errno == EACCES || errno == EPERM) {
			fd = dhcpcd_open(ctx->con, false);
			if (fd != -1)
				goto unprived;
		}
		if (errno != last_error) {
			last_error = errno;
			set_status(ctx, strerror(errno));
		}
		eloop_timeout_add_msec(ctx->eloop, DHCPCD_RETRYOPEN,
		    try_open, ctx);
		return;
	}

unprived:
	last_error = 0;

	/* Start listening to WPA events */
	dhcpcd_wpa_start(ctx->con);

	eloop_event_add(ctx->eloop, fd, dispatch, ctx, NULL, NULL);
}

static void
status_cb(DHCPCD_CONNECTION *con,
    unsigned int status, const char *status_msg, void *arg)
{
	struct ctx *ctx = arg;

	debug(ctx, _("Status changed to %s"), status_msg);
	set_status(ctx, status_msg);

	if (status == DHC_DOWN) {
		int fd;

		fd = dhcpcd_get_fd(ctx->con);
		eloop_event_delete(ctx->eloop, fd);
		ctx->online = ctx->carrier = false;
		eloop_timeout_delete(ctx->eloop, NULL, ctx);
		set_summary(ctx, NULL);
		eloop_timeout_add_msec(ctx->eloop, DHCPCD_RETRYOPEN,
		    try_open, ctx);
	} else {
		bool refresh;

		if (ctx->last_status == DHC_UNKNOWN ||
		    ctx->last_status == DHC_DOWN)
		{
			debug(ctx, _("Connected to dhcpcd-%s"),
			    dhcpcd_version(con));
			refresh = true;
		} else
			refresh =
			    ctx->last_status == DHC_OPENED ? true : false;
		update_online(ctx, refresh);
	}

	ctx->last_status = status;
}

static void
if_cb(DHCPCD_IF *i, void *arg)
{
	struct ctx *ctx = arg;

	if (i->state == DHS_RENEW ||
	    i->state == DHS_STOP || i->state == DHS_STOPPED)
	{
		char *msg;
		bool new_msg;

		msg = dhcpcd_if_message(i, &new_msg);
		if (msg) {
			if (i->up)
				warning(ctx, "%s", msg);
			else
				notify(ctx, "%s", msg);
			free(msg);
		}
	}

	update_online(ctx, false);

	if (i->wireless) {
		/* PROCESS SCANS */
	}
}

static void
wpa_dispatch(void *arg)
{
	DHCPCD_WPA *wpa = arg;

	dhcpcd_wpa_dispatch(wpa);
}

static void
wpa_scan_cb(DHCPCD_WPA *wpa, void *arg)
{
	struct ctx *ctx = arg;
	DHCPCD_IF *i;
	WI_SCAN *wi;
	DHCPCD_WI_SCAN *scans, *s1, *s2;
	int fd, lerrno;

	/* This could be a new WPA so watch it */
	if ((fd = dhcpcd_wpa_get_fd(wpa)) == -1) {
		debug(ctx, "%s (%p)", _("no fd for WPA"), wpa);
		return;
	}
	eloop_event_add(ctx->eloop, fd, wpa_dispatch, wpa, NULL, NULL);

	i = dhcpcd_wpa_if(wpa);
	if (i == NULL) {
		debug(ctx, "%s (%p)", _("No interface for WPA"), wpa);
		return;
	}
	debug(ctx, "%s: %s", i->ifname, _("Received scan results"));
	lerrno = errno;
	errno = 0;
	scans = dhcpcd_wi_scans(i);
	if (scans == NULL && errno)
		debug(ctx, "%s: %s", i->ifname, strerror(errno));
	errno = lerrno;
	TAILQ_FOREACH(wi, &ctx->wi_scans, next) {
		if (wi->interface == i)
			break;
	}
	if (wi == NULL) {
		wi = malloc(sizeof(*wi));
		wi->interface = i;
		wi->scans = scans;
		TAILQ_INSERT_TAIL(&ctx->wi_scans, wi, next);
	} else {
		const char *title;
		char *msgs, *nmsg;
		size_t msgs_len, mlen;

		title = NULL;
		msgs = NULL;
		for (s1 = scans; s1; s1 = s1->next) {
			for (s2 = wi->scans; s2; s2 = s2->next)
				if (strcmp(s1->ssid, s2->ssid) == 0)
					break;
			if (s2 == NULL) {
				if (msgs == NULL) {
					msgs = strdup(s1->ssid);
					msgs_len = strlen(msgs) + 1;
				} else {
					if (title == NULL)
						title = _("New Access Points");
					mlen = strlen(s1->ssid) + 1;
					nmsg = realloc(msgs, msgs_len + mlen);
					if (nmsg) {
						msgs = nmsg;
						msgs[msgs_len - 1] = '\n';
						memcpy(msgs + msgs_len,
						    s1->ssid, mlen);
						msgs_len += mlen;
					} else
						warn("realloc");
				}
			}
		}
		if (msgs) {
			if (title == NULL)
				title = _("New Access Point");
			mlen = strlen(title) + 1;
			nmsg = realloc(msgs, msgs_len + mlen);
			if (nmsg) {
				msgs = nmsg;
				memmove(msgs + mlen, msgs, msgs_len);
				memcpy(msgs, title, mlen);
				msgs[mlen - 1] = '\n';
			} else
				warn("realloc");
			notify(ctx, "%s", msgs);
			free(msgs);
		}

		dhcpcd_wi_scans_free(wi->scans);
		wi->scans = scans;
	}
}

static void
wpa_status_cb(DHCPCD_WPA *wpa,
    unsigned int status, const char *status_msg, void *arg)
{
	struct ctx *ctx = arg;
	DHCPCD_IF *i;
	WI_SCAN *w, *wn;

	i = dhcpcd_wpa_if(wpa);
	debug(ctx, _("%s: WPA status %s"), i->ifname, status_msg);
	if (status == DHC_DOWN) {
		int fd;

		fd = dhcpcd_wpa_get_fd(wpa);
		eloop_event_delete(ctx->eloop, fd);
		dhcpcd_wpa_close(wpa);
		TAILQ_FOREACH_SAFE(w, &ctx->wi_scans, next, wn) {
			if (w->interface == i) {
				TAILQ_REMOVE(&ctx->wi_scans, w, next);
				dhcpcd_wi_scans_free(w->scans);
				free(w);
			}
		}
	}
}

static void
bg_scan(void *arg)
{
	struct ctx *ctx = arg;
	WI_SCAN *w;
	DHCPCD_WPA *wpa;

	TAILQ_FOREACH(w, &ctx->wi_scans, next) {
		if (w->interface->wireless& w->interface->up) {
			wpa = dhcpcd_wpa_find(ctx->con, w->interface->ifname);
			if (wpa &&
			    (!w->interface->up ||
			    dhcpcd_wpa_can_background_scan(wpa)))
				dhcpcd_wpa_scan(wpa);
		}
	}

	eloop_timeout_add_msec(ctx->eloop, DHCPCD_WPA_SCAN_SHORT,
	    bg_scan, ctx);
}

static void
signal_cb(int sig, void *arg)
{
	struct ctx *ctx = arg;
	struct winsize ws;

	switch(sig) {
	case SIGWINCH:
		if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws) == 0)
			resizeterm(ws.ws_row, ws.ws_col);
		break;
	case SIGINT:
		debug(ctx, _("SIGINT caught, exiting"));
		eloop_exit(ctx->eloop, EXIT_FAILURE);
		break;
	case SIGTERM:
		debug(ctx, _("SIGTERM caught, exiting"));
		eloop_exit(ctx->eloop, EXIT_FAILURE);
		break;
	case SIGHUP:
		debug(ctx, ("SIGHUP caught, ignoring"));
		break;
	case SIGPIPE:
		/* ignore and don't report */
		break;
	}
}

static int
create_windows(struct ctx *ctx)
{
	int h, w;

	getmaxyx(ctx->stdscr, h, w);

	if ((ctx->win_status = newwin(1, w, 0, 0)) == NULL)
		return -1;

	if ((ctx->win_summary_border = newwin(10, w - 2, 2, 1)) == NULL)
		return -1;
	box(ctx->win_summary_border, 0, 0);
	mvwprintw(ctx->win_summary_border, 0, 5, " %s ",
	    _("Connection Summary"));
	wrefresh(ctx->win_summary_border);
	if ((ctx->win_summary = newwin(8, w - 4, 3, 2)) == NULL)
		return -1;
	scrollok(ctx->win_summary, TRUE);

#if 1
	if ((ctx->win_debug_border = newwin(8, w - 2, h - 10, 1)) == NULL)
		return -1;
	box(ctx->win_debug_border, 0, 0);
	mvwprintw(ctx->win_debug_border, 0, 5, " %s ",
	    _("Event Log"));
	wrefresh(ctx->win_debug_border);
	if ((ctx->win_debug = newwin(6, w - 4, h - 9, 2)) == NULL)
		return -1;
	scrollok(ctx->win_debug, TRUE);
#endif
	return 0;
}

int
main(void)
{
	struct ctx ctx;
	WI_SCAN *wi;
	sigset_t sigmask;

	memset(&ctx, 0, sizeof(ctx));
	TAILQ_INIT(&ctx.wi_scans);

	if ((ctx.eloop = eloop_new()) == NULL)
		err(EXIT_FAILURE, "eloop_new");
	if (eloop_signal_set_cb(ctx.eloop, sigs, __arraycount(sigs),
	    signal_cb, &ctx) == -1)
		err(EXIT_FAILURE, "eloop_signal_set_cb");
	if (eloop_signal_mask(ctx.eloop, &sigmask) == -1)
		err(EXIT_FAILURE, "eloop_signal_mask");

	if ((ctx.con = dhcpcd_new()) == NULL)
		err(EXIT_FAILURE, "dhcpcd_new");

	if ((ctx.stdscr = initscr()) == NULL)
		err(EXIT_FAILURE, "initscr");

	if (create_windows(&ctx) == -1)
		err(EXIT_FAILURE, "create_windows");

	curs_set(0);
	noecho();
	keypad(ctx.stdscr, TRUE);

	wprintw(ctx.win_status, "%s %s", _("dhcpcd Curses Interface"), VERSION);
	dhcpcd_set_progname(ctx.con, "dhcpcd-curses");
	dhcpcd_set_status_callback(ctx.con, status_cb, &ctx);
	dhcpcd_set_if_callback(ctx.con, if_cb, &ctx);
	dhcpcd_wpa_set_scan_callback(ctx.con, wpa_scan_cb, &ctx);
	dhcpcd_wpa_set_status_callback(ctx.con, wpa_status_cb, &ctx);

	eloop_timeout_add_sec(ctx.eloop, 0, try_open, &ctx);
	eloop_timeout_add_msec(ctx.eloop, DHCPCD_WPA_SCAN_SHORT,
	    bg_scan, &ctx);
	eloop_start(ctx.eloop, &sigmask);

	/* Un-resgister the callbacks to avoid spam on close */
	dhcpcd_set_status_callback(ctx.con, NULL, NULL);
	dhcpcd_set_if_callback(ctx.con, NULL, NULL);
	dhcpcd_wpa_set_scan_callback(ctx.con, NULL, NULL);
	dhcpcd_wpa_set_status_callback(ctx.con, NULL, NULL);
	dhcpcd_close(ctx.con);
	dhcpcd_free(ctx.con);

	/* Free our saved scans */
	while ((wi = TAILQ_FIRST(&ctx.wi_scans))) {
		TAILQ_REMOVE(&ctx.wi_scans, wi, next);
		dhcpcd_wi_scans_free(wi->scans);
		free(wi);
	}

	/* Free everything else */
	eloop_free(ctx.eloop);
	endwin();

#ifdef HAVE_NC_FREE_AND_EXIT
	/* undefined ncurses function to allow valgrind debugging */
	_nc_free_and_exit();
#endif

	return EXIT_SUCCESS;
}
