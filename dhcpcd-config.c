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

#include "config.h"
#include "dhcpcd-config.h"
#include "dhcpcd-gtk.h"

void
free_config(GPtrArray **config)
{
	unsigned int i;

	if (config == NULL || *config == NULL)
		return;
	for (i = 0; i < (*config)->len; i++)
		g_value_array_free(g_ptr_array_index(*config, i));
	g_ptr_array_free(*config, TRUE);
	*config = NULL;
}	

GPtrArray *
read_config(const char *block, const char *name)
{
	GType otype;
	GError *error;
	GPtrArray *config;

	error = NULL;
	otype = dbus_g_type_get_struct("GValueArray",
	    G_TYPE_STRING, G_TYPE_STRING, G_TYPE_INVALID);
	otype = dbus_g_type_get_collection("GPtrArray", otype);
	if (!dbus_g_proxy_call(dbus, "GetConfig", &error,
		G_TYPE_STRING, block, G_TYPE_STRING, name, G_TYPE_INVALID,
		otype, &config, G_TYPE_INVALID))
	{
		g_critical("GetConfig: %s", error->message);
		g_clear_error(&error);
		return NULL;
	}
	return config;
}

int
get_config(GPtrArray *config, int idx, const char *opt, const char **value)
{
	GValueArray *c;
	GValue *val;
	const char *str;

	if (config == NULL)
		return -1;
	for (; (uint)idx < config->len; idx++) {
		c = g_ptr_array_index(config, idx);
		val = g_value_array_get_nth(c, 0);
		str = g_value_get_string(val);
		if (strcmp(str, opt) != 0)
			continue;
		if (value != NULL) {
			val = g_value_array_get_nth(c, 1);
			str = g_value_get_string(val);
			if (*str == '\0')
				*value = NULL;
			else
				*value = str;
		}
		return idx;
	}
	if (value != NULL)
		*value = NULL;
	return -1;
}

int
get_static_config(GPtrArray *config, const char *var, const char **value)
{
	int idx;
	const char *val;

	idx = -1;
	while ((idx = get_config(config, idx + 1, "static", &val)) != -1) {
		if (g_str_has_prefix(val, var)) {
			if (value)
				*value = val + strlen(var);
			return idx;
		}
	}
	if (value)
		*value = NULL;
	return -1;
}

GPtrArray *
save_config(const char *block, const char *name, GPtrArray *config)
{
	GError *error;
	GType otype;
	
	error = NULL;
	otype = dbus_g_type_get_struct("GValueArray",
	    G_TYPE_STRING, G_TYPE_STRING, G_TYPE_INVALID);
	otype = dbus_g_type_get_collection("GPtrArray", otype);
	if (!dbus_g_proxy_call(dbus, "SetConfig", &error,
		G_TYPE_STRING, block,
		G_TYPE_STRING, name,
		otype, config,
		G_TYPE_INVALID,
		G_TYPE_INVALID))
	{
		g_critical("SetConfig: %s", error->message);
		g_clear_error(&error);
		return NULL;
	}
	return config;
}

GPtrArray *
load_config(const char *block, const char *name, GPtrArray *array)
{

	free_config(&array);
	return read_config(block, name);
}

void
set_option(GPtrArray *array, bool sopt, const char *var, const char *val)
{
	int i;
	GValueArray *va;
	GValue nv, *v;
	char *n;

	if (sopt)
		i = get_static_config(array, var, NULL);
	else
		i = get_config(array, 0, var, NULL);
	if (val == NULL) {
		if (i != -1) {
			va = g_ptr_array_remove_index(array, i);
			g_value_array_free(va);
		}
	} else {
		if (sopt)
			n = g_strconcat(var, val, NULL);
		else
			n = NULL;
		if (i == -1) {
			va = g_value_array_new(2);
			memset(&nv, 0, sizeof(v));
			g_value_init(&nv, G_TYPE_STRING);
			g_value_set_static_string(&nv, sopt ? "static" : var);
			va = g_value_array_append(va, &nv);
			g_value_set_static_string(&nv, sopt ? n : val);
			va = g_value_array_append(va, &nv);
			g_ptr_array_add(array, va);
		} else if (val != NULL) {
			va = g_ptr_array_index(array, i);
			v = g_value_array_get_nth(va, 1);
			g_value_unset(v);
			g_value_init(v, G_TYPE_STRING);
			g_value_set_static_string(v, sopt ? n : val);
		}
		g_free(n);
	}
}
