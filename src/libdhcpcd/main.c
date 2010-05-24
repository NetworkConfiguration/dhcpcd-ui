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
#include <poll.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <dbus/dbus.h>

#define IN_LIBDHCPCD
#include "libdhcpcd.h"
#include "config.h"

#define DHCPCD_TIMEOUT_MS 500

#ifndef _unused
#  ifdef __GNUC__
#    define _unused __attribute__((__unused__))
#  else
#    define _unused
#  endif
#endif

DHCPCD_CONNECTION *dhcpcd_connections;
DHCPCD_WATCH *dhcpcd_watching;

static dbus_bool_t
dhcpcd_add_watch(DBusWatch *watch, void *data)
{
	DHCPCD_WATCH *w;
	int flags;

	flags = dbus_watch_get_unix_fd(watch);
	for (w = dhcpcd_watching; w; w = w->next) {
		if (w->pollfd.fd == flags)
			break;
	}
	if (w == NULL) {
		w = malloc(sizeof(*w));
		if (w == NULL)
			return false;
		w->next = dhcpcd_watching;
		dhcpcd_watching = w;
	}

	w->connection = (DHCPCD_CONNECTION *)data;
	w->watch = watch;
	w->pollfd.fd = flags;
	flags = dbus_watch_get_flags(watch);
	w->pollfd.events = POLLHUP | POLLERR;
	if (flags & DBUS_WATCH_READABLE)
		w->pollfd.events |= POLLIN;
	if (flags & DBUS_WATCH_WRITABLE)
		w->pollfd.events |= POLLOUT;
	if (w->connection->add_watch)
		w->connection->add_watch(w->connection, &w->pollfd,
			w->connection->watch_data);
	return true;
}

static void
dhcpcd_delete_watch(DBusWatch *watch, void *data)
{
	DHCPCD_WATCH *w, *l;
	int fd;

	fd = dbus_watch_get_unix_fd(watch);
	l = data = NULL;
	for (w = dhcpcd_watching; w; w = w->next) {
		if (w->pollfd.fd == fd) {
			if (w->connection->delete_watch)
				w->connection->delete_watch(w->connection,
				    &w->pollfd, w->connection->watch_data);
			if (l == NULL)
				dhcpcd_watching = w->next;
			else
				l->next = w->next;
			free(w);
			w = l;
		}
	}
}

static DBusHandlerResult
dhcpcd_message(_unused DBusConnection *bus, DBusMessage *msg, void *data)
{
	if (dhcpcd_dispatch_message((DHCPCD_CONNECTION *)data, msg))
		return DBUS_HANDLER_RESULT_HANDLED;
	return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
}

void
dhcpcd_if_free(DHCPCD_IF *ifs)
{
	DHCPCD_IF *i;

	while (ifs != NULL) {
		i = ifs->next;
		free(ifs);
		ifs = i;
	}
}

DHCPCD_CONNECTION *
dhcpcd_open(char **error)
{
	DBusError err;
	DHCPCD_CONNECTION *con;
	DBusConnection *bus;
	int tries;

	dbus_error_init(&err);
	bus = dbus_bus_get(DBUS_BUS_SYSTEM, &err);
	if (dbus_error_is_set(&err)) {
		if (error)
			*error = strdup(err.message);
		dbus_error_free(&err);
		return NULL;
	}
	if (bus == NULL)
		return NULL;
	con = calloc(1, sizeof(*con));
	if (con == NULL)
		goto bad;
	con->bus = bus;
	if (!dbus_connection_set_watch_functions(bus,
		dhcpcd_add_watch, dhcpcd_delete_watch, NULL, con, NULL))
		goto bad;
	dbus_bus_add_match(bus,
	    "type='signal',interface='" DHCPCD_SERVICE "'", &err);
	dbus_connection_flush(bus);
	if (dbus_error_is_set(&err)) {
		if (error)
			*error = strdup(err.message);
		dbus_error_free(&err);
		goto bad;
	}
	if (!dbus_connection_add_filter(bus, dhcpcd_message, con, NULL))
		goto bad;

	/* Give dhcpcd-dbus a little time to automatically start */
	tries = 5;
	while (--tries > 0) {
		if (dhcpcd_command(con, "GetVersion", NULL, NULL)) {
			dhcpcd_error_clear(con);
			break;
		}
	}

	con->next = dhcpcd_connections;
	dhcpcd_connections = con;

	return con;

bad:
	free(con);
	dbus_connection_unref(bus);
	return NULL;
}

