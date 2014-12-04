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

static GtkStatusIcon *sicon;
static GtkWidget *menu;
static GtkAboutDialog *about;
#ifdef BG_SCAN
static guint bgscan_timer;
#endif

static void
on_pref(_unused GObject *o, gpointer data)
{

	prefs_show((DHCPCD_CONNECTION *)data);
}

static void
on_quit(void)
{

	wpa_abort();
	gtk_main_quit();
}

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

	if (about == NULL) {
		about = GTK_ABOUT_DIALOG(gtk_about_dialog_new());
		gtk_about_dialog_set_version(about, VERSION);
		gtk_about_dialog_set_copyright(about, copyright);
		gtk_about_dialog_set_website_label(about, "dhcpcd Website");
		gtk_about_dialog_set_website(about,
		    "http://roy.marples.name/projects/dhcpcd");
		gtk_about_dialog_set_logo_icon_name(about,
		    "network-transmit-receive");
		gtk_about_dialog_set_comments(about,
		    "Part of the dhcpcd project");
	}
	gtk_window_set_position(GTK_WINDOW(about), GTK_WIN_POS_MOUSE);
	gtk_window_present(GTK_WINDOW(about));
	gtk_dialog_run(GTK_DIALOG(about));
	gtk_widget_hide(GTK_WIDGET(about));
}

static bool
is_associated(WI_SCAN *wi, DHCPCD_WI_SCAN *scan)
{

	return dhcpcd_wi_associated(wi->interface, scan);
}

static void
update_item(WI_SCAN *wi, WI_MENU *m, DHCPCD_WI_SCAN *scan)
{
	const char *icon;
	GtkWidget *sel;

	m->scan = scan;

	g_object_set_data(G_OBJECT(m->menu), "dhcpcd_wi_scan", scan);

	m->associated = is_associated(wi, scan);
	if (m->associated)
		sel = gtk_image_new_from_icon_name("dialog-ok-apply",
		    GTK_ICON_SIZE_MENU);
	else
		sel = NULL;
	gtk_image_menu_item_set_image(
	    GTK_IMAGE_MENU_ITEM(m->menu), sel);

	if (m->associated) {
		gchar *lbl;

		lbl = g_markup_printf_escaped("<b>%s</b>",
		    scan->ssid);
		gtk_label_set_markup(GTK_LABEL(m->ssid), lbl);
		g_free(lbl);
	} else
		gtk_label_set_text(GTK_LABEL(m->ssid), scan->ssid);
	if (scan->flags & WSF_SECURE)
		icon = "network-wireless-encrypted";
	else
		icon = "dialog-warning";
	m->icon = gtk_image_new_from_icon_name(icon,
	    GTK_ICON_SIZE_MENU);

	icon = get_strength_icon_name(scan->strength.value);
	m->strength = gtk_image_new_from_icon_name(icon,
		GTK_ICON_SIZE_MENU);

#if 0
	if (scan->wpa_flags[0] == '\0')
		gtk_widget_set_tooltip_text(m->menu, scan->bssid);
	else {
		char *tip = g_strconcat(scan->bssid, " ", scan->wpa_flags,
		    NULL);
		gtk_widget_set_tooltip_text(m->menu, tip);
		g_free(tip);
	}
#endif

	g_object_set_data(G_OBJECT(m->menu), "dhcpcd_wi_scan", scan);
}

static WI_MENU *
create_menu(WI_SCAN *wis, DHCPCD_WI_SCAN *scan)
{
	WI_MENU *wim;
	GtkWidget *box;
	const char *icon;

	wim = g_malloc(sizeof(*wim));
	wim->scan = scan;
	wim->menu = gtk_image_menu_item_new();
	box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 4);
	gtk_container_add(GTK_CONTAINER(wim->menu), box);

	wim->ssid = gtk_label_new(NULL);
	gtk_misc_set_alignment(GTK_MISC(wim->ssid), 0.0, 0.5);
	gtk_box_pack_start(GTK_BOX(box), wim->ssid, TRUE, TRUE, 0);

	if (scan->flags & WSF_SECURE)
		icon = "network-wireless-encrypted";
	else
		icon = "dialog-warning";
	wim->icon = gtk_image_new_from_icon_name(icon,
	    GTK_ICON_SIZE_MENU);
	gtk_box_pack_start(GTK_BOX(box), wim->icon, FALSE, FALSE, 0);

	icon = get_strength_icon_name(scan->strength.value);
	wim->strength = gtk_image_new_from_icon_name(icon,
		GTK_ICON_SIZE_MENU);
	gtk_box_pack_start(GTK_BOX(box), wim->strength, FALSE, FALSE, 0);

#if 0
	if (scan->wpa_flags[0] == '\0')
		gtk_widget_set_tooltip_text(wim->menu, scan->bssid);
	else {
		tip = g_strconcat(scan->bssid, " ", scan->wpa_flags, NULL);
		gtk_widget_set_tooltip_text(wim->menu, tip);
		g_free(tip);
	}
#endif
	update_item(wis, wim, scan);

	g_signal_connect(G_OBJECT(wim->menu), "activate",
	    G_CALLBACK(ssid_hook), NULL);
	g_object_set_data(G_OBJECT(wim->menu), "dhcpcd_wi_scan", scan);

	return wim;
}

