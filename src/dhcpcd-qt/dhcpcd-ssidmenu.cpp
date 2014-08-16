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

#include <QWidget>
#include <QEvent>
#include <QHBoxLayout>
#include <QRadioButton>
#include <QIcon>
#include <QLabel>
#include <QPixmap>
#include <QProgressBar>

#include "dhcpcd.h"
#include "dhcpcd-qt.h"
#include "dhcpcd-ssidmenu.h"

DhcpcdSsidMenu::DhcpcdSsidMenu(QWidget *parent, DHCPCD_IF *ifp, DHCPCD_WI_SCAN *scan)
    : QWidget(parent, NULL)
{
	int strength;
	QIcon icon;

	this->ifp = ifp;
	this->scan = scan;

	QHBoxLayout *layout = new QHBoxLayout(this);
	button = new QRadioButton(scan->ssid, this);
	button->setChecked(strcmp(scan->ssid, ifp->ssid) == 0);
	layout->addWidget(button);
	if (scan->flags[0] == '\0') {
		icon = DhcpcdQt::getIcon("devices", "network-wireless");
		setToolTip(scan->bssid);
	} else {
		icon = DhcpcdQt::getIcon("status", "network-wireless-encrypted");
		QString tip = QString::fromAscii(scan->bssid);
		tip += " " + QString::fromAscii(scan->flags);
		setToolTip(tip);
	}
	QPixmap picon = icon.pixmap(22, 22);
	licon = new QLabel(this);
	licon->setPixmap(picon);
	layout->addWidget(licon);
	bar = new QProgressBar(this);
	bar->setMinimum(0);
	bar->setMaximum(100);
	if (scan->quality.value == 0)
	    strength = scan->level.average;
	else
	    strength = scan->quality.average;
	bar->setValue(strength < 0 ? 0 : strength > 100 ? 100 : strength);
	layout->addWidget(bar);

	button->installEventFilter(this);
	licon->installEventFilter(this);
	bar->installEventFilter(this);
}

bool DhcpcdSsidMenu::eventFilter(QObject *, QEvent *event)
{

	if (event->type() == QEvent::MouseButtonPress)
		emit selected(ifp, scan);
	return false;
}
