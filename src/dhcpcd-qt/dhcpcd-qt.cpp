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

#include <QCursor>
#include <QDebug>
#include <QList>
#include <QSocketNotifier>
#include <QtGui>

#include <cerrno>

#include "config.h"
#include "dhcpcd-qt.h"
#include "dhcpcd-about.h"
#include "dhcpcd-preferences.h"
#include "dhcpcd-wi.h"
#include "dhcpcd-ifmenu.h"
#include "dhcpcd-ssidmenu.h"

#ifdef NOTIFY
#include <knotification.h>
#endif

DhcpcdQt::DhcpcdQt()
{

	createActions();
	createTrayIcon();

	onLine = carrier = false;
	lastStatus = NULL;
	aniTimer = new QTimer(this);
	connect(aniTimer, SIGNAL(timeout()), this, SLOT(animate()));
	notifier = NULL;
	retryOpenTimer = NULL;

	about = NULL;
	preferences = NULL;

	wis = new QList<DhcpcdWi *>();
	ssidMenu = NULL;

	qDebug("%s", "Connecting ...");
	con = dhcpcd_new();
	if (con == NULL) {
		qCritical("libdhcpcd: %s", strerror(errno));
		exit(EXIT_FAILURE);
		return;
	}
	dhcpcd_set_progname(con, "dhcpcd-qt");
	dhcpcd_set_status_callback(con, dhcpcd_status_cb, this);
	dhcpcd_set_if_callback(con, dhcpcd_if_cb, this);
	dhcpcd_wpa_set_scan_callback(con, dhcpcd_wpa_scan_cb, this);
	dhcpcd_wpa_set_status_callback(con, dhcpcd_wpa_status_cb, this);
	tryOpen();
}

DhcpcdQt::~DhcpcdQt()
{

	/* This will have already been destroyed,
	 * but the reference may not be. */
	ssidMenu = NULL;

	if (con != NULL) {
		dhcpcd_close(con);
		dhcpcd_free(con);
	}

	free(lastStatus);

	qDeleteAll(*wis);
	delete wis;

}

DHCPCD_CONNECTION *DhcpcdQt::getConnection()
{

	return con;
}

QList<DhcpcdWi *> *DhcpcdQt::getWis()
{

	return wis;
}

void DhcpcdQt::animate()
{
	const char *icon;

	if (onLine) {
		if (aniCounter++ > 6) {
			aniTimer->stop();
			aniCounter = 0;
			return;
		}

		if (aniCounter % 2 == 0)
			icon = "network-idle";
		else
			icon = "network-transmit-receive";
	} else {
		switch(aniCounter++) {
		case 0:
			icon = "network-transmit";
			break;
		case 1:
			icon = "network-receive";
			break;
		default:
			icon = "network-idle";
			aniCounter = 0;
		}
	}

	setIcon("status", icon);
}

void DhcpcdQt::updateOnline(bool showIf)
{
	bool isOn, isCarrier;
	char *msg;
	DHCPCD_IF *ifs, *i;
	QString msgs;

	isOn = isCarrier = false;
	ifs = dhcpcd_interfaces(con);
	for (i = ifs; i; i = i->next) {
		if (strcmp(i->type, "link") == 0) {
			if (i->up)
				isCarrier = true;
		} else {
			if (i->up)
				isOn = true;
		}
		msg = dhcpcd_if_message(i, NULL);
		if (msg) {
			if (showIf)
				qDebug() << msg;
			if (msgs.isEmpty())
				msgs = QString::fromAscii(msg);
			else
				msgs += '\n' + QString::fromAscii(msg);
			free(msg);
		} else if (showIf)
			qDebug() << i->ifname << i->reason;
	}

	if (onLine != isOn || carrier != isCarrier) {
		onLine = isOn;
		carrier = isCarrier;
		aniTimer->stop();
		aniCounter = 0;
		if (isOn) {
			animate();
			aniTimer->start(300);
		} else if (isCarrier) {
			animate();
			aniTimer->start(500);
		} else
			setIcon("status", "network-offline");
	}

	trayIcon->setToolTip(msgs);
}

