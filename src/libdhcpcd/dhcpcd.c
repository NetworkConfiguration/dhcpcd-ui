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

// For strverscmp(3)
#define _GNU_SOURCE

#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/un.h>

#include <arpa/inet.h>

#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <libintl.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define IN_LIBDHCPCD

#include "config.h"
#include "dhcpcd.h"

#ifdef HAS_GETTEXT
#include <libintl.h>
#define _ gettext
#else
#define _(a) (a)
#endif

#ifndef SUN_LEN
#define SUN_LEN(su) \
	(sizeof(*(su)) - sizeof((su)->sun_path) + strlen((su)->sun_path))
#endif

static const char * const dhcpcd_types[] =
    { "link", "ipv4", "ra", "dhcp6", NULL };

static ssize_t
dhcpcd_command_fd(DHCPCD_CONNECTION *con,
    int fd, const char *cmd, char **buffer)
{
	size_t len;
	ssize_t bytes;
	char buf[1024], *p;
	char *nbuf;

	/* Each command is \n terminated.
	 * Each argument is NULL seperated.
	 * We may need to send a space one day, so the API
	 * in this function may need to be improved */
	len = strlen(cmd) + 1;
	if (con->terminate_commands)
		len++;
	if (len > sizeof(buf)) {
		errno = ENOBUFS;
		return -1;
	}
	strlcpy(buf, cmd, sizeof(buf));
	p = buf;
	while ((p = strchr(p, ' ')) != NULL)
		*p++ = '\0';
	if (con->terminate_commands) {
		buf[len - 2] = '\n';
		buf[len - 1] = '\0';
	} else
		buf[len - 1] = '\0';
	if (write(fd, buf, len) == -1)
		return -1;
	if (buffer == NULL)
		return 0;

	bytes = read(fd, buf, sizeof(size_t));
	if (bytes == 0 || bytes == -1)
		return bytes;
	memcpy(&len, buf, sizeof(size_t));
	nbuf = realloc(*buffer, len + 1);
	if (nbuf == NULL)
		return -1;
	*buffer = nbuf;
	bytes = read(fd, *buffer, len);
	if (bytes != -1 && (size_t)bytes < len)
		*buffer[bytes] = '\0';
	return bytes;
}

ssize_t
dhcpcd_command(DHCPCD_CONNECTION *con, const char *cmd, char **buffer)
{

	return dhcpcd_command_fd(con, con->command_fd, cmd, buffer);
}

bool
dhcpcd_realloc(DHCPCD_CONNECTION *con, size_t len)
{

	if (con->buflen < len) {
		char *nbuf;

		nbuf = realloc(con->buf, len);
		if (nbuf == NULL)
			return false;
		con->buf = nbuf;
		con->buflen = len;
	}
	return true;
}

ssize_t
dhcpcd_command_arg(DHCPCD_CONNECTION *con, const char *cmd, const char *arg,
    char **buffer)
{
	size_t cmdlen, len;

	cmdlen = strlen(cmd);
	len = cmdlen + strlen(arg) + 2;
	if (!dhcpcd_realloc(con, len))
		return -1;
	strlcpy(con->buf, cmd, con->buflen);
	con->buf[cmdlen] = ' ';
	strlcpy(con->buf + cmdlen + 1, arg, con->buflen - 1 - cmdlen);

	return dhcpcd_command_fd(con, con->command_fd, con->buf, buffer);
}


static int
dhcpcd_connect(void)
{
	int fd;
	socklen_t len;
	struct sockaddr_un sun;

	fd = socket(AF_UNIX, SOCK_STREAM | SOCK_CLOEXEC, 0);
	if (fd == -1)
		return -1;

	memset(&sun, 0, sizeof(sun));
	sun.sun_family = AF_UNIX;
	strlcpy(sun.sun_path, DHCPCD_SOCKET, sizeof(sun.sun_path));
	len = (socklen_t)SUN_LEN(&sun);
	if (connect(fd, (struct sockaddr *)&sun, len) == 0)
		return fd;
	close(fd);
	return -1;
}

static const char *
get_value(const char *data, size_t len, const char *var)
{
	const char *end, *p;
	size_t vlen;

	assert(var);
	end = data + len;
	vlen = strlen(var);
	p = NULL;
	while (data + vlen + 1 < end) {
		if (strncmp(data, var, vlen) == 0) {
			p = data + vlen;
			break;
		}
		data += strlen(data) + 1;
	}
	if (p != NULL && *p != '\0')
		return p;
	return NULL;
}

