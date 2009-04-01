/*
 * dhcpcd-gtk
 * Copyright 2009 Roy Marples <roy@marples.name>
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

/* TODO: Animate the icon from carrier -> address
 * maybe use network-idle -> network-transmit ->
 * network-receive -> network-transmit-receive */

#include <locale.h>
#include <stdlib.h>
#include <string.h>

#include <libnotify/notify.h>

#include "dhcpcd-gtk.h"
#include "menu.h"

DBusGProxy *dbus = NULL;
GSList *interfaces = NULL;
GtkIconTheme *icontheme = NULL;

static GtkStatusIcon *status_icon;
static int ani_timer;
static int ani_counter;
static bool online;
static bool carrier;
static char **interface_order;
static NotifyNotification *nn;

const char *const up_reasons[] = {
	"BOUND",
	"RENEW",
	"REBIND",
	"REBOOT",
	"IPV4LL",
	"INFORM",
	"STATIC",
	"TIMEOUT",
	NULL
};

const char *const down_reasons[] = {
	"EXPIRE",
	"FAIL",
	"NAK",
	"NOCARRIER",
	"STOP",
	NULL
};

static bool
ignore_if_msg(const struct if_msg *ifm)
{
	if (g_strcmp0(ifm->reason, "STOP") == 0 ||
	    g_strcmp0(ifm->reason, "RELEASE") == 0)
		return true;
	return false;
}

static struct if_msg *
find_if_msg(const char *iface)
{
	GSList *gl;
	struct if_msg *ifm;

	for (gl = interfaces; gl; gl = gl->next) {
		ifm = (struct if_msg *)gl->data;
		if (g_strcmp0(ifm->ifname, iface) == 0)
			return ifm;
	}
	return NULL;
}

static void
free_if_ap(struct if_ap *ifa)
{
	g_free(ifa->ifname);
	g_free(ifa->bssid);
	g_free(ifa->flags);
	g_free(ifa->ssid);
	g_free(ifa);
}

static void
free_if_msg(struct if_msg *ifm)
{
	GSList *gl;

	g_free(ifm->ifname);
	g_free(ifm->reason);
	g_free(ifm->ssid);
	for (gl = ifm->scan_results; gl; gl = gl->next)
		free_if_ap((struct if_ap *)gl->data);
	g_slist_free(ifm->scan_results);
	g_free(ifm);
}

static void
error_exit(const char *msg, GError *error)
{
	GtkWidget *dialog;

	if (error) {
		g_critical("%s: %s", msg, error->message);
		dialog = gtk_message_dialog_new(NULL, 0,
		    GTK_MESSAGE_ERROR, GTK_BUTTONS_CLOSE,
		    "%s: %s", msg, error->message);
	} else {
		g_critical("%s", msg);
		dialog = gtk_message_dialog_new(NULL, 0,
		    GTK_MESSAGE_ERROR, GTK_BUTTONS_CLOSE, "%s", msg);
	}
	gtk_dialog_run(GTK_DIALOG(dialog));
	gtk_widget_destroy(dialog);
	if (gtk_main_level())
		gtk_main_quit();
	else
		exit(EXIT_FAILURE);
}

static GSList *
get_scan_results(struct if_msg *ifm)
{
	GType otype;
	GError *error;
	GPtrArray *array;
	GHashTable *config;
	GSList *list = NULL;
	struct if_ap *ifa;
	guint i;
	GValue *val;

	otype = dbus_g_type_get_map("GHashTable", G_TYPE_STRING, G_TYPE_VALUE);
	otype = dbus_g_type_get_collection("GPtrArray", otype);

	error = NULL;
	if (!dbus_g_proxy_call(dbus, "ScanResults", &error,
		G_TYPE_STRING, ifm->ifname, G_TYPE_INVALID,
		otype, &array, G_TYPE_INVALID))
		error_exit(_("ScanResults"), error);

	for (i = 0; i < array->len; i++) {
		config = g_ptr_array_index(array, i);
		val = g_hash_table_lookup(config, "BSSID");
		if (val == NULL)
			continue;
		ifa = g_malloc0(sizeof(*ifa));
		ifa->ifname = g_strdup(ifm->ifname);
		ifa->bssid = g_strdup(g_value_get_string(val));
		val = g_hash_table_lookup(config, "Frequency");
		if (val)
			ifa->frequency = g_value_get_int(val);
		val = g_hash_table_lookup(config, "Quality");
		if (val)
			ifa->quality = g_value_get_int(val);
		val = g_hash_table_lookup(config, "Noise");
		if (val)
			ifa->noise = g_value_get_int(val);
		val = g_hash_table_lookup(config, "Level");
		if (val)
			ifa->level = g_value_get_int(val);
		val = g_hash_table_lookup(config, "Flags");
		if (val)
			ifa->flags = g_strdup(g_value_get_string(val));
		val = g_hash_table_lookup(config, "SSID");
		if (val)
			ifa->ssid = g_strdup(g_value_get_string(val));
		list = g_slist_append(list, ifa);
	}
	return list;
}

