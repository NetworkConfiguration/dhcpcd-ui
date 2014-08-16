#ifndef DHCPCD_PREFERENCES_H
#define DHCPCD_PREFERENCES_H

#include <QDialog>

class DhcpcdQt;
class QLabel;

class DhcpcdPreferences : public QDialog
{
	Q_OBJECT

public:
	DhcpcdPreferences(DhcpcdQt *parent = 0);

protected:
	void closeEvent(QCloseEvent *e);

private:
	DhcpcdQt *parent;
	QLabel *notLabel;
};

#endif
