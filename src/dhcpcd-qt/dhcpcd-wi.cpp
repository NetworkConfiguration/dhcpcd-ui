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

#include <QAction>
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
#include "dhcpcd-ssid.h"
#include "dhcpcd-ssidmenu.h"

DhcpcdWi::DhcpcdWi(DhcpcdQt *parent, DHCPCD_WPA *wpa)
{

	this->dhcpcdQt = parent;
	this->wpa = wpa;
	menu = NULL;
	scans = NULL;
	ssid = NULL;

	notifier = NULL;
	pingTimer = NULL;
#ifdef BG_SCAN
	scanTimer = NULL;
#endif
}

DhcpcdWi::~DhcpcdWi()
{

	if (menu) {
		dhcpcdQt->menuDeleted(menu);
		menu->setVisible(false);
		menu->deleteLater();
		menu = NULL;
	}

	if (notifier) {
		notifier->setEnabled(false);
		notifier->deleteLater();
		notifier = NULL;
	}

	if (pingTimer) {
		pingTimer->stop();
		pingTimer->deleteLater();
		pingTimer = NULL;
	}

	if (ssid) {
		ssid->reject();
		ssid->deleteLater();
		ssid = NULL;
	}

#ifdef BG_SCAN
	if (scanTimer) {
		scanTimer->stop();
		scanTimer->deleteLater();
		scanTimer = NULL;
	}
#endif

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
	bool changed = false;

	if (menu) {
		QList<DhcpcdSsidMenu*> lst;
		DHCPCD_WI_SCAN *scan;
		DHCPCD_IF *i;
		bool found, associated;
		QAction *before;

		i = dhcpcd_wpa_if(wpa);
		for (scan = scans; scan; scan = scan->next) {
			found = false;
			before = NULL;
			associated = dhcpcd_wi_associated(i, scan);

			lst = menu->findChildren<DhcpcdSsidMenu*>();
			foreach(DhcpcdSsidMenu *sm, lst) {
				DHCPCD_WI_SCAN *s = sm->getScan();

				if (strcmp(scan->ssid, s->ssid) == 0) {
					/* If association changes, remove
					 * the entry and re-create it
					 * so assoicates entries appear at
					 * the top */
					if (associated != sm->isAssociated()) {
						menu->removeAction(sm);
						break;
					}
					sm->setScan(scan);
					found = true;
					break;
				}
				if (!associated &&
				    dhcpcd_wi_scan_compare(scan, s) < 0)
					before = sm;
			}

			if (!found) {
				if (associated) {
					lst = menu->findChildren<DhcpcdSsidMenu*>();
					if (lst.empty())
						before = NULL;
					else
						before = lst.at(0);
				}
				createMenuItem(menu, scan, before);
				changed = true;
			}
		}

		lst = menu->findChildren<DhcpcdSsidMenu*>();
		foreach(DhcpcdSsidMenu *sm, lst) {
			DHCPCD_WI_SCAN *s = sm->getScan();
			for (scan = scans; scan; scan = scan->next) {
				if (strcmp(scan->ssid, s->ssid) == 0)
					break;
			}
			if (scan == NULL) {
				menu->removeAction(sm);
				changed = true;
			}
		}
	}

	dhcpcd_wi_scans_free(this->scans);
	this->scans = scans;

	return (changed && menu && menu->isVisible());
}

void DhcpcdWi::createMenuItem(QMenu *menu, DHCPCD_WI_SCAN *scan,
    QAction *before)
{
	DhcpcdSsidMenu *ssidMenu = new DhcpcdSsidMenu(menu, this, scan);
	menu->insertAction(before, ssidMenu);
	connect(ssidMenu, SIGNAL(triggered(DHCPCD_WI_SCAN *)),
	    this, SLOT(connectSsid(DHCPCD_WI_SCAN *)));
}

void DhcpcdWi::createMenu1(QMenu *menu)
{
	DHCPCD_IF *i;
	DHCPCD_WI_SCAN *scan;
	QAction *before;

#ifdef BG_SCAN
	connect(menu, SIGNAL(aboutToShow()), this, SLOT(menuShown()));
	connect(menu, SIGNAL(aboutToHide()), this, SLOT(menuHidden()));
#endif

	i = dhcpcd_wpa_if(wpa);
	for (scan = scans; scan; scan = scan->next) {
		before = NULL;
		if (dhcpcd_wi_associated(i, scan)) {
			QList<DhcpcdSsidMenu*> lst;

			lst = menu->findChildren<DhcpcdSsidMenu*>();
			if (!lst.empty())
				before = lst.at(0);
		}
		createMenuItem(menu, scan, before);
	}
}