void DhcpcdQt::statusCallback(const char *status)
{

	qDebug("Status changed to %s", status);
	if (strcmp(status, "down") == 0) {
		aniTimer->stop();
		aniCounter = 0;
		onLine = carrier = false;
		setIcon("status", "network-offline");
		trayIcon->setToolTip(tr("Not connected to dhcpcd"));
		/* Close down everything */
		if (notifier) {
			delete notifier;
			notifier = NULL;
		}
		if (ssidMenu) {
			delete ssidMenu;
			ssidMenu = NULL;
		}
		if (preferences) {
			delete preferences;
			preferences = NULL;
		}
		preferencesAction->setEnabled(false);
	} else {
		bool refresh;

		if (lastStatus == NULL || strcmp(lastStatus, "down") == 0) {
			qDebug("Connected to dhcpcd-%s", dhcpcd_version(con));
			refresh = true;
		} else
			refresh = strcmp(lastStatus, "opened") ? false : true;
		updateOnline(refresh);
	}

	free(lastStatus);
	lastStatus = strdup(status);

	if (strcmp(status, "down") == 0) {
		if (retryOpenTimer == NULL) {
			retryOpenTimer = new QTimer(this);
			connect(retryOpenTimer, SIGNAL(timeout()),
			    this, SLOT(tryOpen()));
			retryOpenTimer->start(DHCPCD_RETRYOPEN);
		}
	}
}

void DhcpcdQt::dhcpcd_status_cb(_unused DHCPCD_CONNECTION *con,
    const char *status, void *d)
{
	DhcpcdQt *dhcpcdQt = (DhcpcdQt *)d;

	dhcpcdQt->statusCallback(status);
}

void DhcpcdQt::ifCallback(DHCPCD_IF *i)
{
	char *msg;
	bool new_msg;

	if (strcmp(i->reason, "RENEW") &&
	    strcmp(i->reason, "STOP") &&
	    strcmp(i->reason, "STOPPED"))
	{
		msg = dhcpcd_if_message(i, &new_msg);
		if (msg) {
			qDebug("%s", msg);
			if (new_msg) {
				QSystemTrayIcon::MessageIcon icon =
				    i->up ? QSystemTrayIcon::Information :
				    QSystemTrayIcon::Warning;
				QString t = tr("Network Event");
				QString m = msg;
				notify(t, m, icon);
			}
			free(msg);
		}
	}

	updateOnline(false);

	if (i->wireless) {
		for (auto &wi : *wis) {
			DHCPCD_WPA *wpa = wi->getWpa();
			if (dhcpcd_wpa_if(wpa) == i) {
				DHCPCD_WI_SCAN *scans;

				scans = dhcpcd_wi_scans(i);
				processScans(wi, scans);
			}
		}
	}
	
}

void DhcpcdQt::dhcpcd_if_cb(DHCPCD_IF *i, void *d)
{
	DhcpcdQt *dhcpcdQt = (DhcpcdQt *)d;

	dhcpcdQt->ifCallback(i);
}

DhcpcdWi *DhcpcdQt::findWi(DHCPCD_WPA *wpa)
{

	for (auto &wi : *wis) {
		if (wi->getWpa() == wpa)
			return wi;
	}
	return NULL;
}

void DhcpcdQt::processScans(DhcpcdWi *wi, DHCPCD_WI_SCAN *scans)
{
	DHCPCD_WI_SCAN *s1, *s2;

	QString title = tr("New Access Point");
	QString txt;
	for (s1 = scans; s1; s1 = s1->next) {
		for (s2 = wi->getScans(); s2; s2 = s2->next) {
			if (strcmp(s1->ssid, s2->ssid) == 0)
				break;
		}
		if (s2 == NULL) {
			if (!txt.isEmpty()) {
				title = tr("New Access Points");
				txt += '\n';
			}
			txt += s1->ssid;
		}
	}
	if (!txt.isEmpty() &&
	    (ssidMenu == NULL || !ssidMenu->isVisible()))
		notify(title, txt);

	wi->setScans(scans);
	if (ssidMenu && ssidMenu->isVisible())
		ssidMenu->popup(ssidMenuPos);
}

