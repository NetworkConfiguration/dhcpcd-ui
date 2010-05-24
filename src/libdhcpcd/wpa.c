/*
 * libdhcpcd
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
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define IN_LIBDHCPCD
#include "libdhcpcd.h"
#include "config.h"

#define HIST_MAX 10 /* Max history per ssid/bssid */

void
dhcpcd_wi_history_clear(DHCPCD_CONNECTION *con)
{
	DHCPCD_WI_HIST *h;

	while (con->wi_history) {
		h = con->wi_history->next;
		free(con->wi_history);
		con->wi_history = h;
	}
}

void
dhcpcd_wi_scans_free(DHCPCD_WI_SCAN *wis)
{
	DHCPCD_WI_SCAN *n;

	while (wis) {
		n = wis->next;
		free(wis);
		wis = n;
	}
}

static DHCPCD_WI_SCAN *
dhcpcd_scanresult_new(DHCPCD_CONNECTION *con, DBusMessageIter *array)
{
	DBusMessageIter dict, entry, var;
	DHCPCD_WI_SCAN *wis;
	char *s;
	int32_t i32;
	int errors;

	wis = calloc(1, sizeof(*wis));
	if (wis == NULL) {
		dhcpcd_error_set(con, NULL, errno);
		return NULL;
	}
	errors = con->errors;
	dbus_message_iter_recurse(array, &dict);
	for (;
	     dbus_message_iter_get_arg_type(&dict) == DBUS_TYPE_DICT_ENTRY;
	     dbus_message_iter_next(&dict))
	{
		dbus_message_iter_recurse(&dict, &entry);
		if (!dhcpcd_iter_get(con, &entry, DBUS_TYPE_STRING, &s))
		    break;
		if (dbus_message_iter_get_arg_type(&entry) !=
		    DBUS_TYPE_VARIANT)
			break;
		dbus_message_iter_recurse(&entry, &var);
		if (strcmp(s, "BSSID") == 0) {
			if (!dhcpcd_iter_get(con, &var, DBUS_TYPE_STRING, &s))
				break;
			strlcpy(wis->bssid, s, sizeof(wis->bssid));
		} else if (strcmp(s, "Frequency") == 0) {
			if (!dhcpcd_iter_get(con, &var, DBUS_TYPE_INT32, &i32))
				break;
			wis->frequency = i32;
		} else if (strcmp(s, "Quality") == 0) {
			if (!dhcpcd_iter_get(con, &var, DBUS_TYPE_INT32, &i32))
				break;
			wis->quality.value = i32;
		} else if (strcmp(s, "Noise") == 0) {
			if (!dhcpcd_iter_get(con, &var, DBUS_TYPE_INT32, &i32))
				break;
			wis->noise.value = i32;
		} else if (strcmp(s, "Level") == 0) {
			if (!dhcpcd_iter_get(con, &var, DBUS_TYPE_INT32, &i32))
				break;
			wis->level.value = i32;
		} else if (strcmp(s, "Flags") == 0) {
			if (!dhcpcd_iter_get(con, &var, DBUS_TYPE_STRING, &s))
				break;
			strlcpy(wis->flags, s, sizeof(wis->flags));
		} else if (strcmp(s, "SSID") == 0) {
			if (!dhcpcd_iter_get(con, &var, DBUS_TYPE_STRING, &s))
				break;
			strlcpy(wis->ssid, s, sizeof(wis->ssid));
		}
	}
	if (dbus_message_iter_get_arg_type(&dict) != DBUS_TYPE_INVALID) {
		if (con->errors == errors)
			dhcpcd_error_set(con, NULL, EINVAL);
		free(wis);
		return NULL;
	}
	return wis;
}

