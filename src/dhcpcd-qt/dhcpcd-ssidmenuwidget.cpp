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
#include <QStyle>

#include "dhcpcd.h"
#include "dhcpcd-qt.h"
#include "dhcpcd-ssidmenuwidget.h"

DhcpcdSsidMenuWidget::DhcpcdSsidMenuWidget(QWidget *parent,
    DhcpcdWi *wi, DHCPCD_WI_SCAN *scan)
    : QFrame(parent)
{

	this->wi = wi;
	this->scan = scan;

	QHBoxLayout *layout = new QHBoxLayout(this);
	layout->setContentsMargins(1, 1, 1, 1);
	selicon = new QLabel(this);
	selicon->setMinimumSize(16, 16);
	layout->addWidget(selicon);

	ssid = new QLabel(this);
	ssid->setSizePolicy(QSizePolicy::MinimumExpanding,
	    QSizePolicy::MinimumExpanding);
	layout->addWidget(ssid);

	encicon = new QLabel(this);
	layout->addWidget(encicon);
	layout->setAlignment(encicon, Qt::AlignRight);

	stricon = new QLabel(this);
	layout->addWidget(stricon);
	layout->setAlignment(stricon, Qt::AlignRight);

	setScan(scan);
	installEventFilter(this);

	setStyleSheet("QFrame { border-radius: 4px; } "
	    "QFrame:hover {"
	    "color:palette(highlighted-text);"
	    "background-color:palette(highlight);"
	    "}");
}

DHCPCD_WI_SCAN *DhcpcdSsidMenuWidget::getScan()
{

	return scan;
}

void DhcpcdSsidMenuWidget::setScan(DHCPCD_WI_SCAN *scan)
{
	DHCPCD_WPA *wpa;
	DHCPCD_IF *i;
	QIcon icon;
	QPixmap picon;

	this->scan = scan;
	wpa = wi->getWpa();
	i = dhcpcd_wpa_if(wpa);

	if (i->up && i->ssid && strcmp(scan->ssid, i->ssid) == 0) {
		icon = DhcpcdQt::getIcon("actions", "dialog-ok-apply");
		picon = icon.pixmap(16, 16);
		selicon->setPixmap(picon);
	} else
		selicon->setPixmap(NULL);
	ssid->setText(scan->ssid);
	if (scan->flags[0] == '\0')
		icon = DhcpcdQt::getIcon("status", "dialog-warning");
	else
		icon = DhcpcdQt::getIcon("status",
		    "network-wireless-encrypted");
	picon = icon.pixmap(16, 16);
	encicon->setPixmap(picon);

	if (scan->strength.value > 80)
		icon = DhcpcdQt::getIcon("status",
		    "network-wireless-connected-100");
	else if (scan->strength.value > 55)
		icon = DhcpcdQt::getIcon("status",
		    "network-wireless-connected-75");
	else if (scan->strength.value > 30)
		icon = DhcpcdQt::getIcon("status",
		    "network-wireless-connected-50");
	else if (scan->strength.value > 5)
		icon = DhcpcdQt::getIcon("status",
		    "network-wireless-connected-25");
	else
		icon = DhcpcdQt::getIcon("status",
		    "network-wireless-connected-00");
	picon = icon.pixmap(16, 16);
	stricon->setPixmap(picon);
}

bool DhcpcdSsidMenuWidget::eventFilter(QObject *, QEvent *event)
{

	if (event->type() == QEvent::MouseButtonPress)
		emit triggered();
	return false;
}

void DhcpcdSsidMenuWidget::enterEvent(QEvent *)
{

	ssid->setStyleSheet("color:palette(highlighted-text);");
	emit hovered();
}

void DhcpcdSsidMenuWidget::leaveEvent(QEvent *)
{

	ssid->setStyleSheet("color:palette(text);");
}
