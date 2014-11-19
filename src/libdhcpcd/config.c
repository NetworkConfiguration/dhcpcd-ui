/*
 * libdhcpcd
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

#include <assert.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define IN_LIBDHCPCD

#include "dhcpcd.h"

static DHCPCD_OPTION *
dhcpcd_option_new(const char *opt, const char *val)
{
	DHCPCD_OPTION *o;

	o = malloc(sizeof(*o));
	if (o == NULL)
		return NULL;
	o->option = strdup(opt);
	if (o->option == NULL) {
		free(o);
		return NULL;
	}
	o->value = strdup(val);
	if (o->value == NULL) {
		free(o->option);
		free(o);
		return NULL;
	}
	o->next = NULL;
	return o;
}

static void
dhcpcd_option_free(DHCPCD_OPTION *o)
{

	free(o->option);
	free(o->value);
	free(o);
}

void
dhcpcd_config_free(DHCPCD_OPTION *c)
{
	DHCPCD_OPTION *n;

	while (c) {
		n = c->next;
		dhcpcd_option_free(c);
		c = n;
	}
}

static DHCPCD_OPTION *
dhcpcd_config_get1(DHCPCD_OPTION *config, const char *opt, DHCPCD_OPTION **lst)
{
	DHCPCD_OPTION *o;

	for (o = config; o; o = o->next) {
		if (strcmp(o->option, opt) == 0)
			return o;
		if (lst)
			*lst = o;
	}
	errno = ESRCH;
	return NULL;
}

const char *
dhcpcd_config_get(DHCPCD_OPTION *config, const char *opt)
{
	DHCPCD_OPTION *o;

	assert(opt);
	o = dhcpcd_config_get1(config, opt, NULL);
	if (o == NULL)
		return NULL;
	return o->value;
}

static DHCPCD_OPTION *
dhcpcd_config_get_static1(DHCPCD_OPTION *config, const char *opt,
    DHCPCD_OPTION **lst)
{
	DHCPCD_OPTION *o;
	size_t len;

	o = config;
	len = strlen(opt);
	while ((o = dhcpcd_config_get1(o, "static", lst)) != NULL) {
		if (strncmp(o->value, opt, len) == 0)
			return o;
		if (lst)
			*lst = o;
		o = o->next;
	}
	return NULL;
}

const char *
dhcpcd_config_get_static(DHCPCD_OPTION *config, const char *opt)
{
	DHCPCD_OPTION *o;

	assert(opt);
	o = dhcpcd_config_get_static1(config, opt, NULL);
	if (o == NULL)
		return NULL;
	return o->value + strlen(opt);
}

static bool
dhcpcd_config_set1(DHCPCD_OPTION **config, const char *opt, const char *val,
    bool s)
{
	DHCPCD_OPTION *o, *l;
	char *t;
	size_t len;

	l = NULL;
	if (s)
		o = dhcpcd_config_get_static1(*config, opt, &l);
	else
		o = dhcpcd_config_get1(*config, opt, &l);
	if (val == NULL) {
		if (o == NULL)
			return true;
		if (o == *config)
			*config = o->next;
		else if (l != NULL)
			l->next = o->next;
		free(o->option);
		free(o->value);
		free(o);
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
	if (o == NULL) {
		if (s)
			o = dhcpcd_option_new("static", t);
		else
			o = dhcpcd_option_new(opt, val);
		free(t);
		if (o == NULL)
			return false;
		if (l == NULL)
			*config = o;
		else
			l->next = o;
		return true;
	}
	free(o->value);
	o->value = t;
	return true;
}

bool
dhcpcd_config_set(DHCPCD_OPTION **config, const char *opt, const char *val)
{

	assert(config);
	assert(opt);
	return dhcpcd_config_set1(config, opt, val, false);
}

bool
dhcpcd_config_set_static(DHCPCD_OPTION **config,
    const char *opt, const char *val)
{

	assert(config);
	assert(opt);
	return dhcpcd_config_set1(config, opt, val, true);
}

#define ACT_READ  (1 << 0)
#define ACT_WRITE (1 << 1)
#define ACT_LIST  (1 << 2)

static DHCPCD_OPTION *
config(DHCPCD_CONNECTION *con, int action, const char *block, const char *name,
    const DHCPCD_OPTION *no, char ***list)
{
	FILE *fp;
	DHCPCD_OPTION *options, *o;
	const DHCPCD_OPTION *co;
	char *line, *option, *p;
	char **buf, **nbuf;
	int skip, free_opts;
	size_t len, buf_size, buf_len, i;

	fp = fopen(con->cffile, "r");
	if (fp == NULL)
		return NULL;
	options = o = NULL;
	skip = block && !(action & ACT_LIST) ? 1 : 0;
	buf = NULL;
	buf_len = buf_size = 0;
	free_opts = 1;
	while (getline(&con->buf, &con->buflen, fp) != -1) {
		line = con->buf;
		/* Trim leading trailing newline and whitespace */
		while (*line == ' ' || *line == '\n' || *line == '\t')
			line++;
		/* Trim trailing newline and whitespace */
		if (line && *line) {
			p = line + strlen(line) - 1;
			while (p != line &&
			    (*p == ' ' || *p == '\n' || *p == '\t') &&
			    *(p - 1) != '\\')
				*p-- = '\0';
		}
		option = strsep(&line, " \t");
		/* Trim trailing whitespace */
		if (line && *line) {
			p = line + strlen(line) - 1;
			while (p != line &&
			    (*p == ' ' || *p == '\n' || *p == '\t') &&
			    *(p - 1) != '\\')
				*p-- = '\0';
		}
		if (action & ACT_LIST) {
			if (strcmp(option, block) == 0)
				skip = 0;
			else
				skip = 1;
		} else {
			/* Start of a block, skip if not ours */
			if (strcmp(option, "interface") == 0 ||
			    strcmp(option, "ssid") == 0)
			{
				if (block && name && line &&
				    strcmp(option, block) == 0 &&
				    strcmp(line, name) == 0)
					skip = 0;
				else
					skip = 1;
				if (!(action & ACT_WRITE))
					continue;
			}
		}
		if ((action & ACT_WRITE && skip) ||
		    (action & ACT_LIST && !skip))
		{
			if (buf_len + 2 > buf_size) {
				buf_size += 32;
				nbuf = realloc(buf, sizeof(char *) * buf_size);
				if (nbuf == NULL)
					goto exit;
				buf = nbuf;
			}
			if (action & ACT_WRITE && line && *line != '\0') {
				len = strlen(option) + strlen(line) + 2;
				buf[buf_len] = malloc(len);
				if (buf[buf_len] == NULL)
					goto exit;
				snprintf(buf[buf_len], len,
				    "%s %s", option, line);
			} else {
				if (action & ACT_LIST)
					buf[buf_len] = strdup(line);
				else
					buf[buf_len] = strdup(option);
				if (buf[buf_len] == NULL)
					goto exit;
			}
			buf_len++;
		}
		if (skip || action & ACT_LIST)
			continue;
		if (*option == '\0' || *option == '#' || *option == ';')
			continue;
		if (o == NULL)
			options = o = malloc(sizeof(*options));
		else {
			o->next = malloc(sizeof(*o));
			o = o->next;
		}
		if (o == NULL)
			goto exit;
		o->next = NULL;
		o->option = strdup(option);
		if (o->option == NULL) {
			o->value = NULL;
			goto exit;
		}
		if (line == NULL || *line == '\0')
			o->value = NULL;
		else {
			o->value = strdup(line);
			if (o->value == NULL)
				goto exit;
		}
	}

	if (action & ACT_WRITE) {
		fp = freopen(con->cffile, "w", fp);
		if (fp == NULL)
			goto exit;
		if (block) {
			skip = 0;
			for (i = 0; i < buf_len; i++) {
				fputs(buf[i], fp);
				fputc('\n', fp);
				skip = buf[i][0] == '\0' ? 1 : 0;
			}
		} else
			skip = 1;
		if (no && block) {
			if (!skip)
				fputc('\n', fp);
			fprintf(fp, "%s %s\n", block, name);
		}
		skip = 0;
		for (co = no; co; co = co->next) {
			if (co->value)
				fprintf(fp, "%s %s\n", co->option, co->value);
			else
				fprintf(fp, "%s\n", co->option);
			skip = 1;
		}
		if (block == NULL) {
			if (!skip)
				fputc('\n', fp);
			for (i = 0; i < buf_len; i++) {
				fputs(buf[i], fp);
				fputc('\n', fp);
			}
		}
	} else
		free_opts = 0;

