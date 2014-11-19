/*
 * dhcpcd-qt
 * Copyright 2014 Roy Marples <roy@marples.name>
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <QDialog>
#include <QHBoxLayout>
#include <QIcon>
#include <QLabel>
#include <QPixmap>
#include <QPushButton>
#include <QSpacerItem>
#include <QVBoxLayout>

#include "config.h"
#include "dhcpcd-about.h"
#include "dhcpcd-qt.h"

DhcpcdAbout::DhcpcdAbout(DhcpcdQt *parent)
    : QDialog(parent)
{
	this->parent = parent;
	QVBoxLayout *layout;
	resize(300, 200);
	setWindowIcon(DhcpcdQt::getIcon("status", "network-transmit-receive"));
	setWindowTitle(tr("About Network Configurator"));
	QPoint p = QCursor::pos();
	move(p.x(), p.y());

	layout = new QVBoxLayout(this);

	QIcon icon = DhcpcdQt::getIcon("status", "network-transmit-receive");
	QPixmap picon = icon.pixmap(32, 32);
	iconLabel = new QLabel(this);
	iconLabel->setAlignment(Qt::AlignCenter);
	iconLabel->setPixmap(picon);
	layout->addWidget(iconLabel);

	aboutLabel = new QLabel("<h1>Network Configurator "  VERSION "</h1>", this);
	aboutLabel->setAlignment(Qt::AlignCenter);
	layout->addWidget(aboutLabel);
	partLabel = new QLabel(tr("Part of the dhcpcd project"), this);
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

	QHBoxLayout *hbox = new QHBoxLayout();
	layout->addLayout(hbox);
	hbox->addItem(new QSpacerItem(0, 0, QSizePolicy::Expanding, QSizePolicy::Expanding));
	closeButton = new QPushButton(tr("Close"));
	closeButton->setIcon(QIcon::fromTheme("window-close"));
	hbox->addWidget(closeButton);
	connect(closeButton, SIGNAL(clicked()), this, SLOT(close()));
}

void DhcpcdAbout::closeEvent(QCloseEvent *e)
{

	parent->dialogClosed(this);
	QDialog::closeEvent(e);
}
