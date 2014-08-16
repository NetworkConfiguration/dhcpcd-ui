#ifndef DHCPCD_WI_H
#define DHCPCD_WI_H

#include <QObject>

#include "dhcpcd.h"

class DhcpcdQt;
class QSocketNotifier;
class QTimer;

class DhcpcdWi : public QObject
{
	Q_OBJECT

public:
	DhcpcdWi(DhcpcdQt *dhcpcdQt, DHCPCD_WPA *wpa);
	~DhcpcdWi();
	DHCPCD_WPA *getWpa();

	DHCPCD_WI_SCAN *getScans();
	void setScans(DHCPCD_WI_SCAN *scans);

private slots:
	void dispatch();
	void wpaOpen();

private:
	DhcpcdQt *dhcpcdQt;
	DHCPCD_WPA *wpa;
	DHCPCD_WI_SCAN *scans;

	QSocketNotifier *notifier;
	QTimer *retryOpenTimer;
};

#endif
