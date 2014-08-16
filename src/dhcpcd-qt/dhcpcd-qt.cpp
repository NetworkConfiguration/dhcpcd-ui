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
#include "dhcpcd-ssidmenu.h"

DhcpcdQt::DhcpcdQt()
{

	createActions();
	createTrayIcon();

	onLine = carrier = false;
	lastStatus = NULL;
	aniTimer = new QTimer(this);
	connect(aniTimer, SIGNAL(timeout()), this, SLOT(animate()));
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
	dhcpcd_set_status_callback(con, dhcpcd_status_cb, this);
	dhcpcd_set_if_callback(con, dhcpcd_if_cb, this);
	dhcpcd_wpa_set_scan_callback(con, dhcpcd_wpa_scan_cb, this);
	tryOpen();

}

DhcpcdQt::~DhcpcdQt()
{

	qDeleteAll(*wis);
	delete wis;

	free(lastStatus);

	if (con != NULL) {
		dhcpcd_close(con);
		dhcpcd_free(con);
	}
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
		QString msg;
		if (lastStatus)
			msg = tr("Connection to dhcpcd lost");
		else
			msg = tr("dhcpcd not running");
		aniTimer->stop();
		aniCounter = 0;
		setIcon("status", "network-offline");
	} else {
		bool refresh;

		if ((lastStatus == NULL || strcmp(lastStatus, "down") == 0)) {
			qDebug("Connected to dhcpcd-%s", dhcpcd_version(con));
			refresh = true;
		} else
			refresh = false;
		updateOnline(refresh);
	}

	free(lastStatus);
	lastStatus = strdup(status);
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

	updateOnline(false);

	if (strcmp(i->reason, "RENEW") == 0 ||
	    strcmp(i->reason, "STOP") == 0 ||
	    strcmp(i->reason, "STOPPED") == 0)
		return;

	msg = dhcpcd_if_message(i, &new_msg);
	if (msg) {
		qDebug("%s", msg);
		if (new_msg) {
			QSystemTrayIcon::MessageIcon icon =
			    i->up ? QSystemTrayIcon::Information :
			    QSystemTrayIcon::Warning;
			trayIcon->showMessage(tr("Network Event"), msg, icon);
		}
		free(msg);
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

void DhcpcdQt::scanCallback(DHCPCD_WPA *wpa)
{
	DHCPCD_WI_SCAN *scans, *s1, *s2;
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
	} else {
		QString title = tr("New Access Point");
		QString txt;
		for (s1 = scans; s1; s1 = s1->next) {
			for (s2 = wi->getScans(); s2; s2 = s2->next) {
				if (strcmp(s1->ssid, s2->ssid) == 0)
					break;
				if (s2 == NULL) {
					if (!txt.isEmpty()) {
						title = tr("New Access Points");
						txt += '\n';
					}
					txt += s1->ssid;
				}
			}
		}
		if (!txt.isEmpty())
			notify(title, txt);
	}
	wi->setScans(scans);
}

void DhcpcdQt::dhcpcd_wpa_scan_cb(DHCPCD_WPA *wpa, void *d)
{
	DhcpcdQt *dhcpcdQt = (DhcpcdQt *)d;

	dhcpcdQt->scanCallback(wpa);
}

bool DhcpcdQt::tryOpen() {
	int fd = dhcpcd_open(con);
	static int last_error;

	if (fd == -1) {
		if (errno != last_error) {
		        last_error = errno;
			qCritical("dhcpcd_open: %s", strerror(errno));
		}
		if (retryOpenTimer == NULL) {
			retryOpenTimer = new QTimer(this);
			connect(retryOpenTimer, SIGNAL(timeout()),
			    this, SLOT(tryOpen()));
			retryOpenTimer->start(DHCPCD_RETRYOPEN);
		}
		return false;
	}

	if (retryOpenTimer) {
		delete retryOpenTimer;
		retryOpenTimer = NULL;
	}

	notifier = new QSocketNotifier(fd, QSocketNotifier::Read);
	connect(notifier, SIGNAL(activated(int)), this, SLOT(dispatch()));

	return true;
}

void DhcpcdQt::dispatch() {

	if (dhcpcd_get_fd(con) == -1) {
		qWarning("dhcpcd connection lost");
		return;
	}

	dhcpcd_dispatch(con);
}

void DhcpcdQt::notify(QString &title, QString &msg,
    QSystemTrayIcon::MessageIcon icon)
{

	qDebug("%s", qPrintable(msg));
	trayIcon->showMessage(title, msg, icon);
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

void DhcpcdQt::addSsidMenu(QMenu *&menu, DHCPCD_IF *ifp, DhcpcdWi *&wi)
{
	DHCPCD_WI_SCAN *scan;

	for (scan = wi->getScans(); scan; scan = scan->next) {
		QWidgetAction *wa = new QWidgetAction(menu);
		DhcpcdSsidMenu *ssidMenu = new DhcpcdSsidMenu(menu, ifp, scan);
		wa->setDefaultWidget(ssidMenu);
		menu->addAction(wa);
		connect(ssidMenu, SIGNAL(selected(DHCPCD_IF *, DHCPCD_WI_SCAN *)),
		    this, SLOT(connectSsid(DHCPCD_IF *, DHCPCD_WI_SCAN *)));
	}
}

void DhcpcdQt::connectSsid(DHCPCD_IF *, DHCPCD_WI_SCAN *)
{

	QMessageBox::information(this, "Not implemented",
	    "SSID selection is not yet implemented");
}

void DhcpcdQt::createSsidMenu()
{
	DHCPCD_WPA *wpa;
	DHCPCD_IF *ifp;

	if (ssidMenu) {
		delete ssidMenu;
		ssidMenu = NULL;
	}
	if (wis->size() == 0)
		return;

	ssidMenu = new QMenu(this);
	if (wis->size() == 1) {
		DhcpcdWi *wi = wis->first();
		wpa = wi->getWpa();
		ifp = dhcpcd_wpa_if(wpa);
		addSsidMenu(ssidMenu, ifp, wi);
	} else {
		for (auto &wi : *wis) {
			wpa = wi->getWpa();
			ifp = dhcpcd_wpa_if(wpa);
			if (ifp) {
				QMenu *ifmenu = ssidMenu->addMenu(ifp->ifname);
				addSsidMenu(ifmenu, ifp, wi);
			}
		}
	}
	ssidMenu->popup(QCursor::pos());
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