void
menu_update_scans(WI_SCAN *wi, DHCPCD_WI_SCAN *scans)
{
	WI_MENU *wim, *win;
	DHCPCD_WI_SCAN *s;
	bool found;
	int position;

	if (wi->ifmenu == NULL) {
		dhcpcd_wi_scans_free(wi->scans);
		wi->scans = scans;
		return;
	}

	TAILQ_FOREACH_SAFE(wim, &wi->menus, next, win) {
		found = false;
		for (s = scans; s; s = s->next) {
			if (strcmp(wim->scan->ssid, s->ssid) == 0) {
				/* If assoication changes, we
				 * need to remove the item to replace it */
				if (wim->associated ==
				    is_associated(wi, s))
				{
					found = true;
					update_item(wi, wim, s);
				}
				break;
			}
		}
		if (!found) {
			TAILQ_REMOVE(&wi->menus, wim, next);
			gtk_widget_destroy(wim->menu);
			g_free(wim);
		}
	}

	for (s = scans; s; s = s->next) {
		found = false;
		position = 0;
		TAILQ_FOREACH(wim, &wi->menus, next) {
			if (strcmp(wim->scan->ssid, s->ssid) == 0) {
				found = true;
				break;
			}
			/* Assoicated scans are always first */
			if (!is_associated(wi, s) &&
			    dhcpcd_wi_scan_compare(wim->scan, s) < 0)
				position++;
		}
		if (!found) {
			wim = create_menu(wi, s);
			TAILQ_INSERT_TAIL(&wi->menus, wim, next);
			gtk_menu_shell_insert(GTK_MENU_SHELL(wi->ifmenu),
			    wim->menu, position);
			gtk_widget_show_all(wim->menu);
		}
	}

	dhcpcd_wi_scans_free(wi->scans);
	wi->scans = scans;

	if (gtk_widget_get_visible(wi->ifmenu))
		gtk_menu_reposition(GTK_MENU(wi->ifmenu));
}

void
menu_remove_if(WI_SCAN *wi)
{
	WI_MENU *wim;

	if (wi->ifmenu == NULL)
		return;

	if (wi->ifmenu == menu)
		menu = NULL;

	gtk_widget_destroy(wi->ifmenu);
	wi->ifmenu = NULL;
	while ((wim = TAILQ_FIRST(&wi->menus))) {
		TAILQ_REMOVE(&wi->menus, wim, next);
		g_free(wim);
	}

	if (menu && gtk_widget_get_visible(menu))
		gtk_menu_reposition(GTK_MENU(menu));
}

static GtkWidget *
add_scans(WI_SCAN *wi)
{
	GtkWidget *m;
	DHCPCD_WI_SCAN *wis;
	WI_MENU *wim;
	int position;

	if (wi->scans == NULL)
		return NULL;

	m = gtk_menu_new();
	position = 0;
	for (wis = wi->scans; wis; wis = wis->next) {
		wim = create_menu(wi, wis);
		TAILQ_INSERT_TAIL(&wi->menus, wim, next);
		gtk_menu_shell_insert(GTK_MENU_SHELL(m),
		    wim->menu, is_associated(wi, wis) ? 0 : position);
		position++;
	}

	return m;
}

void
menu_abort(void)
{
	WI_SCAN *wis;
	WI_MENU *wim;

#ifdef BG_SCAN
	if (bgscan_timer) {
		g_source_remove(bgscan_timer);
		bgscan_timer = 0;
	}
#endif

	TAILQ_FOREACH(wis, &wi_scans, next) {
		wis->ifmenu = NULL;
		while ((wim = TAILQ_FIRST(&wis->menus))) {
			TAILQ_REMOVE(&wis->menus, wim, next);
			g_free(wim);
		}
	}

	if (menu != NULL) {
		gtk_widget_destroy(menu);
		g_object_ref_sink(menu);
		g_object_unref(menu);
		menu = NULL;
	}
}

#ifdef BG_SCAN
static gboolean
menu_bgscan(gpointer data)
{
	WI_SCAN *w;
	DHCPCD_CONNECTION *con;
	DHCPCD_WPA *wpa;

	if (menu == NULL || !gtk_widget_get_visible(menu)) {
		bgscan_timer = 0;
		return FALSE;
	}

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

static void
on_activate(GtkStatusIcon *icon)
{
	WI_SCAN *w, *l;
	GtkWidget *item, *image;

	sicon = icon;
	notify_close();
	prefs_abort();
	menu_abort();

	if ((w = TAILQ_FIRST(&wi_scans)) == NULL)
		return;

	if ((l = TAILQ_LAST(&wi_scans, wi_scan_head)) && l != w) {
		menu = gtk_menu_new();
		TAILQ_FOREACH(w, &wi_scans, next) {
			item = gtk_image_menu_item_new_with_label(
				w->interface->ifname);
			image = gtk_image_new_from_icon_name(
				"network-wireless", GTK_ICON_SIZE_MENU);
			gtk_image_menu_item_set_image(
				GTK_IMAGE_MENU_ITEM(item), image);
			gtk_menu_shell_append(GTK_MENU_SHELL(menu), item);
			w->ifmenu = add_scans(w);
			gtk_menu_item_set_submenu(GTK_MENU_ITEM(item),
			    w->ifmenu);
		}
	} else {
		w->ifmenu = menu = add_scans(w);
	}

	if (menu) {
		gtk_widget_show_all(GTK_WIDGET(menu));
		gtk_menu_popup(GTK_MENU(menu), NULL, NULL,
		    gtk_status_icon_position_menu, icon,
		    1, gtk_get_current_event_time());

#ifdef BG_SCAN
		bgscan_timer = g_timeout_add(DHCPCD_WPA_SCAN_SHORT,
		    menu_bgscan, dhcpcd_if_connection(w->interface));
#endif
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