static struct if_msg *
make_if_msg(GHashTable *config)
{
	GValue *val;
	struct if_msg *ifm;

	val = g_hash_table_lookup(config, "Interface");
	if (val == NULL)
		return NULL;
	ifm = g_malloc0(sizeof(*ifm));
	ifm->ifname = g_strdup(g_value_get_string(val));
	val = g_hash_table_lookup(config, "Flags");
	if (val)
		ifm->flags = g_value_get_uint(val);
	val = g_hash_table_lookup(config, "Reason");
	if (val)
		ifm->reason = g_strdup(g_value_get_string(val));
	val = g_hash_table_lookup(config, "Wireless");
	if (val)
		ifm->wireless = g_value_get_boolean(val);
	if (ifm->wireless) {
		val = g_hash_table_lookup(config, "SSID");
		if (val)
			ifm->ssid = g_strdup(g_value_get_string(val));
	}
	val = g_hash_table_lookup(config, "IPAddress");
	if (val)
		ifm->ip.s_addr = g_value_get_uint(val);
	val = g_hash_table_lookup(config, "SubnetCIDR");
	if (val)
		ifm->cidr = g_value_get_uchar(val);
	val = g_hash_table_lookup(config, "InterfaceOrder");
	if (val) {
		g_strfreev(interface_order);
		interface_order = g_strsplit(g_value_get_string(val), " ", 0);
	}
	return ifm;
}

static bool
if_up(const struct if_msg *ifm)
{
	const char *const *r;

	for (r = up_reasons; *r; r++)
		if (g_strcmp0(*r, ifm->reason) == 0)
			return true;
	return false;
}

static char *
print_if_msg(const struct if_msg *ifm)
{
	char *msg, *p;
	const char *reason = NULL;
	size_t len;
	bool showip, showssid;
    
	showip = true;
	showssid = false;
	if (if_up(ifm))
		reason = N_("Acquired address");
	else if (g_strcmp0(ifm->reason, "EXPIRE") == 0)
		reason = N_("Expired");
	else if (g_strcmp0(ifm->reason, "CARRIER") == 0) {
		if (ifm->wireless) {
			reason = N_("Associated with");
			if (ifm->ssid != NULL)
				showssid = true;
		} else
			reason = N_("Cable plugged in");
		showip = false;
	} else if (g_strcmp0(ifm->reason, "NOCARRIER") == 0) {
		if (ifm->wireless) {
			if (ifm->ssid != NULL || ifm->ip.s_addr != 0) {
				reason = N_("Disassociated from");
				showssid = true;
			} else
				reason = N_("Not associated");
		} else
			reason = N_("Cable unplugged");
		showip = false;
	} else if (g_strcmp0(ifm->reason, "FAIL") == 0)
		reason = N_("Automatic configuration not possible");
	else if (g_strcmp0(ifm->reason, "3RDPARTY") == 0)
		reason = N_("Waiting for 3rd Party configuration");

	if (reason == NULL)
		reason = ifm->reason;
	
	len = strlen(ifm->ifname) + 3;
	len += strlen(reason) + 1;
	if (ifm->ip.s_addr != 0) {
		len += 16; /* 000. * 4 */
		if (ifm->cidr != 0)
			len += 3; /* /32 */
	}
	if (showssid)
		len += strlen(ifm->ssid) + 1;
	msg = p = g_malloc(len);
	p += g_snprintf(msg, len, "%s: %s", ifm->ifname, reason);
	if (showssid)
		p += g_snprintf(p, len - (p - msg), " %s", ifm->ssid);
	if (ifm->ip.s_addr != 0 && showip) {
		p += g_snprintf(p, len - (p - msg), " %s", inet_ntoa(ifm->ip));
		if (ifm->cidr != 0)
			g_snprintf(p, len - (p - msg), "/%d", ifm->cidr);
	}
	return msg;
}

