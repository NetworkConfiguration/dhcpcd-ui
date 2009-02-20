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

#include <dbus/dbus.h>

#include "dhcpcd-gtk.h"
#include "prefs.h"

static GPtrArray *config;
static GtkWidget *dialog, *blocks, *names, *controls;
static GtkWidget *hostname, *fqdn, *clientid, *duid, *arp, *ipv4ll;

static void
free_config(GPtrArray **array)
{
	GPtrArray *a;
	guint i;
	GValueArray *c;

	a = *array;
	if (a == NULL)
		return;
	for (i = 0; i < a->len; i++) {
		c = g_ptr_array_index(a, i);
		g_value_array_free(c);
	}
	g_ptr_array_free(a, TRUE);
	*array = NULL;
}	

static GPtrArray *
read_config(const char *block, const char *name)
{
	GType otype;
	GError *error;
	GPtrArray *array;

	error = NULL;
	otype = dbus_g_type_get_struct("GValueArray",
	    G_TYPE_STRING, G_TYPE_STRING, G_TYPE_INVALID);
	otype = dbus_g_type_get_collection("GPtrArray", otype);
	if (!dbus_g_proxy_call(dbus, "GetConfig", &error,
		G_TYPE_STRING, block, G_TYPE_STRING, name, G_TYPE_INVALID,
		otype, &array, G_TYPE_INVALID))
	{
		g_critical("GetConfig: %s", error->message);
		g_clear_error(&error);
		return NULL;
	}
	return array;
}

static void
save_config(const char *block, const char *name)
{
	GType otype;
	GError *error;

	error = NULL;
	otype = dbus_g_type_get_struct("GValueArray",
	    G_TYPE_STRING, G_TYPE_STRING, G_TYPE_INVALID);
	otype = dbus_g_type_get_collection("GPtrArray", otype);
	if (!dbus_g_proxy_call(dbus, "SetConfig", &error,
		G_TYPE_STRING, block, G_TYPE_STRING, name,
		otype, config, G_TYPE_INVALID, G_TYPE_INVALID))
	{
		g_critical("SetConfig: %s", error->message);
		g_clear_error(&error);
	}
}

static gboolean
get_config(GPtrArray *array, const char *option, const char **value)
{
	guint i;
	GValueArray *c;
	GValue *val;
	const char *str;

	if (array == NULL)
		return FALSE;
	for (i = 0; i < array->len; i++) {
		c = g_ptr_array_index(array, i);
		val = g_value_array_get_nth(c, 0);
		str = g_value_get_string(val);
		if (strcmp(str, option) != 0)
			continue;
		if (value != NULL) {
			val = g_value_array_get_nth(c, 1);
			str = g_value_get_string(val);
			if (*str == '\0')
				*value = NULL;
			else
				*value = str;
		}
		return TRUE;
	}
	if (value != NULL)
		*value = NULL;
	return FALSE;
}

static gboolean
toggle_generic(gboolean has, const char *val)
{
	return (has && (val == NULL || g_strcmp0(val, "\"\"") == 0));
}

static gboolean
toggle_generic_neg(gboolean has, _unused const char *val)
{
	return !has;
}

static gboolean
toggle_fqdn(gboolean has, const char *val)
{
	return (has &&
	    (val == NULL ||
		g_strcmp0(val, "both") == 0 ||
		g_strcmp0(val, "ptr") == 0));
}

static void
set_check(GtkToggleButton *button,
    GPtrArray *global, GPtrArray *conf, const char *name,
    gboolean (*test)(gboolean, const char *))
{
	const char *val;
	gboolean has, incons;
	
	if (get_config(conf, name, &val)) {
		has = TRUE;
		incons = FALSE;
	} else if (global == NULL) {
		incons = FALSE;
		has = FALSE;
	} else {
		has = get_config(global, name, &val);
		incons = TRUE;
	}
	gtk_toggle_button_set_active(button, test(has, val));
	gtk_toggle_button_set_inconsistent(button, incons);
}

