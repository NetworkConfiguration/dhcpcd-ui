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

#include <locale.h>
#include <poll.h>
#include <stdlib.h>
#include <string.h>

#ifdef NOTIFY
#  include <libnotify/notify.h>
static NotifyNotification *nn;
#endif

#include "config.h"
#include "dhcpcd-gtk.h"

static GtkStatusIcon *status_icon;
static int ani_timer;
static int ani_counter;
static bool online;
static bool carrier;

struct watch {
	struct pollfd pollfd;
	int eventid;
	GIOChannel *gio;
	struct watch *next;
};
static struct watch *watches;

WI_SCAN *wi_scans;

WI_SCAN *
wi_scan_find(DHCPCD_WI_SCAN *scan)
{
	WI_SCAN *w;
	DHCPCD_WI_SCAN *dw;

	for (w = wi_scans; w; w = w->next) {
		for (dw = w->scans; dw; dw = dw->next)
			if (dw == scan)
				break;
		if (dw)
			return w;
	}
	return NULL;
}

static gboolean
animate_carrier(_unused gpointer data)
{
	const char *icon;
	
	if (ani_timer == 0)
		return false;

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
	gtk_status_icon_set_from_icon_name(status_icon, icon);
	return true;
}

static gboolean
animate_online(_unused gpointer data)
{
	const char *icon;
	
	if (ani_timer == 0)
		return false;

	if (ani_counter++ > 6) {
		ani_timer = 0;
		ani_counter = 0;
		return false;
	}

	if (ani_counter % 2 == 0)
		icon = "network-idle";
	else
		icon = "network-transmit-receive";
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
		if (showif)
			g_message("%s: %s", i->ifname, i->reason);
		if (strcmp(i->reason, "RELEASE") == 0 ||
		    strcmp(i->reason, "STOP") == 0)
			continue;
		if (dhcpcd_if_up(i))
			ison = iscarrier = true;
		if (!iscarrier && g_strcmp0(i->reason, "CARRIER") == 0)
			iscarrier = true;
		msg = dhcpcd_if_message(i);
		if (msgs) {
			tmp = g_strconcat(msgs, "\n", msg, NULL);
			g_free(msgs);
			g_free(msg);
			msgs = tmp;
		} else
			msgs = msg;
	}

	if (online != ison || carrier != iscarrier) {
		online = ison;
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
	gtk_status_icon_set_tooltip(status_icon, msgs);
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
	char **msgs, **m;

	/* Don't spam the same message */
	if (notify_last_msg) {
		if (strcmp(msg, notify_last_msg) == 0)
			return;
		g_free(notify_last_msg);
	}
	notify_last_msg = g_strdup(msg);

	msgs = g_strsplit(msg, "\n", 0);
	for (m = msgs; *m; m++)
		g_message("%s", *m);
	g_strfreev(msgs);
	if (nn != NULL)
		notify_notification_close(nn, NULL);
	if (gtk_status_icon_get_visible(status_icon))
		nn = notify_notification_new_with_status_icon(title,
		    msg, icon, status_icon);
	else
		nn = notify_notification_new(title, msg, icon, NULL);
	notify_notification_set_timeout(nn, 5000);
	g_signal_connect(nn, "closed", G_CALLBACK(notify_closed), NULL);
	notify_notification_show(nn, NULL);
}
#else
#  define notify(a, b, c)
#endif

static void
event_cb(DHCPCD_CONNECTION *con, DHCPCD_IF *i, _unused void *data)
{
	char *msg;
	const char *icon;

	g_message("%s: %s", i->ifname, i->reason);
	update_online(con, false);
	
	/* We should ignore renew and stop so we don't annoy the user */
	if (g_strcmp0(i->reason, "RENEW") == 0 ||
	    g_strcmp0(i->reason, "STOP") == 0)
		return;

	msg = dhcpcd_if_message(i);
	if (dhcpcd_if_up(i))
		icon = "network-transmit-receive";
	else
		icon = "network-transmit";
	if (dhcpcd_if_down(i))
		icon = "network-offline";
	notify(_("Network event"), msg, icon);
	g_free(msg);
}

static void
status_cb(DHCPCD_CONNECTION *con, const char *status, _unused void *data)
{
	static char *last = NULL;
	char *version;
	const char *msg;
	bool refresh;
	WI_SCAN *w;

	g_message("Status changed to %s", status);
	if (g_strcmp0(status, "down") == 0) {
		msg = N_(last ?
		    "Connection to dhcpcd lost" : "dhcpcd not running");
		gtk_status_icon_set_tooltip(status_icon, msg);
		notify(_("No network"), msg, "network-offline");
		dhcpcd_prefs_abort();
		while (wi_scans) {
			w = wi_scans->next;
			dhcpcd_wi_scans_free(wi_scans->scans);
			g_free(wi_scans);
			wi_scans = w;
		}
	} else {
		if ((last == NULL || g_strcmp0(last, "down") == 0) &&
		    dhcpcd_command(con, "GetDhcpcdVersion", NULL, &version))
		{
			g_message(_("Connected to %s-%s"), "dhcpcd", version);
			g_free(version);
			refresh = true;
		} else
			refresh = false;
		update_online(con, refresh);
	}
	last = g_strdup(status);
}

