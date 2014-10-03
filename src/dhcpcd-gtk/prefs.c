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

#include <sys/types.h>
#include <arpa/inet.h>
#include <net/if.h>

#include <errno.h>

#include "dhcpcd-gtk.h"

static GtkWidget *dialog, *blocks, *names, *controls, *clear, *rebind;
static GtkWidget *autoconf, *address, *router, *dns_servers, *dns_search;
static DHCPCD_OPTION *config;
static char *block, *name;
static DHCPCD_IF *iface;

static void
config_err_dialog(DHCPCD_CONNECTION *con, bool writing, const char *txt)
{
	GtkWidget *edialog;
	char *t;

	t = g_strconcat(_(writing ? "Error saving" : "Error reading"), " ",
	    dhcpcd_cffile(con), "\n\n", txt, NULL);
	edialog = gtk_message_dialog_new(NULL, GTK_DIALOG_MODAL,
	    GTK_MESSAGE_ERROR, GTK_BUTTONS_OK, "%s", t);
	gtk_window_set_title(GTK_WINDOW(edialog), _("Config error"));
	gtk_dialog_run(GTK_DIALOG(edialog));
	gtk_widget_destroy(edialog);
	g_free(t);
}

static void
show_config(DHCPCD_OPTION *conf)
{
	const char *val;
	bool autocnf;

	if ((val = dhcpcd_config_get_static(conf, "ip_address=")) != NULL)
		autocnf = false;
	else {
		if ((val = dhcpcd_config_get(conf, "inform")) == NULL &&
		    (iface && iface->flags & IFF_POINTOPOINT))
			autocnf = false;
		else
			autocnf = true;
	}
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(autoconf), autocnf);
	gtk_entry_set_text(GTK_ENTRY(address), val ? val : "");
	val = dhcpcd_config_get_static(conf, "routers=");
	gtk_entry_set_text(GTK_ENTRY(router), val ? val : "");
	val = dhcpcd_config_get_static(conf, "domain_name_servers=");
	gtk_entry_set_text(GTK_ENTRY(dns_servers), val ? val : "");
	val = dhcpcd_config_get_static(conf, "domain_search=");
	gtk_entry_set_text(GTK_ENTRY(dns_search), val ? val : "");
}

static char *
combo_active_text(GtkWidget *widget)
{
	GtkListStore *store;
	GtkTreeIter iter;
	GValue val;
	char *text;

	store = GTK_LIST_STORE(gtk_combo_box_get_model(GTK_COMBO_BOX(widget)));
	if (!gtk_combo_box_get_active_iter(GTK_COMBO_BOX(widget), &iter))
		return NULL;
	memset(&val, 0, sizeof(val));
	gtk_tree_model_get_value(GTK_TREE_MODEL(store), &iter, 1, &val);
	text = g_strdup(g_value_get_string(&val));
	g_value_unset(&val);
	return text;
}

static bool
set_option(DHCPCD_OPTION **conf, bool s, const char *opt, const char *val,
	bool *ret)
{

	if (s) {
		if (!dhcpcd_config_set_static(conf, opt, val))
			g_critical("dhcpcd_config_set_static: %s",
			    strerror(errno));
		else
			return true;
	} else {
		if (!dhcpcd_config_set(conf, opt, val))
			g_critical("dhcpcd_config_set: %s",
			    strerror(errno));
		else
			return true;
	}

	if (ret)
		*ret = false;
	return false;
}

static bool
make_config(DHCPCD_OPTION **conf)
{
	const char *val, ns[] = "";
	bool a, ret;

	ret = true;
	a = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(autoconf));
	if (iface && iface->flags & IFF_POINTOPOINT)
		set_option(conf, true, "ip_address=", a ? NULL : ns, &ret);
	else {
		val = gtk_entry_get_text(GTK_ENTRY(address));
		if (*val == '\0')
			val = NULL;
		set_option(conf, false, "inform", a ? val : NULL, &ret);
		set_option(conf, true, "ip_address=", a ? NULL : val, &ret);
	}

	val = gtk_entry_get_text(GTK_ENTRY(router));
	if (a && *val == '\0')
		val = NULL;
	set_option(conf, true, "routers=", val, &ret);

	val = gtk_entry_get_text(GTK_ENTRY(dns_servers));
	if (a && *val == '\0')
		val = NULL;
	set_option(conf, true, "domain_name_servers=", val, &ret);

	val = gtk_entry_get_text(GTK_ENTRY(dns_search));
	if (a && *val == '\0')
		val = NULL;
	set_option(conf, true, "domain_search=", val, &ret);

	return ret;
}

