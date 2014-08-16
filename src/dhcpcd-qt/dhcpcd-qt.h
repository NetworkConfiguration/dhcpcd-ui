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
class QPushButton;
class QSocketNotifier;
class QTimer;

class DhcpcdQt : public QWidget
{
	Q_OBJECT

public:
	DhcpcdQt();
	~DhcpcdQt();

	void closeAbout();

	static void dhcpcd_status_cb(DHCPCD_CONNECTION *con,
	    const char *status, void *d);
	void statusCallback(const char *status);
	static void dhcpcd_if_cb(DHCPCD_IF *i, void *d);
	void ifCallback(DHCPCD_IF *i);

	static void dhcpcd_wpa_scan_cb(DHCPCD_WPA *wpa, void *d);
	void scanCallback(DHCPCD_WPA *wpa);

	static QIcon getIcon(QString category, QString name);

	void dialogClosed(QDialog *dialog);

protected:
	void closeEvent(QCloseEvent *event);

private slots:
	void animate();
	void dispatch();
	void showAbout();
	void showPreferences();
	void iconActivated(QSystemTrayIcon::ActivationReason reason);

	void connectSsid(DHCPCD_IF *ifp, DHCPCD_WI_SCAN *scan);

private:
	DHCPCD_CONNECTION *con;
	bool tryOpen();
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

	void notify(QString &title, QString &msg,
	    QSystemTrayIcon::MessageIcon icon = QSystemTrayIcon::Information);
};

#endif