void DhcpcdWi::createMenu(QMenu *menu)
{

	if (this->menu && this->menu != menu)
		this->menu->deleteLater();
	this->menu = menu;
	createMenu1(menu);
}

QMenu *DhcpcdWi::createIfMenu(QMenu *parent)
{
	DHCPCD_IF *ifp;
	QIcon icon;

	ifp = dhcpcd_wpa_if(wpa);
	if (this->menu)
		this->menu->deleteLater();
	menu = new DhcpcdIfMenu(ifp, parent);
	icon = DhcpcdQt::getIcon("devices", "network-wireless");
	menu->setIcon(icon);
	createMenu1(menu);
	return menu;
}

bool DhcpcdWi::open()
{
	int fd = dhcpcd_wpa_open(wpa);

	if (fd == -1) {
		qCritical("%s: dhcpcd_wpa_open: %s",
		    dhcpcd_wpa_if(wpa)->ifname,
		    strerror(errno));
		dhcpcd_wpa_close(wpa);
		return false;
	}

	notifier = new QSocketNotifier(fd, QSocketNotifier::Read);
	connect(notifier, SIGNAL(activated(int)), this, SLOT(dispatch()));
	pingTimer = new QTimer(this);
	connect(pingTimer, SIGNAL(timeout()), this, SLOT(ping()));
	pingTimer->start(DHCPCD_WPA_PING);
#ifdef BG_SCAN
	scanTimer = new QTimer(this);
	connect(scanTimer, SIGNAL(timeout()), this, SLOT(scan()));
	scanTimer->start(DHCPCD_WPA_SCAN_LONG);
#endif
	return true;
}

void DhcpcdWi::dispatch()
{

	dhcpcd_wpa_dispatch(wpa);
}

void DhcpcdWi::ping()
{

	if (!dhcpcd_wpa_ping(wpa))
		dhcpcd_wpa_close(wpa);
}

void DhcpcdWi::connectSsid(DHCPCD_WI_SCAN *scan)
{
	DHCPCD_WI_SCAN s;
	int err;

	/* Take a copy of scan incase it's destroyed by a scan update */
	memcpy(&s, scan, sizeof(s));
	s.next = NULL;

	if (s.flags & WSF_PSK) {
		bool ok;
		QString pwd;

		ssid = new DhcpcdSsid(this, &s);
		pwd = ssid->getPsk(&ok);
		ssid->deleteLater();
		ssid = NULL;
		if (!ok)
			return;
		if (pwd.isNull() || pwd.isEmpty())
			err = dhcpcd_wpa_select(wpa, &s);
		else
			err = dhcpcd_wpa_configure(wpa, &s, pwd.toAscii());
	} else
		err = dhcpcd_wpa_configure(wpa, &s, NULL);

	QString errt;
	switch (err) {
	case DHCPCD_WPA_SUCCESS:
		return;
	case DHCPCD_WPA_ERR_DISCONN:
		errt = tr("Failed to disconnect.");
		break;
	case DHCPCD_WPA_ERR_RECONF:
		errt = tr("Faile to reconfigure.");
		break;
	case DHCPCD_WPA_ERR_SET:
		errt = tr("Failed to set key management.");
		break;
	case DHCPCD_WPA_ERR_SET_PSK:
		errt = tr("Failed to set password, probably too short.");
		break;
	case DHCPCD_WPA_ERR_ENABLE:
		errt = tr("Failed to enable the network.");
		break;
	case DHCPCD_WPA_ERR_SELECT:
		errt = tr("Failed to select the network.");
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

#ifdef BG_SCAN
void DhcpcdWi::scan()
{

	dhcpcd_wpa_scan(wpa);
}

void DhcpcdWi::menuHidden()
{

	if (scanTimer) {
		scanTimer->stop();
		scanTimer->start(DHCPCD_WPA_SCAN_LONG);
	}
}

void DhcpcdWi::menuShown()
{

	if (scanTimer) {
		scanTimer->stop();
		scanTimer->start(DHCPCD_WPA_SCAN_SHORT);
	}
}
#endif
