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

#include "config.h"
#include "dhcpcd-gtk.h"

static const char *copyright = "Copyright (c) 2009-2014 Roy Marples";
static const char *authors[] = { "Roy Marples <roy@marples.name>", NULL };

static GtkStatusIcon *sicon;
static GtkWidget *menu;
static bool ifmenu;

typedef struct wi_menu {
	DHCPCD_IF *interface;
	DHCPCD_WI_SCAN *scan;

	GtkWidget *menu;
	GtkWidget *ssid;
	GtkWidget *icon;
	GtkWidget *bar;
} WI_MENU;
static GList *wi_menus;

static void
on_pref(_unused GObject *o, gpointer data)
{

	dhcpcd_prefs_show((DHCPCD_CONNECTION *)data);
}

static void
on_quit(void)
{

	gtk_main_quit();
}

#if GTK_MAJOR_VERSION == 2
static void
url_show(GtkAboutDialog *dialog, const char *url)
{
	GdkScreen *screen;

	screen = gtk_widget_get_screen(GTK_WIDGET(dialog));
	gtk_show_uri(screen, url, GDK_CURRENT_TIME, NULL);
}

static void
email_hook(GtkAboutDialog *dialog, const char *url, _unused gpointer data)
{
	char *address;

	address = g_strdup_printf("mailto:%s", url);
	url_show(dialog, address);
	g_free(address);
}


static void
url_hook(GtkAboutDialog *dialog, const char *url, _unused gpointer data)
{
	url_show(dialog, url);
}
#endif

static void
ssid_hook(GtkMenuItem *item, _unused gpointer data)
{
	DHCPCD_WI_SCAN *scan;
	WI_SCAN *wi;

	scan = g_object_get_data(G_OBJECT(item), "dhcpcd_wi_scan");
	wi = wi_scan_find(scan);
	if (wi) {
		DHCPCD_CONNECTION *con;

		con = dhcpcd_if_connection(wi->interface);
		if (con) {
			DHCPCD_WPA *wpa;

			wpa = dhcpcd_wpa_find(con, wi->interface->ifname);
			if (wpa)
				wpa_configure(wpa, scan);
		}
	}
}

static void
on_about(_unused GtkMenuItem *item)
{

	gtk_window_set_default_icon_name("network-transmit-receive");
#if GTK_MAJOR_VERSION == 2
	gtk_about_dialog_set_email_hook(email_hook, NULL, NULL);
	gtk_about_dialog_set_url_hook(url_hook, NULL, NULL);
#endif
	gtk_show_about_dialog(NULL,
	    "version", VERSION,
	    "copyright", copyright,
	    "website-label", "dhcpcd Website",
	    "website", "http://roy.marples.name/projects/dhcpcd",
	    "authors", authors,
	    "logo-icon-name", "network-transmit-receive",
	    "comments", "Part of the dhcpcd project",
	    NULL);
}

static void
update_item(WI_MENU *m, DHCPCD_WI_SCAN *scan)
{
	const char *icon;
	double perc;

	gtk_label_set_text(GTK_LABEL(m->ssid), scan->ssid);
	if (scan->flags[0] == '\0')
		icon = "network-wireless";
	else
		icon = "network-wireless-encrypted";
	m->icon = gtk_image_new_from_icon_name(icon,
	    GTK_ICON_SIZE_MENU);

	perc = scan->strength.value / 100.0;
	gtk_progress_bar_set_fraction(GTK_PROGRESS_BAR(m->bar), perc);

	if (scan->flags[0] == '\0')
		gtk_widget_set_tooltip_text(m->menu, scan->bssid);
	else {
		char *tip = g_strconcat(scan->bssid, " ", scan->flags, NULL);
		gtk_widget_set_tooltip_text(m->menu, tip);
		g_free(tip);
	}

	g_object_set_data(G_OBJECT(m->menu), "dhcpcd_wi_scan", scan);
}

