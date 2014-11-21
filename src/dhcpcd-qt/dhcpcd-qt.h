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

#ifndef DHCPCD_QT_H
#define DHCPCD_QT_H

#include <QSystemTrayIcon>
#include <QWidget>

#include "dhcpcd.h"
#include "dhcpcd-wi.h"

#ifdef __GNUC__
#  define _unused __attribute__((__unused__))
#else
#  define _unused
#endif

class QAction;
class QDialog;
class QLabel;
class QMenu;
class QPoint;
class QPushButton;
class QSocketNotifier;
class QTimer;

class DhcpcdQt : public QWidget
{
	Q_OBJECT

public:
	DhcpcdQt();
	~DhcpcdQt();

	DHCPCD_CONNECTION *getConnection();
	static void dhcpcd_status_cb(DHCPCD_CONNECTION *con,
	    const char *status, void *d);
	void statusCallback(const char *status);
	static void dhcpcd_if_cb(DHCPCD_IF *i, void *d);
	void ifCallback(DHCPCD_IF *i);

	static void dhcpcd_wpa_scan_cb(DHCPCD_WPA *wpa, void *d);
	void scanCallback(DHCPCD_WPA *wpa);
	static void dhcpcd_wpa_status_cb(DHCPCD_WPA *wpa, const char *status,
	    void *d);
	void wpaStatusCallback(DHCPCD_WPA *wpa, const char *status);

	static QIcon getIcon(QString category, QString name);
	QList<DhcpcdWi *> *getWis();

	void closeAbout();
	void dialogClosed(QDialog *dialog);

protected:
	void closeEvent(QCloseEvent *event);

private slots:
	void tryOpen();
	void animate();
	void dispatch();
	void showAbout();
	void showPreferences();
	void iconActivated(QSystemTrayIcon::ActivationReason reason);

private:
	DHCPCD_CONNECTION *con;
	QSocketNotifier *notifier;
	QTimer *retryOpenTimer;
	QList<DhcpcdWi *> *wis;
	DhcpcdWi *findWi(DHCPCD_WPA *wpa);

	char *lastStatus;
	bool onLine;
	bool carrier;
	QTimer *aniTimer;
	int aniCounter;
	void updateOnline(bool showIf);

	QDialog *about;
	QDialog *preferences;

	void processScans(DhcpcdWi *wi, DHCPCD_WI_SCAN *scans);
	void addSsidMenu(QMenu *&menu, DHCPCD_IF *ifp, DhcpcdWi *&wi);
	void createSsidMenu();

	/* Tray Icon */
	void setIcon(QString category, QString name);
	QIcon icon();
	void createActions();
	void createTrayIcon();

	QSystemTrayIcon *trayIcon;
	QIcon *realTrayIcon;
	QAction *preferencesAction;
	QAction *quitAction;
	QAction *aboutAction;
	QMenu *trayIconMenu;
	QMenu *ssidMenu;
	QPoint ssidMenuPos;

	void notify(QString &title, QString &msg,
	    QSystemTrayIcon::MessageIcon icon = QSystemTrayIcon::Information);
};

#endif
