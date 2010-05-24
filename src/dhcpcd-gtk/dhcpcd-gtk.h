/*
 * dhcpcd-gtk
 * Copyright 2009-2010 Roy Marples <roy@marples.name>
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

#ifndef DHCPCD_GTK_H
#define DHCPCD_GTK_H

#include <arpa/inet.h>

#include <stdbool.h>

#include <glib.h>
#include <glib/gi18n.h>
#include <gtk/gtk.h>
#include <libintl.h>

#include "libdhcpcd.h"

#define PACKAGE "dhcpcd-gtk"

/* Work out if we have a private address or not
 * 10/8
 * 172.16/12
 * 192.168/16
 */
#ifndef IN_PRIVATE
#  define IN_PRIVATE(addr) (((addr & IN_CLASSA_NET) == 0x0a000000) ||	      \
	    ((addr & 0xfff00000)    == 0xac100000) ||			      \
	    ((addr & IN_CLASSB_NET) == 0xc0a80000))
#endif
#ifndef IN_LINKLOCAL
#  define IN_LINKLOCAL(addr) ((addr & IN_CLASSB_NET) == 0xa9fe0000)
#endif

#define UNCONST(a)              ((void *)(unsigned long)(const void *)(a))

#ifdef __GNUC__
#  define _unused __attribute__((__unused__))
#else
#  define _unused
#endif

typedef struct wi_scan {
	DHCPCD_CONNECTION *connection;
	DHCPCD_IF *interface;
	DHCPCD_WI_SCAN *scans;
	struct wi_scan *next;
} WI_SCAN;

extern WI_SCAN *wi_scans;

WI_SCAN * wi_scan_find(DHCPCD_WI_SCAN *);

void menu_init(GtkStatusIcon *, DHCPCD_CONNECTION *);

void notify_close(void);

void dhcpcd_prefs_show(DHCPCD_CONNECTION *con);
void dhcpcd_prefs_abort(void);

bool wpa_configure(DHCPCD_CONNECTION *, DHCPCD_IF *, DHCPCD_WI_SCAN *);
#endif