static bool
write_config(DHCPCD_CONNECTION *con, DHCPCD_OPTION **conf)
{

	if (make_config(conf) &&
	    !dhcpcd_config_write(con, block, name, *conf))
	{
		const char *s;

		s = strerror(errno);
		g_warning("dhcpcd_config_write: %s", s);
		config_err_dialog(con, true, s);
		return false;
	}
	return true;
}

static GdkPixbuf *
load_icon(const char *iname)
{
	int width, height;

	if (!gtk_icon_size_lookup(GTK_ICON_SIZE_MENU, &width, &height))
		return NULL;
	return gtk_icon_theme_load_icon(gtk_icon_theme_get_default(),
	    iname, MIN(width, height), 0, NULL);
}

static void
set_name_active_icon(const char *iname)
{
	GtkListStore *store;
	GtkTreeIter iter;
	GtkTreePath *path;
	GdkPixbuf *pb;

	store = GTK_LIST_STORE(gtk_combo_box_get_model(GTK_COMBO_BOX(names)));
	if (!gtk_combo_box_get_active_iter(GTK_COMBO_BOX(names), &iter))
		return;
	pb = load_icon(iname);
	if (pb) {
		path = gtk_tree_model_get_path(GTK_TREE_MODEL(store), &iter);
		gtk_list_store_set(store, &iter, 0, pb, -1);
		g_object_unref(pb);
		gtk_tree_model_row_changed(GTK_TREE_MODEL(store), path, &iter);
		gtk_tree_path_free(path);
	}
}

static GSList *
list_interfaces(DHCPCD_CONNECTION *con)
{
	GSList *list;
	DHCPCD_IF *i;

	list = NULL;
	for (i = dhcpcd_interfaces(con); i; i = i->next)
		if (strcmp(i->type, "ipv4") == 0)
			list = g_slist_append(list, UNCONST(i->ifname));
	return list;
}

static GSList *
list_ssids(WI_SCANS *scans)
{
	GSList *list, *l;
	WI_SCAN *w;
	DHCPCD_WI_SCAN *wis;

	list = NULL;
	TAILQ_FOREACH(w, scans, next) {
		for (wis = w->scans; wis; wis = wis->next) {
			for (l = list; l; l = l->next)
				if (g_strcmp0((const char *)l->data,
					wis->ssid) == 0)
					break;
			if (l == NULL)
				list = g_slist_append(list, wis->ssid);
		}
	}
	return list;
}

static void
blocks_on_change(GtkWidget *widget, gpointer data)
{
	DHCPCD_CONNECTION *con;
	GtkListStore *store;
	GtkTreeIter iter;
	char **list, **lp;
	const char *iname, *nn;
	GSList *l, *new_names;
	GdkPixbuf *pb;
	int n;

	con = (DHCPCD_CONNECTION *)data;
	if (name) {
		write_config(con, &config);
		dhcpcd_config_free(config);
		config = NULL;
		show_config(config);
		g_free(block);
		g_free(name);
		name = NULL;
	}
	block = combo_active_text(widget);
	store = GTK_LIST_STORE(gtk_combo_box_get_model(GTK_COMBO_BOX(names)));
	gtk_list_store_clear(store);
	list = dhcpcd_config_blocks(con, block);

	if (g_strcmp0(block, "interface") == 0)
		new_names = list_interfaces(con);
	else
		new_names = list_ssids(&wi_scans);

	n = 0;
	for (l = new_names; l; l = l->next) {
		nn = (const char *)l->data;
		if (list) {
			for (lp = list; *lp; lp++)
				if (g_strcmp0(nn, *lp) == 0)
					break;
			if (*lp)
				iname = "document-save";
			else
				iname = "document-new";
		} else
			iname = "document-new";
		pb = load_icon(iname);
		gtk_list_store_append(store, &iter);
		gtk_list_store_set(store, &iter, 0, pb, 1, nn, -1);
		g_object_unref(pb);
		n++;
	}

	for (lp = list; lp && *lp; lp++) {
		for (l = new_names; l; l = l->next)
			if (g_strcmp0((const char *)l->data, *lp) == 0)
				break;
		if (l != NULL)
			continue;
		pb = load_icon("document-save");
		gtk_list_store_append(store, &iter);
		gtk_list_store_set(store, &iter, 0, pb, 1, *lp, -1);
		g_object_unref(pb);
		n++;
	}
	gtk_widget_set_sensitive(names, n);
	g_slist_free(new_names);
	g_strfreev(list);
}