static WI_MENU *
create_menu(GtkWidget *m, WI_SCAN *scan, DHCPCD_WI_SCAN *wis)
{
	WI_MENU *wim;
	GtkWidget *box;
	double perc;
	const char *icon;
	char *tip;

	wim = g_malloc(sizeof(*wim));
	wim->interface = scan->interface;
	wim->scan = wis;
	wim->menu = gtk_check_menu_item_new();
	gtk_check_menu_item_set_draw_as_radio(
	    GTK_CHECK_MENU_ITEM(wim->menu), true);
	box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 4);
	gtk_container_add(GTK_CONTAINER(wim->menu), box);
	wim->ssid = gtk_label_new(wis->ssid);
	gtk_box_pack_start(GTK_BOX(box), wim->ssid, TRUE, TRUE, 0);

	if (g_strcmp0(wis->ssid, scan->interface->ssid) == 0)
		gtk_check_menu_item_set_active(
		    GTK_CHECK_MENU_ITEM(wim->menu), true);
	if (wis->flags[0] == '\0')
		icon = "network-wireless";
	else
		icon = "network-wireless-encrypted";
	wim->icon = gtk_image_new_from_icon_name(icon,
	    GTK_ICON_SIZE_MENU);
	gtk_box_pack_start(GTK_BOX(box), wim->icon, FALSE, FALSE, 0);

	wim->bar = gtk_progress_bar_new();
	gtk_widget_set_size_request(wim->bar, 100, -1);
	gtk_box_pack_end(GTK_BOX(box), wim->bar, FALSE, TRUE, 0);
	perc = wis->strength.value / 100.0;
	gtk_progress_bar_set_fraction(GTK_PROGRESS_BAR(wim->bar), perc);

	if (wis->flags[0] == '\0')
		gtk_widget_set_tooltip_text(wim->menu, wis->bssid);
	else {
		tip = g_strconcat(wis->bssid, " ", wis->flags, NULL);
		gtk_widget_set_tooltip_text(wim->menu, tip);
		g_free(tip);
	}

	g_signal_connect(G_OBJECT(wim->menu), "activate",
	    G_CALLBACK(ssid_hook), NULL);
	g_object_set_data(G_OBJECT(wim->menu), "dhcpcd_wi_scan", wis);
	gtk_menu_shell_append(GTK_MENU_SHELL(m), wim->menu);

	return wim;
}

void
menu_update_scans(WI_SCAN *wi, DHCPCD_WI_SCAN *scans)
{
	GList *item, *next;
	WI_MENU *wim;
	DHCPCD_WI_SCAN *s;
	bool found, added;

	item = wi_menus;
	while (item) {
		next = item->next;
		wim = (WI_MENU *)item->data;
		if (wim->interface != wi->interface)
			goto cont;
		found = false;
		for (s = scans; s; s = s->next) {
			if (memcmp(wim->scan->bssid, s->bssid,
			    sizeof(s->bssid)) == 0)
			{
				found = true;
				update_item(wim, s);
			}
		}
		if (!found) {
			g_message("removed %s", wim->scan->ssid);
			gtk_widget_destroy(wim->menu);
			g_free(wim);
			wi_menus = g_list_delete_link(wi_menus, item);
		}
cont:
		item = next;
	}

	added = false;
	for (s = scans; s; s = s->next) {
		found = false;
		for (item = wi_menus; item; item = item->next) {
			wim = (WI_MENU *)item->data;
			if (memcmp(wim->scan->bssid, s->bssid,
			    sizeof(s->bssid)) == 0)
			{
				found = true;
				break;
			}
		}
		if (!found) {
			added = true;
			wim = create_menu(wi->menu, wi, s);
			gtk_widget_show_all(wim->menu);
			g_message("added %s", wim->scan->ssid);
			wi_menus = g_list_append(wi_menus, wim);
		}
	}
	if (added) {
		gtk_widget_hide(wi->menu);
		gtk_menu_popup(GTK_MENU(wi->menu), NULL, NULL,
		    gtk_status_icon_position_menu, sicon,
		    1, gtk_get_current_event_time());
		gtk_widget_show(wi->menu);
	}
}


static GtkWidget *
add_scans(WI_SCAN *scan)
{
	GtkWidget *m;
	DHCPCD_WI_SCAN *wis;
	WI_MENU *wim;

	if (scan->scans == NULL)
		return NULL;

	m = gtk_menu_new();
	for (wis = scan->scans; wis; wis = wis->next) {
		wim = create_menu(m, scan, wis);
		wi_menus = g_list_append(wi_menus, wim);
	}
	return m;
}

void
dhcpcd_menu_abort(void)
{

	while (wi_menus) {
		g_free(wi_menus->data);
		wi_menus = g_list_delete_link(wi_menus, wi_menus);
	}

	if (menu != NULL) {
		gtk_widget_destroy(menu);
		g_object_ref_sink(menu);
		g_object_unref(menu);
		menu = NULL;
	}
}

