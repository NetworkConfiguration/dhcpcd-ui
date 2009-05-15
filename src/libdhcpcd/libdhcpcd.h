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

#ifndef LIBDHCPCD_H
#define LIBDHCPCD_H

#include <net/if.h>
#include <netinet/in.h>

#include <poll.h>
#include <stdbool.h>

#define IF_SSIDSIZE 33
#define IF_BSSIDSIZE 64
#define FLAGSIZE 64
#define REASONSIZE 16

typedef struct dhcpcd_wi_avs {
	int value;
	int average;
} DHCPCD_WI_AV;

typedef struct dhcpcd_wi_scan {
	struct dhcpcd_wi_scan *next;
	char bssid[IF_BSSIDSIZE];
	int frequency;
	DHCPCD_WI_AV quality;
	DHCPCD_WI_AV noise;
	DHCPCD_WI_AV level;
	char ssid[IF_SSIDSIZE];
	char flags[FLAGSIZE];
} DHCPCD_WI_SCAN;

typedef struct dhcpcd_if {
	struct dhcpcd_if *next;
	char ifname[IF_NAMESIZE];
	unsigned int flags;
	char reason[REASONSIZE];
	struct in_addr ip;
	unsigned char cidr;
	bool wireless;
	char ssid[IF_SSIDSIZE];
} DHCPCD_IF;

/* Although we use DBus, we don't have to rely on it for our API */
#ifdef IN_LIBDHCPCD
#include <dbus/dbus.h>
typedef DBusMessage DHCPCD_MESSAGE;
typedef DBusMessageIter DHCPCD_MESSAGEITER;

typedef struct dhcpcd_wi_hist {
	struct dhcpcd_wi_hist *next;
	char ifname[IF_NAMESIZE];
	char bssid[IF_BSSIDSIZE];
	int quality;
	int noise;
	int level;
} DHCPCD_WI_HIST;

typedef struct dhcpcd_connection {
	struct dhcpcd_connection *next;
	DBusConnection *bus;
	char *error;
	int err;
	int errors;
	char *status;
	void (*add_watch)(struct dhcpcd_connection *, const struct pollfd *,
	    void *);
	void (*delete_watch)(struct dhcpcd_connection *, const struct pollfd *,
	    void *);
	void *watch_data;
	void (*event)(struct dhcpcd_connection *, DHCPCD_IF *, void *);
	void (*status_changed)(struct dhcpcd_connection *, const char *,
	    void *);
	void (*wi_scanresults)(struct dhcpcd_connection *, DHCPCD_IF *,
	    void *);
	void *signal_data;
	DHCPCD_IF *interfaces;
	DHCPCD_WI_HIST *wi_history;
} DHCPCD_CONNECTION;

typedef struct dhcpcd_watch {
	struct dhcpcd_watch *next;
	DHCPCD_CONNECTION *connection;
	DBusWatch *watch;
	struct pollfd pollfd;
} DHCPCD_WATCH;
extern DHCPCD_WATCH *dhcpcd_watching;

#define DHCPCD_SERVICE	"name.marples.roy.dhcpcd"
#define DHCPCD_PATH	"/name/marples/roy/dhcpcd"

#ifdef __GLIBC__
#  define strlcpy(dst, src, n) snprintf(dst, n, "%s", src)
#endif

bool dhcpcd_iter_get(DHCPCD_CONNECTION *, DHCPCD_MESSAGEITER *, int, void *);
DHCPCD_MESSAGE * dhcpcd_send_reply(DHCPCD_CONNECTION *, DHCPCD_MESSAGE *);
DHCPCD_MESSAGE * dhcpcd_message_reply(DHCPCD_CONNECTION *,
    const char *, const char *);
