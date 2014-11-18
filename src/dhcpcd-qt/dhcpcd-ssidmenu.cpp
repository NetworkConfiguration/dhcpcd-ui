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
#include "dhcpcd-ssidmenu.h"
#include "dhcpcd-ssidmenuwidget.h"

DhcpcdSsidMenu::DhcpcdSsidMenu(QWidget *parent,
    DhcpcdWi *wi, DHCPCD_WI_SCAN *scan)
    : QWidgetAction(parent)
{

	this->wi = wi;
	this->scan = scan;
}

QWidget *DhcpcdSsidMenu::createWidget(QWidget *parent)
{
	ssidWidget = new DhcpcdSsidMenuWidget(parent, wi, scan);
	connect(ssidWidget, SIGNAL(hovered()), this, SLOT(hover()));
	connect(ssidWidget, SIGNAL(triggered()), this, SLOT(trigger()));
	return ssidWidget;
}

DHCPCD_WI_SCAN *DhcpcdSsidMenu::getScan()
{

	return scan;
}

void DhcpcdSsidMenu::setScan(DHCPCD_WI_SCAN *scan)
{

	this->scan = scan;
	if (ssidWidget)
		ssidWidget->setScan(scan);
}

void DhcpcdSsidMenu::hover()
{

	activate(QAction::Hover);
}

void DhcpcdSsidMenu::trigger()
{

	emit triggered(scan);
}