static void
show_config(const char *block, const char *name)
{
	GPtrArray *global;

	if (block || name)
		global = read_config(NULL, NULL);
	else
		global = NULL;
	
	free_config(&config);
	config = read_config(block, name);
	set_check(GTK_TOGGLE_BUTTON(hostname), global, config,
	    "hostname", toggle_generic);
	set_check(GTK_TOGGLE_BUTTON(fqdn), global, config,
	    "fqdn", toggle_fqdn);
	set_check(GTK_TOGGLE_BUTTON(clientid), global, config,
	    "clientid", toggle_generic);
	set_check(GTK_TOGGLE_BUTTON(duid), global, config,
	    "duid", toggle_generic);
	gtk_widget_set_sensitive(duid,
	    gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(clientid)));
	set_check(GTK_TOGGLE_BUTTON(arp), global, config,
	    "noarp", toggle_generic_neg);
	set_check(GTK_TOGGLE_BUTTON(ipv4ll), global, config,
	    "noipv4ll", toggle_generic_neg);
	gtk_widget_set_sensitive(ipv4ll,
	    gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(arp)));
	free_config(&global);
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
blocks_on_change(GtkWidget *widget, _unused gpointer data)
{
	GtkListStore *store;
	GtkTreeIter iter;
	GError *error;
	char **list, **lp, *block;
	const char *iname, *nn;
	GSList *l, *new_names;
	GtkIconTheme *it;
	GdkPixbuf *pb;
	gint n;

	block = combo_active_text(widget);
	store = GTK_LIST_STORE(gtk_combo_box_get_model(GTK_COMBO_BOX(names)));
	gtk_list_store_clear(store);
	if (strcmp(block, "global") == 0) {
		g_free(block);
		gtk_widget_set_sensitive(names, FALSE);
		gtk_widget_set_sensitive(controls, TRUE);
		show_config(NULL, NULL);
		return;
	}
	error = NULL;
	if (!dbus_g_proxy_call(dbus, "GetConfigBlocks", &error,
		G_TYPE_STRING, block, G_TYPE_INVALID,
		G_TYPE_STRV, &list, G_TYPE_INVALID))
	{
		g_free(block);
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
	g_free(block);
}

static void
names_on_change(GtkWidget *widget, _unused gpointer data)
{
	char *block, *name;

	block = combo_active_text(blocks);
	name = combo_active_text(widget);
	gtk_widget_set_sensitive(controls, TRUE);
	show_config(block, name);
	g_free(block);
	g_free(name);
}

static void
on_toggle(GtkWidget *widget, gpointer data)
{
	gboolean active;

	gtk_toggle_button_set_inconsistent(GTK_TOGGLE_BUTTON(widget), FALSE);
	if (data == NULL)
		return;
	active = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(widget));
	gtk_widget_set_sensitive(GTK_WIDGET((GtkWidget *)data), active);
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
	if (dialog) {
		gtk_widget_destroy(dialog);
		dialog = NULL;
	}
}

static void
on_redo(void)
{
	char *block, *name;
	GValueArray *array;
	GValue val;

	block = combo_active_text(blocks);
	name = combo_active_text(names);
	free_config(&config);
	if (g_strcmp0(block, "global") == 0) {
		config = g_ptr_array_sized_new(3);
		memset(&val, 0, sizeof(val));
		g_value_init(&val, G_TYPE_STRING);
		array = g_value_array_new(2);
		g_value_set_static_string(&val, "hostname");
		array = g_value_array_append(array, &val);
		g_value_set_static_string(&val, "");
		array = g_value_array_append(array, &val);
		g_ptr_array_add(config, array);
		array = g_value_array_new(2);
		g_value_set_static_string(&val, "option");
		array = g_value_array_append(array, &val);
		g_value_set_static_string(&val, "domain_name_servers, "
		    "domain_name, domain_search, host_name");
		array = g_value_array_append(array, &val);
		g_ptr_array_add(config, array);
		array = g_value_array_new(2);
		g_value_set_static_string(&val, "option");
		array = g_value_array_append(array, &val);
		g_value_set_static_string(&val, "ntp_servers");
		array = g_value_array_append(array, &val);
		g_ptr_array_add(config, array);
	} else
		config = g_ptr_array_new();
	save_config(name ? block : NULL, name);
	show_config(name ? block : NULL, name);
	g_free(block);
	g_free(name);
}

