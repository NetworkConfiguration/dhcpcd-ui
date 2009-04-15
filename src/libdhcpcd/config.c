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
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <dbus/dbus.h>

#define IN_LIBDHCPCD
#include "libdhcpcd.h"

static DHCPCD_CONFIG *
dhcpcd_config_new(const char *opt, const char *val)
{
	DHCPCD_CONFIG *c;

	c = malloc(sizeof(*c));
	if (c == NULL)
		return NULL;
	c->option = strdup(opt);
	if (c->option == NULL) {
		free(c);
		return NULL;
	}
	c->value = strdup(val);
	if (c->value == NULL) {
		free(c->option);
		free(c);
		return NULL;
	}
	c->next = NULL;
	return c;
}

void
dhcpcd_config_free(DHCPCD_CONFIG *config)
{
	DHCPCD_CONFIG *c;
	
	while (config) {
		c = config->next;
		free(config->option);
		free(config->value);
		free(config);
		config = c;
	}
}	

char **
dhcpcd_config_blocks_get(DHCPCD_CONNECTION *con, const char *block)
{
	DBusMessage *msg, *reply;
	DBusMessageIter args;
	DBusError error;
	char **blocks;
	int n_blocks;

	msg = dbus_message_new_method_call(DHCPCD_SERVICE, DHCPCD_PATH,
	    DHCPCD_SERVICE, "GetConfigBlocks");
	if (msg == NULL) {
		dhcpcd_error_set(con, NULL, errno);
		return NULL;
	}
	dbus_message_iter_init_append(msg, &args);
	dbus_message_iter_append_basic(&args, DBUS_TYPE_STRING, &block);
	reply = dhcpcd_send_reply(con, msg);
	dbus_message_unref(msg);
	if (reply == NULL)
		return NULL;
	dbus_error_init(&error);
	blocks = NULL;
	n_blocks = 0;
	if (!dbus_message_get_args(reply, &error, DBUS_TYPE_ARRAY,
		DBUS_TYPE_STRING, &blocks, &n_blocks,
		DBUS_TYPE_INVALID))
	{
		dhcpcd_error_set(con, error.message, 0);
		dbus_error_free(&error);
	}
	dbus_message_unref(reply);
	return blocks;	
}

DHCPCD_CONFIG *
dhcpcd_config_load(DHCPCD_CONNECTION *con, const char *block, const char *name)
{
	DHCPCD_CONFIG *config, *c, *l;
	DBusMessage *msg, *reply;
	DBusMessageIter args, array, item;
	const char ns[] = "", *option, *value;
	int errors;

	msg = dbus_message_new_method_call(DHCPCD_SERVICE, DHCPCD_PATH,
	    DHCPCD_SERVICE, "GetConfig");
	if (msg == NULL) {
		dhcpcd_error_set(con, NULL, errno);
		return NULL;
	}
	dbus_message_iter_init_append(msg, &args);
	if (block == NULL)
		block = ns;
	if (name == NULL)
		name = ns;
	dbus_message_iter_append_basic(&args, DBUS_TYPE_STRING, &block);
	dbus_message_iter_append_basic(&args, DBUS_TYPE_STRING, &name);
	reply = dhcpcd_send_reply(con, msg);
	dbus_message_unref(msg);
	if (reply == NULL)
		return NULL;
	if (!dbus_message_iter_init(reply, &args) ||
	    dbus_message_iter_get_arg_type(&args) != DBUS_TYPE_ARRAY)
	{
		dbus_message_unref(reply);
		dhcpcd_error_set(con, NULL, EINVAL);
		return NULL;
	}
	config = l = NULL;
	errors = con->errors;
	dbus_message_iter_recurse(&args, &array);
	for (;
	     dbus_message_iter_get_arg_type(&array) == DBUS_TYPE_STRUCT;
	     dbus_message_iter_next(&array))
	{
		dbus_message_iter_recurse(&array, &item);
		if (!dhcpcd_iter_get(con, &item, DBUS_TYPE_STRING, &option) ||
		    !dhcpcd_iter_get(con, &item, DBUS_TYPE_STRING, &value))
			break;
		c = dhcpcd_config_new(option, value);
		if (c == NULL) {
			dhcpcd_error_set(con, NULL, errno);
			break;
		}
		if (l == NULL)
			config = c;
		else
			l->next = c;
		l = c;
	}
	if (dbus_message_iter_get_arg_type(&array) != DBUS_TYPE_INVALID) {
		if (con->errors == errors)
			dhcpcd_error_set(con, NULL, EINVAL);
		dhcpcd_config_free(config);
		config = NULL;
	}
	dbus_message_unref(reply);
	return config;
}

