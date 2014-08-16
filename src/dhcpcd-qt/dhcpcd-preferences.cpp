#include <QDialog>
#include <QIcon>
#include <QLabel>
#include <QPixmap>
#include <QPushButton>
#include <QVBoxLayout>

#include "config.h"
#include "dhcpcd-preferences.h"
#include "dhcpcd-qt.h"

DhcpcdPreferences::DhcpcdPreferences(DhcpcdQt *parent)
    : QDialog(NULL)
{
	QVBoxLayout *layout;

	this->parent = parent;
	resize(300, 200);
	setWindowTitle("dhcpcd-qt prefs");
	layout = new QVBoxLayout(this);

	notLabel = new QLabel("<h1>Not implemented yet</h1>", this);
	notLabel->setAlignment(Qt::AlignCenter);
	layout->addWidget(notLabel);
}

void DhcpcdPreferences::closeEvent(QCloseEvent *e)
{

	parent->dialogClosed(this);
	QDialog::closeEvent(e);
}
