/*
 * dhcpcd-gtk
 * Copyright 2009-2014 Roy Marples <roy@marples.name>
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
#include <locale.h>
#include <stdlib.h>
#include <string.h>

#ifdef NOTIFY
#  include <libnotify/notify.h>
#ifndef NOTIFY_CHECK_VERSION
#  define NOTIFY_CHECK_VERSION(a,b,c) 0
#endif
static NotifyNotification *nn;
#endif

#include "config.h"
#include "dhcpcd.h"
#include "dhcpcd-gtk.h"

static GtkStatusIcon *status_icon;
static guint ani_timer;
static int ani_counter;
static bool online;
static bool carrier;

struct watch {
	gpointer ref;
	int fd;
	guint eventid;
	GIOChannel *gio;
	struct watch *next;
};
static struct watch *watches;

WI_SCANS wi_scans;

static gboolean dhcpcd_try_open(gpointer data);
static gboolean dhcpcd_wpa_try_open(gpointer data);

WI_SCAN *
wi_scan_find(DHCPCD_WI_SCAN *scan)
{
	WI_SCAN *w;
	DHCPCD_WI_SCAN *dw;

	TAILQ_FOREACH(w, &wi_scans, next) {
		for (dw = w->scans; dw; dw = dw->next)
			if (dw == scan)
				return w;
	}
	return NULL;
}

const char *
get_strength_icon_name(int strength)
{

	if (strength > 80)
		return "network-wireless-connected-100";
	else if (strength > 55)
		return "network-wireless-connected-75";
	else if (strength > 30)
		return "network-wireless-connected-50";
	else if (strength > 5)
		return "network-wireless-connected-25";
	else
		return "network-wireless-connected-00";
}

static DHCPCD_WI_SCAN *
get_strongest_scan()
{
	WI_SCAN *w;
	DHCPCD_WI_SCAN *scan, *s;

	scan = NULL;
	TAILQ_FOREACH(w, &wi_scans, next) {
		for (s = w->scans; s; s = s->next) {
			if (dhcpcd_wi_associated(w->interface, s) &&
			    (scan == NULL ||
			    s->strength.value > scan->strength.value))
				scan = s;
		}
	}
	return scan;
}

static gboolean
animate_carrier(_unused gpointer data)
{
	const char *icon;
	DHCPCD_WI_SCAN *scan;

	if (ani_timer == 0)
		return false;

	scan = get_strongest_scan();
	if (scan) {
		switch(ani_counter++) {
		case 0:
			icon = "network-wireless-connected-00";
			break;
		case 1:
			icon = "network-wireless-connected-25";
			break;
		case 2:
			icon = "network-wireless-connected-50";
			break;
		case 3:
			icon = "network-wireless-connected-75";
			break;
		default:
			icon = "network-wireless-connected-100";
			ani_counter = 0;
		}

	} else {
		switch(ani_counter++) {
		case 0:
			icon = "network-transmit";
			break;
		case 1:
			icon = "network-receive";
			break;
		default:
			icon = "network-idle";
			ani_counter = 0;
			break;
		}
	}
	gtk_status_icon_set_from_icon_name(status_icon, icon);
	return true;
}

static gboolean
animate_online(_unused gpointer data)
{
	const char *icon;
	DHCPCD_WI_SCAN *scan;

	if (ani_timer == 0)
		return false;

	if (ani_counter++ > 6) {
		ani_timer = 0;
		ani_counter = 0;
		return false;
	}

	scan = get_strongest_scan();
	if (ani_counter % 2 == 0)
		icon = scan ? "network-wireless-connected-00" :
		    "network-idle";
	else
		icon = scan ? get_strength_icon_name(scan->strength.value) :
		    "network-transmit-receive";
	gtk_status_icon_set_from_icon_name(status_icon, icon);
	return true;
}

static void
update_online(DHCPCD_CONNECTION *con, bool showif)
{
	bool ison, iscarrier;
	char *msg, *msgs, *tmp;
	DHCPCD_IF *ifs, *i;

	ison = iscarrier = false;
	msgs = NULL;
	ifs = dhcpcd_interfaces(con);
	for (i = ifs; i; i = i->next) {
		if (g_strcmp0(i->type, "link") == 0) {
			if (i->up)
				iscarrier = true;
		} else {
			if (i->up)
				ison = true;
		}
		msg = dhcpcd_if_message(i, NULL);
		if (msg) {
			if (showif)
				g_message("%s", msg);
			if (msgs) {
				tmp = g_strconcat(msgs, "\n", msg, NULL);
				g_free(msgs);
				g_free(msg);
				msgs = tmp;
			} else
				msgs = msg;
		} else if (showif)
			g_message("%s: %s", i->ifname, i->reason);
	}

	if (online != ison || carrier != iscarrier) {
		online = ison;
		carrier = iscarrier;
		if (ani_timer != 0) {
			g_source_remove(ani_timer);
			ani_timer = 0;
			ani_counter = 0;
		}
		if (ison) {
			animate_online(NULL);
			ani_timer = g_timeout_add(300, animate_online, NULL);
		} else if (iscarrier) {
			animate_carrier(NULL);
			ani_timer = g_timeout_add(500, animate_carrier, NULL);
		} else {
			gtk_status_icon_set_from_icon_name(status_icon,
			    "network-offline");
		}
	}
	gtk_status_icon_set_tooltip_text(status_icon, msgs);
	g_free(msgs);
}

void
notify_close(void)
{
#ifdef NOTIFY
	if (nn != NULL)
		notify_notification_close(nn, NULL);
#endif
}

#ifdef NOTIFY
static char *notify_last_msg;

static void
notify_closed(void)
{
	nn = NULL;
}

static void
notify(const char *title, const char *msg, const char *icon)
{

	if (msg == NULL)
		return;
	/* Don't spam the same message */
	if (notify_last_msg) {
		if (notify_last_msg && strcmp(msg, notify_last_msg) == 0)
			return;
		g_free(notify_last_msg);
	}
	notify_last_msg = g_strdup(msg);

	if (nn != NULL)
		notify_notification_close(nn, NULL);