static void
on_activate(GtkStatusIcon *icon)
{
	WI_SCAN *w;
	GtkWidget *item, *image;

	sicon = icon;
	notify_close();
	dhcpcd_menu_abort();
	if (wi_scans == NULL)
		return;
	if (wi_scans->next) {
		menu = gtk_menu_new();
		ifmenu = true;
		for (w = wi_scans; w; w = w->next) {
			item = gtk_image_menu_item_new_with_label(
				w->interface->ifname);
			image = gtk_image_new_from_icon_name(
				"network-wireless", GTK_ICON_SIZE_MENU);
			gtk_image_menu_item_set_image(
				GTK_IMAGE_MENU_ITEM(item), image);
			gtk_menu_shell_append(GTK_MENU_SHELL(menu), item);
			w->menu = add_scans(w);
			gtk_menu_item_set_submenu(GTK_MENU_ITEM(item),
			    w->menu);
		}
	} else if (wi_scans->scans) {
		ifmenu = false;
		wi_scans->menu = menu = add_scans(wi_scans);
	} else
		menu = NULL;

	if (menu) {
		gtk_widget_show_all(GTK_WIDGET(menu));
		gtk_menu_popup(GTK_MENU(menu), NULL, NULL,
		    gtk_status_icon_position_menu, icon,
		    1, gtk_get_current_event_time());
	}
}

static void
on_popup(GtkStatusIcon *icon, guint button, guint32 atime, gpointer data)
{
	DHCPCD_CONNECTION *con;
	GtkMenu *mnu;
	GtkWidget *item, *image;

	notify_close();

	con = (DHCPCD_CONNECTION *)data;
	mnu = (GtkMenu *)gtk_menu_new();

	item = gtk_image_menu_item_new_with_mnemonic(_("_Preferences"));
	image = gtk_image_new_from_icon_name("preferences-system-network",
	    GTK_ICON_SIZE_MENU);
	gtk_image_menu_item_set_image(GTK_IMAGE_MENU_ITEM(item), image);
	if (g_strcmp0(dhcpcd_status(con), "down") == 0)
		gtk_widget_set_sensitive(item, false);
	else
		g_signal_connect(G_OBJECT(item), "activate",
		    G_CALLBACK(on_pref), data);
	gtk_menu_shell_append(GTK_MENU_SHELL(mnu), item);

	item = gtk_separator_menu_item_new();
	gtk_menu_shell_append(GTK_MENU_SHELL(mnu), item);

	item = gtk_image_menu_item_new_with_mnemonic(_("_About"));
	image = gtk_image_new_from_icon_name("help-about",
	    GTK_ICON_SIZE_MENU);
	gtk_image_menu_item_set_image(GTK_IMAGE_MENU_ITEM(item), image);
	g_signal_connect(G_OBJECT(item), "activate",
	    G_CALLBACK(on_about), icon);
	gtk_menu_shell_append(GTK_MENU_SHELL(mnu), item);

	item = gtk_image_menu_item_new_with_mnemonic(_("_Quit"));
	image = gtk_image_new_from_icon_name("application-exit",
	    GTK_ICON_SIZE_MENU);
	gtk_image_menu_item_set_image(GTK_IMAGE_MENU_ITEM(item), image);
	g_signal_connect(G_OBJECT(item), "activate",
	    G_CALLBACK(on_quit), icon);
	gtk_menu_shell_append(GTK_MENU_SHELL(mnu), item);

	gtk_widget_show_all(GTK_WIDGET(mnu));
	gtk_menu_popup(GTK_MENU(mnu), NULL, NULL,
	    gtk_status_icon_position_menu, icon, button, atime);
	if (button == 0)
		gtk_menu_shell_select_first(GTK_MENU_SHELL(mnu), FALSE);
}

void
menu_init(GtkStatusIcon *icon, DHCPCD_CONNECTION *con)
{

	g_signal_connect(G_OBJECT(icon), "activate",
	    G_CALLBACK(on_activate), con);
	g_signal_connect(G_OBJECT(icon), "popup_menu",
	    G_CALLBACK(on_popup), con);
}


#if GTK_MAJOR_VERSION == 2
GtkWidget *
gtk_box_new(GtkOrientation o, gint s)
{

	if (o == GTK_ORIENTATION_HORIZONTAL)
		return gtk_hbox_new(false, s);
	else
		return gtk_vbox_new(false, s);
}
#endif
