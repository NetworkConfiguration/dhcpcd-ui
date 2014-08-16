#include <QWidget>

#include "dhcpcd.h"

class QRadioButton;
class QLabel;
class QProgressBar;

class DhcpcdSsidMenu : public QWidget
{
	Q_OBJECT

public:
	DhcpcdSsidMenu(QWidget *parent, DHCPCD_IF *ifp, DHCPCD_WI_SCAN *scan);
	~DhcpcdSsidMenu() {};

signals:
	void selected(DHCPCD_IF *ifp, DHCPCD_WI_SCAN *scan);

private slots:
	bool eventFilter(QObject *obj, QEvent *event);

private:
	DHCPCD_IF *ifp;
	DHCPCD_WI_SCAN *scan;

	QRadioButton *button;
	QLabel *licon;
	QProgressBar *bar;
};