#if NOTIFY_CHECK_VERSION(0,7,0)
	nn = notify_notification_new(title, msg, icon);
	notify_notification_set_hint(nn, "transient",
	    g_variant_new_boolean(TRUE));
#else
	if (gtk_status_icon_get_visible(status_icon))
		nn = notify_notification_new_with_status_icon(title,
		    msg, icon, status_icon);
	else
		nn = notify_notification_new(title, msg, icon, NULL);
#endif

	notify_notification_set_timeout(nn, 5000);
	g_signal_connect(nn, "closed", G_CALLBACK(notify_closed), NULL);
	notify_notification_show(nn, NULL);
}
#else
#  define notify(a, b, c)
#endif

static struct watch *
dhcpcd_findwatch(int fd, gpointer data, struct watch **last)
{
	struct watch *w;

	if (last)
		*last = NULL;
	for (w = watches; w; w = w->next) {
		if (w->fd == fd || w->ref == data)
			return w;
		if (last)
			*last = w;
	}
	return NULL;
}

static void
dhcpcd_unwatch(int fd, gpointer data)
{
	struct watch *w, *l;

	if ((w = dhcpcd_findwatch(fd, data, &l))) {
		if (l)
			l->next = w->next;
		else
			watches = w->next;
		g_source_remove(w->eventid);
		g_io_channel_unref(w->gio);
		g_free(w);
	}
}

static gboolean
dhcpcd_watch(int fd,
    gboolean (*cb)(GIOChannel *, GIOCondition, gpointer),
    gpointer data)
{
	struct watch *w, *l;
	GIOChannel *gio;
	GIOCondition flags;
	guint eventid;

	/* Sanity */
	if ((w = dhcpcd_findwatch(fd, data, &l))) {
		if (w->fd == fd)
			return TRUE;
		if (l)
			l->next = w->next;
		else
			watches = w->next;
		g_source_remove(w->eventid);
		g_io_channel_unref(w->gio);
		g_free(w);
	}

	gio = g_io_channel_unix_new(fd);
	if (gio == NULL) {
		g_warning(_("Error creating new GIO Channel\n"));
		return FALSE;
	}
	flags = G_IO_IN | G_IO_ERR | G_IO_HUP;
	if ((eventid = g_io_add_watch(gio, flags, cb, data)) == 0) {
		g_warning(_("Error creating watch\n"));
		g_io_channel_unref(gio);
		return FALSE;
	}

	w = g_try_malloc(sizeof(*w));
	if (w == NULL) {
		g_warning(_("g_try_malloc\n"));
		g_source_remove(eventid);
		g_io_channel_unref(gio);
		return FALSE;
	}

	w->ref = data;
	w->fd = fd;
	w->eventid = eventid;
	w->gio = gio;
	w->next = watches;
	watches = w;

	return TRUE;
}

