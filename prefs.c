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

#include <errno.h>

#include "dhcpcd-config.h"
#include "dhcpcd-gtk.h"
#include "prefs.h"

static GtkWidget *dialog, *blocks, *names, *controls;
static GtkWidget *autoconf, *address, *router, *dns_servers, *dns_search;
static GtkWidget *ntp_servers;
static GPtrArray *config;
static char *block, *name;
static bool ignore_change;

static void
show_config(GPtrArray *array)
{
	const char *val;
	bool autocnf;

	ignore_change = TRUE;
	if (get_static_config(array, "ip_address=", &val) != -1)
		autocnf = false;
	else {
		get_config(array, 0, "inform", &val);
		autocnf = true;
	}
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(autoconf), autocnf);
	gtk_entry_set_text(GTK_ENTRY(address), val ? val : "");
	get_static_config(array, "routers=", &val);
	gtk_entry_set_text(GTK_ENTRY(router), val ? val : "");
	get_static_config(array, "domain_name_servers=", &val);
	gtk_entry_set_text(GTK_ENTRY(dns_servers), val ? val : "");
	get_static_config(array, "domain_name_search=", &val);
	gtk_entry_set_text(GTK_ENTRY(dns_search), val ? val : "");
	get_static_config(array, "ntp_servers=", &val);
	gtk_entry_set_text(GTK_ENTRY(ntp_servers), val ? val : "");
	ignore_change = FALSE;
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

static void
make_config(GPtrArray *array)
{
	const char *val;
	bool a;

	a = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(autoconf));
	val = gtk_entry_get_text(GTK_ENTRY(address));
	if (*val == '\0')
		val = NULL;
	set_option(array, false, "inform", a ? val : NULL);
	set_option(array, true, "ip_address=", a ? NULL : val);
	
	val = gtk_entry_get_text(GTK_ENTRY(router));
	if (a && *val == '\0')
		val = NULL;
	set_option(array, true, "routers=", val);
	
	val = gtk_entry_get_text(GTK_ENTRY(dns_servers));
	if (a && *val == '\0')
		val = NULL;
	set_option(array, true, "domain_name_servers=", val);
	
	val = gtk_entry_get_text(GTK_ENTRY(dns_search));
	if (a && *val == '\0')
		val = NULL;
	set_option(array, true, "domain_name_search=", val);
	
	val = gtk_entry_get_text(GTK_ENTRY(ntp_servers));
	if (a && *val == '\0')
		val = NULL;
	set_option(array, true, "ntp_servers=", val);
}

static void
set_name_active_icon(const char *iname)
{
	GtkListStore *store;
	GtkTreeIter iter;
	GtkIconTheme *it;
	GtkTreePath *path;
	GdkPixbuf *pb;

	store = GTK_LIST_STORE(gtk_combo_box_get_model(GTK_COMBO_BOX(names)));
	if (!gtk_combo_box_get_active_iter(GTK_COMBO_BOX(names), &iter))
		return;
	it = gtk_icon_theme_get_default();
	pb = gtk_icon_theme_load_icon(it, iname,
	    GTK_ICON_SIZE_MENU, 0, NULL);
	if (pb) {
		path = gtk_tree_model_get_path(GTK_TREE_MODEL(store), &iter);
		gtk_list_store_set(store, &iter, 0, pb, -1);
		g_object_unref(pb);
		gtk_tree_model_row_changed(GTK_TREE_MODEL(store), path, &iter);
		gtk_tree_path_free(path);
	}
}

static GSList *
list_interfaces(void)
{
	GSList *list, *l;
	const struct if_msg *ifm;

	list = NULL;
	for (l = interfaces; l; l = l->next) {
		ifm = (const struct if_msg *)l->data;
		list = g_slist_append(list, ifm->ifname);
	}
	return list;
}