const char *
dhcpcd_get_value(const DHCPCD_IF *i, const char *var)
{

	assert(i);
	return get_value(i->data, i->data_len, var);
}

const char *
dhcpcd_get_prefix_value(const DHCPCD_IF *i, const char *prefix, const char *var)
{
	char pvar[128], *p;
	size_t plen, l;

	p = pvar;
	plen = sizeof(pvar);
	l = strlcpy(p, prefix, plen);
	if (l >= sizeof(pvar)) {
		errno = ENOBUFS;
		return NULL;
	}
	p += l;
	plen -= l;
	if (strlcpy(p, var, plen) >= plen) {
		errno = ENOBUFS;
		return NULL;
	}
	return dhcpcd_get_value(i, pvar);
}

static bool strtobool(const char *var)
{

	if (var == NULL)
		return false;

	 return (*var == '0' || *var == '\0' ||
	    strcmp(var, "false") == 0 ||
	    strcmp(var, "no") == 0) ? false : true;
}

static const char *
get_status(DHCPCD_CONNECTION *con)
{
	DHCPCD_IF *i;
	const char *status;

	assert(con);
	if (con->command_fd == -1 || con->listen_fd == -1)
		return "down";

	status = "disconnected";
	for (i = con->interfaces; i; i = i->next) {
		if (i->up) {
			if (strcmp(i->type, "link")) {
				status = "connected";
				break;
			} else
				status = "connecting";
		}
	}
	return status;
}

static void
update_status(DHCPCD_CONNECTION *con)
{
	const char *nstatus;

	assert(con);
	nstatus = get_status(con);
	if (con->status == NULL || strcmp(nstatus, con->status)) {
		con->status = nstatus;
		if (con->status_cb)
			con->status_cb(con, con->status, con->status_context);
	}
}

DHCPCD_IF *
dhcpcd_interfaces(DHCPCD_CONNECTION *con)
{

	assert(con);
	return con->interfaces;
}

DHCPCD_IF *
dhcpcd_get_if(DHCPCD_CONNECTION *con, const char *ifname, const char *type)
{
	DHCPCD_IF *i;

	assert(con);
	for (i = con->interfaces; i; i = i->next)
		if (strcmp(i->ifname, ifname) == 0 &&
		    strcmp(i->type, type) == 0)
			return i;
	return NULL;
}