static void
dhcpcd_status_cb(DHCPCD_CONNECTION *con, const char *status,
    _unused void *data)
{
	static char *last = NULL;
	const char *msg;
	bool refresh;
	WI_SCAN *w;

	g_message("Status changed to %s", status);
	if (g_strcmp0(status, "down") == 0) {
		msg = N_(last ?
		    "Connection to dhcpcd lost" : "dhcpcd not running");
		if (ani_timer != 0) {
			g_source_remove(ani_timer);
			ani_timer = 0;
			ani_counter = 0;
		}
		online = carrier = false;
		gtk_status_icon_set_from_icon_name(status_icon,
		    "network-offline");
		gtk_status_icon_set_tooltip_text(status_icon, msg);
		prefs_abort();
		menu_abort();
		wpa_abort();
		while ((w = TAILQ_FIRST(&wi_scans))) {
			TAILQ_REMOVE(&wi_scans, w, next);
			dhcpcd_wi_scans_free(w->scans);
			g_free(w);
		}
		dhcpcd_unwatch(-1, con);
		g_timeout_add(DHCPCD_RETRYOPEN, dhcpcd_try_open, con);
	} else {
		if ((last == NULL || g_strcmp0(last, "down") == 0)) {
			g_message(_("Connected to %s-%s"), "dhcpcd",
			    dhcpcd_version(con));
			refresh = true;
		} else
			refresh = g_strcmp0(last, "opened") ? false : true;
		update_online(con, refresh);
	}

	g_free(last);
	last = g_strdup(status);
}

static gboolean
dhcpcd_cb(_unused GIOChannel *gio, _unused GIOCondition c, gpointer data)
{
	DHCPCD_CONNECTION *con;

	con = (DHCPCD_CONNECTION *)data;
	if (dhcpcd_get_fd(con) == -1) {
		g_warning(_("dhcpcd connection lost"));
		dhcpcd_unwatch(-1, con);
		g_timeout_add(DHCPCD_RETRYOPEN, dhcpcd_try_open, con);
		return FALSE;
	}

	dhcpcd_dispatch(con);
	return TRUE;
}

static gboolean
dhcpcd_try_open(gpointer data)
{
	DHCPCD_CONNECTION *con;
	int fd;
	static int last_error;

	con = (DHCPCD_CONNECTION *)data;
	fd = dhcpcd_open(con, true);
	if (fd == -1) {
		if (errno == EACCES || errno == EPERM) {
			if ((fd = dhcpcd_open(con, false)) != -1)
				goto unprived;
		}
		if (errno != last_error) {
			g_critical("dhcpcd_open: %s", strerror(errno));
			last_error = errno;
		}
		return TRUE;
	}

unprived:
	if (!dhcpcd_watch(fd, dhcpcd_cb, con)) {
		dhcpcd_close(con);
		return TRUE;
	}

	/* Start listening to WPA events */
	dhcpcd_wpa_start(con);

	return FALSE;
}