DHCPCD_WI_SCAN *
dhcpcd_wi_scans(DHCPCD_CONNECTION *con, DHCPCD_IF *i)
{
	DBusMessage *msg;
	DBusMessageIter args, array;
	DHCPCD_WI_SCAN *wis, *scans, *l;
	DHCPCD_WI_HIST *h, *hl;
	int errors, nh;

	msg = dhcpcd_message_reply(con, "ScanResults", i->ifname);
	if (msg == NULL)
		return NULL;
	if (!dbus_message_iter_init(msg, &args) ||
	    dbus_message_iter_get_arg_type(&args) != DBUS_TYPE_ARRAY)
	{
		dhcpcd_error_set(con, NULL, EINVAL);
		return NULL;
	}

	scans = l = NULL;
	errors = con->errors;
	dbus_message_iter_recurse(&args, &array);
	for(;
	    dbus_message_iter_get_arg_type(&array) == DBUS_TYPE_ARRAY;
	    dbus_message_iter_next(&array))
	{
		wis = dhcpcd_scanresult_new(con, &array);
		if (wis == NULL)
			break;
		nh = 1;
		hl = NULL;
		wis->quality.average = wis->quality.value;
		wis->noise.average = wis->noise.value;
		wis->level.average = wis->level.value;
		for (h = con->wi_history; h; h = h->next) {
			if (strcmp(h->ifname, i->ifname) == 0 &&
			    strcmp(h->bssid, wis->bssid) == 0)
			{
				wis->quality.average += h->quality;
				wis->noise.average += h->noise;
				wis->level.average += h->level;
				if (++nh == HIST_MAX) {
					hl->next = h->next;
					free(h);
					break;
				}
			}
			hl = h;
		}
		if (nh != 1) {
			wis->quality.average /= nh;
       			wis->noise.average /= nh;
			wis->level.average /= nh;
		}
		h = malloc(sizeof(*h));
		if (h) {
			strlcpy(h->ifname, i->ifname, sizeof(h->ifname));
			strlcpy(h->bssid, wis->bssid, sizeof(h->bssid));
			h->quality = wis->quality.value;
			h->noise = wis->noise.value;
			h->level = wis->level.value;
			h->next = con->wi_history;
			con->wi_history = h;
		}
		if (l == NULL)
			scans = l = wis;
		else {
			l->next = wis;
			l = l->next;
		}
	}
	if (dbus_message_iter_get_arg_type(&array) != DBUS_TYPE_INVALID) {
		if (con->errors == errors)
			dhcpcd_error_set(con, NULL, EINVAL);
		dhcpcd_wi_scans_free(scans);
		scans = NULL;
	}
	dbus_message_unref(msg);
	return scans;
}

static int
dhcpcd_wpa_find_network(DHCPCD_CONNECTION *con, DHCPCD_IF *i, const char *ssid)
{
	DBusMessage *msg;
	DBusMessageIter args, array, entry;
	int32_t id;
	char *s;
	int errors;

	msg = dhcpcd_message_reply(con, "ListNetworks", i->ifname);
	if (msg == NULL)
		return -1;
	if (!dbus_message_iter_init(msg, &args)) {
		dhcpcd_error_set(con, NULL, EINVAL);
		return -1;
	}

	errors = con->errors;
   	for(;
	    dbus_message_iter_get_arg_type(&args) == DBUS_TYPE_ARRAY;
	    dbus_message_iter_next(&args))
	{
		dbus_message_iter_recurse(&args, &array);
		for(;
		    dbus_message_iter_get_arg_type(&array) == DBUS_TYPE_STRUCT;
		    dbus_message_iter_next(&array))
		{
			dbus_message_iter_recurse(&array, &entry);
			if (!dhcpcd_iter_get(con, &entry,
				DBUS_TYPE_INT32, &id) ||
			    !dhcpcd_iter_get(con, &entry,
				DBUS_TYPE_STRING, &s))
				break;
			if (strcmp(s, ssid) == 0) {
				dbus_message_unref(msg);
				return (int)id;
			}
		}
	}
	if (dbus_message_iter_get_arg_type(&args) != DBUS_TYPE_INVALID &&
	    con->errors == errors)
		dhcpcd_error_set(con, NULL, EINVAL);
	dbus_message_unref(msg);
	return -1;
}
	