static DHCPCD_IF *
dhcpcd_new_if(DHCPCD_CONNECTION *con, char *data, size_t len)
{
	const char *ifname, *ifclass, *reason, *type, *order, *flags;
	char *orderdup, *o, *p;
	DHCPCD_IF *e, *i, *l, *n, *nl;
	int ti;

	ifname = get_value(data, len, "interface=");
	if (ifname == NULL || *ifname == '\0') {
		errno = ESRCH;
		return NULL;
	}
	reason = get_value(data, len, "reason=");
	if (reason == NULL || *reason == '\0') {
		errno = ESRCH;
		return NULL;
	}
	ifclass = get_value(data, len, "ifclass=");
	/* Skip pseudo interfaces */
	if (ifclass && *ifclass != '\0') {
		errno = ENOTSUP;
		return NULL;
	}
	if (strcmp(reason, "RECONFIGURE") == 0) {
		errno = ENOTSUP;
		return NULL;
	}
	order = get_value(data, len, "interface_order=");
	if (order == NULL || *order == '\0') {
		errno = ESRCH;
		return NULL;
	}

	if (strcmp(reason, "PREINIT") == 0 ||
	    strcmp(reason, "UNKNOWN") == 0 ||
	    strcmp(reason, "CARRIER") == 0 ||
	    strcmp(reason, "NOCARRIER") == 0 ||
	    strcmp(reason, "DEPARTED") == 0 ||
	    strcmp(reason, "STOPPED") == 0)
		type = "link";
	else if (strcmp(reason, "ROUTERADVERT") == 0)
		type = "ra";
	else if (reason[strlen(reason) - 1] == '6')
		type = "dhcp6";
	else
		type = "ipv4";

	i = NULL;
       /* Remove all instances on carrier drop */
        if (strcmp(reason, "NOCARRIER") == 0 ||
            strcmp(reason, "DEPARTED") == 0 ||
            strcmp(reason, "STOPPED") == 0)
        {
                l = NULL;
                for (e = con->interfaces; e; e = n) {
                        n = e->next;
                        if (strcmp(e->ifname, ifname) == 0) {
                                if (strcmp(e->type, type) == 0)
                                        l = i = e;
                                else {
                                        if (l)
                                                l->next = e->next;
                                        else
                                                con->interfaces = e->next;
                                        free(e);
                                }
                        } else
                                l = e;
                }
        } else if (strcmp(type, "link")) {
		/* If link is down, ignore it */
		e = dhcpcd_get_if(con, ifname, "link");
		if (e && strcmp(e->reason, "NOCARRIER") == 0)
			return NULL;
	}

	orderdup = strdup(order);
	if (orderdup == NULL)
		return NULL;

	/* Find our pointer */
        if (i == NULL) {
                l = NULL;
                for (e = con->interfaces; e; e = e->next) {
                        if (strcmp(e->ifname, ifname) == 0 &&
                            strcmp(e->type, type) == 0)
                        {
                                i = e;
                                break;
                        }
                        l = e;
                }
        }
	if (i == NULL) {
		i = malloc(sizeof(*i));
		if (i == NULL) {
			free(orderdup);
			return NULL;
		}
		if (l)
			l->next = i;
		else
			con->interfaces = i;
		i->next = NULL;
		i->last_message = NULL;
	} else
		free(i->data);

	/* Now fill out our interface structure */
	i->con = con;
	i->data = data;
	i->data_len = len;
	i->ifname = ifname;
	i->type = type;
	i->reason = reason;
	flags = dhcpcd_get_value(i, "ifflags=");
	if (flags)
		i->flags = (unsigned int)strtoul(flags, NULL, 0);
	else
		i->flags = 0;
	if (strcmp(reason, "CARRIER") == 0)
		i->up = true;
	else
		i->up = strtobool(dhcpcd_get_value(i, "if_up="));
	i->wireless = strtobool(dhcpcd_get_value(i, "ifwireless="));
	i->ssid = dhcpcd_get_value(i, i->up ? "new_ssid=" : "old_ssid=");

       /* Sort! */
	n = nl = NULL;
	p = orderdup;
        while ((o = strsep(&p, " ")) != NULL) {
                for (ti = 0; dhcpcd_types[ti]; ti++) {
                        l = NULL;
                        for (e = con->interfaces; e; e = e->next) {
                                if (strcmp(e->ifname, o) == 0 &&
                                    strcmp(e->type, dhcpcd_types[ti]) == 0)
                                        break;
                                l = e;
                        }
                        if (e == NULL)
                                continue;
                        if (l)
                                l->next = e->next;
                        else
                                con->interfaces = e->next;
                        e->next = NULL;
                        if (nl == NULL)
                                n = nl = e;
                        else {
                                nl->next = e;
                                nl = e;
                        }
                }
        }
	free(orderdup);
        /* Free any stragglers */
        while (con->interfaces) {
                e = con->interfaces->next;
		free(con->interfaces->data);
		free(con->interfaces->last_message);
                free(con->interfaces);
                con->interfaces = e;
        }
        con->interfaces = n;

	return i;
}

static DHCPCD_IF *
dhcpcd_read_if(DHCPCD_CONNECTION *con, int fd)
{
	char sbuf[sizeof(size_t)], *rbuf;
	size_t len;
	ssize_t bytes;
	DHCPCD_IF *i;

	bytes = read(fd, sbuf, sizeof(sbuf));
	if (bytes == 0 || bytes == -1) {
		dhcpcd_close(con);
		return NULL;
	}
	memcpy(&len, sbuf, sizeof(len));
	rbuf = malloc(len + 1);
	if (rbuf == NULL)
		return NULL;
	bytes = read(fd, rbuf, len);
	if (bytes == 0 || bytes == -1) {
		free(rbuf);
		dhcpcd_close(con);
		return NULL;
	}
	if ((size_t)bytes != len) {
		free(rbuf);
		errno = EINVAL;
		return NULL;
	}
	rbuf[bytes] = '\0';

	i = dhcpcd_new_if(con, rbuf, len);
	if (i == NULL)
		free(rbuf);
	return i;
}

static void
dhcpcd_dispatchif(DHCPCD_IF *i)
{

	assert(i);
	if (i->con->if_cb)
		i->con->if_cb(i, i->con->if_context);
	dhcpcd_wpa_if_event(i);
}