exit:
	if (fp != NULL)
		fclose(fp);
	if (action & ACT_LIST) {
		if (buf)
			buf[buf_len] = NULL;
		*list = buf;
	} else {
		for (i = 0; i < buf_len; i++)
			free(buf[i]);
		free(buf);
	}
	if (free_opts) {
		dhcpcd_config_free(options);
		options = NULL;
	}
	return options;
}

DHCPCD_OPTION *
dhcpcd_config_read(DHCPCD_CONNECTION *con, const char *block, const char *name)
{

	assert(con);
	return config(con, ACT_READ, block, name, NULL, NULL);
}

bool
dhcpcd_config_writeable(DHCPCD_CONNECTION *con)
{

	return (access(con->cffile, W_OK) == 0);
}

bool
dhcpcd_config_write(DHCPCD_CONNECTION *con,
    const char *block, const char *name,
    const DHCPCD_OPTION *opts)
{
	int serrno;

	assert(con);
	serrno = errno;
	errno = 0;
	config(con, ACT_WRITE, block, name, opts, NULL);
	if (errno)
		return false;
	errno = serrno;
	return true;
}

char **
dhcpcd_config_blocks(DHCPCD_CONNECTION *con, const char *block)
{
	char **blocks;

	assert(con);
	blocks = NULL;
	config(con, ACT_LIST, block, NULL, NULL, &blocks);
	return blocks;
}