static int
dhcpcd_wpa_add_network(DHCPCD_CONNECTION *con, DHCPCD_IF *i)
{
	DBusMessage *msg;
	DBusMessageIter args;
	int32_t id;
	int ret;

	msg = dhcpcd_message_reply(con, "AddNetwork", i->ifname);
	if (msg == NULL)
		return -1;
	ret = -1;
	if (dbus_message_iter_init(msg, &args)) {
		if (dhcpcd_iter_get(con, &args, DBUS_TYPE_INT32, &id))
			ret = id;
	} else
		dhcpcd_error_set(con, NULL, EINVAL);
	dbus_message_unref(msg);
	return ret;
}

bool
dhcpcd_wpa_set_network(DHCPCD_CONNECTION *con, DHCPCD_IF *i, int id,
    const char *opt, const char *val)
{
	DBusMessage *msg, *reply;
	DBusMessageIter args;
	bool retval;
	char *ifname;

	msg = dbus_message_new_method_call(DHCPCD_SERVICE, DHCPCD_PATH,
	    DHCPCD_SERVICE, "SetNetwork");
	if (msg == NULL) {
		dhcpcd_error_set(con, 0, errno);
		return false;
	}
	dbus_message_iter_init_append(msg, &args);
	ifname = i->ifname;
	dbus_message_iter_append_basic(&args, DBUS_TYPE_STRING, &ifname);
	dbus_message_iter_append_basic(&args, DBUS_TYPE_INT32, &id);
	dbus_message_iter_append_basic(&args, DBUS_TYPE_STRING, &opt);
	dbus_message_iter_append_basic(&args, DBUS_TYPE_STRING, &val);
	reply = dhcpcd_send_reply(con, msg);
	dbus_message_unref(msg);
	if (reply == NULL)
		retval = false;
	else {
		dbus_message_unref(reply);
		retval = true;
	}
	return retval;
}

int
dhcpcd_wpa_find_network_new(DHCPCD_CONNECTION *con, DHCPCD_IF *i,
    const char *ssid)
{
	int errors, id;
	char *q;
	size_t len;
	bool retval;

	len = strlen(ssid) + 3;
	q = malloc(len);
	if (q == NULL) {
		dhcpcd_error_set(con, 0, errno);
		return -1;
	}
	errors = con->errors;
	id = dhcpcd_wpa_find_network(con, i, ssid);
	if (id != -1 || con->errors != errors) {
		free(q);
		return id;
	}
	id = dhcpcd_wpa_add_network(con, i);
	if (id == -1) {
		free(q);
		return -1;
	}
	snprintf(q, len, "\"%s\"", ssid);
	retval = dhcpcd_wpa_set_network(con, i, id, "ssid", q);
	free(q);
	return retval;
}

bool
dhcpcd_wpa_command(DHCPCD_CONNECTION *con, DHCPCD_IF *i,
    const char *cmd, int id)
{
	DBusMessage *msg, *reply;
	DBusMessageIter args;
	char *ifname;
	bool retval;
	
	msg = dbus_message_new_method_call(DHCPCD_SERVICE, DHCPCD_PATH,
	    DHCPCD_SERVICE, cmd);
	if (msg == NULL) {
		dhcpcd_error_set(con, 0, errno);
		return false;
	}
	dbus_message_iter_init_append(msg, &args);
	ifname = i->ifname;
	dbus_message_iter_append_basic(&args, DBUS_TYPE_STRING, &ifname);
	if (id != -1)
		dbus_message_iter_append_basic(&args, DBUS_TYPE_INT32, &id);
	reply = dhcpcd_send_reply(con, msg);
	dbus_message_unref(msg);
	if (reply == NULL)
		retval = false;
	else {
		dbus_message_unref(reply);
		retval = true;
	}
	return retval;
}