bool
dhcpcd_close(DHCPCD_CONNECTION *con)
{
	DHCPCD_CONNECTION *c, *l;

	l = NULL;
	for (c = dhcpcd_connections; c; c = c->next) {
		if (c == con) {
			if (l == NULL)
				dhcpcd_connections = con->next;
			else
				l->next = con->next;
			dbus_connection_unref(con->bus);
			dhcpcd_if_free(con->interfaces);
			dhcpcd_wi_history_clear(con);
			free(con->status);
			free(con->error);
			free(con);
			return true;
		}
		l = c;
	}
	dhcpcd_error_set(con, NULL, ESRCH);
	return false;
}

DHCPCD_CONNECTION *
dhcpcd_if_connection(DHCPCD_IF *interface)
{
	DHCPCD_CONNECTION *c;
	DHCPCD_IF *i;

	for (c = dhcpcd_connections; c; c = c->next) {
		for (i = c->interfaces; i; i = i->next)
			if (i == interface)
				return c;
	}
	return NULL;
}

void
dhcpcd_error_clear(DHCPCD_CONNECTION *con)
{
	free(con->error);
	con->error = NULL;
	con->err = 0;
}

void
dhcpcd_error_set(DHCPCD_CONNECTION *con, const char *error, int err)
{
	dhcpcd_error_clear(con);
	if (error != NULL) {
		con->error = strdup(error);
		if (err == 0)
			err = EPROTO;
	} else if (err != 0)
		con->error = strdup(strerror(err));
	con->err = err;
	con->errors++;
}

const char *
dhcpcd_error(DHCPCD_CONNECTION *con)
{
	return con->error;
}

bool
dhcpcd_iter_get(DHCPCD_CONNECTION *con, DBusMessageIter *iter,
    int type, void *arg)
{
	if (dbus_message_iter_get_arg_type(iter) == type) {
		dbus_message_iter_get_basic(iter, arg);
		dbus_message_iter_next(iter);
		return true;
	}
	dhcpcd_error_set(con, 0, EINVAL);
	return false;
}

static bool
dhcpcd_message_error(DHCPCD_CONNECTION *con, DHCPCD_MESSAGE *msg)
{
	DBusMessageIter args;
	char *s;
	
	if (dbus_message_get_type(msg) != DBUS_MESSAGE_TYPE_ERROR)
		return false;
	if (dbus_message_iter_init(msg, &args) &&
	    dbus_message_iter_get_arg_type(&args) == DBUS_TYPE_STRING)
	{
		dbus_message_iter_get_basic(&args, &s);
		dhcpcd_error_set(con, s, 0);
	}
	return true;
}

DHCPCD_MESSAGE *
dhcpcd_send_reply(DHCPCD_CONNECTION *con, DHCPCD_MESSAGE *msg)
{
	DBusMessage *reply;
	DBusPendingCall *pending;

	pending = NULL;
	if (!dbus_connection_send_with_reply(con->bus, msg, &pending,
		DHCPCD_TIMEOUT_MS) ||
	    pending == NULL)
		return NULL;
	dbus_connection_flush(con->bus);

	/* Block until we receive a reply */
	dbus_pending_call_block(pending);
	reply = dbus_pending_call_steal_reply(pending);
	dbus_pending_call_unref(pending);
	if (dhcpcd_message_error(con, reply)) {
		dbus_message_unref(reply);
		return NULL;
	}
	return reply;
}

DHCPCD_MESSAGE *
dhcpcd_message_reply(DHCPCD_CONNECTION *con, const char *cmd, const char *arg)
{
	DBusMessage *msg, *reply;
	DBusMessageIter args;
	
	msg = dbus_message_new_method_call(DHCPCD_SERVICE, DHCPCD_PATH,
	    DHCPCD_SERVICE, cmd);
	if (msg == NULL) {
		dhcpcd_error_set(con, 0, errno);
		return NULL;
	}
	dbus_message_iter_init_append(msg, &args);
	if (arg != NULL &&
	    !dbus_message_iter_append_basic(&args, DBUS_TYPE_STRING, &arg))
	{
		dbus_message_unref(msg);
		dhcpcd_error_set(con, 0, EINVAL);
		return NULL;
	}
	reply = dhcpcd_send_reply(con, msg);
	dbus_message_unref(msg);
	return reply;
}

