#include <QDialog>
#include <QIcon>
#include <QLabel>
#include <QPixmap>
#include <QPushButton>
#include <QVBoxLayout>

#include "config.h"
#include "dhcpcd-about.h"
#include "dhcpcd-qt.h"

DhcpcdAbout::DhcpcdAbout(DhcpcdQt *parent)
    : QDialog(NULL)
{
	QVBoxLayout *layout;

	this->parent = parent;
	resize(300, 200);
	setWindowTitle("About dhcpcd-qt");
	layout = new QVBoxLayout(this);

	QIcon icon = DhcpcdQt::getIcon("status", "network-transmit-receive");
	QPixmap picon = icon.pixmap(48, 48);
	iconLabel = new QLabel(this);
	iconLabel->setAlignment(Qt::AlignCenter);
	iconLabel->setPixmap(picon);
	layout->addWidget(iconLabel);

	aboutLabel = new QLabel("<h1>Network Configurator "  VERSION "</h1>", this);
	aboutLabel->setAlignment(Qt::AlignCenter);
	layout->addWidget(aboutLabel);
	partLabel = new QLabel("Part of the dhcpcd project", this);
	partLabel->setAlignment(Qt::AlignCenter);
	layout->addWidget(partLabel);
	copyrightLabel = new QLabel("Copyright (c) 2009-2014 Roy Marples", this);
	copyrightLabel->setAlignment(Qt::AlignCenter);
	layout->addWidget(copyrightLabel);
	urlLabel = new QLabel(
	    "<a href=\"http://roy.marples.name/projects/dhcpcd\">"
	    "dhcpcd Website"
	    "</a>",
	    this);
	urlLabel->setAlignment(Qt::AlignCenter);
	urlLabel->setOpenExternalLinks(true);
	layout->addWidget(urlLabel);

	closeButton = new QPushButton("Close", this);
	closeButton->setIcon(QIcon::fromTheme("window-close"));
	layout->addWidget(closeButton);
	connect(closeButton, SIGNAL(clicked()), this, SLOT(close()));
}

void DhcpcdAbout::closeEvent(QCloseEvent *e)
{

	parent->dialogClosed(this);
	QDialog::closeEvent(e);
}