void
dhcpcd_dispatch(DHCPCD_CONNECTION *con)
{
	DHCPCD_IF *i;

	assert(con);
	i = dhcpcd_read_if(con, con->listen_fd);
	if (i)
		dhcpcd_dispatchif(i);

	/* Have to call update_status last as it could
	 * cause the interface to be destroyed. */
	update_status(con);
}

DHCPCD_CONNECTION *
dhcpcd_new(void)
{
	DHCPCD_CONNECTION *con;

	con = calloc(1, sizeof(*con));
	con->command_fd = con->listen_fd = -1;
	return con;
}

#ifndef __GLIBC__
/* Good enough for our needs */
static int
strverscmp(const char *s1, const char *s2)
{
	int s1maj, s1min, s1part;
	int s2maj, s2min, s2part;
	int r;

	s1min = s1part = 0;
	if (sscanf(s1, "%d.%d.%d", &s1maj, &s1min, &s1part) < 1)
		return -1;
	s2min = s2part = 0;
	if (sscanf(s2, "%d.%d.%d", &s2maj, &s2min, &s2part) < 1)
		return -1;
	r = s1maj - s2maj;
	if (r != 0)
		return r;
	r = s1min - s2min;
	if (r != 0)
		return r;
	return s1part - s2part;
}
#endif

int
dhcpcd_open(DHCPCD_CONNECTION *con)
{
	char cmd[128];
	ssize_t bytes;
	size_t nifs, n;
	DHCPCD_IF *i;

	assert(con);
	if (con->listen_fd != -1)
		return con->listen_fd;
	con->command_fd = dhcpcd_connect();
	if (con->command_fd == -1) {
		update_status(con);
		return -1;
	}

	con->terminate_commands = false;
	if (dhcpcd_command(con, "--version", &con->version) <= 0)
		return -1;
	con->terminate_commands =
	    strverscmp(con->version, "6.4.1") >= 0 ? true : false;

	if (dhcpcd_command(con, "--getconfigfile", &con->cffile) <= 0)
		return -1;

	con->listen_fd = dhcpcd_connect();
	if (con->listen_fd == -1) {
		close(con->command_fd);
		con->command_fd = -1;
		update_status(con);
		return -1;
	}
	dhcpcd_command_fd(con, con->listen_fd, "--listen", NULL);

	dhcpcd_command_fd(con, con->command_fd, "--getinterfaces", NULL);
	bytes = read(con->command_fd, cmd, sizeof(nifs));
	if (bytes != sizeof(nifs)) {
		close(con->command_fd);
		con->command_fd = -1;
		close(con->listen_fd);
		con->listen_fd = -1;
	} else {
		memcpy(&nifs, cmd, sizeof(nifs));
		/* We don't dispatch each interface here as that
		 * causes too much notification spam when the GUI starts */
		for (n = 0; n < nifs; n++) {
			i = dhcpcd_read_if(con, con->command_fd);
			if (i)
				dhcpcd_wpa_if_event(i);
		}
	}
	update_status(con);

	return con->listen_fd;
}

int
dhcpcd_get_fd(DHCPCD_CONNECTION *con)
{

	assert(con);
	return con->listen_fd;
}

const char *
dhcpcd_status(DHCPCD_CONNECTION *con)
{

	assert(con);
	return con->status;
}

const char *
dhcpcd_version(DHCPCD_CONNECTION *con)
{

	assert(con);
	return con->version;
}

const char *
dhcpcd_cffile(DHCPCD_CONNECTION *con)
{

	assert(con);
	return con->cffile;
}

void
dhcpcd_set_if_callback(DHCPCD_CONNECTION *con,
    void (*cb)(DHCPCD_IF *, void *), void *ctx)
{

	assert(con);
	con->if_cb = cb;
	con->if_context = ctx;
}

void
dhcpcd_set_status_callback(DHCPCD_CONNECTION *con,
    void (*cb)(DHCPCD_CONNECTION *, const char *, void *), void *ctx)
{

	assert(con);
	con->status_cb = cb;
	con->status_context = ctx;
}

void
dhcpcd_close(DHCPCD_CONNECTION *con)
{
	DHCPCD_WPA *wpa;

	assert(con);

	/* Shut down WPA listeners as they aren't much good without dhcpcd.
	 * They'll be restarted anyway when dhcpcd comes back up. */
	for (wpa = con->wpa; wpa; wpa = wpa->next)
		dhcpcd_wpa_close(con->wpa);

	if (con->command_fd != -1) {
		shutdown(con->command_fd, SHUT_RDWR);
		con->command_fd = -1;
	}
	if (con->listen_fd != -1) {
		shutdown(con->listen_fd, SHUT_RDWR);
		con->listen_fd = -1;
	}

	if (con->cffile) {
		free(con->cffile);
		con->cffile = NULL;
	}
	if (con->version) {
		free(con->version);
		con->version = NULL;
	}
	if (con->buf) {
		free(con->buf);
		con->buf = NULL;
		con->buflen = 0;
	}
}