bool
dhcpcd_command(DHCPCD_CONNECTION *con, const char *cmd, const char *arg,
	char **reply)
{
	DBusMessage *msg;
	DBusMessageIter args;
	char *s;

	msg = dhcpcd_message_reply(con, cmd, arg);
	if (msg == NULL)
		return false;
	if (dhcpcd_message_error(con, msg)) {
		dbus_message_unref(msg);
		return false;
	}
	if (reply != NULL &&
	    dbus_message_iter_init(msg, &args) &&
	    dbus_message_iter_get_arg_type(&args) == DBUS_TYPE_STRING)
	{
		dbus_message_iter_get_basic(&args, &s);
		*reply = strdup(s);
	}
	dbus_message_unref(msg);
	return true;
}

void
dhcpcd_dispatch(int fd)
{
	DHCPCD_WATCH *w;
	struct pollfd fds;
	int n, flags;

	fds.fd = fd;
	fds.events = (POLLIN | POLLHUP | POLLOUT | POLLERR);
	n = poll(&fds, 1, 0);
	flags = 0;
	if (n == 1) {
		if (fds.revents & POLLIN)
			flags |= DBUS_WATCH_READABLE;
		if (fds.revents & POLLOUT)
			flags |= DBUS_WATCH_WRITABLE;
		if (fds.revents & POLLHUP)
			flags |= DBUS_WATCH_HANGUP;
		if (fds.revents & POLLERR)
			flags |= DBUS_WATCH_ERROR;
	}
	for (w = dhcpcd_watching; w; w = w->next) {
		if (w->pollfd.fd == fd) {
			if (flags != 0)
				dbus_watch_handle(w->watch, flags);
			dbus_connection_ref(w->connection->bus);
			while (dbus_connection_dispatch(w->connection->bus) ==
			    DBUS_DISPATCH_DATA_REMAINS)
				;
			dbus_connection_unref(w->connection->bus);
		}
	}
}

DHCPCD_IF *
dhcpcd_if_new(DHCPCD_CONNECTION *con, DBusMessageIter *array, char **order)
{
 	DBusMessageIter dict, entry, var;
	DHCPCD_IF *i;
	char *s;
	uint32_t u32;
	int b, errors;

	if (dbus_message_iter_get_arg_type(array) != DBUS_TYPE_ARRAY) {
		errno = EINVAL;
		return NULL;
	}
	dbus_message_iter_recurse(array, &dict);
	i = calloc(1, sizeof(*i));
	if (i == NULL) {
		dhcpcd_error_set(con, 0, errno);
		return NULL;
	}
	errors = con->errors;
	for (;
	     dbus_message_iter_get_arg_type(&dict) == DBUS_TYPE_DICT_ENTRY;
	     dbus_message_iter_next(&dict))
	{
		dbus_message_iter_recurse(&dict, &entry);
		if (!dhcpcd_iter_get(con, &entry, DBUS_TYPE_STRING, &s))
			break;
		if (dbus_message_iter_get_arg_type(&entry) != DBUS_TYPE_VARIANT)
			break;
		dbus_message_iter_recurse(&entry, &var);
		if (strcmp(s, "Interface") == 0) {
			if (!dhcpcd_iter_get(con, &var, DBUS_TYPE_STRING, &s))
				break;
			strlcpy(i->ifname, s, sizeof(i->ifname));
		} else if (strcmp(s, "Flags") == 0) {
			if (!dhcpcd_iter_get(con, &var, DBUS_TYPE_UINT32, &u32))
				break;
			i->flags = u32;
		} else if (strcmp(s, "Reason") == 0) {
			if (!dhcpcd_iter_get(con, &var, DBUS_TYPE_STRING, &s))
				break;
			strlcpy(i->reason, s, sizeof(i->reason));
		} else if (strcmp(s, "Wireless") == 0) {
			/* b is an int as DBus booleans want more space than
			 * a C99 boolean */
			if (!dhcpcd_iter_get(con, &var, DBUS_TYPE_BOOLEAN, &b))
				break;
			i->wireless = b;
		} else if (strcmp(s, "SSID") == 0) {
			if (!dhcpcd_iter_get(con, &var, DBUS_TYPE_STRING, &s))
				break;
			strlcpy(i->ssid, s, sizeof(i->ssid));
		} else if (strcmp(s, "IPAddress") == 0) {
			if (!dhcpcd_iter_get(con, &var, DBUS_TYPE_UINT32, &u32))
				break;
			i->ip.s_addr = u32;
		} else if (strcmp(s, "SubnetCIDR") == 0)
			dbus_message_iter_get_basic(&var, &i->cidr);
		else if (order != NULL && strcmp(s, "InterfaceOrder") == 0)
			if (!dhcpcd_iter_get(con, &var, DBUS_TYPE_STRING, order))
				break;
	}

	if (dbus_message_iter_get_arg_type(&dict) != DBUS_TYPE_INVALID) {
		free(i);
		if (con->errors == errors)
			dhcpcd_error_set(con, NULL, EINVAL);
		return NULL;
	}
	return i;
}

