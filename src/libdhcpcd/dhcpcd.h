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

#ifndef DHCPCD_H
#define DHCPCD_H

#include <net/if.h>
#include <netinet/in.h>

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

#ifndef DHCPCD_SOCKET
#define DHCPCD_SOCKET		"/var/run/dhcpcd.sock"
#endif
#ifndef DHCPCD_UNPRIV_SOCKET
#define DHCPCD_UNPRIV_SOCKET	"/var/run/dhcpcd.unpriv.sock"
#endif

#ifndef WPA_CTRL_DIR
#define WPA_CTRL_DIR		"/var/run/wpa_supplicant"
#endif

#define DHCPCD_RETRYOPEN	100	/* milliseconds */
#define DHCPCD_WI_HIST_MAX	10	/* Recall 10 scans for averages */

/* Each non printable byte of the SSID is represented as \000 */
#define IF_SSIDSIZE		((32 * 4) + 1)
#define IF_BSSIDSIZE		64
#define FLAGSIZE		64
#define TYPESIZE		8
#define REASONSIZE		16

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
	DHCPCD_WI_AV strength;
	char ssid[IF_SSIDSIZE];
	char flags[FLAGSIZE];
} DHCPCD_WI_SCAN;

#ifdef IN_LIBDHCPCD
typedef struct dhcpcd_if {
	struct dhcpcd_if *next;
	const char *ifname;
	const char *type;
	const char *reason;

	unsigned int flags;
	bool up;
	bool wireless;
	const char *ssid;

	char *data;
	size_t data_len;

	char *last_message;

	struct dhcpcd_connection *con;
} DHCPCD_IF;
#else
typedef struct dhcpcd_if {
	struct dhcpcd_if *next;
	const char *ifname;
	const char *type;
	const char *reason;

	int flags;
	bool up;
	bool wireless;
	const char *ssid;
} DHCPCD_IF;
#endif

typedef struct dhcpcd_config {
	struct dhcpcd_config *next;
	char *option;
	char *value;
} DHCPCD_OPTION;

#ifdef IN_LIBDHCPCD
typedef struct dhcpcd_wi_hist {
	struct dhcpcd_wi_hist *next;
	char ifname[IF_NAMESIZE];
	char bssid[IF_BSSIDSIZE];
	int quality;
	int noise;
	int level;
	int strength;
} DHCPCD_WI_HIST;

typedef struct dhcpcd_wpa {
	struct dhcpcd_wpa *next;
	char ifname[IF_NAMESIZE];
	bool open;
	int command_fd;
	char *command_path;
	int listen_fd;
	char *listen_path;
	bool attached;
	struct dhcpcd_connection *con;
} DHCPCD_WPA;

typedef struct dhcpcd_connection {
	struct dhcpcd_connection *next;
	bool open;
	bool privileged;
	int command_fd;
	int listen_fd;
	const char *progname;

	DHCPCD_IF *interfaces;
	DHCPCD_WPA *wpa;
	DHCPCD_WI_HIST *wi_history;

	void (*if_cb)(DHCPCD_IF *, void *);
	void *if_context;
	void (*status_cb)(struct dhcpcd_connection *, const char *, void *);
	void *status_context;
	bool wpa_started;
	void (*wi_scanresults_cb)(DHCPCD_WPA *, void *);
	void *wi_scanresults_context;
	void (*wpa_status_cb)(DHCPCD_WPA *, const char *, void *);
	void *wpa_status_context;

	char *buf;
	size_t buflen;

	char *version;
	bool terminate_commands;
	char *error;
	int err;
	int errors;
	const char *status;

	char *cffile;
} DHCPCD_CONNECTION;

#else
typedef void *DHCPCD_CONFIG;
typedef void *DHCPCD_WPA;
typedef void *DHCPCD_CONNECTION;
#endif

DHCPCD_CONNECTION * dhcpcd_new(void);
const char * dhcpcd_version(DHCPCD_CONNECTION *);
void dhcpcd_set_progname(DHCPCD_CONNECTION *, const char *);
const char * dhcpcd_get_progname(const DHCPCD_CONNECTION *);
const char * dhcpcd_status(DHCPCD_CONNECTION *);
const char * dhcpcd_cffile(DHCPCD_CONNECTION *);
bool dhcpcd_realloc(DHCPCD_CONNECTION *, size_t);
int dhcpcd_open(DHCPCD_CONNECTION *, bool priv);
void dhcpcd_close(DHCPCD_CONNECTION *);
void dhcpcd_free(DHCPCD_CONNECTION *);
void dhcpcd_set_if_callback(DHCPCD_CONNECTION *,
    void (*)(DHCPCD_IF *, void *), void *);
void dhcpcd_set_status_callback(DHCPCD_CONNECTION *,
    void (*)(DHCPCD_CONNECTION *, const char *, void *), void *);
int dhcpcd_get_fd(DHCPCD_CONNECTION *);
bool dhcpcd_privileged(DHCPCD_CONNECTION *);
void dhcpcd_dispatch(DHCPCD_CONNECTION *);
DHCPCD_IF * dhcpcd_interfaces(DHCPCD_CONNECTION *);
char **dhcpcd_interface_names(DHCPCD_CONNECTION *, size_t *);
void dhcpcd_freev(char **);
char **dhcpcd_interface_names_sorted(DHCPCD_CONNECTION *);
DHCPCD_IF * dhcpcd_get_if(DHCPCD_CONNECTION *, const char *, const char *);
DHCPCD_CONNECTION * dhcpcd_if_connection(DHCPCD_IF *);
const char *dhcpcd_get_value(const DHCPCD_IF *, const char *);
const char *dhcpcd_get_prefix_value(const DHCPCD_IF *, const char *,
    const char *);