static void
names_on_change(_unused GtkWidget *widget, gpointer data)
{
	DHCPCD_CONNECTION *con;
	DHCPCD_IF *i;

	con = (DHCPCD_CONNECTION *)data;
	if (name) {
		write_config(con, &config);
		g_free(name);
	}
	name = combo_active_text(names);
	dhcpcd_config_free(config);
	iface = NULL;
	if (g_strcmp0(block, "interface") == 0) {
		for (i = dhcpcd_interfaces(con); i; i = i->next)
			if (g_strcmp0(name, i->ifname) == 0) {
				iface = i;
				break;
			}
	}
	gtk_widget_set_sensitive(address,
	    iface && (iface->flags & IFF_POINTOPOINT) == 0);
	if (block && name) {
		errno = 0;
		config = dhcpcd_config_read(con, block, name);
		if (config == NULL && errno) {
			const char *s;

			s = strerror(errno);
			g_warning("dhcpcd_config_read: %s", s);
			config_err_dialog(con, false, s);
		}
	} else
		config = NULL;
	show_config(config);
	gtk_widget_set_sensitive(controls, name ? true : false);
	gtk_widget_set_sensitive(clear, name ? true : false);
	gtk_widget_set_sensitive(rebind, name ? true : false);
}

static bool
valid_address(const char *val, bool allow_cidr)
{
	char *addr, *p, *e;
	struct in_addr in;
	gint64 cidr;
	bool retval;

	addr = g_strdup(val);
	if (allow_cidr) {
		p = strchr(addr, '/');
		if (p != NULL) {
			*p++ = '\0';
			errno = 0;
			e = NULL;
			cidr = g_ascii_strtoll(p, &e, 10);
			if (cidr < 0 || cidr > 32 ||
			    errno != 0 || *e != '\0')
			{
				retval = false;
				goto out;
			}
		}
	}
	retval = inet_aton(addr, &in) == 0 ? false : true;

out:
	g_free(addr);
	return retval;
}

static bool
address_lost_focus(GtkEntry *entry)
{
	const char *val;

	val = gtk_entry_get_text(entry);
	if (*val != '\0' && !valid_address(val, true))
		gtk_entry_set_text(entry, "");
	return false;
}

static bool
entry_lost_focus(GtkEntry *entry)
{
	const char *val;
	char **a, **p;

	val = gtk_entry_get_text(entry);
	a = g_strsplit(val, " ", 0);
	for (p = a; *p; p++) {
		if (**p != '\0' && !valid_address(*p, false)) {
			gtk_entry_set_text(entry, "");
			break;
		}
	}
	g_strfreev(a);
	return false;
}

static void
on_clear(_unused GtkWidget *o, gpointer data)
{
	DHCPCD_CONNECTION *con;

	con = (DHCPCD_CONNECTION *)data;
	dhcpcd_config_free(config);
	config = NULL;
	if (dhcpcd_config_write(con, block, name, config)) {
		set_name_active_icon("document-new");
		show_config(config);
	} else
		g_critical("dhcpcd_config_write: %s", strerror(errno));
}

static void
on_rebind(_unused GObject *widget, gpointer data)
{
	DHCPCD_CONNECTION *con;
	DHCPCD_IF *i;

	con = (DHCPCD_CONNECTION *)data;
	if (write_config(con, &config)) {
		set_name_active_icon(config == NULL ?
		    "document-new" : "document-save");
		show_config(config);
		if (g_strcmp0(block, "interface") == 0) {
			if (dhcpcd_rebind(con, iface->ifname) == -1)
				g_critical("dhcpcd_rebind %s: %s",
				    iface->ifname, strerror(errno));
		} else {
			for (i = dhcpcd_interfaces(con); i; i = i->next) {
				if (g_strcmp0(i->ssid, name) == 0) {
					if (dhcpcd_rebind(con, i->ifname) == -1)
						g_critical(
						    "dhcpcd_rebind %s: %s",
						    i->ifname,
						    strerror(errno));
				}
			}
		}
	}
}

static void
on_destroy(_unused GObject *o, gpointer data)
{

	if (name != NULL) {
		write_config((DHCPCD_CONNECTION *)data, &config);
		g_free(block);
		g_free(name);
		block = name = NULL;
	}
	dhcpcd_config_free(config);
	config = NULL;
	dialog = NULL;
}