static GSList *
list_ssids(void)
{
	GSList *list, *l, *a, *la;
	const struct if_msg *ifm;
	const struct if_ap *ifa;

	list = NULL;
	for (l = interfaces; l; l = l->next) {
		ifm = (const struct if_msg *)l->data;
		if (!ifm->wireless)
			continue;
		for (a = ifm->scan_results; a; a = a->next) {
			ifa = (const struct if_ap *)a->data;
			for (la = list; la; la = la->next)
				if (g_strcmp0((const char *)la->data,
					ifa->ssid) == 0)
					break;
			if (la == NULL)
				list = g_slist_append(list, ifa->ssid);
		}
	}
	return list;
}

static void
blocks_on_change(GtkWidget *widget)
{
	GtkListStore *store;
	GtkTreeIter iter;
	GError *error;
	char **list, **lp;
	const char *iname, *nn;
	GSList *l, *new_names;
	GtkIconTheme *it;
	GdkPixbuf *pb;
	int n;

	if (name) {
		make_config(config);
		save_config(block, name, config);
		g_free(block);
		g_free(name);
		name = NULL;
	}
	block = combo_active_text(widget);
	store = GTK_LIST_STORE(gtk_combo_box_get_model(GTK_COMBO_BOX(names)));
	gtk_list_store_clear(store);
	error = NULL;
	if (!dbus_g_proxy_call(dbus, "GetConfigBlocks", &error,
		G_TYPE_STRING, block, G_TYPE_INVALID,
		G_TYPE_STRV, &list, G_TYPE_INVALID))
	{
		g_warning("GetConfigBlocks: %s", error->message);
		g_clear_error(&error);
		return;
	}

	it = gtk_icon_theme_get_default();
	if (g_strcmp0(block, "interface") == 0)
		new_names = list_interfaces();
	else
		new_names = list_ssids();

	n = 0;
	for (l = new_names; l; l = l->next) {
		nn = (const char *)l->data;
		for (lp = list; *lp; lp++)
			if (g_strcmp0(nn, *lp) == 0)
				break;
		if (*lp)
			iname = "document-save";
		else
			iname = "document-new";
		pb = gtk_icon_theme_load_icon(it, iname,
		    GTK_ICON_SIZE_MENU, 0, &error);
		gtk_list_store_append(store, &iter);
		gtk_list_store_set(store, &iter, 0, pb, 1, nn, -1);
		g_object_unref(pb);
		n++;
	}

	for (lp = list; *lp; lp++) {
		for (l = new_names; l; l = l->next)
			if (g_strcmp0((const char *)l->data, *lp) == 0)
				break;
		if (l != NULL)
			continue;
		pb = gtk_icon_theme_load_icon(it, "document-save",
		    GTK_ICON_SIZE_MENU, 0, &error);
		gtk_list_store_append(store, &iter);
		gtk_list_store_set(store, &iter, 0, pb, 1, *lp, -1);
		g_object_unref(pb);
		n++;
	}
	gtk_widget_set_sensitive(names, n);
	gtk_widget_set_sensitive(controls, FALSE);
	g_slist_free(new_names);
	g_strfreev(list);
}