ssize_t dhcpcd_encode_string_escape(char *, size_t, const char *, size_t);
ssize_t dhcpcd_decode_string_escape(char *, size_t, const char *);
ssize_t dhcpcd_decode_hex(char *, size_t, const char *);
char * dhcpcd_if_message(DHCPCD_IF *, bool *);

ssize_t dhcpcd_command(DHCPCD_CONNECTION *, const char *, char **);
ssize_t dhcpcd_command_arg(DHCPCD_CONNECTION *, const char *, const char *,
    char **);
#define dhcpcd_rebind(c, i)	dhcpcd_command_arg((c), "-n", (i), NULL)
#define dhcpcd_release(c, i)	dhcpcd_command_arg((c), "-k", (i), NULL)

void dhcpcd_wpa_start(DHCPCD_CONNECTION *);
DHCPCD_WPA *dhcpcd_wpa_find(DHCPCD_CONNECTION *, const char *);
DHCPCD_WPA *dhcpcd_wpa_new(DHCPCD_CONNECTION *, const char *);
DHCPCD_CONNECTION *dhcpcd_wpa_connection(DHCPCD_WPA *);
int dhcpcd_wpa_open(DHCPCD_WPA *);
void dhcpcd_wpa_close(DHCPCD_WPA *);
void dhcpcd_wpa_dispatch(DHCPCD_WPA *);
int dhcpcd_wpa_get_fd(DHCPCD_WPA *);
DHCPCD_IF *dhcpcd_wpa_if(DHCPCD_WPA *);
void dhcpcd_wpa_if_event(DHCPCD_IF *);
void dhcpcd_wpa_set_scan_callback(DHCPCD_CONNECTION *,
    void (*)(DHCPCD_WPA *, void *), void *);
void dhcpcd_wpa_set_status_callback(DHCPCD_CONNECTION *,
    void (*)(DHCPCD_WPA *, const char *, void *), void *);
int dhcpcd_wi_scan_compare(DHCPCD_WI_SCAN *a, DHCPCD_WI_SCAN *b);
DHCPCD_WI_SCAN * dhcpcd_wi_scans(DHCPCD_IF *);
void dhcpcd_wi_scans_free(DHCPCD_WI_SCAN *);
void dhcpcd_wi_history_clear(DHCPCD_CONNECTION *);
bool dhcpcd_wpa_set_network(DHCPCD_WPA *, int, const char *, const char *);
int dhcpcd_wpa_find_network_new(DHCPCD_WPA *, const char *);
bool dhcpcd_wpa_command(DHCPCD_WPA *, const char *);
bool dhcpcd_wpa_command_arg(DHCPCD_WPA *, const char *, const char *);

bool dhcpcd_wpa_scan(DHCPCD_WPA *);
bool dhcpcd_wpa_reassociate(DHCPCD_WPA *);
bool dhcpcd_wpa_disconnect(DHCPCD_WPA *);
bool dhcpcd_wpa_config_write(DHCPCD_WPA *);
int dhcpcd_wpa_network_find_new(DHCPCD_WPA *, const char *);
bool dhcpcd_wpa_network_disable(DHCPCD_WPA *, int);
bool dhcpcd_wpa_network_enable(DHCPCD_WPA *, int);
bool dhcpcd_wpa_network_remove(DHCPCD_WPA *, int);
char * dhcpcd_wpa_network_get(DHCPCD_WPA *, int, const char *);
bool dhcpcd_wpa_network_set(DHCPCD_WPA *, int, const char *, const char *);

#define DHCPCD_WPA_SUCCESS	 0
#define DHCPCD_WPA_ERR		-1
#define DHCPCD_WPA_ERR_SET	-2
#define DHCPCD_WPA_ERR_SET_PSK	-3
#define DHCPCD_WPA_ERR_ENABLE	-4
#define DHCPCD_WPA_ERR_WRITE	-5
#define DHCPCD_WPA_ERR_ASSOC	-6
int dhcpcd_wpa_configure_psk(DHCPCD_WPA *w, DHCPCD_WI_SCAN *s, const char *p);

char ** dhcpcd_config_blocks(DHCPCD_CONNECTION *, const char *);
DHCPCD_OPTION *dhcpcd_config_read(DHCPCD_CONNECTION *,
    const char *, const char *);
void dhcpcd_config_free(DHCPCD_OPTION *);
const char * dhcpcd_config_get(DHCPCD_OPTION *, const char *);
const char * dhcpcd_config_get_static(DHCPCD_OPTION *, const char *);
bool dhcpcd_config_set(DHCPCD_OPTION **, const char *, const char *);
bool dhcpcd_config_set_static(DHCPCD_OPTION **, const char *, const char *);
bool dhcpcd_config_writeable(DHCPCD_CONNECTION *con);
bool dhcpcd_config_write(DHCPCD_CONNECTION *,
    const char *, const char *, const DHCPCD_OPTION *);

#ifdef __cplusplus
}
#endif

#endif