void
dhcpcd_prefs_show(void)
{
	GtkWidget *dialog_vbox, *hbox, *vbox, *label;
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
	gtk_signal_connect(GTK_OBJECT(dialog), "destroy", on_destroy, NULL);

	gtk_window_set_title(GTK_WINDOW(dialog), _("dhcpcd preferences"));
	gtk_window_set_resizable(GTK_WINDOW(dialog), FALSE);
	gtk_window_set_icon_name(GTK_WINDOW(dialog), "config-users");
	gtk_window_set_position(GTK_WINDOW(dialog), GTK_WIN_POS_CENTER);
	gtk_window_set_type_hint(GTK_WINDOW(dialog),
				 GDK_WINDOW_TYPE_HINT_DIALOG);

	dialog_vbox = gtk_vbox_new(FALSE, 3);
	gtk_container_add(GTK_CONTAINER(dialog), dialog_vbox);

	hbox = gtk_hbox_new(FALSE, 10);
	gtk_box_pack_start(GTK_BOX(dialog_vbox), hbox, FALSE, FALSE, 3);
	label = gtk_label_new("Configuration block:");
	gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 3);
	store = gtk_list_store_new(2, GDK_TYPE_PIXBUF, G_TYPE_STRING);
	it = gtk_icon_theme_get_default();
	error = NULL;
	pb = gtk_icon_theme_load_icon(it, "config-users",
	    GTK_ICON_SIZE_MENU, 0, &error);	
	gtk_list_store_append(store, &iter);
	gtk_list_store_set(store, &iter, 0, pb, 1, "global", -1);
	g_object_unref(pb);
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
	label = gtk_label_new("Block name:");
	gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 3);
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
	gtk_signal_connect(GTK_OBJECT(blocks), "changed",
	    G_CALLBACK(blocks_on_change), NULL);
	gtk_signal_connect(GTK_OBJECT(names), "changed",
	    G_CALLBACK(names_on_change), NULL);
	
	label = gtk_hseparator_new();
	gtk_box_pack_start(GTK_BOX(dialog_vbox), label, TRUE, FALSE, 3);
	controls = gtk_hbox_new(FALSE, 10);
	gtk_box_pack_start(GTK_BOX(dialog_vbox), controls, TRUE, TRUE, 0);
	vbox = gtk_vbox_new(FALSE, 3);
	gtk_box_pack_start(GTK_BOX(controls), vbox, FALSE, FALSE, 0);
	hostname = gtk_check_button_new_with_label(_("Send Hostname"));
	gtk_signal_connect(GTK_OBJECT(hostname), "toggled",
	    G_CALLBACK(on_toggle), NULL);
	gtk_box_pack_start(GTK_BOX(vbox), hostname, FALSE, FALSE, 3);
	fqdn = gtk_check_button_new_with_label(_("Send FQDN"));
	gtk_signal_connect(GTK_OBJECT(fqdn), "toggled",
	    G_CALLBACK(on_toggle), NULL);
	gtk_box_pack_start(GTK_BOX(vbox), fqdn, FALSE, FALSE, 3);
	clientid = gtk_check_button_new_with_label(_("Send ClientID"));
	gtk_box_pack_start(GTK_BOX(vbox), clientid, FALSE, FALSE, 3);
	duid = gtk_check_button_new_with_label(_("Send DUID"));
	gtk_signal_connect(GTK_OBJECT(clientid), "toggled",
	    G_CALLBACK(on_toggle), duid);
	gtk_signal_connect(GTK_OBJECT(duid), "toggled",
	    G_CALLBACK(on_toggle), NULL);
	gtk_box_pack_start(GTK_BOX(vbox), duid, FALSE, FALSE, 3);
	arp = gtk_check_button_new_with_label(_("Enable ARP checking"));
	gtk_box_pack_start(GTK_BOX(vbox), arp, FALSE, FALSE, 3);
	ipv4ll = gtk_check_button_new_with_label(_("Enable Zeroconf"));
	gtk_box_pack_start(GTK_BOX(vbox), ipv4ll, FALSE, FALSE, 3);
	gtk_signal_connect(GTK_OBJECT(arp), "toggled",
	    G_CALLBACK(on_toggle), ipv4ll);
	gtk_signal_connect(GTK_OBJECT(ipv4ll), "toggled",
	    G_CALLBACK(on_toggle), NULL);

	hbox = gtk_hbox_new(FALSE, 0);
	gtk_box_pack_start(GTK_BOX(dialog_vbox), hbox, TRUE, TRUE, 3);
	label = gtk_button_new_from_stock(GTK_STOCK_REDO);
	gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 0);
	gtk_signal_connect(GTK_OBJECT(label), "clicked", on_redo, NULL);
	label = gtk_button_new_from_stock(GTK_STOCK_CLOSE);
	gtk_box_pack_end(GTK_BOX(hbox), label, FALSE, FALSE, 0);
	gtk_signal_connect(GTK_OBJECT(label), "clicked",
			   dhcpcd_prefs_close, NULL);

	show_config(NULL, NULL);
	gtk_widget_show_all(dialog);
}