static void
scan_cb(DHCPCD_CONNECTION *con, DHCPCD_IF *i, _unused void *data)
{
	WI_SCAN *w;
	DHCPCD_WI_SCAN *scans, *s1, *s2;
	char *txt, *t;
	const char *msg;

	g_message(_("%s: Received scan results"), i->ifname);
	scans = dhcpcd_wi_scans(con, i);
	if (scans == NULL && dhcpcd_error(con) != NULL) {
		g_warning("%s: %s", i->ifname, dhcpcd_error(con));
		dhcpcd_error_clear(con);
	}
	for (w = wi_scans; w; w = w->next)
		if (w->connection == con && w->interface == i)
			break;
	if (w == NULL) {
		w = g_malloc(sizeof(*w));
		w->connection = con;
		w->interface = i;
		w->next = wi_scans;
		wi_scans = w;
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
		dhcpcd_wi_scans_free(w->scans);
	}
	w->scans = scans;
}

static gboolean
gio_callback(GIOChannel *gio, _unused GIOCondition c, _unused gpointer d)
{
	int fd;

	fd = g_io_channel_unix_get_fd(gio);
	dhcpcd_dispatch(fd);
	return true;
}

static void
delete_watch_cb(_unused DHCPCD_CONNECTION *con, const struct pollfd *fd,
    _unused void *data)
{
	struct watch *w, *l;

	l = NULL;
	for (w = watches; w; w = w->next) {
		if (w->pollfd.fd == fd->fd) {
			if (l == NULL)
				watches = w->next;
			else
				l->next = w->next;
			g_source_remove(w->eventid);
			g_io_channel_unref(w->gio);
			g_free(w);
			break;
		}
	}
}

static void
add_watch_cb(DHCPCD_CONNECTION *con, const struct pollfd *fd,
    _unused void *data)
{
	struct watch *w;
	GIOChannel *gio;
	int flags, eventid;

	/* Remove any existing watch */
	delete_watch_cb(con, fd, data);
	
	gio = g_io_channel_unix_new(fd->fd);
	if (gio == NULL) {
		g_error(_("Error creating new GIO Channel\n"));
		return;
	}
	flags = 0;
	if (fd->events & POLLIN)
		flags |= G_IO_IN;
	if (fd->events & POLLOUT)
		flags |= G_IO_OUT;
	if (fd->events & POLLERR)
		flags |= G_IO_ERR;
	if (fd->events & POLLHUP)
		flags |= G_IO_HUP;
	if ((eventid = g_io_add_watch(gio, flags, gio_callback, con)) == 0) {
		g_io_channel_unref(gio);
		g_error(_("Error creating watch\n"));
		return;
	}
	w = g_malloc(sizeof(*w));
	memcpy(&w->pollfd, fd, sizeof(w->pollfd));
	w->eventid = eventid;
	w->gio = gio;
	w->next = watches;
	watches = w;
}

int
main(int argc, char *argv[])
{
	char *error = NULL;
	char *version = NULL;
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
	
	gtk_status_icon_set_tooltip(status_icon,
	    _("Connecting to dhcpcd ..."));
	gtk_status_icon_set_visible(status_icon, true);

#ifdef NOTIFY
	notify_init(PACKAGE);
#endif

	g_message(_("Connecting ..."));
	con = dhcpcd_open(&error);
	if (con ==  NULL) {
		g_critical("libdhcpcd: %s", error);
		exit(EXIT_FAILURE);
	}

	gtk_status_icon_set_tooltip(status_icon, _("Triggering dhcpcd ..."));
	online = false;

	if (!dhcpcd_command(con, "GetVersion", NULL, &version)) {
		g_critical("libdhcpcd: GetVersion: %s", dhcpcd_error(con));
		exit(EXIT_FAILURE);
	}
	g_message(_("Connected to %s-%s"), "dhcpcd-dbus", version);
	g_free(version);

	dhcpcd_set_watch_functions(con, add_watch_cb, delete_watch_cb, NULL);
	dhcpcd_set_signal_functions(con, event_cb, status_cb, scan_cb, NULL);
	if (dhcpcd_error(con))
		g_error("libdhcpcd: %s", dhcpcd_error(con));

	menu_init(status_icon, con);

	gtk_main();
	dhcpcd_close(con);
	return 0;
}
