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

#include "dhcpcd.h"

class QRadioButton;
class QLabel;
class QProgressBar;
class QWidgetAction;

class DhcpcdWi;

class DhcpcdSsidMenu : public QWidget
{
	Q_OBJECT

public:
	DhcpcdSsidMenu(QWidget *parent, QWidgetAction *wa,
	    DhcpcdWi *wi, DHCPCD_WI_SCAN *scan);
	~DhcpcdSsidMenu() {};

	QWidgetAction *getWidgetAction();
	DHCPCD_WI_SCAN *getScan();
	void setScan(DHCPCD_WI_SCAN *scan);

signals:
	void selected(DHCPCD_WI_SCAN *scan);

private slots:
	bool eventFilter(QObject *obj, QEvent *event);

private:
	QWidgetAction *wa;
	DhcpcdWi *wi;
	DHCPCD_WI_SCAN *scan;

	QRadioButton *button;
	QLabel *licon;
	QProgressBar *bar;
};
