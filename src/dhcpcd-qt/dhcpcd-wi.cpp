/*
 * dhcpcd-qt
 * Copyright 2014 Roy Marples <roy@marples.name>
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

#include <QObject>
#include <QInputDialog>
#include <QLineEdit>
#include <QMenu>
#include <QMessageBox>
#include <QSocketNotifier>
#include <QTimer>
#include <QWidgetAction>

#include <cerrno>

#include "config.h"
#include "dhcpcd-wi.h"
#include "dhcpcd-qt.h"
#include "dhcpcd-ifmenu.h"
#include "dhcpcd-ssidmenu.h"

DhcpcdWi::DhcpcdWi(DhcpcdQt *parent, DHCPCD_WPA *wpa)
{

	this->dhcpcdQt = parent;
	this->wpa = wpa;
	menu = NULL;
	scans = NULL;

	int fd = dhcpcd_wpa_get_fd(wpa);
	notifier = new QSocketNotifier(fd, QSocketNotifier::Read);
	connect(notifier, SIGNAL(activated(int)), this, SLOT(dispatch()));
	retryOpenTimer = NULL;
}

DhcpcdWi::~DhcpcdWi()
{

	if (menu) {
		delete menu;
		menu = NULL;
	}

	if (notifier) {
		delete notifier;
		notifier = NULL;
	}

	dhcpcd_wi_scans_free(scans);
}

DHCPCD_WPA *DhcpcdWi::getWpa()
{

	return wpa;
}

DHCPCD_WI_SCAN *DhcpcdWi::getScans()
{

	return scans;
}

bool DhcpcdWi::setScans(DHCPCD_WI_SCAN *scans)
{
	int changed = 0;

	if (menu) {
		QList<DhcpcdSsidMenu*> lst;
		DHCPCD_WI_SCAN *scan;

		lst = menu->findChildren<DhcpcdSsidMenu*>();
		for (scan = scans; scan; scan = scan->next) {
			bool found = false;

			foreach(DhcpcdSsidMenu *sm, lst) {
				DHCPCD_WI_SCAN *s = sm->getScan();
				if (memcmp(scan->bssid, s->bssid,
				    sizeof(scan->bssid)) == 0)
				{
					sm->setScan(scan);
					found = true;
					break;
				}
			}

			if (!found) {
				createMenuItem(menu, scan);
				changed++;
			}
		}

		foreach(DhcpcdSsidMenu *sm, lst) {
			DHCPCD_WI_SCAN *s = sm->getScan();
			for (scan = scans; scan; scan = scan->next) {
				if (memcmp(scan->bssid, s->bssid,
				    sizeof(scan->bssid)) == 0)
					break;
			}
			if (scan == NULL) {
				menu->removeAction(sm->getWidgetAction());
				changed--;
			}
		}
	}

	dhcpcd_wi_scans_free(this->scans);
	this->scans = scans;

	return !(changed == 0);
}

void DhcpcdWi::createMenuItem(QMenu *menu, DHCPCD_WI_SCAN *scan)
{
	QWidgetAction *wa = new QWidgetAction(menu);
	DhcpcdSsidMenu *ssidMenu = new DhcpcdSsidMenu(menu, wa, this, scan);
	wa->setDefaultWidget(ssidMenu);
	menu->addAction(wa);
	connect(ssidMenu, SIGNAL(selected(DHCPCD_WI_SCAN *)),
	    this, SLOT(connectSsid(DHCPCD_WI_SCAN *)));
}

void DhcpcdWi::createMenu1(QMenu *menu)
{
	DHCPCD_WI_SCAN *scan;

	for (scan = scans; scan; scan = scan->next)
		createMenuItem(menu, scan);
}

void DhcpcdWi::createMenu(QMenu *menu)
{

	this->menu = menu;
	createMenu1(menu);
}

QMenu *DhcpcdWi::createIfMenu(QMenu *parent)
{
	DHCPCD_IF *ifp;
	QIcon icon;

	ifp = dhcpcd_wpa_if(wpa);
	menu = new DhcpcdIfMenu(ifp, parent);
	icon = DhcpcdQt::getIcon("devices", "network-wireless");
	menu->setIcon(icon);
	createMenu1(menu);
	return menu;
}

void DhcpcdWi::wpaOpen()
{
	int fd = dhcpcd_wpa_open(wpa);
	static int last_error;

	if (fd == -1) {
		if (errno != last_error) {
			last_error = errno;
			qCritical("%s: dhcpcd_wpa_open: %s",
			    dhcpcd_wpa_if(wpa)->ifname,
			    strerror(last_error));
		}
		return;
	}

	notifier = new QSocketNotifier(fd, QSocketNotifier::Read);
	connect(notifier, SIGNAL(activated(int)), this, SLOT(dispatch()));
	if (retryOpenTimer) {
		delete retryOpenTimer;
		retryOpenTimer = NULL;
	}
}

void DhcpcdWi::dispatch()
{

	if (dhcpcd_wpa_get_fd(wpa) == -1) {
		delete notifier;
		notifier = NULL;
		DHCPCD_IF *i = dhcpcd_wpa_if(wpa);
		if (i == NULL ||
		    strcmp(i->reason, "DEPARTED") == 0 ||
		    strcmp(i->reason, "STOPPED") == 0)
			return;
		qWarning("%s: %s",
		    i->ifname,
		    qPrintable(tr("dhcpcd WPA connection lost")));
		if (retryOpenTimer == NULL) {
			retryOpenTimer = new QTimer(this);
			connect(retryOpenTimer, SIGNAL(timeout()),
			    this, SLOT(wpaOpen()));
			retryOpenTimer->start(DHCPCD_RETRYOPEN);
		}
		return;
	}

	dhcpcd_wpa_dispatch(wpa);
}

void DhcpcdWi::connectSsid(DHCPCD_WI_SCAN *scan)
{
	bool ok;
	DHCPCD_WI_SCAN s;

	/* Take a copy of scan incase it's destroyed by a scan update */
	memcpy(&s, scan, sizeof(s));
	s.next = NULL;

	QString psk = QInputDialog::getText(dhcpcdQt, s.ssid,
	    tr("Pre Shared key"), QLineEdit::Normal, NULL, &ok);

	if (!ok)
		return;

	QString errt;

	switch (dhcpcd_wpa_configure_psk(wpa, &s, psk.toAscii())) {
	case DHCPCD_WPA_SUCCESS:
		return;
	case DHCPCD_WPA_ERR_SET:
		errt = tr("Failed to set key management.");
		break;
	case DHCPCD_WPA_ERR_SET_PSK:
		errt = tr("Failed to set password, probably too short.");
		break;
	case DHCPCD_WPA_ERR_ENABLE:
		errt = tr("Failed to enable the network.");
		break;
	case DHCPCD_WPA_ERR_ASSOC:
		errt = tr("Failed to start association.");
		break;
	case DHCPCD_WPA_ERR_WRITE:
		errt = tr("Failed to save wpa_supplicant configuration.\n\nYou should add update_config=1 to /etc/wpa_supplicant.conf.");
		break;
	default:
		errt = strerror(errno);
		break;
	}

	QMessageBox::critical(dhcpcdQt, tr("Error setting wireless properties"),
	    errt);
}