static void
prefs_close(void)
{
	if (dialog != NULL) {
		gtk_widget_destroy(dialog);
		dialog = NULL;
	}
}

void
prefs_abort(void)
{
	g_free(name);
	name = NULL;
	prefs_close();
}

#if GTK_MAJOR_VERSION == 2
static GtkWidget *
gtk_separator_new(GtkOrientation o)
{

	if (o == GTK_ORIENTATION_HORIZONTAL)
		return gtk_hseparator_new();
	else
		return gtk_vseparator_new();
}
#endif

void
prefs_show(DHCPCD_CONNECTION *con)
{
	GtkWidget *dialog_vbox, *hbox, *vbox, *table, *w;
	GtkListStore *store;
	GtkTreeIter iter;
	GtkCellRenderer *rend;
	GdkPixbuf *pb;

	if (dialog) {
		gtk_window_present(GTK_WINDOW(dialog));
		return;
	}

	if (g_strcmp0(dhcpcd_status(con), "down") == 0)
		return;

	dialog = gtk_window_new(GTK_WINDOW_TOPLEVEL);
	g_signal_connect(G_OBJECT(dialog), "destroy",
	    G_CALLBACK(on_destroy), con);

	gtk_window_set_title(GTK_WINDOW(dialog), _("Network Preferences"));
	gtk_window_set_resizable(GTK_WINDOW(dialog), false);
	gtk_window_set_icon_name(GTK_WINDOW(dialog),
	    "preferences-system-network");
	gtk_window_set_position(GTK_WINDOW(dialog), GTK_WIN_POS_CENTER);
	gtk_window_set_type_hint(GTK_WINDOW(dialog),
	    GDK_WINDOW_TYPE_HINT_DIALOG);

	dialog_vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 10);
	gtk_container_set_border_width(GTK_CONTAINER(dialog), 10);
	gtk_container_add(GTK_CONTAINER(dialog), dialog_vbox);

	hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
	gtk_box_pack_start(GTK_BOX(dialog_vbox), hbox, false, false, 3);
	w = gtk_label_new("Configure:");
	gtk_box_pack_start(GTK_BOX(hbox), w, false, false, 3);
	store = gtk_list_store_new(2, GDK_TYPE_PIXBUF, G_TYPE_STRING);
	pb = load_icon("network-wired");
	gtk_list_store_append(store, &iter);
	gtk_list_store_set(store, &iter, 0, pb, 1, "interface", -1);
	g_object_unref(pb);
	pb = load_icon("network-wireless");
	gtk_list_store_append(store, &iter);
	gtk_list_store_set(store, &iter, 0, pb, 1, "ssid", -1);
	g_object_unref(pb);
	blocks = gtk_combo_box_new_with_model(GTK_TREE_MODEL(store));
	rend = gtk_cell_renderer_pixbuf_new();
	gtk_cell_layout_pack_start(GTK_CELL_LAYOUT(blocks), rend, false);
	gtk_cell_layout_add_attribute(GTK_CELL_LAYOUT(blocks),
	    rend, "pixbuf", 0);
	rend = gtk_cell_renderer_text_new();
	gtk_cell_layout_pack_start(GTK_CELL_LAYOUT(blocks), rend, true);
	gtk_cell_layout_add_attribute(GTK_CELL_LAYOUT(blocks),
	    rend, "text", 1);
	gtk_combo_box_set_active(GTK_COMBO_BOX(blocks), 0);
	gtk_box_pack_start(GTK_BOX(hbox), blocks, false, false, 3);
	store = gtk_list_store_new(2, GDK_TYPE_PIXBUF, G_TYPE_STRING);
	names = gtk_combo_box_new_with_model(GTK_TREE_MODEL(store));
	rend = gtk_cell_renderer_pixbuf_new();
	gtk_cell_layout_pack_start(GTK_CELL_LAYOUT(names), rend, false);
	gtk_cell_layout_add_attribute(GTK_CELL_LAYOUT(names),
	    rend, "pixbuf", 0);
	rend = gtk_cell_renderer_text_new();
	gtk_cell_layout_pack_start(GTK_CELL_LAYOUT(names), rend, true);
	gtk_cell_layout_add_attribute(GTK_CELL_LAYOUT(names), rend, "text", 1);
	gtk_widget_set_sensitive(names, false);
	gtk_box_pack_start(GTK_BOX(hbox), names, false, false, 3);
	g_signal_connect(G_OBJECT(blocks), "changed",
	    G_CALLBACK(blocks_on_change), con);
	g_signal_connect(G_OBJECT(names), "changed",
	    G_CALLBACK(names_on_change), con);

	w = gtk_separator_new(GTK_ORIENTATION_HORIZONTAL);
	gtk_box_pack_start(GTK_BOX(dialog_vbox), w, true, false, 3);
	controls = gtk_box_new(GTK_ORIENTATION_VERTICAL, 10);
	gtk_widget_set_sensitive(controls, false);
	gtk_box_pack_start(GTK_BOX(dialog_vbox), controls, true, true, 0);
	vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 3);
	gtk_box_pack_start(GTK_BOX(controls), vbox, false, false, 0);
	autoconf = gtk_check_button_new_with_label(
		_("Automatically configure empty options"));
	gtk_box_pack_start(GTK_BOX(vbox), autoconf, false, false, 3);
	table = gtk_table_new(6, 2, false);
	gtk_box_pack_start(GTK_BOX(controls), table, false, false, 0);

