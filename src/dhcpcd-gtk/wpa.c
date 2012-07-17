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

#include "dhcpcd-gtk.h"

static void
wpa_dialog(const char *title, const char *txt)
{
	GtkWidget *dialog;

	dialog = gtk_message_dialog_new(NULL, GTK_DIALOG_MODAL,
	    GTK_MESSAGE_ERROR, GTK_BUTTONS_OK, "%s", txt);
	gtk_window_set_title(GTK_WINDOW(dialog), title);
	gtk_dialog_run(GTK_DIALOG(dialog));
	gtk_widget_destroy(dialog);
}

static bool
configure_network(DHCPCD_CONNECTION *con, DHCPCD_IF *i,
    int id, const char *mgmt, const char *var, const char *val, bool quote)
{
	char *str;
	static bool warned = false;

	if (!dhcpcd_wpa_set_network(con, i, id, "key_mgmt", mgmt))
		return false;
	if (quote)
		str = g_strconcat("\"", val, "\"", NULL);
	else
		str = NULL;
	if (!dhcpcd_wpa_set_network(con, i, id, var, quote ? str : val)) {
		g_warning("libdhcpcd: %s", dhcpcd_error(con));
		dhcpcd_error_clear(con);
		g_free(str);
		wpa_dialog(_("Error setting password"),
		    _("Failed to set password, probably too short."));
		return false;
	}
	g_free(str);
	if (!dhcpcd_wpa_command(con, i, "EnableNetwork", id))
		return false;
	if (!dhcpcd_wpa_command(con, i, "SaveConfig", -1)) {
		g_warning("libdhcpcd: %s", dhcpcd_error(con));
		dhcpcd_error_clear(con);
		if (!warned) {
			warned = true;
			wpa_dialog(_("Error saving configuration"),
			    _("Failed to save wpa_supplicant configuration.\n\nYou should add update_config=1 to /etc/wpa_supplicant.conf.\nThis warning will not appear again until program restarted."));
		}
		return false;
	}
/*
  if (!dbus_g_proxy_call(dbus, "Disconnect", &error,
  G_TYPE_STRING, ifname,
  G_TYPE_INVALID,
  G_TYPE_INVALID))
  {
  g_warning("Disconnect: %s", error->message);
  g_error_free(error);
  }
*/
	if (!dhcpcd_wpa_command(con, i, "Reassociate", -1))
		return false;
	return true;
}

static void
onEnter(_unused GtkWidget *widget, gpointer *data)
{
	gtk_dialog_response(GTK_DIALOG(data), GTK_RESPONSE_ACCEPT);
}

bool
wpa_configure(DHCPCD_CONNECTION *con, DHCPCD_IF *i, DHCPCD_WI_SCAN *s)
{
	GtkWidget *dialog, *label, *psk, *vbox, *hbox;
	const char *var, *mgt;
	int result, id;
	bool retval;

	dialog = gtk_dialog_new_with_buttons(s->ssid,
	    NULL,
	    GTK_DIALOG_MODAL,
	    GTK_STOCK_OK, GTK_RESPONSE_ACCEPT,
	    GTK_STOCK_CANCEL, GTK_RESPONSE_REJECT,
	    NULL);
	gtk_window_set_resizable(GTK_WINDOW(dialog), false);
	gtk_window_set_icon_name(GTK_WINDOW(dialog),
	    "network-wireless-encrypted");
	gtk_dialog_set_default_response(GTK_DIALOG(dialog),
	    GTK_RESPONSE_ACCEPT);
	vbox = gtk_dialog_get_content_area(GTK_DIALOG(dialog));

	hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 2);
	label = gtk_label_new(_("Pre Shared Key:"));
	gtk_box_pack_start(GTK_BOX(hbox), label, false, false, 0);
	psk = gtk_entry_new();
	gtk_entry_set_max_length(GTK_ENTRY(psk), 130);
	g_signal_connect(G_OBJECT(psk), "activate",
	    G_CALLBACK(onEnter), dialog);
	gtk_box_pack_start(GTK_BOX(hbox), psk, true, true, 0);
	gtk_container_add(GTK_CONTAINER(vbox), hbox);

	gtk_widget_show_all(dialog);
again:
	result = gtk_dialog_run(GTK_DIALOG(dialog));
	
	id = -1;
	retval = false;
	if (result == GTK_RESPONSE_ACCEPT) {
		id = dhcpcd_wpa_find_network_new(con, i, s->ssid);
		if (g_strcmp0(s->flags, "[WEP]") == 0) {
			mgt = "NONE";
			var = "wep_key0";
		} else {
			mgt = "WPA-PSK";
			var = "psk";
		}
		if (id != -1) {
			retval = configure_network(con, i, id, mgt, var,
			    gtk_entry_get_text(GTK_ENTRY(psk)), true);
		}
		if (!retval && dhcpcd_error(con)) {
			wpa_dialog(_("Error"), dhcpcd_error(con));
			goto again;
		}
	}
	gtk_widget_destroy(dialog);
	return retval;
}