bool
dhcpcd_config_save(DHCPCD_CONNECTION *con, const char *block, const char *name,
    DHCPCD_CONFIG *config)
{
	DBusMessage *msg, *reply;
	DBusMessageIter args, array, item;
	DHCPCD_CONFIG *c;
	const char ns[] = "", *p;
	bool retval;

	msg = dbus_message_new_method_call(DHCPCD_SERVICE, DHCPCD_PATH,
	    DHCPCD_SERVICE, "SetConfig");
	if (msg == NULL) {
		dhcpcd_error_set(con, 0, errno);
		return false;
	}
	dbus_message_iter_init_append(msg, &args);
	dbus_message_iter_append_basic(&args, DBUS_TYPE_STRING, &block);
	dbus_message_iter_append_basic(&args, DBUS_TYPE_STRING, &name);
	dbus_message_iter_open_container(&args, DBUS_TYPE_ARRAY,
	    DBUS_STRUCT_BEGIN_CHAR_AS_STRING
	    DBUS_TYPE_STRING_AS_STRING
	    DBUS_TYPE_STRING_AS_STRING
	    DBUS_STRUCT_END_CHAR_AS_STRING,
	    &array);
	for (c = config; c; c = c->next) {
		dbus_message_iter_open_container(&array,
		    DBUS_TYPE_STRUCT, NULL, &item);
		dbus_message_iter_append_basic(&item,
		    DBUS_TYPE_STRING, &c->option);
		if (c->value == NULL)
			p = ns;
		else
			p = c->value;
		dbus_message_iter_append_basic(&item, DBUS_TYPE_STRING, &p);
		dbus_message_iter_close_container(&array, &item);
	}
	dbus_message_iter_close_container(&args, &array);

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

static DHCPCD_CONFIG *
dhcpcd_config_get1(DHCPCD_CONFIG *config, const char *opt, DHCPCD_CONFIG **lst)
{
	DHCPCD_CONFIG *c;

	for (c = config; c; c = c->next) {
		if (strcmp(c->option, opt) == 0)
			return c;
		if (lst)
			*lst = c;
	}
	errno = ESRCH;
	return NULL;
}

const char *
dhcpcd_config_get(DHCPCD_CONFIG *config, const char *opt)
{
	DHCPCD_CONFIG *c;

	c = dhcpcd_config_get1(config, opt, NULL);
	if (c == NULL)
		return NULL;
	return c->value;
}

static DHCPCD_CONFIG *
dhcpcd_config_get_static1(DHCPCD_CONFIG *config, const char *opt,
    DHCPCD_CONFIG **lst)
{
	DHCPCD_CONFIG *c;
	size_t len;

	c = config;
	len = strlen(opt);
	while ((c = dhcpcd_config_get1(c, "static", lst)) != NULL) {
		if (strncmp(c->value, opt, len) == 0)
			return c;
		if (lst)
			*lst = c;
		c = c->next;
	}
	return NULL;
}

const char *
dhcpcd_config_get_static(DHCPCD_CONFIG *config, const char *opt)
{
	DHCPCD_CONFIG *c;

	c = dhcpcd_config_get_static1(config, opt, NULL);
	if (c == NULL)
		return NULL;
	return c->value + strlen(opt);
}

static bool
dhcpcd_config_set1(DHCPCD_CONFIG **config, const char *opt, const char *val,
    bool s)
{
	DHCPCD_CONFIG *c, *l;
	char *t;
	size_t len;

	l = NULL;
	if (s)
		c = dhcpcd_config_get_static1(*config, opt, &l);
	else
		c = dhcpcd_config_get1(*config, opt, &l);
	if (val == NULL) {
		if (c == NULL)
			return true;
		if (c == *config)
			*config = c->next;
		else if (l != NULL)
			l->next = c->next;
		free(c->option);
		free(c->value);
		free(c);
		return true;
	}
	if (s) {
		len = strlen(opt) + strlen(val) + 2;
		t = malloc(len);
		if (t == NULL)
			return false;
		snprintf(t, len, "%s%s", opt, val);
	} else {
		t = strdup(val);
		if (t == NULL)
			return false;
	}
	if (c == NULL) {
		if (s)
			c = dhcpcd_config_new("static", t);
		else
			c = dhcpcd_config_new(opt, val);
		if (c == NULL)
			return false;
		if (l == NULL)
			*config = c;
		else
			l->next = c;
		return true;
	}
	free(c->value);
	c->value = t;
	return true;
}

bool
dhcpcd_config_set(DHCPCD_CONFIG **config, const char *opt, const char *val)
{
	return dhcpcd_config_set1(config, opt, val, false);
}

bool
dhcpcd_config_set_static(DHCPCD_CONFIG **config,
    const char *opt, const char *val)
{
	return dhcpcd_config_set1(config, opt, val, true);
}
