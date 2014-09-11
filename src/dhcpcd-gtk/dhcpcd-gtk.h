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

#include <stdbool.h>

#include <glib.h>
#include <glib/gi18n.h>
#include <gtk/gtk.h>
#include <libintl.h>

#include "dhcpcd.h"

#define PACKAGE "dhcpcd-gtk"

#define UNCONST(a)              ((void *)(unsigned long)(const void *)(a))

#ifdef __GNUC__
#  define _unused __attribute__((__unused__))
#else
#  define _unused
#endif

typedef struct wi_scan {
	DHCPCD_IF *interface;
	DHCPCD_WI_SCAN *scans;
	struct wi_scan *next;
} WI_SCAN;

extern WI_SCAN *wi_scans;

WI_SCAN * wi_scan_find(DHCPCD_WI_SCAN *);

void menu_init(GtkStatusIcon *, DHCPCD_CONNECTION *);
void menu_update_scans(DHCPCD_IF *, DHCPCD_WI_SCAN *);

void notify_close(void);

void dhcpcd_prefs_show(DHCPCD_CONNECTION *con);
void dhcpcd_prefs_abort(void);

bool wpa_configure(DHCPCD_WPA *, DHCPCD_WI_SCAN *);

#if GTK_MAJOR_VERSION == 2
GtkWidget *gtk_box_new(GtkOrientation, gint);
#endif

#endif