void DhcpcdQt::scanCallback(DHCPCD_WPA *wpa)
{
	DHCPCD_WI_SCAN *scans;
	int fd = dhcpcd_wpa_get_fd(wpa);
	DhcpcdWi *wi;

	wi = findWi(wpa);
	if (fd == -1) {
		qCritical("No fd for WPA");
		if (wi) {
			wis->removeOne(wi);
			delete wi;
		}
		return;
	}

	DHCPCD_IF *i = dhcpcd_wpa_if(wpa);
	if (i == NULL) {
		qCritical("No interface for WPA");
		if (wi) {
			wis->removeOne(wi);
			delete wi;
		}
		return;
	}

	qDebug("%s: Received scan results", i->ifname);
	scans = dhcpcd_wi_scans(i);
	if (wi == NULL) {
		wi = new DhcpcdWi(this, wpa);
		wis->append(wi);
		wi->setScans(scans);
	} else
		processScans(wi, scans);

}

void DhcpcdQt::dhcpcd_wpa_scan_cb(DHCPCD_WPA *wpa, void *d)
{
	DhcpcdQt *dhcpcdQt = (DhcpcdQt *)d;

	dhcpcdQt->scanCallback(wpa);
}

void DhcpcdQt::wpaStatusCallback(DHCPCD_WPA *wpa, const char *status)
{
	DHCPCD_IF *i;

	i = dhcpcd_wpa_if(wpa);
	qDebug("%s: WPA status %s", i->ifname, status);
	if (strcmp(status, "down") == 0) {
		DhcpcdWi *wi = findWi(wpa);
		if (wi) {
			wis->removeOne(wi);
			delete wi;
		}
	}
}

void DhcpcdQt::dhcpcd_wpa_status_cb(DHCPCD_WPA *wpa, const char *status,
    void *d)
{
	DhcpcdQt *dhcpcdQt = (DhcpcdQt *)d;

	dhcpcdQt->wpaStatusCallback(wpa, status);
}

void DhcpcdQt::tryOpen() {
	int fd = dhcpcd_open(con, true);
	static int last_error;

	if (fd == -1) {
		if (errno == EACCES || errno == EPERM) {
			if ((fd = dhcpcd_open(con, false)) != -1)
				goto unprived;
		}
		if (errno != last_error) {
		        last_error = errno;
			const char *errt = strerror(errno);
			qCritical("dhcpcd_open: %s", errt);
			trayIcon->setToolTip(
			    tr("Error connecting to dhcpcd: %1").arg(errt));
		}
		if (retryOpenTimer == NULL) {
			retryOpenTimer = new QTimer(this);
			connect(retryOpenTimer, SIGNAL(timeout()),
			    this, SLOT(tryOpen()));
			retryOpenTimer->start(DHCPCD_RETRYOPEN);
		}
		return;
	}

unprived:
	/* Start listening to WPA events */
	dhcpcd_wpa_start(con);

	if (retryOpenTimer) {
		delete retryOpenTimer;
		retryOpenTimer = NULL;
	}

	notifier = new QSocketNotifier(fd, QSocketNotifier::Read);
	connect(notifier, SIGNAL(activated(int)), this, SLOT(dispatch()));

	preferencesAction->setEnabled(dhcpcd_privileged(con));
}

void DhcpcdQt::dispatch() {

	if (dhcpcd_get_fd(con) == -1) {
		qWarning("dhcpcd connection lost");
		return;
	}

	dhcpcd_dispatch(con);
}

