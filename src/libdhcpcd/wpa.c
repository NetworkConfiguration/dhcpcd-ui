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

#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/un.h>

#include <assert.h>
#include <ctype.h>
#include <errno.h>
#include <limits.h>
#include <poll.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define IN_LIBDHCPCD
#include "config.h"
#include "dhcpcd.h"

#ifndef SUN_LEN
#define SUN_LEN(su) \
	(sizeof(*(su)) - sizeof((su)->sun_path) + strlen((su)->sun_path))
#endif

#define CLAMP(x, low, high) \
	(((x) > (high)) ? (high) : (((x) < (low)) ? (low) : (x)))

static int
wpa_open(const char *ifname, char **path)
{
	static int counter;
	int fd;
	socklen_t len;
	struct sockaddr_un sun;

	if ((fd = socket(AF_UNIX,
	    SOCK_DGRAM | SOCK_CLOEXEC | SOCK_NONBLOCK, 0)) == -1)
		return -1;
	memset(&sun, 0, sizeof(sun));
	sun.sun_family = AF_UNIX;
	snprintf(sun.sun_path, sizeof(sun.sun_path),
	    "/tmp/libdhcpcd-wpa-%d.%d", getpid(), counter++);
	*path = strdup(sun.sun_path);
	len = (socklen_t)SUN_LEN(&sun);
	if (bind(fd, (struct sockaddr *)&sun, len) == -1) {
		close(fd);
		unlink(*path);
		free(*path);
		*path = NULL;
		return -1;
	}
	snprintf(sun.sun_path, sizeof(sun.sun_path),
	    WPA_CTRL_DIR "/%s", ifname);
	len = (socklen_t)SUN_LEN(&sun);
	if (connect(fd, (struct sockaddr *)&sun, len) == -1) {
		close(fd);
		unlink(*path);
		free(*path);
		*path = NULL;
		return -1;
	}

	return fd;
}

static ssize_t
wpa_cmd(int fd, const char *cmd, char *buffer, size_t len)
{
	int retval;
	ssize_t bytes;
	struct pollfd pfd;

	if (buffer)
		*buffer = '\0';
	bytes = write(fd, cmd, strlen(cmd));
	if (bytes == -1 || bytes == 0)
		return -1;
	if (buffer == NULL || len == 0)
		return 0;
	pfd.fd = fd;
	pfd.events = POLLIN | POLLHUP;
	pfd.revents = 0;
	retval = poll(&pfd, 1, 2000);
	if (retval == -1)
		return -1;
	if (retval == 0 || !(pfd.revents & (POLLIN | POLLHUP)))
		return -1;

	bytes = read(fd, buffer, len == 1 ? 1 : len - 1);
	if (bytes != -1)
		buffer[bytes] = '\0';
	return bytes;
}

bool
dhcpcd_wpa_command(DHCPCD_WPA *wpa, const char *cmd)
{
	char buf[10];
	ssize_t bytes;

	bytes = wpa_cmd(wpa->command_fd, cmd, buf, sizeof(buf));
	return (bytes == -1 || bytes == 0 ||
	    strcmp(buf, "OK\n")) ? false : true;
}

bool
dhcpcd_wpa_command_arg(DHCPCD_WPA *wpa, const char *cmd, const char *arg)
{
	size_t cmdlen, nlen;

	cmdlen = strlen(cmd);
	nlen = cmdlen + strlen(arg) + 2;
	if (!dhcpcd_realloc(wpa->con, nlen))
		return -1;
	strlcpy(wpa->con->buf, cmd, wpa->con->buflen);
	wpa->con->buf[cmdlen] = ' ';
	strlcpy(wpa->con->buf + cmdlen + 1, arg, wpa->con->buflen - 1 - cmdlen);
	return dhcpcd_wpa_command(wpa, wpa->con->buf);
}

static bool
dhcpcd_attach_detach(DHCPCD_WPA *wpa, bool attach)
{
	char buf[10];
	ssize_t bytes;

	if (wpa->attached == attach)
		return true;

	bytes = wpa_cmd(wpa->listen_fd, attach > 0 ? "ATTACH" : "DETACH",
	    buf, sizeof(buf));
	if (bytes == -1 || bytes == 0 || strcmp(buf, "OK\n"))
		return false;

	wpa->attached = attach;
	return true;
}

