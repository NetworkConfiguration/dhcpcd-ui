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

#ifndef DHCPCD_CURSES_H
#define DHCPCD_CURSES_H

#include <event.h>

#ifdef HAS_GETTEXT
#include <libintl.h>
#define _ gettext
#else
#define _(a) (a)
#endif

#include <curses.h>

#include "config.h"
#include "dhcpcd.h"
#include "queue.h"
#include "event-object.h"

#ifndef __printflike
#ifdef __GNUC__
#define __printflike(a, b) __attribute__((format(printf, a, b)))
#else
#define __printflike(a, b)
#endif
#endif

#ifndef __unused
#ifdef __GNUC__
#define __unused   __attribute__((__unused__))
#else
#define __unused
#endif
#endif

#define MSECS_PER_NSEC	1000

typedef struct wi_scan {
	TAILQ_ENTRY(wi_scan) next;
	DHCPCD_IF *interface;
	DHCPCD_WI_SCAN *scans;
} WI_SCAN;
typedef TAILQ_HEAD(wi_scan_head, wi_scan) WI_SCANS;

struct ctx {
	struct event_base *evbase;
	EVENT_OBJECTS *evobjects;
	DHCPCD_CONNECTION *con;
	int fd;
	bool online;
	bool carrier;
	unsigned int last_status;
	size_t status_len;
	WI_SCANS wi_scans;

	struct event *sigint;
	struct event *sigterm;
	struct event *sigwinch;

	WINDOW *stdscr;
	WINDOW *win_status;
	WINDOW *win_debug;
	WINDOW *win_debug_border;
	WINDOW *win_summary;
	WINDOW *win_summary_border;
};

#endif
