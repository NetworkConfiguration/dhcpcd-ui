#ifndef DHCPCD_ABOUT_H
#define DHCPCD_ABOUT_H

#include <QDialog>

class DhcpcdQt;
class QLabel;
class QPushButton;

class DhcpcdAbout : public QDialog
{
	Q_OBJECT

public:
	DhcpcdAbout(DhcpcdQt *parent = 0);

protected:
	void closeEvent(QCloseEvent *e);

private:
	DhcpcdQt *parent;
	QLabel *iconLabel;
	QLabel *aboutLabel;
	QLabel *partLabel;
	QLabel *copyrightLabel;
	QLabel *urlLabel;
	QPushButton *closeButton;
};

#endif