DHCPCD_IF *
dhcpcd_interfaces(DHCPCD_CONNECTION *con)
{
	DBusMessage *msg;
	DBusMessageIter args, dict, entry;
	DHCPCD_IF *i, *l;
	int errors;
	
	if (con->interfaces != NULL)
		return con->interfaces;
	l = NULL;
	msg = dhcpcd_message_reply(con, "GetInterfaces", NULL);
	if (msg == NULL)
		return NULL;
	if (!dbus_message_iter_init(msg, &args) ||
	    dbus_message_iter_get_arg_type(&args) != DBUS_TYPE_ARRAY)
	{
		dbus_message_unref(msg);
		dhcpcd_error_set(con, 0, EINVAL);
		return NULL;
	}

	l = NULL;
	errors = con->errors;
	dbus_message_iter_recurse(&args, &dict);
	for (;
	     dbus_message_iter_get_arg_type(&dict) == DBUS_TYPE_DICT_ENTRY;
	     dbus_message_iter_next(&dict))    
	{
		dbus_message_iter_recurse(&dict, &entry);
		dbus_message_iter_next(&entry);
		i = dhcpcd_if_new(con, &entry, NULL);
		if (i == NULL)
			break;
		if (l == NULL)
			con->interfaces = i;
		else
			l->next = i;
		l = i;
	}
	dbus_message_unref(msg);
	if (dbus_message_iter_get_arg_type(&dict) != DBUS_TYPE_INVALID) {
		if (con->errors == errors)
			dhcpcd_error_set(con, 0, EINVAL);
		dhcpcd_if_free(con->interfaces);
		con->interfaces = NULL;
	}
	return con->interfaces;
}

DHCPCD_IF *
dhcpcd_if_find(DHCPCD_CONNECTION *con, const char *ifname)
{
	DHCPCD_IF *i;

	if (con->interfaces == NULL)
		dhcpcd_interfaces(con);
	for (i = con->interfaces; i; i = i ->next)
		if (strcmp(i->ifname, ifname) == 0)
			return i;
	return NULL;
}

const char *
dhcpcd_status(DHCPCD_CONNECTION *con)
{
	if (con->status == NULL)
		dhcpcd_command(con, "GetStatus", NULL, &con->status);
	return con->status;
}

void
dhcpcd_set_watch_functions(DHCPCD_CONNECTION *con,
    void (*add_watch)(DHCPCD_CONNECTION *, const struct pollfd *, void *),
    void (*delete_watch)(DHCPCD_CONNECTION *, const struct pollfd *, void *),
    void *data)
{
	DHCPCD_WATCH *w;
	
	con->add_watch = add_watch;
	con->delete_watch = delete_watch;
	con->watch_data = data;
	if (con->add_watch) {
		for (w = dhcpcd_watching; w; w = w->next)
			if (w->connection == con)
				con->add_watch(con, &w->pollfd, data);
	}
}

void
dhcpcd_set_signal_functions(DHCPCD_CONNECTION *con,
    void (*event)(DHCPCD_CONNECTION *, DHCPCD_IF *, void *),
    void (*status_changed)(DHCPCD_CONNECTION *, const char *, void *),
    void (*wi_scanresults)(DHCPCD_CONNECTION *, DHCPCD_IF *, void *),
    void *data)
{
	DHCPCD_IF *i;
	
	con->event = event;
	con->status_changed = status_changed;
	con->wi_scanresults = wi_scanresults;
	con->signal_data = data;
	if (con->status_changed) {
		if (con->status == NULL)
			dhcpcd_status(con);
		con->status_changed(con, con->status, data);
	}
	if (con->wi_scanresults) {
		for (i = dhcpcd_interfaces(con); i; i = i->next)
			if (i->wireless)
				con->wi_scanresults(con, i, data);
	}
}