static void
dhcpcd_if_cb(DHCPCD_IF *i, _unused void *data)
{
	DHCPCD_CONNECTION *con;
	char *msg;
	const char *icon;
	bool new_msg;

	/* We should ignore renew and stop so we don't annoy the user */
	if (g_strcmp0(i->reason, "RENEW") &&
	    g_strcmp0(i->reason, "STOP") &&
	    g_strcmp0(i->reason, "STOPPED"))
	{
		msg = dhcpcd_if_message(i, &new_msg);
		if (msg) {
			g_message("%s", msg);
			if (new_msg) {
				if (i->up)
					icon = "network-transmit-receive";
				//else
				//	icon = "network-transmit";
				if (!i->up)
					icon = "network-offline";
				notify(_("Network event"), msg, icon);
			}
			g_free(msg);
		}
	}

	/* Update the tooltip with connection information */
	con = dhcpcd_if_connection(i);
	update_online(con, false);

	if (i->wireless) {
		DHCPCD_WI_SCAN *scans;
		WI_SCAN *w;

		TAILQ_FOREACH(w, &wi_scans, next) {
			if (w->interface == i)
				break;
		}
		if (w) {
			scans = dhcpcd_wi_scans(i);
			menu_update_scans(w, scans);
		}
	}
}

static gboolean
dhcpcd_wpa_cb(_unused GIOChannel *gio, _unused GIOCondition c,
    gpointer data)
{
	DHCPCD_WPA *wpa;
	DHCPCD_IF *i;

	wpa = (DHCPCD_WPA *)data;
	if (dhcpcd_wpa_get_fd(wpa) == -1) {
		dhcpcd_unwatch(-1, wpa);

		/* If the interface hasn't left, try re-opening */
		i = dhcpcd_wpa_if(wpa);
		if (i == NULL ||
		    g_strcmp0(i->reason, "DEPARTED") == 0 ||
		    g_strcmp0(i->reason, "STOPPED") == 0)
			return TRUE;
		g_warning(_("dhcpcd WPA connection lost: %s"), i->ifname);
		g_timeout_add(DHCPCD_RETRYOPEN, dhcpcd_wpa_try_open, wpa);
		return FALSE;
	}

	dhcpcd_wpa_dispatch(wpa);
	return TRUE;
}

static gboolean
dhcpcd_wpa_try_open(gpointer data)
{
	DHCPCD_WPA *wpa;
	int fd;
	static int last_error;

	wpa = (DHCPCD_WPA *)data;
	fd = dhcpcd_wpa_open(wpa);
	if (fd == -1) {
		if (errno != last_error)
			g_critical("dhcpcd_wpa_open: %s", strerror(errno));
		last_error = errno;
		return TRUE;
	}

	if (!dhcpcd_watch(fd, dhcpcd_wpa_cb, wpa)) {
		dhcpcd_wpa_close(wpa);
		return TRUE;
	}

	return FALSE;
}

static void
dhcpcd_wpa_scan_cb(DHCPCD_WPA *wpa, _unused void *data)
{
	DHCPCD_IF *i;
	WI_SCAN *w;
	DHCPCD_WI_SCAN *scans, *s1, *s2;
	char *txt, *t;
	int lerrno, fd;
	const char *msg;

	/* This could be a new WPA so watch it */
	fd = dhcpcd_wpa_get_fd(wpa);
	if (fd == -1) {
		g_critical("No fd for WPA %p", wpa);
		dhcpcd_unwatch(-1, wpa);
		return;
	}
	dhcpcd_watch(fd, dhcpcd_wpa_cb, wpa);

	i = dhcpcd_wpa_if(wpa);
	if (i == NULL) {
		g_critical("No interface for WPA %p", wpa);
		return;
	}
	g_message(_("%s: Received scan results"), i->ifname);
	lerrno = errno;
	errno = 0;
	scans = dhcpcd_wi_scans(i);
	if (scans == NULL && errno)
		g_warning("%s: %s", i->ifname, strerror(errno));
	errno = lerrno;
	TAILQ_FOREACH(w, &wi_scans, next) {
		if (w->interface == i)
			break;
	}
	if (w == NULL) {
		w = g_malloc(sizeof(*w));
		w->interface = i;
		w->scans = scans;
		w->ifmenu = NULL;
		TAILQ_INIT(&w->menus);
		TAILQ_INSERT_TAIL(&wi_scans, w, next);
	} else {
		txt = NULL;
		msg = N_("New Access Point");
		for (s1 = scans; s1; s1 = s1->next) {
			for (s2 = w->scans; s2; s2 = s2->next)
				if (g_strcmp0(s1->ssid, s2->ssid) == 0)
					break;
			if (s2 == NULL) {
				if (txt == NULL)
					txt = g_strdup(s1->ssid);
				else {
					msg = N_("New Access Points");
					t = g_strconcat(txt, "\n",
					    s1->ssid, NULL);
					g_free(txt);
					txt = t;
				}
			}
		}
		if (txt) {
			notify(msg, txt, "network-wireless");
			g_free(txt);
		}
		menu_update_scans(w, scans);
	}

	if (!ani_timer) {
		s1 = get_strongest_scan();
		if (s1)
			msg = get_strength_icon_name(s1->strength.value);
		else if (online)
			msg = "network-transmit-receive";
		else
			msg = "network-offline";
		gtk_status_icon_set_from_icon_name(status_icon, msg);
	}
}

