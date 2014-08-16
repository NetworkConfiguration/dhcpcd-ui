#include <QObject>
#include <QSocketNotifier>
#include <QTimer>

#include <cerrno>

#include "config.h"
#include "dhcpcd-wi.h"

DhcpcdWi::DhcpcdWi(DhcpcdQt *parent, DHCPCD_WPA *wpa)
{

	this->dhcpcdQt = parent;
	this->wpa = wpa;
	scans = NULL;

	int fd = dhcpcd_wpa_get_fd(wpa);
	notifier = new QSocketNotifier(fd, QSocketNotifier::Read);
	connect(notifier, SIGNAL(activated(int)), this, SLOT(dispatch()));
	retryOpenTimer = NULL;
}

DhcpcdWi::~DhcpcdWi()
{

	dhcpcd_wi_scans_free(scans);
	if (notifier != NULL)
		delete notifier;
}

DHCPCD_WPA * DhcpcdWi::getWpa()
{

	return wpa;
}

DHCPCD_WI_SCAN *DhcpcdWi::getScans()
{

	return scans;
}
void DhcpcdWi::setScans(DHCPCD_WI_SCAN *scans)
{

	dhcpcd_wi_scans_free(this->scans);
	this->scans = scans;
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
