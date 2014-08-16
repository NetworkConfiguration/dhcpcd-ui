#include <QtGui>

#include "dhcpcd-qt.h"

int
main(int argc, char **argv)
{

	QApplication app(argc, argv);

	if (!QSystemTrayIcon::isSystemTrayAvailable()) {
		QMessageBox::critical(0, QObject::tr("Systray"),
		    QObject::tr("No system tray available"));
		return EXIT_FAILURE;
	}

	QApplication::setQuitOnLastWindowClosed(false);

	DhcpcdQt dhcpcdQt;
	return app.exec();
}