static int
if_msg_comparer(gconstpointer a, gconstpointer b)
{
	const struct if_msg *ifa, *ifb;
	const char *const *order;

	ifa = (const struct if_msg *)a;
	ifb = (const struct if_msg *)b;
	for (order = (const char *const *)interface_order; *order; order++) {
		if (g_strcmp0(*order, ifa->ifname) == 0)
			return -1;
		if (g_strcmp0(*order, ifb->ifname) == 0)
			return 1;
	}
	return 0;
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
update_online(void)
{
	bool ison, iscarrier;
	char *msg, *msgs, *tmp;
	const GSList *gl;
	const struct if_msg *ifm;

	ison = iscarrier = false;
	msgs = NULL;
	for (gl = interfaces; gl; gl = gl->next) {
		ifm = (const struct if_msg *)gl->data;
		if (if_up(ifm))
			ison = iscarrier = true;
		if (!iscarrier && g_strcmp0(ifm->reason, "CARRIER") == 0)
			iscarrier = true;
		msg = print_if_msg(ifm);
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
	if (nn != NULL)
		notify_notification_close(nn, NULL);
}

static void
notify_closed(void)
{
	nn = NULL;
}

static void
notify(const char *title, const char *msg, const char *icon)
{
	char **msgs, **m;

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

static void
dhcpcd_event(_unused DBusGProxy *proxy, GHashTable *config, _unused void *data)
{
	struct if_msg *ifm, *ifp;
	bool rem;
	GSList *gl;
	char *msg;
	const char *icon;
	const char *const *r;

	ifm = make_if_msg(config);
	if (ifm == NULL)
		return;

	rem = ignore_if_msg(ifm);
	ifp = NULL;
	for (gl = interfaces; gl; gl = gl->next) {
		ifp = (struct if_msg *)gl->data;
		if (g_strcmp0(ifp->ifname, ifm->ifname) == 0) {
			ifm->scan_results = ifp->scan_results;
			ifp->scan_results = NULL;
			free_if_msg(ifp);
			if (rem)
				interfaces =
				    g_slist_delete_link(interfaces, gl);
			else
				gl->data = ifm;
			break;
		}
	}
	if (ifp == NULL && !rem)
		interfaces = g_slist_prepend(interfaces, ifm);
	interfaces = g_slist_sort(interfaces, if_msg_comparer);
	update_online();

	/* We should ignore renew and stop so we don't annoy the user */
	if (g_strcmp0(ifm->reason, "RENEW") == 0 ||
	    g_strcmp0(ifm->reason, "STOP") == 0)
		return;

	msg = print_if_msg(ifm);
	if (if_up(ifm))
		icon = "network-transmit-receive";
	else
		icon = "network-transmit";
	for (r = down_reasons; *r; r++) {
		if (g_strcmp0(*r, ifm->reason) == 0) {
			icon = "network-offline";
			break;
		}
 	}
	notify(_("Network event"), msg, icon);
	g_free(msg);
}

static void
foreach_make_ifm(_unused gpointer key, gpointer value, _unused gpointer data)
{
	struct if_msg *ifm;

	ifm = make_if_msg((GHashTable *)value);
	if (ignore_if_msg(ifm))
		g_free(ifm);
	else if (ifm)
		interfaces = g_slist_prepend(interfaces, ifm);
}

static void
dhcpcd_get_interfaces()
{
	GHashTable *ifs;
	GError *error = NULL;
	GType otype;
	GSList *gl, *gsl;
	GPtrArray *array;
	struct if_msg *ifm;

	otype = dbus_g_type_get_map("GHashTable", G_TYPE_STRING, G_TYPE_VALUE);
	otype = dbus_g_type_get_map("GHashTable", G_TYPE_STRING, otype);
	if (!dbus_g_proxy_call(dbus, "GetInterfaces", &error,
		G_TYPE_INVALID,
		otype, &ifs, G_TYPE_INVALID))
		error_exit("GetInterfaces", error);
	g_hash_table_foreach(ifs, foreach_make_ifm, NULL);
	g_hash_table_unref(ifs);

	/* Each interface config only remembers the last order when
	 * that interface was configured, so get the real order now. */
	g_strfreev(interface_order);
	interface_order = NULL;
	if (!dbus_g_proxy_call(dbus, "ListInterfaces", &error,
		G_TYPE_INVALID,
		G_TYPE_STRV, &interface_order, G_TYPE_INVALID))
		error_exit("ListInterfaces", error);
	interfaces = g_slist_sort(interfaces, if_msg_comparer);

	otype = dbus_g_type_get_map("GHashTable", G_TYPE_STRING, G_TYPE_VALUE);
	otype = dbus_g_type_get_collection("GPtrArray", otype);
	for (gl = interfaces; gl; gl = gl->next) {
		ifm = (struct if_msg *)gl->data;
		if (!ifm->wireless)
			continue;
		if (!dbus_g_proxy_call(dbus, "ScanResults", &error,
			G_TYPE_STRING, ifm->ifname, G_TYPE_INVALID,
			otype, &array, G_TYPE_INVALID)) 
		{
			g_message("ScanResults: %s", error->message);
			g_clear_error(&error);
			continue;
		}
		for (gsl = ifm->scan_results; gsl; gsl = gsl->next)
			g_free(gsl->data);
		g_slist_free(ifm->scan_results);
		ifm->scan_results = get_scan_results(ifm);
	}

	update_online();
}

static void
check_status(const char *status)
{
	static char *last = NULL;
	GSList *gl;
	char *version;
	const char *msg;
	bool refresh;
	GError *error = NULL;

	g_message("Status changed to %s", status);
	if (g_strcmp0(status, "down") == 0) {
		for (gl = interfaces; gl; gl = gl->next)
			free_if_msg((struct if_msg *)gl->data);
		g_slist_free(interfaces);
		interfaces = NULL;
		update_online();
		msg = N_(last ?
		    "Connection to dhcpcd lost" : "dhcpcd not running");
		gtk_status_icon_set_tooltip(status_icon, msg);
		notify(_("No network"), msg, "network-offline");
	}

	refresh = false;
	if (last == NULL) {
		if (g_strcmp0(status, "down") != 0)
			refresh = true;
	} else {
		if (g_strcmp0(status, last) == 0)
			return;
		if (g_strcmp0(last, "down") == 0)
			refresh = true;
		g_free(last);
	}
	last = g_strdup(status);

	if (!refresh)
		return;
	if (!dbus_g_proxy_call(dbus, "GetDhcpcdVersion", &error,
		G_TYPE_INVALID,
		G_TYPE_STRING, &version, G_TYPE_INVALID))
		error_exit(_("GetDhcpcdVersion"), error);
	g_message(_("Connected to %s-%s"), "dhcpcd", version);
	g_free(version);
	dhcpcd_get_interfaces();
}

static void
dhcpcd_status(_unused DBusGProxy *proxy, const char *status,
    _unused void *data)
{
	check_status(status);
}

static void
dhcpcd_scan_results(_unused DBusGProxy *proxy, const char *iface,
    _unused void *data)
{
	struct if_msg *ifm;
	struct if_ap *ifa, *ifa2;
	GSList *gl, *aps, *l;
	char *txt, *ntxt;

	ifm = find_if_msg(iface);
	if (ifm == NULL)
		return;
	g_message(_("%s: Received scan results"), ifm->ifname);
	aps = get_scan_results(ifm);
	txt = NULL;
	for (gl = aps; gl; gl = gl->next) {
		ifa = (struct if_ap *)gl->data;
		for (l = ifm->scan_results; l; l = l->next) {
			ifa2 = (struct if_ap *)l->data;
			if (g_strcmp0(ifa->ssid, ifa2->ssid) == 0)
				break;
		}
		if (l == NULL) {
			if (txt == NULL)
				txt = g_strdup(ifa->ssid);
			else {
				ntxt = g_strconcat(txt, "\n", ifa->ssid, NULL);
				g_free(txt);
				txt = ntxt;
			}
		}
	}
	for (gl = ifm->scan_results; gl; gl = gl->next)
		free_if_ap((struct if_ap *)gl->data);
	g_slist_free(ifm->scan_results);
	ifm->scan_results = aps;
	if (txt != NULL) {
		notify(strchr(txt, '\n') ?
		    N_("New Access Points") : N_("New Access Point"),
		    txt, "network-wireless");
		g_free(txt);
	}
}

int
main(int argc, char *argv[])
{
	DBusGConnection *bus;
	GError *error = NULL;
	char *version = NULL;
	GType otype;
	int tries = 5;
		
	setlocale(LC_ALL, "");
	bindtextdomain(PACKAGE, NULL);
	bind_textdomain_codeset(PACKAGE, "UTF-8");
	textdomain(PACKAGE); 

	gtk_init(&argc, &argv);
	g_set_application_name("Network Configurator");
	icontheme = gtk_icon_theme_get_default();
	gtk_icon_theme_append_search_path(icontheme, ICONDIR);
	status_icon = gtk_status_icon_new_from_icon_name("network-offline");
	
	gtk_status_icon_set_tooltip(status_icon,
	    _("Connecting to dhcpcd ..."));
	gtk_status_icon_set_visible(status_icon, true);

	notify_init(PACKAGE);

	g_message(_("Connecting to dbus ..."));
	bus = dbus_g_bus_get(DBUS_BUS_SYSTEM, &error);
	if (bus == NULL || error != NULL)
		error_exit(_("Could not connect to system bus"), error);
	dbus = dbus_g_proxy_new_for_name(bus,
	    DHCPCD_SERVICE,
	    DHCPCD_PATH,
	    DHCPCD_SERVICE);

	g_message(_("Connecting to dhcpcd-dbus ..."));
	while (--tries > 0) {
		g_clear_error(&error);
		if (dbus_g_proxy_call_with_timeout(dbus, "GetVersion", 500,
			&error, G_TYPE_INVALID,
			G_TYPE_STRING, &version, G_TYPE_INVALID))
			break;
	}
	if (tries == 0)
		error_exit(_("GetVersion"), error);
	g_message(_("Connected to %s-%s"), "dhcpcd-dbus", version);
	g_free(version);

	gtk_status_icon_set_tooltip(status_icon, _("Triggering dhcpcd ..."));
	online = false;
	menu_init(status_icon);

	if (!dbus_g_proxy_call(dbus, "GetStatus", &error,
		G_TYPE_INVALID,
		G_TYPE_STRING, &version, G_TYPE_INVALID))
		error_exit(_("GetStatus"), error);
	check_status(version);
	g_free(version);

	otype = dbus_g_type_get_map("GHashTable", G_TYPE_STRING, G_TYPE_VALUE);
	dbus_g_proxy_add_signal(dbus, "Event",
	    otype, G_TYPE_INVALID);
	dbus_g_proxy_connect_signal(dbus, "Event",
	    G_CALLBACK(dhcpcd_event), bus, NULL);
	dbus_g_proxy_add_signal(dbus, "StatusChanged",
	    G_TYPE_STRING, G_TYPE_INVALID);
	dbus_g_proxy_connect_signal(dbus, "StatusChanged",
	    G_CALLBACK(dhcpcd_status), bus, NULL);
	dbus_g_proxy_add_signal(dbus, "ScanResults",
	    G_TYPE_STRING, G_TYPE_INVALID);
	dbus_g_proxy_connect_signal(dbus, "ScanResults",
	    G_CALLBACK(dhcpcd_scan_results), bus, NULL);

	gtk_main();
	return 0;
}