DHCPCD_CONNECTION *
dhcpcd_if_connection(DHCPCD_IF *i)
{

	assert(i);
	return i->con;
}

char *
dhcpcd_if_message(DHCPCD_IF *i, bool *new_msg)
{
	const char *ip, *iplen, *pfx;
	char *msg, *p;
	const char *reason = NULL;
	size_t len;
	bool showssid;

	assert(i);
	/* Don't report non SLAAC configurations */
	if (strcmp(i->type, "ra") == 0 && i->up &&
	    dhcpcd_get_value(i, "ra1_prefix=") == NULL)
		return NULL;

	showssid = false;
	if (strcmp(i->reason, "EXPIRE") == 0)
		reason = _("Expired");
	else if (strcmp(i->reason, "CARRIER") == 0) {
		if (i->wireless) {
			showssid = true;
			reason = _("Associated with");
		} else
			reason = _("Cable plugged in");
	} else if (strcmp(i->reason, "NOCARRIER") == 0) {
		if (i->wireless) {
			if (i->ssid) {
				reason = _("Disassociated from");
				showssid = true;
			} else
				reason = _("Not associated");
		} else
			reason = _("Cable unplugged");
	} else if (strcmp(i->reason, "UNKNOWN") == 0)
		reason = _("Unknown link state");
	else if (strcmp(i->reason, "FAIL") == 0)
		reason = _("Automatic configuration not possible");
	else if (strcmp(i->reason, "3RDPARTY") == 0)
		reason = _("Waiting for 3rd Party configuration");

	if (reason == NULL) {
		if (i->up)
			reason = _("Configured");
		else if (strcmp(i->type, "ra") == 0)
			reason = "Expired RA";
		else
			reason = i->reason;
	}

	pfx = i->up ? "new_" : "old_";
	if ((ip = dhcpcd_get_prefix_value(i, pfx, "ip_address=")))
		iplen = dhcpcd_get_prefix_value(i, pfx, "subnet_cidr=");
	else if ((ip = dhcpcd_get_value(i, "ra1_prefix=")))
		iplen = NULL;
	else if ((ip = dhcpcd_get_prefix_value(i, pfx,
	    "dhcp6_ia_na1_ia_addr1=")))
		iplen = "128";
	else {
		ip = NULL;
		iplen = NULL;
	}

	len = strlen(i->ifname) + strlen(reason) + 3;
	if (showssid && i->ssid)
		len += strlen(i->ssid) + 1;
	if (ip)
		len += strlen(ip) + 1;
	if (iplen)
		len += strlen(iplen) + 1;
	msg = p = malloc(len);
	if (msg == NULL)
		return NULL;
	p += snprintf(msg, len, "%s: %s", i->ifname, reason);
	if (showssid)
		p += snprintf(p, len - (size_t)(p - msg), " %s", i->ssid);
	if (iplen)
		p += snprintf(p, len - (size_t)(p - msg), " %s/%s", ip, iplen);
	else if (ip)
		p += snprintf(p, len - (size_t)(p - msg), " %s", ip);

	if (new_msg) {
		if (i->last_message == NULL || strcmp(i->last_message, msg))
			*new_msg = true;
		else
			*new_msg = false;
	}
	free(i->last_message);
	i->last_message = strdup(msg);

	return msg;
}

void
dhcpcd_free(DHCPCD_CONNECTION *con)
{
	DHCPCD_IF *nif;
	DHCPCD_WPA *nwpa;
	DHCPCD_WI_HIST *nh;

	assert(con);
	while (con->interfaces) {
		nif = con->interfaces->next;
		free(con->interfaces->data);
		free(con->interfaces->last_message);
		free(con->interfaces);
		con->interfaces = nif;
	}
	while (con->wpa) {
		nwpa = con->wpa->next;
		dhcpcd_wpa_close(con->wpa);
		free(con->wpa);
		con->wpa = nwpa;
	}
	while (con->wi_history) {
		nh = con->wi_history->next;
		free(con->wi_history);
		con->wi_history = nh;
	}
	free(con);
}