static void
dhcpcd_wpa_status_cb(DHCPCD_WPA *wpa, const char *status, _unused void *data)
{
	DHCPCD_IF *i;
	WI_SCAN *w, *wn;

	i = dhcpcd_wpa_if(wpa);
	g_message("%s: WPA status %s", i->ifname, status);
	if (g_strcmp0(status, "down") == 0) {
		dhcpcd_unwatch(-1, wpa);
		TAILQ_FOREACH_SAFE(w, &wi_scans, next, wn) {
			if (w->interface == i) {
				TAILQ_REMOVE(&wi_scans, w, next);
				menu_remove_if(w);
				dhcpcd_wi_scans_free(w->scans);
				g_free(w);
			}
		}
	}
}

#ifdef BG_SCAN
static gboolean
bgscan(gpointer data)
{
	WI_SCAN *w;
	DHCPCD_CONNECTION *con;
	DHCPCD_WPA *wpa;

	con = (DHCPCD_CONNECTION *)data;
	TAILQ_FOREACH(w, &wi_scans, next) {
		if (w->interface->wireless && w->interface->up) {
			wpa = dhcpcd_wpa_find(con, w->interface->ifname);
			if (wpa)
				dhcpcd_wpa_scan(wpa);
		}
	}

	return TRUE;
}
#endif

int
main(int argc, char *argv[])
{
	DHCPCD_CONNECTION *con;

	setlocale(LC_ALL, "");
	bindtextdomain(PACKAGE, NULL);
	bind_textdomain_codeset(PACKAGE, "UTF-8");
	textdomain(PACKAGE);

	gtk_init(&argc, &argv);
	g_set_application_name("Network Configurator");
	gtk_icon_theme_append_search_path(gtk_icon_theme_get_default(),
	    ICONDIR);
	status_icon = gtk_status_icon_new_from_icon_name("network-offline");

	gtk_status_icon_set_tooltip_text(status_icon,
	    _("Connecting to dhcpcd ..."));
	gtk_status_icon_set_visible(status_icon, true);
	online = false;
#ifdef NOTIFY
	notify_init(PACKAGE);
#endif

	TAILQ_INIT(&wi_scans);
	g_message(_("Connecting ..."));
	con = dhcpcd_new();
	if (con ==  NULL) {
		g_critical("libdhcpcd: %s", strerror(errno));
		exit(EXIT_FAILURE);
	}
	dhcpcd_set_progname(con, "dhcpcd-gtk");
	dhcpcd_set_status_callback(con, dhcpcd_status_cb, NULL);
	dhcpcd_set_if_callback(con, dhcpcd_if_cb, NULL);
	dhcpcd_wpa_set_scan_callback(con, dhcpcd_wpa_scan_cb, NULL);
	dhcpcd_wpa_set_status_callback(con, dhcpcd_wpa_status_cb, NULL);
	if (dhcpcd_try_open(con))
		g_timeout_add(DHCPCD_RETRYOPEN, dhcpcd_try_open, con);

	menu_init(status_icon, con);
#ifdef BG_SCAN
	g_timeout_add(DHCPCD_WPA_SCAN_LONG, bgscan, con);
#endif

	gtk_main();
	dhcpcd_close(con);
	dhcpcd_free(con);
	return 0;
}