#define attach_label(a, b, c, d, e)					      \
	do {								      \
		gtk_misc_set_alignment(GTK_MISC(a), 0.0, 0.5);		      \
		gtk_table_attach(GTK_TABLE(table), a, b, c, d, e,	      \
		    GTK_FILL | GTK_SHRINK, GTK_FILL | GTK_SHRINK, 3, 3);      \
	} while (0)
#define attach_entry(a, b, c, d, e)					      \
	gtk_table_attach(GTK_TABLE(table), a, b, c, d, e,		      \
	    GTK_EXPAND | GTK_FILL | GTK_SHRINK, GTK_FILL | GTK_SHRINK, 3, 3);

	w = gtk_label_new(_("IP Address:"));
	address = gtk_entry_new();
	gtk_entry_set_max_length(GTK_ENTRY(address), 18);
	g_signal_connect(G_OBJECT(address), "focus-out-event",
	    G_CALLBACK(address_lost_focus), NULL);
	attach_label(w, 0, 1, 0, 1);
	attach_entry(address, 1, 2, 0, 1);

	w = gtk_label_new(_("Router:"));
	router = gtk_entry_new();
	gtk_entry_set_max_length(GTK_ENTRY(router), 15);
	g_signal_connect(G_OBJECT(router), "focus-out-event",
	    G_CALLBACK(entry_lost_focus), NULL);
	attach_label(w, 0, 1, 2, 3);
	attach_entry(router, 1, 2, 2, 3);

	w = gtk_label_new(_("DNS Servers:"));
	dns_servers = gtk_entry_new();
	g_signal_connect(G_OBJECT(dns_servers), "focus-out-event",
	    G_CALLBACK(entry_lost_focus), NULL);
	attach_label(w, 0, 1, 3, 4);
	attach_entry(dns_servers, 1, 2, 3, 4);

	w = gtk_label_new(_("DNS Search:"));
	dns_search = gtk_entry_new();
	attach_label(w, 0, 1, 4, 5);
	attach_entry(dns_search, 1, 2, 4, 5);

	hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 10);
	gtk_box_pack_start(GTK_BOX(dialog_vbox), hbox, true, true, 3);
	clear = gtk_button_new_from_stock(GTK_STOCK_CLEAR);
	gtk_widget_set_sensitive(clear, false);
	gtk_box_pack_start(GTK_BOX(hbox), clear, false, false, 0);
	g_signal_connect(G_OBJECT(clear), "clicked",
	    G_CALLBACK(on_clear), con);
	rebind = gtk_button_new_with_mnemonic(_("_Rebind"));
	gtk_widget_set_sensitive(rebind, false);
	w = gtk_image_new_from_stock(GTK_STOCK_EXECUTE,
	    GTK_ICON_SIZE_BUTTON);
	gtk_button_set_image(GTK_BUTTON(rebind), w);
	gtk_box_pack_start(GTK_BOX(hbox), rebind, false, false, 0);
	g_signal_connect(G_OBJECT(rebind), "clicked",
	    G_CALLBACK(on_rebind), con);
	w = gtk_button_new_from_stock(GTK_STOCK_CLOSE);
	gtk_box_pack_end(GTK_BOX(hbox), w, false, false, 0);
	g_signal_connect(G_OBJECT(w), "clicked",
	    G_CALLBACK(prefs_close), NULL);

	blocks_on_change(blocks, con);
	show_config(NULL);
	gtk_widget_show_all(dialog);
}