void DhcpcdQt::notify(QString &title, QString &msg,
#ifdef NOTIFY
    QSystemTrayIcon::MessageIcon
#else
    QSystemTrayIcon::MessageIcon icon
#endif
    )
{

#ifdef NOTIFY
	KNotification *n = new KNotification("event", this);
	n->setTitle(title);
	n->setText(msg);
	n->sendEvent();
#else
	trayIcon->showMessage(title, msg, icon);
#endif
}


void DhcpcdQt::closeEvent(QCloseEvent *event)
{

	if (trayIcon->isVisible()) {
		hide();
		event->ignore();
	}
}

QIcon DhcpcdQt::getIcon(QString category, QString name)
{
	QIcon icon;

	if (QIcon::hasThemeIcon(name))
		icon = QIcon::fromTheme(name);
	else
		icon = QIcon(ICONDIR "/hicolor/scalable/" + category + "/" + name + ".svg");

	return icon;
}

void DhcpcdQt::setIcon(QString category, QString name)
{
	QIcon icon = getIcon(category, name);

	trayIcon->setIcon(icon);
}

QIcon DhcpcdQt::icon()
{

	return getIcon("status", "network-transmit-receive");
}

void DhcpcdQt::createSsidMenu()
{

	if (ssidMenu) {
		delete ssidMenu;
		ssidMenu = NULL;
	}
	if (wis->size() == 0)
		return;

	ssidMenu = new QMenu(this);
	if (wis->size() == 1)
		wis->first()->createMenu(ssidMenu);
	else {
		for (auto &wi : *wis)
			ssidMenu->addMenu(wi->createIfMenu(ssidMenu));
	}
	ssidMenuPos = QCursor::pos();
	ssidMenu->popup(ssidMenuPos);
}

void DhcpcdQt::iconActivated(QSystemTrayIcon::ActivationReason reason)
{

	if (reason == QSystemTrayIcon::Trigger)
		createSsidMenu();
}

void DhcpcdQt::dialogClosed(QDialog *dialog)
{

	if (dialog == about)
		about = NULL;
	else if (dialog == preferences)
		preferences = NULL;
}

void DhcpcdQt::showPreferences()
{

	if (preferences == NULL) {
		preferences = new DhcpcdPreferences(this);
		preferences->show();
	} else
		preferences->activateWindow();
}

void DhcpcdQt::showAbout()
{

	if (about == NULL) {
		about = new DhcpcdAbout(this);
		about->show();
	} else
		about->activateWindow();
}

void DhcpcdQt::createActions()
{

	preferencesAction = new QAction(tr("&Preferences"), this);
	preferencesAction->setIcon(QIcon::fromTheme("preferences-system-network"));
	preferencesAction->setEnabled(false);
	connect(preferencesAction, SIGNAL(triggered()),
	    this, SLOT(showPreferences()));

	aboutAction = new QAction(tr("&About"), this);
	aboutAction->setIcon(QIcon::fromTheme("help-about"));
	connect(aboutAction, SIGNAL(triggered()), this, SLOT(showAbout()));

	quitAction = new QAction(tr("&Quit"), this);
	quitAction->setIcon(QIcon::fromTheme("application-exit"));
	connect(quitAction, SIGNAL(triggered()), qApp, SLOT(quit()));

}

void DhcpcdQt::createTrayIcon()
{

        trayIconMenu = new QMenu(this);
	trayIconMenu->addAction(preferencesAction);
	trayIconMenu->addSeparator();
	trayIconMenu->addAction(aboutAction);
	trayIconMenu->addAction(quitAction);

	trayIcon = new QSystemTrayIcon(this);
	setIcon("status", "network-offline");
	trayIcon->setContextMenu(trayIconMenu);

	connect(trayIcon, SIGNAL(activated(QSystemTrayIcon::ActivationReason)),
	    this, SLOT(iconActivated(QSystemTrayIcon::ActivationReason)));

	trayIcon->show();
}