bool
dhcpcd_wpa_scan(DHCPCD_WPA *wpa)
{

	return dhcpcd_wpa_command(wpa, "SCAN");
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

static void
dhcpcd_strtoi(int *val, const char *s)
{
	long l;

	l = strtol(s, NULL, 0);
	if (l >= INT_MIN && l <= INT_MAX)
		*val = (int)l;
	else
		errno = ERANGE;
}

static int
dhcpcd_wpa_hex2num(char c)
{

	if (c >= '0' && c <= '9')
		return c - '0';
	if (c >= 'a' && c <= 'f')
		return c - 'a' + 10;
	if (c >= 'A' && c <= 'F')
		return c - 'A' + 10;
	return -1;
}

static int
dhcpcd_wpa_hex2byte(const char *src)
{
	int h, l;

	if ((h = dhcpcd_wpa_hex2num(*src++)) == -1 ||
	    (l = dhcpcd_wpa_hex2num(*src)) == -1)
		return -1;
	return (h << 4) | l;
}

static ssize_t
dhcpcd_wpa_decode_ssid(char *dst, size_t dlen, const char *src)
{
	const char *start;
	char c, esc;
	int xb;

	start = dst;
	for (;;) {
		if (*src == '\0')
			break;
		if (--dlen == 0) {
			errno = ENOSPC;
			return -1;
		}
		c = *src++;
		switch (c) {
		case '\\':
			if (*src == '\0') {
				errno = EINVAL;
				return -1;
			}
			esc = *src++;
			switch (esc) {
			case '\\':
			case '"': *dst++ = esc; break;
			case 'n': *dst++ = '\n'; break;
			case 'r': *dst++ = '\r'; break;
			case 't': *dst++ = '\t'; break;
			case 'e': *dst++ = '\033'; break;
			case 'x':
				if (src[0] == '\0' || src[1] == '\0') {
					errno = EINVAL;
					return -1;
				}
				if ((xb = dhcpcd_wpa_hex2byte(src)) == -1)
					return -1;
				*dst++ = (char)xb;
				src += 2;
				break;
			default: errno = EINVAL; return -1;
			}
		default: *dst++ = c; break;
		}
	}
	if (dlen == 0) {
		errno = ENOSPC;
		return -1;
	}
	*dst = '\0';
	return dst - start;
}

static DHCPCD_WI_SCAN *
dhcpcd_wpa_scans_read(DHCPCD_WPA *wpa)
{
	size_t i;
	ssize_t bytes, dl;
	DHCPCD_WI_SCAN *wis, *w, *l;
	char *s, *p, buf[32];
	char wssid[sizeof(w->ssid)];

	if (!dhcpcd_realloc(wpa->con, 2048))
		return NULL;
	wis = NULL;
	for (i = 0; i < 1000; i++) {
		snprintf(buf, sizeof(buf), "BSS %zu", i);
		bytes = wpa_cmd(wpa->command_fd, buf,
		    wpa->con->buf, wpa->con->buflen);
		if (bytes == 0 || bytes == -1 ||
		    strncmp(wpa->con->buf, "FAIL", 4) == 0)
			break;
		p = wpa->con->buf;
		w = calloc(1, sizeof(*w));
		if (w == NULL)
			break;
		dl = 0;
		wssid[0] = '\0';
		while ((s = strsep(&p, "\n"))) {
			if (*s == '\0')
				continue;
			if (strncmp(s, "bssid=", 6) == 0)
				strlcpy(w->bssid, s + 6, sizeof(w->bssid));
			else if (strncmp(s, "freq=", 5) == 0)
				dhcpcd_strtoi(&w->frequency, s + 5);
//			else if (strncmp(s, "beacon_int=", 11) == 0)
//				;
			else if (strncmp(s, "qual=", 5) == 0)
				dhcpcd_strtoi(&w->quality.value, s + 5);
			else if (strncmp(s, "noise=", 6) == 0)
				dhcpcd_strtoi(&w->noise.value, s + 6);
			else if (strncmp(s, "level=", 6) == 0)
				dhcpcd_strtoi(&w->level.value, s + 6);
			else if (strncmp(s, "flags=", 6) == 0)
				strlcpy(w->flags, s + 6, sizeof(w->flags));
			else if (strncmp(s, "ssid=", 5) == 0) {
				/* Decode it from \xNN to \NNN
				 * so we're consistent */
				dl = dhcpcd_wpa_decode_ssid(wssid,
				    sizeof(wssid), s + 5);
				if (dl == -1)
					break;
				dl = dhcpcd_encode_string_escape(w->ssid,
				    sizeof(w->ssid), wssid, (size_t)dl);
				if (dl == -1)
					break;
			}
		}
		if (dl == -1) {
			free(w);
			break;
		}

		if (wis == NULL)
			wis = w;
		else
			l->next = w;
		l = w;

		w->strength.value = w->level.value;
		if (w->strength.value > 110 && w->strength.value < 256)
			/* Convert WEXT level to dBm */
			w->strength.value -= 256;

		if (w->strength.value < 0) {
			/* Assume dBm */
			w->strength.value =
			    abs(CLAMP(w->strength.value, -100, -40) + 40);
			w->strength.value =
			    100 - ((100 * w->strength.value) / 60);
		} else {
			/* Assume quality percentage */
			w->strength.value = CLAMP(w->strength.value, 0, 100);
		}
	}
	return wis;
}

DHCPCD_WI_SCAN *
dhcpcd_wi_scans(DHCPCD_IF *i)
{
	DHCPCD_WPA *wpa;
	DHCPCD_WI_SCAN *wis, *w;
	int nh;
	DHCPCD_WI_HIST *h, *hl;

	wpa = dhcpcd_wpa_find(i->con, i->ifname);
	if (wpa == NULL)
		return NULL;
	wis = dhcpcd_wpa_scans_read(wpa);
	for (w = wis; w; w = w->next) {
		nh = 1;
		hl = NULL;
		w->quality.average = w->quality.value;
		w->noise.average = w->noise.value;
		w->level.average = w->level.value;
		w->strength.average = w->strength.value;

		for (h = wpa->con->wi_history; h; h = h->next) {
			if (strcmp(h->ifname, i->ifname) == 0 &&
			    strcmp(h->bssid, wis->bssid) == 0)
			{
				w->quality.average += h->quality;
				w->noise.average += h->noise;
				w->level.average += h->level;
				w->strength.average += h->strength;
				if (++nh == DHCPCD_WI_HIST_MAX) {
					hl->next = h->next;
					free(h);
					break;
				}
			}
			hl = h;
		}

		if (nh != 1) {
			w->quality.average /= nh;
			w->noise.average /= nh;
			w->level.average /= nh;
			w->strength.average /= nh;
		}
		h = malloc(sizeof(*h));
		if (h) {
			strlcpy(h->ifname, i->ifname, sizeof(h->ifname));
			strlcpy(h->bssid, w->bssid, sizeof(h->bssid));
			h->quality = w->quality.value;
			h->noise = w->noise.value;
			h->level = w->level.value;
			h->strength = w->strength.value;
			h->next = wpa->con->wi_history;
			wpa->con->wi_history = h;
		}
	}

	return wis;
}

bool
dhcpcd_wpa_reassociate(DHCPCD_WPA *wpa)
{

	return dhcpcd_wpa_command(wpa, "REASSOCIATE");
}

bool
dhcpcd_wpa_disconnect(DHCPCD_WPA *wpa)
{

	return dhcpcd_wpa_command(wpa, "DISCONNECT");
}

bool
dhcpcd_wpa_config_write(DHCPCD_WPA *wpa)
{

	return dhcpcd_wpa_command(wpa, "SAVE_CONFIG");
}

static bool
dhcpcd_wpa_network(DHCPCD_WPA *wpa, const char *cmd, int id)
{
	size_t len;

	len = strlen(cmd) + 32;
	if (!dhcpcd_realloc(wpa->con, len))
		return false;
	snprintf(wpa->con->buf, wpa->con->buflen, "%s %d", cmd, id);
	return dhcpcd_wpa_command(wpa, wpa->con->buf);
}

bool
dhcpcd_wpa_network_disable(DHCPCD_WPA *wpa, int id)
{

	return dhcpcd_wpa_network(wpa, "DISABLE_NETWORK", id);
}

bool
dhcpcd_wpa_network_enable(DHCPCD_WPA *wpa, int id)
{

	return dhcpcd_wpa_network(wpa, "ENABLE_NETWORK", id);
}

bool
dhcpcd_wpa_network_remove(DHCPCD_WPA *wpa, int id)
{

	return dhcpcd_wpa_network(wpa, "REMOVE_NETWORK", id);
}

char *
dhcpcd_wpa_network_get(DHCPCD_WPA *wpa, int id, const char *param)
{
	ssize_t bytes;

	if (!dhcpcd_realloc(wpa->con, 2048))
		return NULL;
	snprintf(wpa->con->buf, wpa->con->buflen, "GET_NETWORK %d %s",
	    id, param);
	bytes = wpa_cmd(wpa->command_fd, wpa->con->buf,
	    wpa->con->buf, wpa->con->buflen);
	if (bytes == 0 || bytes == -1)
		return NULL;
	if (strcmp(wpa->con->buf, "FAIL\n") == 0) {
		errno = EINVAL;
		return NULL;
	}
	return wpa->con->buf;
}

bool
dhcpcd_wpa_network_set(DHCPCD_WPA *wpa, int id,
    const char *param, const char *value)
{
	size_t len;

	len = strlen("SET_NETWORK") + 32 + strlen(param) + strlen(value) + 3;
	if (!dhcpcd_realloc(wpa->con, len))
		return false;
	snprintf(wpa->con->buf, wpa->con->buflen, "SET_NETWORK %d %s %s",
	    id, param, value);
	return dhcpcd_wpa_command(wpa, wpa->con->buf);
}

static int
dhcpcd_wpa_network_find(DHCPCD_WPA *wpa, const char *fssid)
{
	ssize_t bytes, dl, tl;
	size_t fl;
	char *s, *t, *ssid, *bssid, *flags;
	char dssid[IF_SSIDSIZE], tssid[IF_SSIDSIZE];
	long l;

	dhcpcd_realloc(wpa->con, 2048);
	bytes = wpa_cmd(wpa->command_fd, "LIST_NETWORKS",
	    wpa->con->buf, wpa->con->buflen);
	if (bytes == 0 || bytes == -1)
		return -1;

	fl = strlen(fssid);

	s = strchr(wpa->con->buf, '\n');
	if (s == NULL)
		return -1;
	while ((t = strsep(&s, "\b"))) {
		if (*t == '\0')
			continue;
		ssid = strchr(t, '\t');
		if (ssid == NULL)
			break;
		*ssid++ = '\0';
		bssid = strchr(ssid, '\t');
		if (bssid == NULL)
			break;
		*bssid++ = '\0';
		flags = strchr(bssid, '\t');
		if (flags == NULL)
			break;
		*flags++ = '\0';
		l = strtol(t, NULL, 0);
		if (l < 0 || l > INT_MAX) {
			errno = ERANGE;
			break;
		}

		/* Decode the wpa_supplicant SSID into raw chars and
		 * then encode into our octal escaped string to
		 * compare. */
		dl = dhcpcd_wpa_decode_ssid(dssid, sizeof(dssid), ssid);
		if (dl == -1)
			return -1;
		tl = dhcpcd_encode_string_escape(tssid,
		    sizeof(tssid), dssid, (size_t)dl);
		if (tl == -1)
			return -1;
		if ((size_t)tl == fl && memcmp(tssid, fssid, (size_t)tl) == 0)
			return (int)l;
	}
	errno = ENOENT;
	return -1;
}

static int
dhcpcd_wpa_network_new(DHCPCD_WPA *wpa)
{
	ssize_t bytes;
	long l;

	dhcpcd_realloc(wpa->con, 32);
	bytes = wpa_cmd(wpa->command_fd, "ADD_NETWORK",
	    wpa->con->buf, sizeof(wpa->con->buf));
	if (bytes == 0 || bytes == -1)
		return -1;
	l = strtol(wpa->con->buf, NULL, 0);
	if (l < 0 || l > INT_MAX) {
		errno = ERANGE;
		return -1;
	}
	return (int)l;
}

static const char hexchrs[] = "0123456789abcdef";
int
dhcpcd_wpa_network_find_new(DHCPCD_WPA *wpa, const char *ssid)
{
	int id;
	char dssid[IF_SSIDSIZE], essid[IF_SSIDSIZE], *ep;
	ssize_t dl, i;
	char *dp;

	id = dhcpcd_wpa_network_find(wpa, ssid);
	if (id != -1)
		return id;

	dl = dhcpcd_decode_string_escape(dssid, sizeof(dssid), ssid);
	if (dl == -1)
		return -1;

	for (i = 0; i < dl; i++) {
		if (!isascii((int)dssid[i]) && !isprint((int)dssid[i]))
			break;
	}
	dp = dssid;
	ep = essid;
	if (i < dl) {
		/* Non standard characters found! Encode as hex string */
		unsigned char c;

		for (; dl; dl--) {
			c = (unsigned char)*dp++;
			*ep++ = hexchrs[(c & 0xf0) >> 4];
			*ep++ = hexchrs[(c & 0x0f)];
		}
	} else {
		*ep++ = '\"';
		do
			*ep++ = *dp;
		while (*++dp != '\0');
		*ep++ = '\"';
	}
	*ep = '\0';

	id = dhcpcd_wpa_network_new(wpa);
	if (id != -1)
		dhcpcd_wpa_network_set(wpa, id, "ssid", essid);
	return id;
}

void
dhcpcd_wpa_close(DHCPCD_WPA *wpa)
{

	assert(wpa);

	if (wpa->command_fd == -1 || !wpa->open)
		return;

	wpa->open = false;
	dhcpcd_attach_detach(wpa, false);
	shutdown(wpa->command_fd, SHUT_RDWR);
	shutdown(wpa->listen_fd, SHUT_RDWR);

	if (wpa->con->wpa_status_cb)
		wpa->con->wpa_status_cb(wpa, "down",
		    wpa->con->wpa_status_context);

	close(wpa->command_fd);
	wpa->command_fd = -1;
	close(wpa->listen_fd);
	wpa->listen_fd = -1;
	unlink(wpa->command_path);
	free(wpa->command_path);
	wpa->command_path = NULL;
	unlink(wpa->listen_path);
	free(wpa->listen_path);
	wpa->listen_path = NULL;
}

DHCPCD_WPA *
dhcpcd_wpa_find(DHCPCD_CONNECTION *con, const char *ifname)
{
	DHCPCD_WPA *wpa;

	for (wpa = con->wpa; wpa; wpa = wpa->next) {
		if (strcmp(wpa->ifname, ifname) == 0)
			return wpa;
	}
	errno = ENOENT;
	return NULL;
}

DHCPCD_WPA *
dhcpcd_wpa_new(DHCPCD_CONNECTION *con, const char *ifname)
{
	DHCPCD_WPA *wpa;

	wpa = dhcpcd_wpa_find(con, ifname);
	if (wpa)
		return wpa;

	wpa = malloc(sizeof(*wpa));
	if (wpa == NULL)
		return NULL;

	wpa->con = con;
	strlcpy(wpa->ifname, ifname, sizeof(wpa->ifname));
	wpa->command_fd = wpa->listen_fd = -1;
	wpa->command_path = wpa->listen_path = NULL;
	wpa->next = con->wpa;
	con->wpa = wpa;
	return wpa;
}

DHCPCD_CONNECTION *
dhcpcd_wpa_connection(DHCPCD_WPA *wpa)
{

	assert(wpa);
	return wpa->con;
}

DHCPCD_IF *
dhcpcd_wpa_if(DHCPCD_WPA *wpa)
{

	return dhcpcd_get_if(wpa->con, wpa->ifname, "link");
}

int
dhcpcd_wpa_open(DHCPCD_WPA *wpa)
{
	int cmd_fd, list_fd = -1;
	char *cmd_path = NULL, *list_path = NULL;

	if (wpa->listen_fd != -1) {
		if (!wpa->open) {
			errno = EISCONN;
			return -1;
		}
		return wpa->listen_fd;
	}

	cmd_fd = wpa_open(wpa->ifname, &cmd_path);
	if (cmd_fd == -1)
		goto fail;

	list_fd = wpa_open(wpa->ifname, &list_path);
	if (list_fd == -1)
		goto fail;

	wpa->open = true;
	wpa->attached = false;
	wpa->command_fd = cmd_fd;
	wpa->command_path = cmd_path;
	wpa->listen_fd = list_fd;
	wpa->listen_path = list_path;
	if (!dhcpcd_attach_detach(wpa, true)) {
		dhcpcd_wpa_close(wpa);
		return -1;
	}

	if (wpa->con->wi_scanresults_cb)
		wpa->con->wi_scanresults_cb(wpa,
		    wpa->con->wi_scanresults_context);

	return wpa->listen_fd;

fail:
	if (cmd_fd != -1)
		close(cmd_fd);
	if (list_fd != -1)
		close(list_fd);
	if (cmd_path)
		unlink(cmd_path);
	free(cmd_path);
	if (list_path)
		free(list_path);
	return -1;
}

int
dhcpcd_wpa_get_fd(DHCPCD_WPA *wpa)
{

	assert(wpa);
	return wpa->open ? wpa->listen_fd : -1;
}

void
dhcpcd_wpa_set_scan_callback(DHCPCD_CONNECTION *con,
    void (*cb)(DHCPCD_WPA *, void *), void *context)
{

	assert(con);
	con->wi_scanresults_cb = cb;
	con->wi_scanresults_context = context;
}


void dhcpcd_wpa_set_status_callback(DHCPCD_CONNECTION * con,
    void (*cb)(DHCPCD_WPA *, const char *, void *), void *context)
{

	assert(con);
	con->wpa_status_cb = cb;
	con->wpa_status_context = context;
}

void
dhcpcd_wpa_dispatch(DHCPCD_WPA *wpa)
{
	char buffer[256], *p;
	size_t bytes;

	assert(wpa);
	bytes = (size_t)read(wpa->listen_fd, buffer, sizeof(buffer));
	if ((ssize_t)bytes == -1 || bytes == 0) {
		dhcpcd_wpa_close(wpa);
		return;
	}

	buffer[bytes] = '\0';
	bytes = strlen(buffer);
	if (buffer[bytes - 1] == ' ')
		buffer[--bytes] = '\0';
	for (p = buffer + 1; *p != '\0'; p++)
		if (*p == '>') {
			p++;
			break;
		}
	if (strcmp(p, "CTRL-EVENT-SCAN-RESULTS") == 0 &&
	    wpa->con->wi_scanresults_cb)
		wpa->con->wi_scanresults_cb(wpa,
		    wpa->con->wi_scanresults_context);
	return;
}

void
dhcpcd_wpa_if_event(DHCPCD_IF *i)
{
	DHCPCD_WPA *wpa;

	assert(i);
	if (strcmp(i->type, "link") == 0) {
		if (strcmp(i->reason, "STOPPED") == 0 ||
		    strcmp(i->reason, "DEPARTED") == 0)
		{
			wpa = dhcpcd_wpa_find(i->con, i->ifname);
			if (wpa)
				dhcpcd_wpa_close(wpa);
		} else if (i->wireless && i->con->wpa_started) {
			wpa = dhcpcd_wpa_new(i->con, i->ifname);
			if (wpa && wpa->listen_fd == -1)
				dhcpcd_wpa_open(wpa);
		}
	}
}

void
dhcpcd_wpa_start(DHCPCD_CONNECTION *con)
{
	DHCPCD_IF *i;

	assert(con);
	con->wpa_started = true;

	for (i = con->interfaces; i; i = i->next)
		dhcpcd_wpa_if_event(i);
}

int
dhcpcd_wpa_configure_psk(DHCPCD_WPA *wpa, DHCPCD_WI_SCAN *s, const char *psk)
{
	const char *mgmt, *var;
	int id;
	char *npsk;
	size_t psk_len;
	bool r;

	assert(wpa);
	assert(s);

	id = dhcpcd_wpa_network_find_new(wpa, s->ssid);
	if (id == -1)
		return DHCPCD_WPA_ERR;

	if (strcmp(s->flags, "[WEP]") == 0) {
		mgmt = "NONE";
		var = "wep_key0";
	} else {
		mgmt = "WPA-PSK";
		var = "psk";
	}

	if (!dhcpcd_wpa_network_set(wpa, id, "key_mgmt", mgmt))
		return DHCPCD_WPA_ERR_SET;

	if (psk)
		psk_len = strlen(psk);
	else
		psk_len = 0;
	npsk = malloc(psk_len + 3);
	if (npsk == NULL)
		return DHCPCD_WPA_ERR;
	npsk[0] = '"';
	if (psk_len)
		memcpy(npsk + 1, psk, psk_len);
	npsk[psk_len + 1] = '"';
	npsk[psk_len + 2] = '\0';
	r = dhcpcd_wpa_network_set(wpa, id, var, npsk);
	free(npsk);
	if (!r)
		return DHCPCD_WPA_ERR_SET_PSK;

	if (!dhcpcd_wpa_network_enable(wpa, id))
		return DHCPCD_WPA_ERR_ENABLE;
	if (!dhcpcd_wpa_reassociate(wpa))
		return DHCPCD_WPA_ERR_ASSOC;
	if (!dhcpcd_wpa_config_write(wpa))
		return DHCPCD_WPA_ERR_WRITE;
	return DHCPCD_WPA_SUCCESS;
}