void dhcpcd_error_set(DHCPCD_CONNECTION *, const char *, int);
DHCPCD_IF * dhcpcd_if_new(DHCPCD_CONNECTION *, DBusMessageIter *, char **);
void dhcpcd_if_free(DHCPCD_IF *);
void dhcpcd_dispatch_signal(DHCPCD_CONNECTION *, const char *, void *);
bool dhcpcd_dispatch_message(DHCPCD_CONNECTION *, DHCPCD_MESSAGE *);
#else
typedef void * DHCPCD_CONNECTION;
#endif

DHCPCD_CONNECTION * dhcpcd_open(char **);
bool dhcpcd_close(DHCPCD_CONNECTION *);
const char * dhcpcd_error(DHCPCD_CONNECTION *);
void dhcpcd_error_clear(DHCPCD_CONNECTION *);
void dhcpcd_set_watch_functions(DHCPCD_CONNECTION *,
    void (*)(DHCPCD_CONNECTION *, const struct pollfd *, void *),
    void (*)(DHCPCD_CONNECTION *, const struct pollfd *, void *),
    void *);
void dhcpcd_set_signal_functions(DHCPCD_CONNECTION *,
    void (*)(DHCPCD_CONNECTION *, DHCPCD_IF *, void *),
    void (*)(DHCPCD_CONNECTION *, const char *, void *),
    void (*)(DHCPCD_CONNECTION *, DHCPCD_IF *, void *),
    void *);
const char * dhcpcd_status(DHCPCD_CONNECTION *);
bool dhcpcd_command(DHCPCD_CONNECTION *, const char *, const char *, char **);
void dhcpcd_dispatch(int);
DHCPCD_IF * dhcpcd_interfaces(DHCPCD_CONNECTION *);
DHCPCD_IF * dhcpcd_if_find(DHCPCD_CONNECTION *, const char *);
DHCPCD_CONNECTION * dhcpcd_if_connection(DHCPCD_IF *);

bool dhcpcd_if_up(const DHCPCD_IF *);
bool dhcpcd_if_down(const DHCPCD_IF *);
char * dhcpcd_if_message(const DHCPCD_IF *);

DHCPCD_WI_SCAN * dhcpcd_wi_scans(DHCPCD_CONNECTION *, DHCPCD_IF *);
void dhcpcd_wi_scans_free(DHCPCD_WI_SCAN *);
void dhcpcd_wi_history_clear(DHCPCD_CONNECTION *);
bool dhcpcd_wpa_command(DHCPCD_CONNECTION *, DHCPCD_IF *, const char *, int);
bool dhcpcd_wpa_set_network(DHCPCD_CONNECTION *, DHCPCD_IF *,
    int, const char *, const char *);
int dhcpcd_wpa_find_network_new(DHCPCD_CONNECTION *, DHCPCD_IF *,
    const char *);

#define dhcpcd_rebind(c, i)						      \
	dhcpcd_command(c, "Rebind", i ? (i)->ifname : NULL, NULL)
#define dhcpcd_release(c, i)						      \
	dhcpcd_command(c, "Release", i ? (i)->ifname : NULL, NULL)

/* Our configuration is probably going to change ... */
#ifdef IN_LIBDHCPCD
typedef struct dhcpcd_config {
	char *option;
	char *value;
	struct dhcpcd_config *next;
} DHCPCD_CONFIG;
#else
typedef void *DHCPCD_CONFIG;
#endif

char ** dhcpcd_config_blocks_get(DHCPCD_CONNECTION *, const char *);
DHCPCD_CONFIG * dhcpcd_config_load(DHCPCD_CONNECTION *,
    const char *, const char *);
void dhcpcd_config_free(DHCPCD_CONFIG *);
const char * dhcpcd_config_get(DHCPCD_CONFIG *, const char *);
const char * dhcpcd_config_get_static(DHCPCD_CONFIG *, const char *);
bool dhcpcd_config_set(DHCPCD_CONFIG **, const char *, const char *);
bool dhcpcd_config_set_static(DHCPCD_CONFIG **, const char *, const char *);
bool dhcpcd_config_save(DHCPCD_CONNECTION *,
    const char *, const char *, DHCPCD_CONFIG *);

#endif