static void
names_on_change(void)
{
	
	if (name) {
		make_config(config);
		save_config(block, name, config);
		g_free(name);
	}
	gtk_widget_set_sensitive(controls, TRUE);
	name = combo_active_text(names);
	config = load_config(block, name, config);
	show_config(config);
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
on_clear(void)
{

	free_config(&config);
	config = g_ptr_array_new();
	if (save_config(block, name, config)) {
		set_name_active_icon("document-new");
		show_config(config);
	}
}

static void
on_destroy(void)
{
	
	free_config(&config);
	dialog = NULL;
}

void
dhcpcd_prefs_close(void)
{
	
	if (dialog != NULL) {
		if (name != NULL) {
			make_config(config);
			save_config(block, name, config);
		}
		gtk_widget_destroy(dialog);
		dialog = NULL;
	}
}

void
dhcpcd_prefs_show(void)
{
	GtkWidget *dialog_vbox, *hbox, *vbox, *table, *w;
	GtkListStore *store;
	GtkTreeIter iter;
	GtkCellRenderer *rend;
	GError *error;
	GtkIconTheme *it;
	GdkPixbuf *pb;
	
	if (dialog) {
		gtk_window_present(GTK_WINDOW(dialog));
		return;
	}

	dialog = gtk_window_new(GTK_WINDOW_TOPLEVEL);
	g_signal_connect(G_OBJECT(dialog), "destroy", on_destroy, NULL);

	gtk_window_set_title(GTK_WINDOW(dialog), _("dhcpcd preferences"));
	gtk_window_set_resizable(GTK_WINDOW(dialog), FALSE);
	gtk_window_set_icon_name(GTK_WINDOW(dialog), "config-users");
	gtk_window_set_position(GTK_WINDOW(dialog), GTK_WIN_POS_CENTER);
	gtk_window_set_type_hint(GTK_WINDOW(dialog),
				 GDK_WINDOW_TYPE_HINT_DIALOG);

	dialog_vbox = gtk_vbox_new(FALSE, 10);
	gtk_container_set_border_width(GTK_CONTAINER(dialog), 10);
	gtk_container_add(GTK_CONTAINER(dialog), dialog_vbox);

	hbox = gtk_hbox_new(FALSE, 0);
	gtk_box_pack_start(GTK_BOX(dialog_vbox), hbox, FALSE, FALSE, 3);
	w = gtk_label_new("Configure:");
	gtk_box_pack_start(GTK_BOX(hbox), w, FALSE, FALSE, 3);
	store = gtk_list_store_new(2, GDK_TYPE_PIXBUF, G_TYPE_STRING);
	it = gtk_icon_theme_get_default();
	error = NULL;
	pb = gtk_icon_theme_load_icon(it, "network-wired",
	    GTK_ICON_SIZE_MENU, 0, &error);	
	gtk_list_store_append(store, &iter);
	gtk_list_store_set(store, &iter, 0, pb, 1, "interface", -1);
	g_object_unref(pb);
	pb = gtk_icon_theme_load_icon(it, "network-wireless",
	    GTK_ICON_SIZE_MENU, 0, &error);
	gtk_list_store_append(store, &iter);
	gtk_list_store_set(store, &iter, 0, pb, 1, "ssid", -1);
	g_object_unref(pb);
	blocks = gtk_combo_box_new_with_model(GTK_TREE_MODEL(store));
	rend = gtk_cell_renderer_pixbuf_new();
	gtk_cell_layout_pack_start(GTK_CELL_LAYOUT(blocks), rend, FALSE);
	gtk_cell_layout_add_attribute(GTK_CELL_LAYOUT(blocks),
	    rend, "pixbuf", 0);
	rend = gtk_cell_renderer_text_new();
	gtk_cell_layout_pack_start(GTK_CELL_LAYOUT(blocks), rend, TRUE);
	gtk_cell_layout_add_attribute(GTK_CELL_LAYOUT(blocks),
	    rend, "text", 1);
	gtk_combo_box_set_active(GTK_COMBO_BOX(blocks), 0);
	gtk_box_pack_start(GTK_BOX(hbox), blocks, FALSE, FALSE, 3);
	store = gtk_list_store_new(2, GDK_TYPE_PIXBUF, G_TYPE_STRING);
	names = gtk_combo_box_new_with_model(GTK_TREE_MODEL(store));
	rend = gtk_cell_renderer_pixbuf_new();
	gtk_cell_layout_pack_start(GTK_CELL_LAYOUT(names), rend, FALSE);
	gtk_cell_layout_add_attribute(GTK_CELL_LAYOUT(names),
	    rend, "pixbuf", 0);
	rend = gtk_cell_renderer_text_new();
	gtk_cell_layout_pack_start(GTK_CELL_LAYOUT(names), rend, TRUE);
	gtk_cell_layout_add_attribute(GTK_CELL_LAYOUT(names), rend, "text", 1);
	gtk_widget_set_sensitive(names, FALSE);
	gtk_box_pack_start(GTK_BOX(hbox), names, FALSE, FALSE, 3);
	g_signal_connect(G_OBJECT(blocks), "changed",
	    G_CALLBACK(blocks_on_change), NULL);
	g_signal_connect(G_OBJECT(names), "changed",
	    G_CALLBACK(names_on_change), NULL);
	
	w = gtk_hseparator_new();
	gtk_box_pack_start(GTK_BOX(dialog_vbox), w, TRUE, FALSE, 3);
	controls = gtk_vbox_new(FALSE, 10);
	gtk_box_pack_start(GTK_BOX(dialog_vbox), controls, TRUE, TRUE, 0);
	vbox = gtk_vbox_new(FALSE, 3);
	gtk_box_pack_start(GTK_BOX(controls), vbox, FALSE, FALSE, 0);
	autoconf = gtk_check_button_new_with_label(
		_("Automatically configure empty options"));
	gtk_box_pack_start(GTK_BOX(vbox), autoconf, FALSE, FALSE, 3);
	table = gtk_table_new(6, 2, FALSE);
	gtk_box_pack_start(GTK_BOX(controls), table, FALSE, FALSE, 0);

#define attach_label(a, b, c, d, e)					      \
	do {								      \
		gtk_misc_set_alignment(GTK_MISC(a), 0.0, 0.5);		      \
		gtk_table_attach(GTK_TABLE(table), a, b, c, d, e,	      \
		    GTK_FILL | GTK_SHRINK, GTK_FILL | GTK_SHRINK, 3, 3);      \
	} while (0)
#define attach_entry(a, b, c, d, e)					      \
	gtk_table_attach(GTK_TABLE(table), a, b, c, d, e,		      \
	    GTK_EXPAND | GTK_FILL | GTK_SHRINK, GTK_FILL | GTK_SHRINK, 3, 3); \
	
	w = gtk_label_new(_("IP Address:"));
	address = gtk_entry_new();
	gtk_entry_set_max_length(GTK_ENTRY(address), 15);
	g_signal_connect(G_OBJECT(address), "focus-out-event",
	    G_CALLBACK(address_lost_focus), NULL);
	attach_label(w, 0, 1, 0, 1);
	attach_entry(address, 1, 2, 0, 1);

	w = gtk_label_new(_("Router:"));
	router = gtk_entry_new();
	gtk_entry_set_max_length(GTK_ENTRY(router), 12);
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

	w = gtk_label_new(_("NTP Servers:"));
	ntp_servers = gtk_entry_new();
	g_signal_connect(G_OBJECT(ntp_servers), "focus-out-event",
	    G_CALLBACK(entry_lost_focus), NULL);
	attach_label(w, 0, 1, 5, 6);
	attach_entry(ntp_servers, 1, 2, 5, 6);

	hbox = gtk_hbox_new(FALSE, 0);
	gtk_box_pack_start(GTK_BOX(dialog_vbox), hbox, TRUE, TRUE, 3);
	w = gtk_button_new_from_stock(GTK_STOCK_CLEAR);
	gtk_box_pack_start(GTK_BOX(hbox), w, FALSE, FALSE, 0);
	g_signal_connect(G_OBJECT(w), "clicked", on_clear, NULL);
	w = gtk_button_new_from_stock(GTK_STOCK_CLOSE);
	gtk_box_pack_end(GTK_BOX(hbox), w, FALSE, FALSE, 0);
	g_signal_connect(G_OBJECT(w), "clicked",
	    dhcpcd_prefs_close, NULL);
	
	blocks_on_change(blocks);
	show_config(NULL);
	gtk_widget_show_all(dialog);
}
